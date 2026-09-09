// Installers to plop in the interrupt handler as-needed, and
// the low-overhead timestamp-counter clock it reads (largely a formalization
// of the `LLRDTSCTimer` pattern in viewer and server).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "Luau/Executor.h"

#include "Luau/Common.h"

#include "lua.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#define SLUA_QUANTA_TSC_X86 1
#elif defined(__aarch64__) && !defined(_MSC_VER)
#define SLUA_QUANTA_TSC_ARM64 1
#endif

#if defined(SLUA_QUANTA_TSC_X86) || defined(SLUA_QUANTA_TSC_ARM64)
#define SLUA_QUANTA_TSC 1
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#if defined(__linux__)
#include <cerrno>
#include <csignal>
#include <ctime>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
// Only newer glibc names the SIGEV_THREAD_ID target, older ones just have the union member
#if defined(__GLIBC__) && !defined(sigev_notify_thread_id)
#define sigev_notify_thread_id _sigev_un._tid
#endif
#endif
#endif

LUAU_FASTFLAGVARIABLE(SLuaThreadedQuantaWatchdog)
LUAU_FASTFLAGVARIABLE(SLuaElevateWatchdogPriority)

namespace Luau
{
namespace Executor
{

#if defined(SLUA_QUANTA_TSC_X86)
static inline uint64_t read_tsc()
{
    // Shouldn't need a fence.
    return __rdtsc();
}

static double measure_seconds_per_tick()
{
    // One-time calibration against lua_clock(), assuming an invariant TSC.
    // Similar to the `ms_sleep(1)` strategy in indra.
    double wall_begin = lua_clock();
    uint64_t tsc_begin = read_tsc();

    // Long enough for a stable ratio, short enough not to matter at setup
    constexpr double kCalibrationWindow = 0.004;
    double wall_end;
    do
    {
        wall_end = lua_clock();
    } while (wall_end - wall_begin < kCalibrationWindow);
    uint64_t tsc_end = read_tsc();

    LUAU_ASSERT(tsc_end > tsc_begin);
    if (tsc_end <= tsc_begin)
        return 0.0;

    return (wall_end - wall_begin) / (double)(tsc_end - tsc_begin);
}
#elif defined(SLUA_QUANTA_TSC_ARM64)
static inline uint64_t read_tsc()
{
    // Generic timer virtual count, readable from EL0 on Linux and macOS.
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

static double measure_seconds_per_tick()
{
    // The architecture publishes the exact frequency, no calibration needed.
    // Apple Silicon reports 24MHz -> ~41.7ns per tick, ~3600 ticks per 150us
    // quanta.
    // TODO: is this the case for _all_ modern ARM64?
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    LUAU_ASSERT(freq != 0);
    return freq != 0 ? 1.0 / (double)freq : 0.0;
}
#endif

#if defined(SLUA_QUANTA_TSC)
static double tsc_quanta_clock()
{
    // `static` so it's only computed on first call.
    static const double seconds_per_tick = measure_seconds_per_tick();
    return (double)read_tsc() * seconds_per_tick;
}
#endif

QuantaClock resolveDefaultQuantaClock()
{
#if defined(SLUA_QUANTA_TSC)
    // Warm up now so any calibration cost lands at
    // provisioner setup rather than inside the first script's run window
    tsc_quanta_clock();
    return tsc_quanta_clock;
#else
    // If we don't have anything better, just use whatever `lua_clock()` uses for the platform.
    return lua_clock;
#endif
}

InterruptInstallPolicy resolveDefaultInterruptInstallPolicy()
{
    return FFlag::SLuaThreadedQuantaWatchdog ? InterruptInstallPolicy::Threaded : InterruptInstallPolicy::Resident;
}

InterruptInstaller::~InterruptInstaller() = default;

namespace
{

/// Stays resident on `cb->interrupt`, triggering the interrupt callback
/// wherever it is possible.
///
/// Expensive on platforms where constantly reading the current cycle count
/// is expensive, but preferred everywhere else (like modern ARM64)
class ResidentInterruptInstaller final : public InterruptInstaller
{
public:
    using InterruptInstaller::InterruptInstaller;

    void installBy(lua_Callbacks* target, double /* deadline */) override
    {
        mTarget = target;
        installNow();
    }

    void installNow() override
    {
        LUAU_ASSERT(mTarget != nullptr);
        mTarget->interrupt = mHandler;
    }

    void cancel() override {}

private:
    // Never pending, the install is immediate
    void uninstallPending() override
    {
        LUAU_ASSERT(false);
    }

    lua_Callbacks* mTarget = nullptr;
};

// How early the watchdog installs the handler, to cover wakeup jitter
constexpr double kWatchdogFireLead = 50e-6;  // 50us

// Elevating the priority helps a lot if we want to stick to a strict schedule.
// If we don't do this, the watchdog may fire much later than we intend because
// the kernel is allowed to take its time waking the thread up.
void elevate_watchdog_thread_priority()
{
#if defined(_WIN32)
    // wait_for granularity is ~1ms at best on Windows, fires will be late regardless
    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL))
    {
        logInfo("InterruptInstaller", "Watchdog thread priority raised to TIME_CRITICAL");
        return;
    }
    logWarn("InterruptInstaller", "Watchdog priority elevation failed: SetThreadPriority error %lu", GetLastError());
#else
#if defined(__linux__)
    // Default timer slack is 50us, the whole fire lead
    if (prctl(PR_SET_TIMERSLACK, 1) != 0)
        logWarn("InterruptInstaller", "Couldn't reduce watchdog timer slack: errno %d", errno);
#endif
    // The lowest RT priority is enough, we only need to preempt SCHED_OTHER threads
    sched_param param = {};
    param.sched_priority = sched_get_priority_min(SCHED_RR);
    int rr_err = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    if (rr_err == 0)
    {
        logInfo("InterruptInstaller", "Watchdog thread scheduling class raised to SCHED_RR");
        return;
    }
#if defined(__linux__)
    // who == 0 is the calling thread under NPTL
    if (setpriority(PRIO_PROCESS, 0, -10) == 0)
    {
        logInfo("InterruptInstaller", "Watchdog thread niced to -10 (SCHED_RR unavailable, errno %d)", rr_err);
        return;
    }
    logWarn("InterruptInstaller", "Watchdog priority elevation failed: SCHED_RR errno %d, setpriority errno %d", rr_err, errno);
#else
    logWarn("InterruptInstaller", "Watchdog priority elevation failed: SCHED_RR errno %d", rr_err);
#endif
#endif
}

/// Uses a background watchdog thread to only set `cb->interrupt` when quanta is up.
///
/// A little more convoluted than the above "resident" installer, but yields a 30% performance
/// boost in math-heavy code on x86-64 and x86-32.
class ThreadedInterruptInstaller final : public InterruptInstaller
{
public:
    ThreadedInterruptInstaller(InterruptCallback handler, QuantaClock quanta_clock)
        : InterruptInstaller(handler)
        , mQuantaClock(quanta_clock)
    {
        mThread = std::thread([this] { loopAndWatch(); });
    }

