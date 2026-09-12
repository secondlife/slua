// ServerLua: scaffolding for driving a Script from a test, shared by the
// executor tests and the golden fixture ones.
#pragma once

#include "lua.h"
#include "lualib.h"
#include "Luau/Executor.h"
#include "Luau/Script.h"
#include "llsl.h"

#include "Luau/Compiler.h"

#include "doctest.h"
#include "ScopedFlags.h"

#ifdef LUAU_USE_TAILSLIDE
#include "Luau/LSLCompiler.h"
#endif

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

// A virtual quanta clock that advances by clock_step on every reading. The
// quanta clock takes no lua_State, so the fake finds its host through
// `current`: last constructed wins, and no test steps the clock while two
// hosts are alive.
struct FakeQuantaClock
{
    double clock = 0.0;
    double clock_step = 0.0;

    FakeQuantaClock()
        : previous(current)
    {
        current = this;
    }

    ~FakeQuantaClock()
    {
        current = previous;
    }

    FakeQuantaClock(const FakeQuantaClock&) = delete;
    FakeQuantaClock& operator=(const FakeQuantaClock&) = delete;

    static double read()
    {
        LUAU_ASSERT(current != nullptr);
        current->clock += current->clock_step;
        return current->clock;
    }

private:
    FakeQuantaClock* previous;
    inline static FakeQuantaClock* current = nullptr;
};

// Provisioner with deterministic fakes: the virtual quanta clock above, plus
// capture of print output and dynamic handler registrations.
struct TestProvisioner : Luau::Executor::Provisioner<>, FakeQuantaClock
{
    // The script-visible stopwatch LLTimers schedules against. Deliberately
    // separate from the quanta clock above, which advances on every reading.
    double script_clock = 0.0;
    double last_timer_interval = -1.0;
    std::vector<std::string> printed;
    std::vector<std::string> registrations;

    // Subclasses pass their own callbacks, composed off makeCallbacks()
    explicit TestProvisioner(const Luau::Executor::HostCallbacks& callbacks = makeCallbacks())
        : Luau::Executor::Provisioner<>(callbacks)
    {
    }

    static Luau::Executor::HostCallbacks makeCallbacks()
    {
        Luau::Executor::HostCallbacks callbacks;
        callbacks.clockProvider = script_clock_provider;
        callbacks.performanceClockProvider = script_clock_provider;
        callbacks.setTimerEventCb = set_timer_event;
        callbacks.eventHandlerRegistrationCb = handler_registration;
        callbacks.quantaClockProvider = FakeQuantaClock::read;
        // The fake clock only moves when the test says, no use for a watchdog thread
        callbacks.interruptInstallPolicy = Luau::Executor::InterruptInstallPolicy::Resident;
        callbacks.populateEnvironment = populate_environment;
        return callbacks;
    }

    static void populate_environment(Luau::Executor::IEnvironment& environment, lua_State* L)
    {
        lua_pushcfunction(L, capture_print, "capture_print");
        lua_setglobal(L, "print");

        lua_pushcfunction(L, lua_break, "preempt");
        lua_setglobal(L, "preempt");

        lua_pushcfunction(L, jump_clock, "jump_clock");
        lua_setglobal(L, "jump_clock");

        lua_pushcfunction(L, gc_count, "gc_count");
        lua_setglobal(L, "gc_count");
    }

    static double script_clock_provider(lua_State* L)
    {
        return TestProvisioner::of(L).script_clock;
    }

    // In real usage this would schedule a timer event with the sim; here we
    // just record what the VM asked for and drive ticks by hand.
    static void set_timer_event(lua_State* L, double interval)
    {
        TestProvisioner::of(L).last_timer_interval = interval;
    }

    // Recovers the host the way a real host C callback has to: from the
    // lua_State the VM handed it.
    static TestProvisioner& of(lua_State* L)
    {
        Luau::Executor::Script* executor = Luau::Executor::Script::fromLuaState(L);
        LUAU_ASSERT(executor != nullptr);
        return static_cast<TestProvisioner&>(executor->getProvisioner());
    }

    // An engine pause the executor has no bracket for: time passes with no
    // exclusion banked, as if the host got descheduled mid-execution.
    static int jump_clock(lua_State* L)
    {
        of(L).clock += luaL_checknumber(L, 1);
        return 0;
    }

    static int capture_print(lua_State* L)
    {
        TestProvisioner::of(L).printed.emplace_back(luaL_checkstring(L, 1));
        return 0;
    }

