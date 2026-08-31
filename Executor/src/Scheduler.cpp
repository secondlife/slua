// Quanta preemption scheduling: the watchdog policies that arm the VM's
// interrupt pointer, and the low-overhead timestamp-counter clock they read.
// The clock is largely a formalization of the `LLRDTSCTimer` pattern in
// viewer and server.
#include <algorithm>
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
#include <sys/resource.h>
#endif
#endif

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
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    LUAU_ASSERT(freq != 0);
    return freq != 0 ? 1.0 / (double)freq : 0.0;
}
#endif

#if defined(SLUA_QUANTA_TSC)
static double tsc_quanta_clock([[maybe_unused]] lua_State* L)
{
    static const double seconds_per_tick = measure_seconds_per_tick();
    return (double)read_tsc() * seconds_per_tick;
}
#endif

#if !defined(SLUA_QUANTA_TSC)
static double fallback_quanta_clock([[maybe_unused]] lua_State* L)
{
    return lua_clock();
}
#endif

lua_clockProvider resolveDefaultQuantaClock()
{
#if defined(SLUA_QUANTA_TSC)
    // Warm up now so any calibration cost lands at
    // provisioner setup rather than inside the first script's run window
    tsc_quanta_clock(nullptr);
    return tsc_quanta_clock;
#else
    return fallback_quanta_clock;
#endif
}

QuantaWatchdog::~QuantaWatchdog() = default;

namespace
{

class ImmediateQuantaWatchdog final : public QuantaWatchdog
{
public:
    using QuantaWatchdog::QuantaWatchdog;

    void arm(lua_Callbacks* target, double /* deadline */) override
    {
        mTarget = target;
        fireNow();
    }

    void disarm() override {}

    void fireNow() override
    {
        LUAU_ASSERT(mTarget != nullptr);
        mTarget->interrupt = mHandler;
    }

    bool clearIfArmed() override
    {
        // The install always stands: the resident handler is the mechanism.
        return false;
    }

private:
    lua_Callbacks* mTarget = nullptr;
};

// How far ahead of the deadline the watch loop delivers its fire.
// Allows some slop to
constexpr double kWatchdogFireLead = 50e-6;  // 50us

// Best-effort hoist of the watch loop above the timesharing class, so a
// deadline wakeup preempts a busy script thread immediately instead of
// waiting out the kernel's wakeup-preemption granularity (multi-ms against
// a spinning peer, enough to draw false punishments). Failure is survivable
// -- fires still happen, just late -- but the outcome must be logged, or
// "the flag didn't help" is indistinguishable from a silent EPERM.
void elevate_watchdog_priority()
{
#if defined(_WIN32)
    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL))
    {
        logInfo("QuantaWatchdog", "Watchdog thread priority raised to TIME_CRITICAL");
        return;
    }
    logWarn("QuantaWatchdog", "Watchdog priority elevation failed: SetThreadPriority error %lu", GetLastError());
#else
    // The lowest RT priority is enough: the goal is class-based wakeup
    // preemption over SCHED_OTHER threads, not competition within RT.
    sched_param param = {};
    param.sched_priority = sched_get_priority_min(SCHED_RR);
    int rr_err = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    if (rr_err == 0)
    {
        logInfo("QuantaWatchdog", "Watchdog thread scheduling class raised to SCHED_RR");
        return;
    }
#if defined(__linux__)
    // SCHED_RR needs CAP_SYS_NICE/RLIMIT_RTPRIO; a negative nice at least
    // lowers the wakeup-preemption bar. who == 0 targets the calling thread
    // under NPTL.
    if (setpriority(PRIO_PROCESS, 0, -10) == 0)
    {
        logInfo("QuantaWatchdog", "Watchdog thread niced to -10 (SCHED_RR unavailable, errno %d)", rr_err);
        return;
    }
    logWarn("QuantaWatchdog", "Watchdog priority elevation failed: SCHED_RR errno %d, setpriority errno %d", rr_err, errno);
#else
    logWarn("QuantaWatchdog", "Watchdog priority elevation failed: SCHED_RR errno %d", rr_err);
#endif
#endif
}

class ThreadedQuantaWatchdog final : public QuantaWatchdog
{
public:
    ThreadedQuantaWatchdog(InterruptCallback handler, lua_clockProvider quanta_clock)
        : QuantaWatchdog(handler)
        , mQuantaClock(quanta_clock)
    {
        mThread = std::thread([this] { watch(); });
    }