    ~ThreadedInterruptInstaller() override
    {
        {
            std::lock_guard<std::mutex> guard(mMutex);
            mExit = true;
        }
        mCondition.notify_one();
        mThread.join();
    }

    ThreadedInterruptInstaller(const ThreadedInterruptInstaller&) = delete;
    ThreadedInterruptInstaller& operator=(const ThreadedInterruptInstaller&) = delete;

    void installBy(lua_Callbacks* target, double deadline) override
    {
        // Clear before publishing the deadline, otherwise a fire landing in between
        // would get wiped and the window would never be preempted.
        target->interrupt = nullptr;

        bool wake;
        {
            std::lock_guard<std::mutex> guard(mMutex);
            LUAU_ASSERT(!mPending.load(std::memory_order_relaxed));
            mTarget = target;
            mDeadline = deadline;
            mOverrun = 0.0;
            mPending.store(true, std::memory_order_relaxed);
            // Waking is a syscall and a context switch, so only do it if the
            // thread isn't already due up in time. Otherwise it wakes at its
            // old target, sees this deadline, and sleeps again toward it.
            wake = mIdle || deadline - kWatchdogFireLead < mSleepUntil;
        }
        if (wake)
            mCondition.notify_one();
    }

    void installNow() override
    {
        std::lock_guard<std::mutex> guard(mMutex);
        // But only if it was pending, don't touch it if we already installed.
        if (mPending.load(std::memory_order_relaxed))
            mTarget->interrupt = mHandler;
    }