    // Heap size in KB as the script sees it mid-window
    static int gc_count(lua_State* L)
    {
        lua_pushnumber(L, lua_gc(L, LUA_GCCOUNT, 0));
        return 1;
    }

    // Keeps the script's handler register current the way a real host does,
    // besides logging the registration
    static bool handler_registration(lua_State* L, const char* event_name, bool registered)
    {
        uint64_t bit = Luau::lslEventBit(Luau::lslEventIndex(event_name));
        if (bit == 0)
            return false;
        Luau::Executor::Script* script = Luau::Executor::Script::fromLuaState(L);
        LUAU_ASSERT(script != nullptr);
        uint64_t handlers = script->getEventHandlers();
        script->setEventHandlers(registered ? handlers | bit : handlers & ~bit);
        TestProvisioner::of(L).registrations.push_back(std::string(registered ? "+" : "-") + event_name);
        return true;
    }
};

// Owns the asset bytes an ImageConfig only borrows, so the config is minted on
// conversion rather than held with a pointer that a move could invalidate.
struct TestAsset
{
    std::string bytes;

    operator Luau::Executor::ImageConfig() const
    {
        Luau::Executor::ImageConfig config;
        config.asset = bytes.data();
        config.assetSize = bytes.size();
        config.name = "test_script";
        return config;
    }

    // The bytecode actually present. Not what a script is billed for unless
    // the header declares no charged size of its own.
    size_t bytecodeSize() const
    {
        Luau::BytecodeHeader header;
        size_t bytecode_start = 0;
        REQUIRE(Luau::readBytecodeHeader(bytes.data(), bytes.size(), header, bytecode_start));
        return bytes.size() - bytecode_start;
    }

    // What a script instantiated from this asset is billed for, by the same
    // rule Image::build() applies
    size_t chargedSize() const
    {
        Luau::BytecodeHeader header;
        size_t bytecode_start = 0;
        REQUIRE(Luau::readBytecodeHeader(bytes.data(), bytes.size(), header, bytecode_start));
        return header.chargedBytecodeSize != 0 ? header.chargedBytecodeSize : bytes.size() - bytecode_start;
    }
};

// Compiles source into what a host would have stored, the same way the real
// producers do. The flavor rides in the header from here on, so nothing
// downstream has to be told it a second time.
inline TestAsset compileTestAsset(const char* source, bool is_lsl = false, uint32_t api_version = 0)
{
#ifdef LUAU_USE_TAILSLIDE
    if (is_lsl)
        return TestAsset{compileLSLAssetOrThrow(source, api_version)};
#endif
    return TestAsset{Luau::compileAssetOrThrow(source, api_version)};
}

inline Luau::Executor::ScriptConfig makeScriptConfig()
{
    Luau::Executor::ScriptConfig config;
    config.scriptId = "test_script";
    return config;
}

// A status the caller didn't expect is almost always a fault, and the script's
// own account of it is the only thing that says why. doctest's INFO only lives
// as long as its scope, so this has to sit beside the assert it explains.
#define CAPTURE_RUN_RESULT(exec, result)                                                                                                   \
    INFO("status ", (int)(result).status, ", fault ", (int)(exec).getFaultKind(), ": ", (exec).getFaultString(), " ",                      \
        (exec).getExtendedFaultString())

// One-line drivers for a single call inside its own run window.
inline Luau::Executor::RunResult dispatchRaw(Luau::Executor::Script& exec, int event,
    Luau::Executor::PushArgsFn push_args = nullptr, void* ctx = nullptr, double quanta = 1.0)
{
    Luau::Executor::RunWindow window(exec, quanta);
    return exec.callEventHandler(event, push_args, ctx);
}

inline Luau::Executor::RunResult resumeRaw(Luau::Executor::Script& exec, double quanta = 1.0)
{
    Luau::Executor::RunWindow window(exec, quanta);
    return exec.resumeEventHandler();
}

inline Luau::Executor::RunResult dispatch(Luau::Executor::Script& exec, int event,
    Luau::Executor::HandlerRunStatus expect = Luau::Executor::HandlerRunStatus::Ok, Luau::Executor::PushArgsFn push_args = nullptr,
    void* ctx = nullptr, double quanta = 1.0)
{
    Luau::Executor::RunResult result = dispatchRaw(exec, event, push_args, ctx, quanta);
    const char* event_name = Luau::lslEventName(event);
    INFO("event ", event, " ", std::string(event_name ? event_name : "?"));
    CAPTURE_RUN_RESULT(exec, result);
    REQUIRE(result.status == expect);
    return result;
}