    ~ThreadedQuantaWatchdog() override
    {
        {
            std::lock_guard<std::mutex> guard(mMutex);
            mExit = true;
        }
        mCondition.notify_one();
        mThread.join();
    }

    ThreadedQuantaWatchdog(const ThreadedQuantaWatchdog&) = delete;
    ThreadedQuantaWatchdog& operator=(const ThreadedQuantaWatchdog&) = delete;

    void arm(lua_Callbacks* target, double deadline) override
    {
        // The null store must precede the armed publish below: stored after,
        // a fire delivered in between would be wiped with the window's
        // one-shot already spent, leaving the window unpreemptable.
        target->interrupt = nullptr;

        {
            std::lock_guard<std::mutex> guard(mMutex);
            // Run windows are serial per provisioner, and every path to a new
            // begin passes through disarm (or a delivered fire), so an armed
            // slot here means concurrent windows -- which would silently
            // overwrite this deadline and lose the other window's preemption.
            LUAU_ASSERT(!mArmed);
            mTarget = target;
            mDeadline = deadline;
            mArmed = true;
        }
        // The watch loop sleeps indefinitely while unarmed (and on a stale
        // deadline after a disarm), so every arm must wake it. Notified
        // outside the lock so the woken thread doesn't stall on it.
        mCondition.notify_one();
    }

    void disarm() override
    {
        std::lock_guard<std::mutex> guard(mMutex);
        mArmed = false;
    }

    void fireNow() override
    {
        std::lock_guard<std::mutex> guard(mMutex);
        // Unarmed mid-window means the deadline fire was already delivered,
        // so the handler is resident and there is nothing to add. The lock
        // makes mTarget safe to read from a cross-thread producer and orders
        // us against the fire.
        if (mArmed)
            mTarget->interrupt = mHandler;
    }

    bool clearIfArmed() override
    {
        std::lock_guard<std::mutex> guard(mMutex);
        // The fire clears mArmed under this same mutex, so armed means the
        // fire is still coming and will re-install; unarmed means the install
        // being questioned IS the fire, and it must stand.
        if (!mArmed)
            return false;
        mTarget->interrupt = nullptr;
        return true;
    }

    WatchdogStats getStats() override
    {
        std::lock_guard<std::mutex> guard(mMutex);
        return mStats;
    }

private:
    void watch()
    {
        if (FFlag::SLuaElevateWatchdogPriority)
            elevate_watchdog_priority();

        std::unique_lock<std::mutex> lock(mMutex);
        while (!mExit)
        {
            if (!mArmed)
            {
                // Nothing scheduled: block until an arm (or exit) notifies.
                // A disarm doesn't notify, so this can also be reached by a
                // stale deadline expiring after its window closed -- harmless,
                // we just go back to sleep here.
                mCondition.wait(lock);
                continue;
            }

            double remaining = mDeadline - mQuantaClock(nullptr);
            if (remaining <= kWatchdogFireLead)
            {
                // Deliver the one-shot while holding the lock, so it can
                // only target the currently-armed window's VM. The VM
                // reads the pointer unsynchronized by design (lua.h's
                // interrupt contract).
                mTarget->interrupt = mHandler;
                mArmed = false;

                double lateness = kWatchdogFireLead - remaining;
                mStats.latenessSum += lateness;
                mStats.latenessMin = mStats.fires == 0 ? lateness : std::min(mStats.latenessMin, lateness);
                mStats.latenessMax = std::max(mStats.latenessMax, lateness);
                mStats.fires++;
                continue;
            }
            mCondition.wait_for(lock, std::chrono::duration<double>(remaining - kWatchdogFireLead));
        }
    }

    lua_clockProvider mQuantaClock = nullptr;

    std::mutex mMutex;
    std::condition_variable mCondition;
    lua_Callbacks* mTarget = nullptr;
    double mDeadline = 0.0;
    WatchdogStats mStats;
    bool mArmed = false;
    bool mExit = false;

    std::thread mThread;
};

} // namespace

std::unique_ptr<QuantaWatchdog> createImmediateQuantaWatchdog(InterruptCallback handler)
{
    return std::make_unique<ImmediateQuantaWatchdog>(handler);
}

std::unique_ptr<QuantaWatchdog> createQuantaWatchdog(InterruptCallback handler, lua_clockProvider quanta_clock)
{
    return std::make_unique<ThreadedQuantaWatchdog>(handler, quanta_clock);
}

} // namespace Executor
} // namespace Luau