    void cancel() override
    {
        std::lock_guard<std::mutex> guard(mMutex);
        mPending.store(false, std::memory_order_relaxed);
    }

    double getInstallOverrun() override
    {
        std::lock_guard<std::mutex> guard(mMutex);
        return mOverrun;
    }

    WatchdogStats getStats() override
    {
        std::lock_guard<std::mutex> guard(mMutex);
        return mStats;
    }

private:
    void uninstallPending() override
    {
        // The inline check that got us here was lockless. Only installBy()
        // goes false -> true, on this thread, so a false reading there is
        // stable, but a true one needs the lock to make sure the fire didn't
        // land in between.
        std::lock_guard<std::mutex> guard(mMutex);
        if (mPending.load(std::memory_order_relaxed))
            mTarget->interrupt = nullptr;
    }

    void loopAndWatch()
    {
        if (FFlag::SLuaElevateWatchdogPriority)
            elevate_watchdog_thread_priority();

        std::unique_lock<std::mutex> lock(mMutex);
        while (!mExit)
        {
            if (!mPending.load(std::memory_order_relaxed))
            {
                // cancel() doesn't notify, a stale deadline lands us here too.
                // Just sit until we're notified.
                mIdle = true;
                mCondition.wait(lock);
                mIdle = false;
                mStats.wakes++;
                continue;
            }

            const double remaining = mDeadline - mQuantaClock();
            if (remaining <= kWatchdogFireLead)
            {
                release_store(&mTarget->interrupt, mHandler);
                mPending.store(false, std::memory_order_relaxed);
                mOverrun = std::max(0.0, -remaining);

                double lateness = kWatchdogFireLead - remaining;
                mStats.latenessSum += lateness;
                mStats.latenessMin = mStats.fires == 0 ? lateness : std::min(mStats.latenessMin, lateness);
                mStats.latenessMax = std::max(mStats.latenessMax, lateness);
                mStats.fires++;
                if (mOverrun > 0.0)
                    mStats.lateFires++;
                continue;
            }
            mSleepUntil = mDeadline - kWatchdogFireLead;
            mCondition.wait_for(lock, std::chrono::duration<double>(remaining - kWatchdogFireLead));
            mStats.wakes++;
        }
    }

    QuantaClock mQuantaClock = nullptr;

    std::mutex mMutex;
    std::condition_variable mCondition;
    lua_Callbacks* mTarget = nullptr;
    double mDeadline = 0.0;
    // Seconds past mDeadline that the last fire landed
    double mOverrun = 0.0;
    WatchdogStats mStats;
    // Moment we expect the thread to wake back up
    double mSleepUntil = 0.0;
    // Are we currently sitting idle waiting for another script to work on
    bool mIdle = true;
    // Should we start tearing down the watchdog
    bool mExit = false;