inline Luau::Executor::RunResult resume(
    Luau::Executor::Script& exec, double quanta = 1.0, Luau::Executor::HandlerRunStatus expect = Luau::Executor::HandlerRunStatus::Ok)
{
    Luau::Executor::RunResult result = resumeRaw(exec, quanta);
    CAPTURE_RUN_RESULT(exec, result);
    REQUIRE(result.status == expect);
    return result;
}

// Runs the staged SLua main function (or a resumable handler) to completion,
// opening a fresh run window before every resume.
inline Luau::Executor::RunResult resumeToCompletion(Luau::Executor::Script& exec, double quanta,
    Luau::Executor::HandlerRunStatus expect = Luau::Executor::HandlerRunStatus::Ok, int* preemptions = nullptr)
{
    Luau::Executor::RunResult result{Luau::Executor::HandlerRunStatus::Preempted, 0};
    while (result.status == Luau::Executor::HandlerRunStatus::Preempted)
    {
        // Bank-and-zero like a real host would
        exec.setSleep(0.0f);
        result = resumeRaw(exec, quanta);
        if (preemptions && result.status == Luau::Executor::HandlerRunStatus::Preempted)
            ++(*preemptions);
    }
    CAPTURE_RUN_RESULT(exec, result);
    REQUIRE(result.status == expect);
    return result;
}

// Whole-log comparison for the host's capture vectors (printed, registrations)
inline void checkCapture(const std::vector<std::string>& actual, std::initializer_list<const char*> expected)
{
    REQUIRE(actual.size() == expected.size());
    // A failed REQUIRE can't unwind under DOCTEST_CONFIG_NO_EXCEPTIONS, so
    // don't walk past what was actually captured when the counts mismatch.
    size_t index = 0;
    for (const char* want : expected)
    {
        if (index >= actual.size())
            break;
        CAPTURE(index);
        CHECK_EQ(actual[index], std::string(want));
        ++index;
    }
}

// serializeState()/restoreState() with success asserted, for tests where the
// round trip itself is not what's under test
inline std::string serialize(Luau::Executor::Script& exec)
{
    std::string payload;
    REQUIRE(exec.serializeState(payload));
    return payload;
}

inline void restore(Luau::Executor::Script& exec, const std::string& payload)
{
    REQUIRE(exec.restoreState(payload.data(), payload.size()));
}

// Reads an integer global off a script's instance
inline int readIntGlobal(Luau::Executor::Script& script, const char* name)
{
    lua_State* instance = script.getInstanceState();
    lua_getglobal(instance, name);
    int value = lua_tointeger(instance, -1);
    lua_pop(instance, 1);
    return value;
}

// Bundles a TestProvisioner with the script it provisions, so tests can stand a
// script up in one line (default 1:1:1 topology via provisionScript(); sharing
// tests compose the factories directly). loadDefaultState()/reset()/
// restoreState() stay explicit in the tests since lifecycle ordering is
// usually part of what is under test.
struct TestScript
{
    TestProvisioner host;
    std::shared_ptr<Luau::Executor::Script> owned;
    Luau::Executor::Script& exec;

    explicit TestScript(const TestAsset& asset)
        : owned(host.provisionScript(asset, makeScriptConfig()))
        , exec(*owned)
    {
        REQUIRE(owned != nullptr);
    }

    // Compiles `source` first (with the LSL compiler when is_lsl)
    explicit TestScript(const char* source, bool is_lsl = false, uint32_t api_version = 0)
        : TestScript(compileTestAsset(source, is_lsl, api_version))
    {
    }

    // Initial load with the failure asserted, for tests not exercising it
    void loadDefaultState()
    {
        REQUIRE(exec.loadDefaultState());
    }

    // reset() with the failure asserted, for tests not exercising reset itself
    void reset()
    {
        REQUIRE(exec.reset());
    }

    // Load and run the main function, which must complete in one window. For
    // LSL that's the constructor; state_entry is still the host's to dispatch.
    void start()
    {
        loadDefaultState();
        resume(exec);
    }
};

// Every test runs under the SLua feature flags, and leaves the process-global
// log hook unset for the next test
struct SLuaFixture
{
    ScopedSLuaFlags slua_flags;

    ~SLuaFixture()
    {
        Luau::Executor::logCallback() = nullptr;
    }
};