    std::thread mThread;
};

#if defined(__linux__)
// How much lead-time to try to have the timer trigger before the actual deadline.
// Timers are an inexact mechanism, so we do want a little bit of slop.
constexpr double kSignalFireLead = 10e-6;

// signal number that shouldn't be taken by indra, value not important
// this is only a function because `SIGRTMIN` does a function call :(
static int get_watchdog_signal()
{
    return SIGRTMIN + 4;
}

// One sigaction per process, however many installers get made
static std::once_flag sSignalHandlerRegistered;

/// A POSIX timer bound to the script thread delivers a signal at the deadline,
/// and the signal handler installs `cb->interrupt` from the script thread itself.
/// Nothing has to be woken up, so nothing has to win a scheduling decision to
/// land on time. That makes it the pick where SCHED_RR isn't on the table.
///
/// Everything here runs on the script thread. The only concurrency is the
/// signal handler landing between two of our own instructions, so the atomics
/// and fences are there for the compiler, not for another core.
class SignalInterruptInstaller final : public InterruptInstaller
{
public:
    SignalInterruptInstaller(InterruptCallback handler, QuantaClock quanta_clock)
        : InterruptInstaller(handler)
        , mQuantaClock(quanta_clock)
    {
        std::call_once(sSignalHandlerRegistered, [] {
            const int signum = get_watchdog_signal();

            // Mono and friends pick their RT signals by searching for SIG_DFL, so
            // anything else here is someone we'd be clobbering.
            struct sigaction current = {};
            int query_rc = sigaction(signum, nullptr, &current);
            LUAU_ASSERT_ALWAYS(query_rc == 0);
            LUAU_ASSERT_ALWAYS(current.sa_handler == SIG_DFL);

            struct sigaction action = {};
            action.sa_sigaction = onSignal;
            // SA_RESTART so no syscall in the host sees an EINTR from us
            action.sa_flags = SA_SIGINFO | SA_RESTART;
            sigemptyset(&action.sa_mask);
            // Without our handler the first timer expiry would terminate the process
            int install_rc = sigaction(signum, &action, nullptr);
            LUAU_ASSERT_ALWAYS(install_rc == 0);
        });
    }

    ~SignalInterruptInstaller() override
    {
        // Any signal still queued for it is dropped along with the timer
        if (mHaveTimer)
            timer_delete(mTimer);
    }

    SignalInterruptInstaller(const SignalInterruptInstaller&) = delete;
    SignalInterruptInstaller& operator=(const SignalInterruptInstaller&) = delete;

    void installBy(lua_Callbacks* target, double deadline) override
    {
        target->interrupt = nullptr;
        LUAU_ASSERT(!mPending.load(std::memory_order_relaxed));
        ensureTimer();

        mTarget = target;
        mDeadline = deadline;
        mOverrun = 0.0;

        // Publish the window before the signal handler can see pending
        std::atomic_signal_fence(std::memory_order_release);
        mPending.store(true, std::memory_order_relaxed);
        // If the timer can't deliver, whether it's too late already or we never
        // got one, the install happens here instead.
        if (!requestSignalIn(deadline - kSignalFireLead - mQuantaClock()))
            deadlineInstall();
    }

    void installNow() override
    {
        // But only if it was pending, don't touch it if we already installed.
        if (mPending.load(std::memory_order_relaxed))
            mTarget->interrupt = mHandler;
    }

    void cancel() override
    {
        // Once the install has happened there's no signal left to withdraw, so
        // a preempted window only pays for the request. The exchange can't be
        // split by the signal handler.
        if (mPending.exchange(false, std::memory_order_relaxed))
        {
            if (!mHaveTimer)
                return;
            itimerspec spec = {};
            timer_settime(mTimer, 0, &spec, nullptr);
        }
    }

    double getInstallOverrun() override
    {
        return mOverrun;
    }

    WatchdogStats getStats() override
    {
        return mStats;
    }

private:
    void uninstallPending() override
    {
        mTarget->interrupt = nullptr;
        // The signal can land between the inline pending check and that store,
        // in which case we just wiped a real install. Recheck and put it back.
        std::atomic_signal_fence(std::memory_order_seq_cst);
        if (!mPending.load(std::memory_order_relaxed))
            mTarget->interrupt = mHandler;
    }

    static void onSignal(int, siginfo_t* info, void*)
    {
        // If we're not being woken up for a timer, this isn't for us.
        if (info->si_code != SI_TIMER)
            return;
        // Get a reference to the specific interrupt installer
        auto* self = static_cast<SignalInterruptInstaller*>(info->si_value.sival_ptr);
        // A signal that was already queued when its window was cancelled still
        // gets delivered
        if (!self->mPending.load(std::memory_order_relaxed))
            return;
        std::atomic_signal_fence(std::memory_order_acquire);
        self->deadlineInstall();
    }

    // The install installBy() promised, plus the timing for it. Usually from the
    // signal handler, so only atomics, plain stores and the clock: it has to
    // stay async-signal-safe.
    void deadlineInstall()
    {
        mTarget->interrupt = mHandler;
        mPending.store(false, std::memory_order_relaxed);

        const double remaining = mDeadline - mQuantaClock();
        mOverrun = std::max(0.0, -remaining);

        double lateness = kSignalFireLead - remaining;
        mStats.latenessSum += lateness;
        mStats.latenessMin = mStats.fires == 0 ? lateness : std::min(mStats.latenessMin, lateness);
        mStats.latenessMax = std::max(mStats.latenessMax, lateness);
        mStats.fires++;
        if (mOverrun > 0.0)
            mStats.lateFires++;
    }

    // The timer is bound to a thread, so it follows whichever one opens windows.
    void ensureTimer()
    {
        pid_t tid = (pid_t)syscall(SYS_gettid);
        // We have a timer and it's for this thread
        if (mHaveTimer && mTimerThread == tid)
            return;

        if (mHaveTimer)
        {
            // This should never be used cross-thread!
            LUAU_ASSERT(mTimerThread == tid);
            timer_delete(mTimer);
            mHaveTimer = false;
        }

        mTimerThread = tid;

        struct sigevent event = {};
        event.sigev_notify = SIGEV_THREAD_ID;
        event.sigev_signo = get_watchdog_signal();
        event.sigev_value.sival_ptr = this;
        event.sigev_notify_thread_id = tid;
        mHaveTimer = timer_create(CLOCK_MONOTONIC, &event, &mTimer) == 0;
        if (!mHaveTimer)
        {
            logWarn("InterruptInstaller", "Couldn't create the watchdog timer (errno %d), installing at window start instead", errno);
            return;
        }

        // Slack is a property of the thread that sets the timer and defaults to
        // 50us, which would swallow the lead several times over. This tightens
        // every timer on the script thread, not just ours.
        // I'm a little squeamish about setting this since it's in the main thread and process-global,
        // so let's see if it's a problem in practice...
        // if (prctl(PR_SET_TIMERSLACK, 1) != 0)
        //     logWarn("InterruptInstaller", "Couldn't reduce script thread timer slack: errno %d", errno);
    }

    // Ask for one signal `seconds` from now. False when the timer can't deliver
    // it: there is no timer, the moment has already passed, or the kernel
    // refused. A zero it_value would cancel the timer instead of firing it,
    // hence the floor.
    bool requestSignalIn(double seconds)
    {
        if (!mHaveTimer || seconds <= 0.0)
            return false;

        itimerspec spec = {};
        spec.it_value.tv_sec = (time_t)seconds;
        spec.it_value.tv_nsec = std::max(1L, (long)((seconds - (double)spec.it_value.tv_sec) * 1e9));
        return timer_settime(mTimer, 0, &spec, nullptr) == 0;
    }

    QuantaClock mQuantaClock = nullptr;

    lua_Callbacks* mTarget = nullptr;
    double mDeadline = 0.0;
    // Seconds past mDeadline that the last fire landed
    double mOverrun = 0.0;
    WatchdogStats mStats;

    timer_t mTimer{};
    bool mHaveTimer = false;
    // Thread the timer signals, 0 until the first window
    pid_t mTimerThread = 0;
};
#endif

} // namespace

std::unique_ptr<InterruptInstaller> createInterruptInstaller(InterruptInstallPolicy policy, InterruptCallback handler, QuantaClock quanta_clock)
{
    if (policy == InterruptInstallPolicy::Default)
        policy = resolveDefaultInterruptInstallPolicy();
    if (policy == InterruptInstallPolicy::Signal)
    {
#if defined(__linux__)
        return std::make_unique<SignalInterruptInstaller>(handler, quanta_clock);
#else
        logWarn("InterruptInstaller", "Signal install policy is Linux only, using the watchdog thread instead");
        policy = InterruptInstallPolicy::Threaded;
#endif
    }
    if (policy == InterruptInstallPolicy::Threaded)
        return std::make_unique<ThreadedInterruptInstaller>(handler, quanta_clock);
    return std::make_unique<ResidentInterruptInstaller>(handler);
}

} // namespace Executor
} // namespace Luau
