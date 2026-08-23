// This is largely a copy of the lscript_execute_luau integration tests from
// `server`, but with the caveat that I allowed AI to rewrite them to fit the new
// API surface and not depend on `tut`. We still have the existing integration
// tests that will cover any deviation _anyway_, so... this is all just gravy.
#include "lua.h"
#include "lualib.h"
#include "Luau/Executor.h"
#include "Luau/Script.h"
#include "llsl.h"

#include "Luau/Compiler.h"

#include "doctest.h"
#include "ScopedFlags.h"

// For the handler thread's stack and CallInfo capacities, which the API surface
// has no reason to expose
#include "../VM/src/lstate.h"

#ifdef LUAU_USE_TAILSLIDE
#include "Luau/LSLCompiler.h"
#endif

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Luau::Executor;

LUAU_FASTFLAG(SLuaEagerWeakClear)

// Provisioner with deterministic fakes: a virtual clock that advances by
// clock_step on every reading, plus capture of print output and dynamic
// handler registrations.
struct TestProvisioner : Provisioner<>
{
    double clock = 0.0;
    double clock_step = 0.0;
    // The script-visible stopwatch LLTimers schedules against. Deliberately
    // separate from the quanta clock above, which advances on every reading.
    double script_clock = 0.0;
    double last_timer_interval = -1.0;
    std::vector<std::string> printed;
    std::vector<std::string> registrations;

    // Subclasses pass their own callbacks, composed off makeCallbacks()
    explicit TestProvisioner(const HostCallbacks& callbacks = makeCallbacks())
        : Provisioner<>(callbacks)
    {
    }

    static HostCallbacks makeCallbacks()
    {
        HostCallbacks callbacks;
        callbacks.clockProvider = script_clock_provider;
        callbacks.performanceClockProvider = script_clock_provider;
        callbacks.setTimerEventCb = set_timer_event;
        callbacks.eventHandlerRegistrationCb = handler_registration;
        callbacks.quantaClockProvider = quanta_clock;
        callbacks.populateEnvironment = populate_environment;
        return callbacks;
    }

    static void populate_environment(IEnvironment& environment, lua_State* L)
    {
        lua_pushcfunction(L, capture_print, "capture_print");
        lua_setglobal(L, "print");

        lua_pushcfunction(L, lua_break, "preempt");
        lua_setglobal(L, "preempt");

        lua_pushcfunction(L, simulate_gc_step, "simulate_gc_step");
        lua_setglobal(L, "simulate_gc_step");

        lua_pushcfunction(L, jump_clock, "jump_clock");
        lua_setglobal(L, "jump_clock");
    }

    static double quanta_clock(lua_State* L)
    {
        TestProvisioner& host = of(L);
        host.clock += host.clock_step;
        return host.clock;
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
        Script* executor = Script::fromLuaState(L);
        LUAU_ASSERT(executor != nullptr);
        return static_cast<TestProvisioner&>(executor->getProvisioner());
    }

    // Invoke the GC callbacks as if GC is happening, without actually triggering it.
    static int simulate_gc_step(lua_State* L)
    {
        double cost = luaL_checknumber(L, 1);
        int post_state = luaL_optinteger(L, 2, 0);
        void (*interrupt)(lua_State*, int) = lua_callbacks(L)->interrupt;
        REQUIRE((interrupt != nullptr));
        interrupt(L, 0);
        of(L).clock += cost;
        interrupt(L, post_state);
        return 0;
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

    static bool handler_registration(lua_State* L, const char* event_name, bool registered)
    {
        TestProvisioner::of(L).registrations.push_back(std::string(registered ? "+" : "-") + event_name);
        return true;
    }
};

static ImageConfig makeImageConfig(const std::string& bytecode, bool is_lsl = false, uint32_t api_version = 0)
{
    ImageConfig config;
    config.bytecode = bytecode.data();
    config.bytecodeSize = bytecode.size();
    config.chargedBytecodeSize = bytecode.size();
    config.isLSL = is_lsl;
    config.apiVersion = api_version;
    config.chunkname = is_lsl ? "=lsl_script" : "=lua_script";
    config.name = "test_script";
    return config;
}

static ScriptConfig makeScriptConfig()
{
    ScriptConfig config;
    config.scriptId = "test_script";
    return config;
}

/*
** One-line drivers for the begin-window-then-call pairs the engine API is
** built around. Each opens a FRESH run window; tests exercising the window
** itself call the engine directly. The `Raw` variants hand the status back;
** the plain ones REQUIRE the status the test expects (Ok unless said
** otherwise) so call sites don't have to.
*/
static RunResult dispatchRaw(Script& exec, int lsl_state, const char* event_name, PushArgsFn push_args = nullptr, void* ctx = nullptr, double quanta = 1.0)
{
    exec.beginRunWindow(quanta);
    return exec.callEventHandler(lsl_state, event_name, push_args, ctx);
}

static RunResult resumeRaw(Script& exec, double quanta = 1.0)
{
    exec.beginRunWindow(quanta);
    return exec.resumeEventHandler();
}

static RunResult dispatch(Script& exec, int lsl_state, const char* event_name, HandlerRunStatus expect = HandlerRunStatus::Ok,
    PushArgsFn push_args = nullptr, void* ctx = nullptr, double quanta = 1.0)
{
    RunResult result = dispatchRaw(exec, lsl_state, event_name, push_args, ctx, quanta);
    CAPTURE(event_name);
    REQUIRE(result.status == expect);
    return result;
}

static RunResult dispatch(Script& exec, const char* event_name, HandlerRunStatus expect = HandlerRunStatus::Ok, PushArgsFn push_args = nullptr,
    void* ctx = nullptr, double quanta = 1.0)
{
    return dispatch(exec, 0, event_name, expect, push_args, ctx, quanta);
}

static RunResult resume(Script& exec, double quanta = 1.0, HandlerRunStatus expect = HandlerRunStatus::Ok)
{
    RunResult result = resumeRaw(exec, quanta);
    REQUIRE(result.status == expect);
    return result;
}

// Runs the staged SLua main function (or a resumable handler) to completion,
// opening a fresh run window before every resume.
static RunResult resumeToCompletion(Script& exec, double quanta, HandlerRunStatus expect = HandlerRunStatus::Ok, int* preemptions = nullptr)
{
    RunResult result{HandlerRunStatus::Preempted, 0};
    while (result.status == HandlerRunStatus::Preempted)
    {
        result = resumeRaw(exec, quanta);
        if (preemptions && result.status == HandlerRunStatus::Preempted)
            ++(*preemptions);
    }
    REQUIRE(result.status == expect);
    return result;
}

// Whole-log comparison for the host's capture vectors (printed, registrations)
static void checkCapture(const std::vector<std::string>& actual, std::initializer_list<const char*> expected)
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
        CHECK(actual[index] == want);
        ++index;
    }
}

// serializeState()/restoreState() with success asserted, for tests where the
// round trip itself is not what's under test
static std::string serialize(Script& exec)
{
    std::string payload;
    REQUIRE(exec.serializeState(payload));
    return payload;
}

static void restore(Script& exec, const std::string& payload)
{
    REQUIRE(exec.restoreState(payload.data(), payload.size()));
}

// Compiles source for the requested flavor
static std::string compileSource(const char* source, bool is_lsl)
{
#ifdef LUAU_USE_TAILSLIDE
    if (is_lsl)
        return compileLSL(source);
#endif
    return Luau::compile(source);
}

// Bundles a TestProvisioner with the script it provisions, so tests can stand a
// script up in one line (default 1:1:1 topology via provisionScript(); sharing
// tests compose the factories directly). loadDefaultState()/reset()/
// restoreState() stay explicit in the tests since lifecycle ordering is
// usually part of what is under test.
struct TestScript
{
    TestProvisioner host;
    std::shared_ptr<Script> owned;
    Script& exec;

    explicit TestScript(const std::string& bytecode, bool is_lsl = false, uint32_t api_version = 0)
        : owned(host.provisionScript(makeImageConfig(bytecode, is_lsl, api_version), makeScriptConfig()))
        , exec(*owned)
    {
        REQUIRE(owned != nullptr);
    }

    // Compiles `source` first (with the LSL compiler when is_lsl)
    explicit TestScript(const char* source, bool is_lsl = false, uint32_t api_version = 0)
        : TestScript(compileSource(source, is_lsl), is_lsl, api_version)
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

    // Load and run the SLua main function, which must complete in one window
    void start()
    {
        loadDefaultState();
        resume(exec);
    }
};

// Builds an image the test expects to load
static std::shared_ptr<IImage> makeImage(TestProvisioner& host, const std::shared_ptr<IEnvironment>& env, const std::string& bytecode)
{
    std::shared_ptr<IImage> image = host.buildImage(env, makeImageConfig(bytecode));
    REQUIRE(image != nullptr);
    REQUIRE(image->isValid());
    return image;
}

// Same, in a fresh environment of its own
static std::shared_ptr<IImage> makeImage(TestProvisioner& host, const std::string& bytecode)
{
    return makeImage(host, host.createEnvironment(false, 0), bytecode);
}

// Instantiates a script on `image` and runs its main function to completion
static std::shared_ptr<Script> startScript(TestProvisioner& host, const std::shared_ptr<IImage>& image)
{
    std::shared_ptr<Script> script = host.instantiateScript(image, makeScriptConfig());
    REQUIRE(script != nullptr);
    REQUIRE(script->loadDefaultState());
    resume(*script);
    return script;
}

// Runs `bytecode` to completion on a throwaway script and returns its
// serialized payload; the restoreState() tests all start from one of these.
static std::string donorPayload(const std::string& bytecode)
{
    TestScript donor(bytecode);
    donor.start();
    return serialize(donor.exec);
}

// Delivers a timer tick the way the sim would: advance the script clock, then
// dispatch a `timer` event and run the callbacks to completion.
static void tickTimers(TestScript& ts, double script_time)
{
    ts.host.script_clock = script_time;
    RunResult result = dispatchRaw(ts.exec, 0, "timer");
    if (result.status == HandlerRunStatus::Preempted)
        resumeToCompletion(ts.exec, 1.0);
    else
        REQUIRE(result.status == HandlerRunStatus::Ok);
}

// The persistent handler thread, which lives at position 1 on the instance
static lua_State* handlerThread(Script& script)
{
    return lua_tothread(script.getInstanceState(), 1);
}

// hardstacktests builds resize a thread's stack and CallInfo arrays down to
// exactly what is in use on every GC pass, so their capacities there are
// whatever the last collection left behind rather than the VM's usual sizes.
#if defined(HARDSTACKTESTS) && HARDSTACKTESTS
constexpr bool kExactThreadSizing = true;
#else
constexpr bool kExactThreadSizing = false;
#endif

// Reads an integer global off a script's instance
static int readIntGlobal(Script& script, const char* name)
{
    lua_State* instance = script.getInstanceState();
    lua_getglobal(instance, name);
    int value = lua_tointeger(instance, -1);
    lua_pop(instance, 1);
    return value;
}

// Every test runs under the SLua feature flags, and leaves the process-global
// log hook unset for the next test
struct SLuaFixture
{
    ScopedSLuaFlags slua_flags;

    ~SLuaFixture()
    {
        logCallback() = nullptr;
    }
};

TEST_SUITE_BEGIN("SLExecutor");

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor invalid bytecode")
{
    std::string bytecode = "definitely not luau bytecode";
    TestProvisioner host;
    std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);
    std::shared_ptr<IImage> image = host.buildImage(env, makeImageConfig(bytecode));
    CHECK_FALSE(image->isValid());
    // The load error is surfaced for the host's compile-error reporting
    CHECK_FALSE(image->getError().empty());
    CHECK(host.instantiateScript(image, makeScriptConfig()) == nullptr);
    CHECK(host.provisionScript(makeImageConfig(bytecode), makeScriptConfig()) == nullptr);

    // A failed load leaves the environment usable
    std::string good = Luau::compile(R"(
        counter = 1
    )");
    makeImage(host, env, good);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor log callback receives engine messages")
{
    struct CapturedLog
    {
        LogLevel level;
        std::string source;
        std::string message;
    };
    static std::vector<CapturedLog> captured;
    captured.clear();

    logCallback() = [](LogLevel level, const char* source, const char* message)
    {
        captured.push_back({level, source, message});
    };

    TestProvisioner host;
    CHECK(host.instantiateScript(nullptr, makeScriptConfig()) == nullptr);
    REQUIRE(captured.size() == 1);
    CHECK(captured[0].level == LogLevel::Warn);
    CHECK(captured[0].source == "");
    CHECK(captured[0].message == "Refusing to instantiate image provisioned elsewhere");

    // A message with format arguments, attributed to the image's name
    std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);
    std::shared_ptr<IImage> image = host.buildImage(env, makeImageConfig("definitely not luau bytecode"));
    REQUIRE_FALSE(image->isValid());
    REQUIRE(captured.size() == 2);
    CHECK(captured[1].level == LogLevel::Warn);
    CHECK(captured[1].source == "test_script");
    CHECK(captured[1].message == "Failed to load Luau bytecode: " + image->getError());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor provisioner topologies")
{
    std::string bytecode = Luau::compile(R"(
        counter = 1
    )");

    SUBCASE("cross-provisioner refusals")
    {
        TestProvisioner host;
        std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);
        std::shared_ptr<IImage> image = makeImage(host, env, bytecode);

        // A provisioner never builds into or instantiates from another
        // provisioner's tiers
        TestProvisioner other_host;
        CHECK(other_host.buildImage(env, makeImageConfig(bytecode)) == nullptr);
        CHECK(other_host.instantiateScript(image, makeScriptConfig()) == nullptr);

        // Nor does an image build into an environment of another flavor
        CHECK(host.buildImage(env, makeImageConfig(bytecode, false, 1)) == nullptr);
    }

    SUBCASE("one environment hosts multiple images")
    {
        std::string bytecode2 = Luau::compile(R"(
            counter = 2
        )");
        TestProvisioner host;
        std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);
        std::weak_ptr<IEnvironment> env_watch = env;
        std::shared_ptr<IImage> image1 = makeImage(host, env, bytecode);
        std::shared_ptr<IImage> image2 = makeImage(host, env, bytecode2);

        // Drop the host's environment handle; the images keep it alive.
        // This is the cache pattern: retain weakly, lock-or-rebuild.
        env.reset();
        REQUIRE_FALSE(env_watch.expired());

        std::shared_ptr<Script> first = startScript(host, image1);
        std::shared_ptr<Script> second = startScript(host, image2);

        // Each script ran its own image's bytecode
        CHECK(readIntGlobal(*first, "counter") == 1);
        CHECK(readIntGlobal(*second, "counter") == 2);

        // Releasing the scripts and images lets the environment expire --
        // the lifetime is governed solely by what still uses it
        first.reset();
        second.reset();
        image1.reset();
        REQUIRE_FALSE(env_watch.expired());
        image2.reset();
        CHECK(env_watch.expired());
    }

    SUBCASE("one image hosts multiple script instances")
    {
        std::string mutable_bytecode = Luau::compile(R"(
            counter = (counter or 0) + 1
        )");
        TestProvisioner host;
        std::shared_ptr<IImage> image = makeImage(host, mutable_bytecode);
        std::shared_ptr<Script> first = startScript(host, image);
        std::shared_ptr<Script> second = startScript(host, image);

        lua_pushinteger(first->getInstanceState(), 100);
        lua_setglobal(first->getInstanceState(), "counter");
        CHECK(readIntGlobal(*first, "counter") == 100);
        CHECK(readIntGlobal(*second, "counter") == 1);

        REQUIRE(first->reset());
        CHECK(readIntGlobal(*second, "counter") == 1);
    }

    SUBCASE("serialize and restore between scripts sharing an environment")
    {
        TestProvisioner host;
        std::shared_ptr<IImage> image = makeImage(host, bytecode);
        std::shared_ptr<Script> first = startScript(host, image);

        std::string payload = serialize(*first);

        // A fresh script of the same image picks the payload up while the
        // donor keeps running in the same environment
        std::shared_ptr<Script> second = host.instantiateScript(image, makeScriptConfig());
        REQUIRE(second != nullptr);
        restore(*second, payload);
        CHECK(second->isMainFunctionComplete());
        CHECK(readIntGlobal(*second, "counter") == 1);
        CHECK(readIntGlobal(*first, "counter") == 1);
    }

    SUBCASE("provisionScript builds a fresh environment per script")
    {
        TestProvisioner host;
        std::shared_ptr<Script> first = host.provisionScript(makeImageConfig(bytecode), makeScriptConfig());
        std::shared_ptr<Script> second = host.provisionScript(makeImageConfig(bytecode), makeScriptConfig());
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        CHECK(&first->getEnvironment() != &second->getEnvironment());
    }
}

// A host that shares one image per distinct bytecode by composing the public
// factories; a real host would key on its asset identity instead.
struct CachingTestHost : TestProvisioner
{
    std::map<std::string, std::shared_ptr<IImage>> image_cache;

    std::shared_ptr<Script> provision(const std::string& bytecode)
    {
        std::shared_ptr<IImage>& image = image_cache[bytecode];
        if (image == nullptr)
            image = buildImage(createEnvironment(false, 0), makeImageConfig(bytecode));
        return instantiateScript(image, makeScriptConfig());
    }

    // Drops cached images no Script is using; nothing evicts on the host's behalf
    size_t releaseUnusedImages()
    {
        size_t dropped = 0;
        for (auto it = image_cache.begin(); it != image_cache.end();)
        {
            if (it->second.use_count() == 1)
            {
                it = image_cache.erase(it);
                ++dropped;
            }
            else
            {
                ++it;
            }
        }
        return dropped;
    }
};

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor host-side image caching")
{
    std::string bytecode = Luau::compile(R"(
        counter = (counter or 0) + 1
    )");
    std::string bytecode2 = Luau::compile(R"(
        counter = 2
    )");

    CachingTestHost host;
    std::shared_ptr<Script> first = host.provision(bytecode);
    std::shared_ptr<Script> second = host.provision(bytecode);
    std::shared_ptr<Script> other = host.provision(bytecode2);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(other != nullptr);

    CHECK(&first->getImage() == &second->getImage());
    CHECK(&first->getEnvironment() == &second->getEnvironment());
    CHECK(&first->getImage() != &other->getImage());

    // The cache owns its entries: releasing the scripts leaves the image
    // resident, and only the host's sweep drops it
    std::weak_ptr<IImage> watch = host.image_cache[bytecode];
    first.reset();
    second.reset();
    CHECK_FALSE(watch.expired());
    // The other asset is still in use, so the sweep leaves it alone
    CHECK(host.releaseUnusedImages() == 1);
    CHECK(watch.expired());

    std::shared_ptr<Script> again = host.provision(bytecode);
    REQUIRE(again != nullptr);
    CHECK(host.image_cache.count(bytecode) == 1);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor co-resident scripts account memory independently")
{
    // Parks a large table in a global on demand, so one instance can grow
    // while its neighbour in the same VM sits idle
    std::string bytecode = Luau::compile(R"(
        function LLEvents.moving_start()
            local hoard = {}
            for i = 1, 500 do
                hoard[i] = string.rep("x", 64) .. tostring(i)
            end
            kept = hoard
            reading = gcinfo()
        end
        function LLEvents.moving_end()
            reading = gcinfo()
        end
    )");

    TestProvisioner host;
    std::shared_ptr<IImage> image = makeImage(host, bytecode);
    std::shared_ptr<Script> first = startScript(host, image);
    std::shared_ptr<Script> second = startScript(host, image);

    int first_baseline = first->getUsedMemory();
    int second_baseline = second->getUsedMemory();

    // Same image, so the bytecode is charged to both in full rather than split
    // between them, and two fresh instances of it start out level
    CHECK(first_baseline >= (int)bytecode.size());
    CHECK(second_baseline >= (int)bytecode.size());
    CHECK(first_baseline == second_baseline);

    dispatch(*first, "moving_start");

    // The allocation lands on the script that made it
    CHECK(first->getUsedMemory() > first_baseline + 30000);

    // ... and not on its neighbour. Running a handler on the idle script is
    // what dirties its cached size, so this is a real re-measurement of the
    // instance rather than the number read back above.
    dispatch(*second, "moving_end");
    CHECK(second->getUsedMemory() < second_baseline + 1000);

    // gcinfo() is the script-visible face of the same accounting: each script
    // reads its own size in KB, not the shared VM's. The second reading was
    // taken after the hoard existed, so a VM-wide gcinfo would have counted
    // it in both.
    CHECK(readIntGlobal(*first, "reading") > readIntGlobal(*second, "reading") + 25);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor interleaves co-resident scripts")
{
    std::string slow_bytecode = Luau::compile(R"(
        total = 0
        for i = 1, 20000 do
            total += i
        end
    )");
    std::string quick_bytecode = Luau::compile(R"(
        counter = 7
    )");

    TestProvisioner host;
    std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);
    std::shared_ptr<Script> slow = host.instantiateScript(makeImage(host, env, slow_bytecode), makeScriptConfig());
    std::shared_ptr<Script> quick = host.instantiateScript(makeImage(host, env, quick_bytecode), makeScriptConfig());
    REQUIRE(slow != nullptr);
    REQUIRE(quick != nullptr);
    REQUIRE(slow->loadDefaultState());
    REQUIRE(quick->loadDefaultState());

    // Preempt the first script mid-main
    host.clock_step = 0.001;
    resume(*slow, 0.005, HandlerRunStatus::Preempted);
    CHECK(slow->isHandlerActive());

    // Run the other one in the same VM to completion while it sits suspended
    host.clock_step = 0.0;
    resume(*quick);
    CHECK(readIntGlobal(*quick, "counter") == 7);

    // The suspended instance picks up exactly where it left off
    CHECK(slow->isHandlerActive());
    resumeToCompletion(*slow, 1.0);
    CHECK(readIntGlobal(*slow, "total") == 200010000);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor refuses to dispatch over a resumable handler")
{
    TestScript ts(R"(
        function LLEvents.moving_start()
            print("moving")
        end
    )");
    ts.loadDefaultState();
    Script& exec = ts.exec;

    // The staged main owns the handler thread until it has run, so an event
    // dispatched on top of it is refused rather than run against its stack;
    // a host that cannot tell Refused apart from the other statuses cannot
    // assert on its own sequencing bugs.
    dispatch(exec, "moving_start", HandlerRunStatus::Refused);
    CHECK(ts.host.printed.empty());

    resume(exec);

    dispatch(exec, "moving_start");
    checkCapture(ts.host.printed, {"moving"});

    // Resuming when there is nothing to resume is the host's sequencing bug,
    // and reports as such rather than as a missing handler
    CHECK(exec.resumeEventHandler().status == HandlerRunStatus::Refused);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor abortHandler discards a resumable handler")
{
    TestScript ts(R"(
        ran = 0
        function LLEvents.moving_start()
            ran += 1
            for i = 1, 100000 do end
            ran += 100
        end
    )");
    ts.start();

    // Preempt the handler mid-loop, then abort it instead of resuming
    ts.host.clock_step = 0.001;
    dispatch(ts.exec, "moving_start", HandlerRunStatus::Preempted, nullptr, nullptr, 0.005);
    REQUIRE(ts.exec.isHandlerActive());

    ts.host.clock_step = 0.0;
    ts.exec.abortHandler();
    CHECK_FALSE(ts.exec.isHandlerActive());

    // Aborting again is a no-op
    ts.exec.abortHandler();

    // The aborted handler's tail never ran, and the thread is clean enough
    // to dispatch on again
    RunResult rerun = dispatch(ts.exec, "moving_start");
    // Main-function completion was already reported, never again
    CHECK_FALSE(rerun.mainFunctionCompleted);
    CHECK(readIntGlobal(ts.exec, "ran") == 102);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor yield-due is scoped to the run window")
{
    TestScript ts(R"(
        function LLEvents.moving_start()
            local total = 0
            for i = 1, 20000 do
                total += i
            end
        end
        function LLEvents.moving_end()
        end
    )");
    ts.start();
    Script& exec = ts.exec;

    // Everything below runs in ONE window, so the engine is driven directly
    // rather than through the fresh-window helpers.
    // Force a preemption without burning virtual time, so the rest of the
    // window is only bounded by the flag under test
    exec.beginRunWindow(1.0);
    exec.setForceYield(true);
    REQUIRE(exec.callEventHandler(0, "moving_start", nullptr).status == HandlerRunStatus::Preempted);
    CHECK(exec.isYieldDue());

    // Finishing that handler and running another one in the same window leaves
    // the flag set: it describes the window, not the last call
    exec.setForceYield(false);
    REQUIRE(exec.resumeEventHandler().status == HandlerRunStatus::Ok);
    CHECK(exec.isYieldDue());
    REQUIRE(exec.callEventHandler(0, "moving_end", nullptr).status == HandlerRunStatus::Ok);
    CHECK(exec.isYieldDue());

    // Only a fresh window clears it
    exec.beginRunWindow(1.0);
    CHECK_FALSE(exec.isYieldDue());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor a host-set fault completes the main function")
{
    // A script the host kills will never register the rest of its handlers, so
    // the host has to be free to start masking its events
    TestScript ts(R"(
        counter = 1
    )");
    ts.loadDefaultState();

    CHECK_FALSE(ts.exec.isMainFunctionComplete());
    ts.exec.setFault(FaultKind::Runtime, "killed by host");
    CHECK(ts.exec.isMainFunctionComplete());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor carries host context and the charged bytecode size")
{
    std::string bytecode = Luau::compile(R"(
        marker = 1
    )");

    int host_object = 42;
    TestProvisioner host;

    ImageConfig image_config = makeImageConfig(bytecode);
    // The host charges the whole asset, headers included, not just the slice it
    // handed over to be loaded
    image_config.chargedBytecodeSize = bytecode.size() + 4096;

    ScriptConfig script_config = makeScriptConfig();
    script_config.hostContext = &host_object;
    script_config.memoryLimit = 64 * 1024;

    std::shared_ptr<Script> script = host.provisionScript(image_config, script_config);
    REQUIRE(script != nullptr);
    CHECK(script->getHostContext() == &host_object);
    CHECK(script->getMemoryLimit() == 64 * 1024);

    // The charge applies before an instance exists and after it does
    CHECK(script->getUsedMemory() >= (int)(bytecode.size() + 4096));
    REQUIRE(script->loadDefaultState());
    resume(*script);
    CHECK(script->getUsedMemory() >= (int)(bytecode.size() + 4096));
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor weak references die before OoM")
{
    ScopedFastFlag eager_weak_clear{FFlag::SLuaEagerWeakClear, true};

    std::string bytecode = Luau::compile(R"(
        local weak = setmetatable({}, { __mode = "kv" })
        local tab1 = table.create(6000, 1)
        weak[1] = tab1
        tab1 = nil
        local tab2 = table.create(6000, 2)
        print(if weak[1] == nil then "cleared" else "stale")
        print(#tab2)
    )");

    TestProvisioner host;
    ScriptConfig script_config = makeScriptConfig();
    // Two 6000-slot arrays (96000 logical bytes each) can never fit under the
    // limit together; one plus the base state comfortably can.
    script_config.memoryLimit = 150000;

    std::shared_ptr<Script> script = host.provisionScript(makeImageConfig(bytecode), script_config);
    REQUIRE(script != nullptr);
    REQUIRE(script->loadDefaultState());

    // Keep the real GC from completing a cycle on its own so any weak
    // clearing is attributable to the memory limit callback's heap walk.
    // Don't use LUA_GCSTOP, it disables the beforeallocate callback entirely.
    lua_gc(script->getInstanceState(), LUA_GCSETGOAL, 100000);
    lua_gc(script->getInstanceState(), LUA_GCCOLLECT, 0);

    resume(*script);
    CHECK(script->getFaultKind() == FaultKind::None);
    checkCapture(host.printed, {"cleared", "6000"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor SLua lifecycle")
{
    TestScript ts(R"(
        local greeting = "hello"
        print(greeting)
    )");
    ts.loadDefaultState();

    // The main function is staged on the handler thread as the state_entry handler
    CHECK(ts.exec.isHandlerActive());
    resume(ts.exec);
    CHECK_FALSE(ts.exec.isHandlerActive());
    checkCapture(ts.host.printed, {"hello"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor quanta preemption")
{
    TestScript ts(R"(
        local total = 0
        for i = 1, 30 do
            total += i
        end
        print("finished")
    )");
    ts.loadDefaultState();
    Script& exec = ts.exec;

    // Each interrupt advances the clock 1ms against a 5ms quanta, so every run
    // window covers a handful of loop iterations before the engine breaks.
    ts.host.clock_step = 0.001;
    resume(exec, 0.005, HandlerRunStatus::Preempted);
    CHECK(exec.isYieldDue());
    CHECK(exec.isHandlerActive());

    int preemptions = 0;
    resumeToCompletion(exec, 0.005, HandlerRunStatus::Ok, &preemptions);
    CHECK(preemptions >= 1);
    // Preemption at ~1.2x quanta stays well under the 3x punishment threshold
    CHECK(exec.getSleep() == 0.0f);
    checkCapture(ts.host.printed, {"finished"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor slowness punishment")
{
    TestScript ts(R"(
        local total = 0
        for i = 1, 100 do
            total += i
        end
        print("finished")
    )");
    ts.loadDefaultState();

    SUBCASE("punished past 3x quanta")
    {
        // First interrupt sees 10ms elapsed against a 1ms quanta: past the 3x
        // threshold, so the engine adds (elapsed - quanta) * 5 of sleep.
        ts.host.clock_step = 0.01;
        resume(ts.exec, 0.001, HandlerRunStatus::Preempted);
        CHECK(ts.exec.getSleep() == doctest::Approx((0.01 - 0.001) * 5.0).epsilon(0.01));
    }

    SUBCASE("tiny quantas use the punishment floor")
    {
        // 2ms elapsed overruns the 0.1ms quanta but stays under 3x the 1ms
        // punishment floor, so the overrun is forgiven.
        ts.host.clock_step = 0.002;
        resume(ts.exec, 0.0001, HandlerRunStatus::Preempted);
        CHECK(ts.exec.getSleep() == 0.0f);
    }

    SUBCASE("reset clears accumulated punishment")
    {
        ts.host.clock_step = 0.01;
        resume(ts.exec, 0.001, HandlerRunStatus::Preempted);
        REQUIRE(ts.exec.getSleep() > 0.0f);

        ts.reset();
        CHECK(ts.exec.getSleep() == 0.0f);
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor mandatory yield kill")
{
    SUBCASE("unyieldable metamethod")
    {
        TestScript ts(R"(
            local mt = {}
            mt.__index = function(t, k)
                local i = 0
                while true do
                    i += 1
                end
            end
            local obj = setmetatable({}, mt)
            print(obj.missing)
        )");
        ts.loadDefaultState();
        Script& exec = ts.exec;

        // The mocked clock jumps a full second per reading, so every interrupt is
        // already past 3.5x the 1ms punishment floor. Interrupts at yieldable
        // points just preempt; the script is only killed once execution is inside
        // the metamethod frame, where a yield can't be injected.
        ts.host.clock_step = 1.0;
        resumeToCompletion(exec, 0.0001, HandlerRunStatus::Fault);
        CHECK(exec.getFaultKind() == FaultKind::Timeout);
        CHECK(exec.getFaultString() == "exceeded time limit");
        CHECK(exec.getExtendedFaultString().find("Failed to perform mandatory yield") != std::string::npos);
        CHECK(exec.getSleep() > 0.0f);
    }

    SUBCASE("a caught mandatory yield lets the script recover")
    {
        // The first mandatory yield of an unyieldable stretch is catchable, so
        // a script whose pcall sits outside that stretch gets to report the
        // failure. It unwinds back into yieldable code, where the next
        // interrupt preempts it normally, so the host keeps control and the
        // script is only out the sleep punishment.
        TestScript ts(R"(
            local mt = {}
            mt.__index = function(t, k)
                local i = 0
                while true do
                    i += 1
                end
            end
            local obj = setmetatable({}, mt)
            local ok, err = pcall(function()
                return obj.missing
            end)
            print(`swallowed {err}`)
        )");
        ts.loadDefaultState();
        Script& exec = ts.exec;

        // Each clock reading advances 0.5x quanta, so every window makes
        // progress before its first over-quanta interrupt, and the
        // metamethod's back-edge interrupts walk elapsed past the 3.5x
        // threshold. The loop serves the accumulated sleep punishment the
        // way a real host would (the host owns the countdown); leaving it
        // pending would force a mandatory break at every interrupt and
        // livelock the resume loop. Capped so a regression fails instead
        // of hanging.
        ts.host.clock_step = 0.001;
        bool punished = false;
        RunResult result{HandlerRunStatus::Preempted, 0};
        for (int resumes = 0; result.status == HandlerRunStatus::Preempted && resumes < 1000; ++resumes)
        {
            exec.setSleep(0.0f);
            result = resumeRaw(exec, 0.002);
            // Read before the next iteration serves it
            punished = punished || exec.getSleep() > 0.0f;
        }
        CHECK(result.status == HandlerRunStatus::Ok);
        CHECK(exec.getFaultKind() == FaultKind::None);
        CHECK(punished);
        REQUIRE(ts.host.printed.size() == 1);
        CHECK(ts.host.printed[0].find("Failed to perform mandatory yield") != std::string::npos);
    }

    SUBCASE("looping on the caught error is not survivable")
    {
        // The adversarial shape the escalation exists for: the retry loop lives
        // *inside* the metamethod frame, where a yield can't be injected, so
        // the script never reaches a yieldable point between catches and the
        // throw and the pcall's catch both happen within one lua_resume. No
        // catchable error breaks that -- the host's resume loop would never
        // regain control -- so the second mandatory yield of the stretch kills
        // uncatchably.
        TestScript ts(R"(
            local mt = {}
            mt.__index = function(t, k)
                while true do
                    pcall(function()
                        local i = 0
                        while true do
                            i += 1
                        end
                    end)
                end
            end
            local obj = setmetatable({}, mt)
            print(obj.missing)
        )");
        ts.loadDefaultState();

        ts.host.clock_step = 1.0;
        resumeToCompletion(ts.exec, 0.0001, HandlerRunStatus::Fault);
        CHECK(ts.exec.getFaultKind() == FaultKind::Timeout);
        CHECK(ts.exec.getFaultString() == "exceeded time limit");
        // The script never got to observe the kill
        CHECK(ts.host.printed.empty());
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor engine pauses are not charged")
{
    SUBCASE("unyieldable region survives a GC-inflated window")
    {
        // A long GC step lands while execution is inside a metamethod frame,
        // where a yield can't be injected. GC isn't the executing code's fault,
        // necessarily, so we discount it against their runtime.
        TestScript ts(R"(
            local mt = {}
            mt.__index = function(t, k)
                simulate_gc_step(10, 2)
                local total = 0
                for i = 1, 20 do
                    total += i
                end
                return total
            end
            local obj = setmetatable({}, mt)
            print(obj.missing)
        )");
        ts.loadDefaultState();
        Script& exec = ts.exec;

        // Charged time only accrues one clock step per interrupt reading, so
        // it stays far below the punishment thresholds against a 5ms quanta.
        ts.host.clock_step = 0.0001;
        resumeToCompletion(exec, 0.005, HandlerRunStatus::Ok);
        CHECK(exec.getFaultKind() == FaultKind::None);
        CHECK(exec.getSleep() == 0.0f);
        checkCapture(ts.host.printed, {"210"});
    }

    SUBCASE("GC-inflated time is not punished, preemption stays on wall time")
    {
        TestScript ts(R"(
            simulate_gc_step(10)
            local total = 0
            for i = 1, 100 do
                total += i
            end
            print("finished")
        )");
        ts.loadDefaultState();
        Script& exec = ts.exec;

        // Wall elapsed is ~10s against a 50ms quanta, so the first yieldable
        // interrupt still preempts, but the charged handful of clock steps
        // stays under the 3x threshold, so no sleep punishment lands.
        ts.host.clock_step = 0.001;
        resume(exec, 0.05, HandlerRunStatus::Preempted);
        CHECK(exec.isYieldDue());
        CHECK(exec.getSleep() == 0.0f);
        // The banked exclusion is readable until the next window resets it
        CHECK(exec.getExcludedTime() >= 10.0);

        // Zero out the sleep so we don't deadlock the suite if the earlier
        // getSleep() == 0.0f check didn't hold.
        exec.setSleep(0.0f);
        resumeToCompletion(exec, 0.05);
        checkCapture(ts.host.printed, {"finished"});
    }

    SUBCASE("heap walk feeds the exclusion accumulator")
    {
        // Trigger a ton of allocations that would cause the
        // lua_userthreadgc() check to kick in to get the
        // actual heap size. The cost of walking the script should be excluded.
        TestScript ts(R"(
            local parts = {}
            for i = 1, 50 do
                parts[i] = string.rep("x", i+100)
            end
            print("done")
        )");
        ts.loadDefaultState();
        lua_State* script = ts.exec.getInstanceState();
        // Set GC goal very high so it doesn't kick in during resume().
        // Totally stopping the GC has the side-effect of also disabling
        // our own heap walking, so don't use LUA_GCSTOP :)
        lua_gc(script, LUA_GCSETGOAL, 100000);
        lua_gc(script, LUA_GCCOLLECT, 0);

        ts.host.clock_step = 0.001;
        resume(ts.exec, 1.0);
        // Verify that we actually did something excludable
        CHECK(ts.exec.getExcludedTime() > 0.0);
        CHECK(ts.exec.getSleep() == 0.0f);
        checkCapture(ts.host.printed, {"done"});
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor mandatory yield grace")
{
    SUBCASE("an unattributed pause inside an unyieldable stretch is survivable")
    {
        // Cause a pause that can't be attributed to either GC or heap walking
        // in the middle of an unyieldable area. Our grace period mechanic should
        // allow a small amount of time to get out of the unyieldable area before
        // we start trying to kill the script.
        TestScript ts(R"(
            local mt = {}
            mt.__index = function(t, k)
                jump_clock(10)
                local total = 0
                for i = 1, 20 do
                    total += i
                end
                return total
            end
            local obj = setmetatable({}, mt)
            print(obj.missing)
        )");
        ts.loadDefaultState();
        Script& exec = ts.exec;

        // Take tiny tiny steps so `jump_clock()` dominates the runtime.
        ts.host.clock_step = 0.000001;
        resume(exec, 0.005, HandlerRunStatus::Preempted);
        CHECK(exec.getFaultKind() == FaultKind::None);
        // We won't beat up the script, but we will make it sleep!
        CHECK(exec.getSleep() > 0.0f);

        exec.setSleep(0.0f);
        resumeToCompletion(exec, 0.005);
        checkCapture(ts.host.printed, {"210"});
    }

    SUBCASE("a stretch that outlasts the grace still dies")
    {
        TestScript ts(R"(
            local mt = {}
            mt.__index = function(t, k)
                jump_clock(10)
                local i = 0
                while true do
                    i += 1
                end
            end
            local obj = setmetatable({}, mt)
            print(obj.missing)
        )");
        ts.loadDefaultState();

        ts.host.clock_step = 0.000001;
        resumeToCompletion(ts.exec, 0.005, HandlerRunStatus::Fault);
        CHECK(ts.exec.getFaultKind() == FaultKind::Timeout);
        CHECK(ts.exec.getFaultString() == "exceeded time limit");
        CHECK(ts.host.printed.empty());
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor pending sleep preempts")
{
    TestScript ts(R"(
        local total = 0
        for i = 1, 100 do
            total += i
        end
        print("finished")
    )");
    ts.loadDefaultState();

    ts.exec.setSleep(1.0f);
    resume(ts.exec, 1.0, HandlerRunStatus::Preempted);
    CHECK(ts.exec.isYieldDue());
    // Sleep-driven preemption carries no punishment
    CHECK(ts.exec.getSleep() == 1.0f);

    ts.exec.setSleep(0.0f);
    resumeToCompletion(ts.exec, 1.0);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor memory limit")
{
    std::string bytecode = Luau::compile(R"(
        local parts = {}
        for i = 1, 400 do
            parts[i] = string.rep("x", 100) .. tostring(i)
        end
        held_parts = parts
        print("allocated")
    )");

    TestScript ts(bytecode);
    ts.loadDefaultState();
    Script& exec = ts.exec;

    SUBCASE("allocations under the limit succeed and are measured")
    {
        int baseline = exec.getUsedMemory();
        CHECK(baseline >= (int)bytecode.size());

        resume(exec);

        // 400 strings of 100+ bytes must show up in the reachability scan
        CHECK(exec.getUsedMemory() >= baseline + 40000);

        // Limit validity rules: never below current use, never raised above the
        // cap, shrinking from an unsafe raise is allowed
        CHECK_FALSE(exec.setMemoryLimit(exec.getUsedMemory() - 1));
        CHECK_FALSE(exec.setMemoryLimit(kDefaultMemoryLimit + 1));
        CHECK(exec.setMemoryLimit(kDefaultMemoryLimit));
        exec.setMemoryLimitUnsafe(kDefaultMemoryLimit * 2);
        CHECK(exec.getMemoryLimit() == kDefaultMemoryLimit * 2);
        CHECK(exec.setMemoryLimit(kDefaultMemoryLimit + 4096));
    }

    SUBCASE("exceeding the limit faults with out-of-memory")
    {
        exec.setMemoryLimitUnsafe(exec.getUsedMemory() + 8192);
        resume(exec, 1.0, HandlerRunStatus::Fault);
        CHECK(exec.getFaultKind() == FaultKind::OutOfMemory);
        CHECK(exec.getFaultString() == "not enough memory");
        CHECK(ts.host.printed.empty());

        // A faulted script can be reset and run again under a sane limit
        exec.setMemoryLimitUnsafe(kDefaultMemoryLimit);
        ts.reset();
        CHECK(exec.getFaultKind() == FaultKind::None);
        resume(exec);
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor LLEvents dispatch")
{
    // A non-detected event, so the pushed arguments reach the handler verbatim
    // (detected events get their count arg replaced with a table of
    // detected-event wrappers before dispatch)
    TestScript ts(R"(
        LLEvents:on("link_message", function(...)
            local args = {...}
            print(`{select("#", ...)}:{args[1]}:{args[2]}:{args[3]}`)
        end)
    )");
    ts.loadDefaultState();

    // Run the main function; it registers the link_message handler dynamically
    resume(ts.exec);
    checkCapture(ts.host.registrations, {"+link_message"});

    // The engine counts what the callback pushed rather than being told
    dispatch(ts.exec, "link_message", HandlerRunStatus::Ok, [](lua_State* handler, void*) {
        luaSL_pushnativeinteger(handler, 7);
        lua_pushstring(handler, "hello");
        luaSL_pushnativeinteger(handler, -3);
    });
    checkCapture(ts.host.printed, {"3:7:hello:-3"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor pushes args outside a resume")
{
    // Args are marshalled before the handler thread is resumed, so nothing on that
    // path may reach the VM and try to yield from a thread with no frame. The
    // interrupt handler asserts on it; this drives `luaSL_pushuuidstring`, the
    // argument type that touches a metatabled table on its way in.
    TestScript ts(R"(
        LLEvents:on("dataserver", function(query_id, data)
            print(`{typeof(query_id)}/{data}`)
        end)
    )");
    ts.loadDefaultState();

    resume(ts.exec);

    // A pending forced yield makes the first interrupt mandatory, so one reaching
    // the marshalling would kill the script rather than pass unnoticed. The first
    // one that does fire lands in `handleEvent`, which is yieldable.
    ts.exec.setForceYield(true);

    RunResult result = dispatchRaw(ts.exec, 0, "dataserver", [](lua_State* handler, void*) {
        luaSL_pushuuidstring(handler, "8d4a1e26-3f2b-4c7d-9a15-6e0b3c8f2d41");
        lua_pushstring(handler, "payload");
    });
    REQUIRE(result.status == HandlerRunStatus::Preempted);
    CHECK(ts.exec.getFaultKind() == FaultKind::None);
    CHECK(ts.host.printed.empty());

    ts.exec.setForceYield(false);
    resumeToCompletion(ts.exec, 1.0);
    checkCapture(ts.host.printed, {"uuid/payload"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor dynamic handler unregistration")
{
    TestScript ts(R"(
        local function handler() end
        LLEvents:on("touch_start", handler)
        LLEvents:off("touch_start", handler)
    )");
    ts.loadDefaultState();

    resume(ts.exec);

    checkCapture(ts.host.registrations, {"+touch_start", "-touch_start"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor timers")
{
    SUBCASE("basic once timer")
    {
        TestScript ts(R"(
            LLTimers:once(1.5, function(scheduled_time, interval)
                assert(interval == nil, "once timer should have nil interval")
                assert(math.abs(scheduled_time - 1.5) < 0.001, "scheduled_time should be 1.5")
                print("PASS")
            end)
        )");
        ts.loadDefaultState();

        resume(ts.exec);
        // The VM asked the host to wake it when the timer comes due
        CHECK(ts.host.last_timer_interval >= 0.0);

        tickTimers(ts, 2.0);
        checkCapture(ts.host.printed, {"PASS"});
    }

    SUBCASE("zero delay timer")
    {
        TestScript ts(R"(
            LLTimers:once(0.0, function(scheduled_time, interval)
                assert(interval == nil, "once timer should have nil interval")
                assert(math.abs(scheduled_time - 0.0) < 0.001, "zero delay scheduled for T=0")
                print("PASS")
            end)
        )");
        ts.loadDefaultState();

        resume(ts.exec);

        tickTimers(ts, 0.1);
        checkCapture(ts.host.printed, {"PASS"});
    }

    SUBCASE("every timer uses absolute scheduling")
    {
        TestScript ts(R"(
            fire_times = {}
            LLTimers:every(1.0, function(scheduled_time, interval)
                assert(interval == 1.0, "every timer interval should be 1.0")
                table.insert(fire_times, scheduled_time)
            end)
        )");
        ts.loadDefaultState();

        resume(ts.exec);

        // :every fires at most once per tick and does not catch up missed slots;
        // after the T=5.2 tick the cadence resets to 5.2 + 1.0 = 6.2
        tickTimers(ts, 1.1);
        tickTimers(ts, 2.5);
        tickTimers(ts, 5.2);
        tickTimers(ts, 6.5);

        lua_State* script = ts.exec.getInstanceState();
        lua_getglobal(script, "fire_times");
        REQUIRE(lua_istable(script, -1));
        CHECK(lua_objlen(script, -1) == 4);
        const double expected[] = {1.0, 2.0, 3.0, 6.2};
        for (int i = 0; i < 4; ++i)
        {
            lua_rawgeti(script, -1, i + 1);
            CHECK(lua_tonumber(script, -1) == doctest::Approx(expected[i]).epsilon(0.001));
            lua_pop(script, 1);
        }
        lua_pop(script, 1);
    }

    SUBCASE("cancelled timer does not fire")
    {
        TestScript ts(R"(
            local function timer1() print("TIMER1") end
            local function timer2() print("TIMER2") end
            LLTimers:once(1.0, timer1)
            LLTimers:once(1.0, timer2)
            LLTimers:off(timer1)
        )");
        ts.loadDefaultState();

        resume(ts.exec);

        tickTimers(ts, 2.0);
        checkCapture(ts.host.printed, {"TIMER2"});
    }

    SUBCASE("concurrent timers fire in definition order")
    {
        TestScript ts(R"(
            LLTimers:once(2.0, function() print("C") end)
            LLTimers:once(1.0, function() print("A") end)
            LLTimers:once(1.5, function() print("B1") end)
            LLTimers:once(1.5, function() print("B2") end)
            LLTimers:once(3.0, function() print("D") end)
        )");
        ts.loadDefaultState();

        resume(ts.exec);

        // All are due at this tick, so they fire in definition order
        tickTimers(ts, 4.0);
        checkCapture(ts.host.printed, {"C", "A", "B1", "B2", "D"});
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor out of memory during event dispatch")
{
    SUBCASE("handler setup with no memory left")
    {
        TestScript ts(R"(
            held = buffer.create(1024 * 100)
            LLEvents:on("touch_start", function() print("HANDLER RAN") end)
        )");
        ts.loadDefaultState();
        Script& exec = ts.exec;

        resume(exec);
        CHECK(exec.getUsedMemory() > 1024 * 100);

        // Leave barely any headroom, so allocations during dispatch fail
        exec.setMemoryLimitUnsafe(exec.getUsedMemory() + 128);
        dispatch(exec, "touch_start", HandlerRunStatus::Fault);
        CHECK(exec.getFaultKind() == FaultKind::OutOfMemory);
        CHECK(ts.host.printed.empty());
    }

    SUBCASE("handler teardown after a restore with no memory left")
    {
        // Tearing the handler thread down resets it, and that reset runs
        // between handlers with no protected frame around it. A restored
        // thread must not need an allocation to get back to the reset state.
        std::string bytecode = Luau::compile(R"(
            held = buffer.create(1024 * 100)
            LLEvents:on("touch_start", function() print("HANDLER RAN") end)
        )");
        TestScript first(bytecode);
        first.start();

        std::string payload = serialize(first.exec);

        TestScript ts(bytecode);
        restore(ts.exec, payload);
        Script& exec = ts.exec;
        REQUIRE(exec.getUsedMemory() > 1024 * 100);

        exec.setMemoryLimitUnsafe(exec.getUsedMemory() + 128);
        dispatch(exec, "touch_start", HandlerRunStatus::Fault);
        CHECK(exec.getFaultKind() == FaultKind::OutOfMemory);
        CHECK(ts.host.printed.empty());
    }

    SUBCASE("oversized argument push faults gracefully")
    {
        TestScript ts(R"(
            LLEvents:on("dataserver", function(s) print("HANDLER RAN") end)
        )");
        ts.loadDefaultState();
        Script& exec = ts.exec;

        resume(exec);

        // Enough headroom for handler setup, nowhere near enough for the argument
        exec.setMemoryLimitUnsafe(exec.getUsedMemory() + 2048);
        dispatch(exec, "dataserver", HandlerRunStatus::Fault, [](lua_State* handler, void*) {
            std::string huge(1024 * 100, 'x');
            lua_pushlstring(handler, huge.data(), huge.size());
        });
        CHECK(exec.getFaultKind() == FaultKind::OutOfMemory);
        // The handler body must never have run
        CHECK(ts.host.printed.empty());
    }
}

static int untracked_cfunc(lua_State* L)
{
    return 0;
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor unserializable global is refused without exploding")
{
    TestScript first(R"(
        counter = 1
    )");
    first.start();

    // A C function on the user-mutable globals has no perms entry
    lua_State* script = first.exec.getInstanceState();
    lua_pushcfunction(script, untracked_cfunc, "untracked_cfunc");
    lua_setglobal(script, "foobar");

    std::string payload;
    CHECK_FALSE(first.exec.serializeState(payload));
}

// Size of the payload a `counter = 1` script serializes to. Update it when the
// wire format moves; a change nobody meant to make is the thing worth catching.
constexpr size_t kExpectedDonorPayloadSize = 514;

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor invalid restore")
{
    std::string bytecode = Luau::compile(R"(
        counter = 1
    )");

    std::string payload = donorPayload(bytecode);
    TestScript second(bytecode);

    SUBCASE("corrupt payload is refused without exploding")
    {
        REQUIRE(payload.size() > 16);

        // Purely a canary: a size change means the wire format moved, or a
        // build laid the payload out differently, and should show up as a
        // number someone updates deliberately.
        CHECK(payload.size() == kExpectedDonorPayloadSize);

        // Flip every byte in turn. Which offsets get refused depends on the
        // wire layout, but the invariants don't: corruption either restores to
        // a state coherent enough to serialize, or is refused cleanly, leaving
        // the script faulted with nothing to serialize.
        size_t refused = 0;
        auto scratch = std::make_unique<TestScript>(bytecode);
        for (size_t offset = 0; offset < payload.size(); ++offset)
        {
            CAPTURE(offset);
            std::string corrupt = payload;
            corrupt[offset] ^= 0xFF;

            std::string after;
            if (scratch->exec.restoreState(corrupt.data(), corrupt.size()))
            {
                CHECK(scratch->exec.serializeState(after));
                // A successful restore leaves a live instance, and restoring
                // over one is refused, so start over
                scratch = std::make_unique<TestScript>(bytecode);
            }
            else
            {
                ++refused;
                CHECK(scratch->exec.getFaultKind() != FaultKind::None);
                CHECK_FALSE(scratch->exec.serializeState(after));
            }
        }

        // Most of a payload this small is structure rather than free-form
        // data, so refusals shouldn't be rare
        CHECK(refused > payload.size() / 4);
    }

    SUBCASE("malformed payloads are rejected before anything is torn down")
    {
        // Wrong magic, bad version, empty, and truncated at several lengths
        std::string bad_magic = payload;
        bad_magic[0] = 'B';
        CHECK_FALSE(second.exec.restoreState(bad_magic.data(), bad_magic.size()));

        std::string bad_version = payload;
        bad_version[4] = (char)(kScriptStateFingerprint.version + 99);
        CHECK_FALSE(second.exec.restoreState(bad_version.data(), bad_version.size()));

        CHECK_FALSE(second.exec.restoreState("", 0));
        for (size_t len : {size_t(4), size_t(8), size_t(16), payload.size() / 2, payload.size() - 1})
            CHECK_FALSE(second.exec.restoreState(payload.data(), len));

        // Nothing was torn down, so a good payload still loads and runs
        second.exec.clearFault();
        restore(second.exec, payload);
        CHECK(second.exec.getFaultKind() == FaultKind::None);
    }

    SUBCASE("restore directly after instantiate succeeds")
    {
        // Restoring persisted state is the common production start path, so
        // restoreState() must not require a throwaway reset() fork in between
        restore(second.exec, payload);
        CHECK(second.exec.getFaultKind() == FaultKind::None);
        CHECK(second.exec.isMainFunctionComplete());

        CHECK(readIntGlobal(second.exec, "counter") == 1);
    }

    SUBCASE("restore over a live instance is refused")
    {
        // A live instance means the caller would be implicitly discarding a
        // running script; that sequencing is a host bug, so it is refused
        second.loadDefaultState();
        CHECK_FALSE(second.exec.restoreState(payload.data(), payload.size()));
        CHECK(second.exec.getFaultKind() == FaultKind::Runtime);

        // The refusal leaves the live instance untouched and runnable
        second.exec.clearFault();
        resume(second.exec);
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor sleep and fault survive a round trip")
{
    // The payload carries the script instance plus the sleep and fault it was
    // holding, so the host stores one opaque blob and cannot drop half of it.
    std::string bytecode = Luau::compile(R"(
        counter = 1
    )");
    TestScript first(bytecode);
    first.start();

    first.exec.setSleep(0.25f);
    first.exec.setFault(FaultKind::Runtime, "runtime error", "boom\nstacktrace");

    std::string payload = serialize(first.exec);

    TestScript second(bytecode);
    REQUIRE(second.exec.getSleep() == 0.0f);
    REQUIRE(second.exec.getFaultKind() == FaultKind::None);

    restore(second.exec, payload);
    CHECK(second.exec.getSleep() == 0.25f);
    CHECK(second.exec.getFaultKind() == FaultKind::Runtime);
    CHECK(second.exec.getFaultString() == "runtime error");
    CHECK(second.exec.getExtendedFaultString() == "boom\nstacktrace");
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor persistence envelope validation")
{
    std::string bytecode = Luau::compile(R"(
        counter = 1
    )");

    SUBCASE("a state from another flavor is refused")
    {
        // Caught in the envelope rather than somewhere inside Ares
        std::string payload = donorPayload(bytecode);

        TestScript other(bytecode, false, 1);
        CHECK_FALSE(other.exec.restoreState(payload.data(), payload.size()));
        CHECK(other.exec.getFaultKind() == FaultKind::Runtime);
        CHECK_FALSE(other.exec.hasInstance());
    }

    SUBCASE("the persisted traceback is capped")
    {
        TestScript first(bytecode);
        first.start();
        std::string huge_trace(kMaxPersistedFaultLen * 4, 'x');
        first.exec.setFault(FaultKind::Runtime, "runtime error", huge_trace.c_str());

        std::string payload = serialize(first.exec);

        TestScript second(bytecode);
        restore(second.exec, payload);
        CHECK(second.exec.getExtendedFaultString().size() == kMaxPersistedFaultLen);
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor memory limit survives a round trip")
{
    std::string bytecode = Luau::compile(R"(
        counter = 1
    )");
    TestScript first(bytecode);
    first.start();

    // The limit drifts at runtime (llSetMemoryLimit), so the payload must
    // carry it: a restored script continuing with the instantiate-time
    // default would silently regain headroom the host took away.
    constexpr int kLoweredLimit = 96 * 1024;
    REQUIRE(first.exec.setMemoryLimit(kLoweredLimit));

    std::string payload = serialize(first.exec);

    TestScript second(bytecode);
    REQUIRE(second.exec.getMemoryLimit() == kDefaultMemoryLimit);
    restore(second.exec, payload);
    CHECK(second.exec.getMemoryLimit() == kLoweredLimit);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor thread capacities survive a round trip")
{
    // Make sure we don't muck up CallInfo arrays with our serialization.
    std::string bytecode = Luau::compile(R"(
        function LLEvents.moving_start()
            local function deep(n)
                if n > 0 then
                    -- not a tail call, so the frame stays live underneath
                    deep(n - 1)
                end
            end
            -- Grow the CallInfo array well past BASIC_CI_SIZE, then unwind, so
            -- the capacity outruns the used portion by a wide margin
            deep(60)
            preempt()
        end
    )");

    SUBCASE("serialized idle between handlers")
    {
        TestScript first(bytecode);
        first.start();

        lua_State* donor = handlerThread(first.exec);
        REQUIRE(lua_isthreadreset(donor));
        const int size_ci = donor->size_ci;
        const int stacksize = donor->stacksize;
        CAPTURE(size_ci);

        // A reset thread is back to the sizes stack_init hands out
        if (!kExactThreadSizing)
        {
            REQUIRE(size_ci == BASIC_CI_SIZE);
            REQUIRE(stacksize == BASIC_STACK_SIZE + EXTRA_STACK);
        }

        std::string payload = serialize(first.exec);

        TestScript second(bytecode);
        restore(second.exec, payload);

        lua_State* restored = handlerThread(second.exec);
        CHECK(restored->size_ci == size_ci);
        CHECK(restored->stacksize == stacksize);
    }

    SUBCASE("serialized while preempted mid-handler")
    {
        TestScript first(bytecode);
        first.start();

        dispatch(first.exec, "moving_start", HandlerRunStatus::Preempted);

        lua_State* donor = handlerThread(first.exec);
        const int size_ci = donor->size_ci;
        const int stacksize = donor->stacksize;
        const int used_ci = (int)(donor->ci - donor->base_ci) + 1;
        CAPTURE(size_ci);
        CAPTURE(used_ci);

        // The array only ever doubles, so it is past BASIC_CI_SIZE and has a
        // slot to spare over the used portion. Not so under hardstacktests,
        // where a GC pass resizes it to exactly ci_used + 1.
        if (!kExactThreadSizing)
        {
            REQUIRE(size_ci > BASIC_CI_SIZE);
            REQUIRE(size_ci > used_ci);
        }

        std::string payload = serialize(first.exec);

        TestScript second(bytecode);
        restore(second.exec, payload);

        lua_State* restored = handlerThread(second.exec);
        CHECK(restored->size_ci == size_ci);
        CHECK(restored->stacksize == stacksize);

        // ...and the restored handler still runs to completion
        resumeToCompletion(second.exec, 1.0);
    }
}

// A Script carrying durable state of its own, standing in for a host's
// analogue of `LLScriptData`.
class StatefulScript : public Script
{
public:
    using Script::Script;

    int32_t counter = 0;
    std::string label;
    int interrupts = 0;

    static void installVMCallbacks(lua_State* L)
    {
        Script::installVMCallbacks(L);
        lua_callbacks(L)->interrupt = counting_interrupt;
    }

protected:
    StateFingerprint getStateFingerprint() const override { return {{'S', 'T', 'F', 'L'}, 1}; }

    bool serializeExtra(ByteWriter& writer) const override
    {
        writer.writeS32(counter);
        writer.writeString(label);
        return true;
    }

    bool restoreExtra(ByteReader& reader) override { return reader.readS32(counter) && reader.readString(label); }

private:
    static void counting_interrupt(lua_State* L, int gc)
    {
        // Sound because makeScript() is the only way a script reaches this VM
        auto* script = static_cast<StatefulScript*>(Script::fromLuaState(L));
        if (script != nullptr)
            ++script->interrupts;

        Script::interruptHandler(L, gc);
    }
};

class StatefulEnvironment : public Environment
{
public:
    StatefulEnvironment(IProvisioner& provisioner, bool is_lsl, uint32_t api_version)
        : Environment(provisioner, is_lsl, api_version)
        , flavor(is_lsl ? "lsl" : "lua")
    {
    }

    std::string flavor;
};

class StatefulImage : public Image
{
public:
    StatefulImage(std::shared_ptr<IEnvironment> environment, const ImageConfig& config)
        : Image(std::move(environment), config)
        , raw_bytecode_size(config.bytecodeSize)
    {
    }

    // Image only retains the charged size, not what was actually handed to it
    size_t raw_bytecode_size = 0;
};

// Replaces every tier. TestProvisioner's callbacks recover it by downcasting
// getProvisioner() to itself, so a provisioner of other types needs its own.
struct StatefulProvisioner : Provisioner<StatefulScript>
{
    double clock = 0.0;
    double clock_step = 0.0;
    int environments_made = 0;
    int images_made = 0;

    StatefulProvisioner()
        : Provisioner<StatefulScript>(makeCallbacks())
    {
    }

    static HostCallbacks makeCallbacks()
    {
        HostCallbacks callbacks;
        callbacks.quantaClockProvider = quanta_clock;
        return callbacks;
    }

    static double quanta_clock(lua_State* L)
    {
        Script* executor = Script::fromLuaState(L);
        LUAU_ASSERT(executor != nullptr);

        auto& host = static_cast<StatefulProvisioner&>(executor->getProvisioner());
        host.clock += host.clock_step;
        return host.clock;
    }

    // The only place the downcast happens, since everything this provisioner
    // mints comes through makeScript()
    std::shared_ptr<StatefulScript> provision(const std::string& bytecode)
    {
        return std::static_pointer_cast<StatefulScript>(provisionScript(makeImageConfig(bytecode), makeScriptConfig()));
    }

    std::shared_ptr<StatefulScript> start(const std::string& bytecode)
    {
        std::shared_ptr<StatefulScript> script = provision(bytecode);
        REQUIRE(script != nullptr);
        REQUIRE(script->loadDefaultState());
        resume(*script);
        return script;
    }

protected:
    std::shared_ptr<Environment> makeEnvironment(bool is_lsl, uint32_t api_version) override
    {
        ++environments_made;
        return std::make_shared<StatefulEnvironment>(*this, is_lsl, api_version);
    }

    std::shared_ptr<Image> makeImage(std::shared_ptr<IEnvironment> environment, const ImageConfig& config) override
    {
        ++images_made;
        return std::make_shared<StatefulImage>(std::move(environment), config);
    }
};

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor subclass state survives a round trip")
{
    // Long enough to take a loop back-edge interrupt, short enough to finish
    // inside one window
    std::string bytecode = Luau::compile(R"(
        local total = 0
        for i = 1, 10000 do
            total += i
        end
        counter = 1
    )");

    StatefulProvisioner host;
    std::shared_ptr<StatefulScript> first = host.start(bytecode);

    // Every tier is recoverable as the host's own type without the engine
    // knowing any of them exist
    CHECK(host.environments_made == 1);
    CHECK(host.images_made == 1);
    CHECK(static_cast<StatefulEnvironment&>(first->getEnvironment()).flavor == "lua");
    CHECK(static_cast<StatefulImage&>(first->getImage()).raw_bytecode_size == bytecode.size());
    // Chaining to ours left the run window intact, so the script still finished
    CHECK(first->interrupts > 0);

    first->counter = 42;
    first->label = "held";

    std::string payload = serialize(*first);

    // The subclass's fields ride in the same payload as the instance, so the
    // host cannot store one without the other
    std::shared_ptr<StatefulScript> second = host.provision(bytecode);
    REQUIRE(second != nullptr);
    REQUIRE(second->counter == 0);

    restore(*second, payload);
    CHECK(second->counter == 42);
    CHECK(second->label == "held");
    CHECK(readIntGlobal(*second, "counter") == 1);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor refuses a payload from another script class")
{
    std::string bytecode = Luau::compile(R"(
        counter = 1
    )");

    StatefulProvisioner host;
    std::string stateful_payload = serialize(*host.start(bytecode));
    std::string plain_payload = donorPayload(bytecode);

    // The fingerprint at the head of the payload says whose it is, so neither
    // class gets far enough to fork an instance out of the other's state
    TestScript plain(bytecode);
    CHECK_FALSE(plain.exec.restoreState(stateful_payload.data(), stateful_payload.size()));
    CHECK(plain.exec.getFaultKind() == FaultKind::Runtime);
    CHECK_FALSE(plain.exec.hasInstance());

    std::shared_ptr<StatefulScript> stateful = host.provision(bytecode);
    REQUIRE(stateful != nullptr);
    CHECK_FALSE(stateful->restoreState(plain_payload.data(), plain_payload.size()));
    CHECK(stateful->getFaultKind() == FaultKind::Runtime);
    CHECK_FALSE(stateful->hasInstance());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor main function completion")
{
    std::string bytecode = Luau::compile(R"(
        local total = 0
        for i = 1, 100000 do
            total += i
        end
        done = total
    )");

    TestScript first(bytecode);
    first.loadDefaultState();
    // The staged main function has not run yet
    CHECK_FALSE(first.exec.isMainFunctionComplete());

    // Preempt the main mid-loop and serialize the suspended instance
    first.host.clock_step = 0.001;
    RunResult preempted = resume(first.exec, 0.005, HandlerRunStatus::Preempted);
    CHECK_FALSE(first.exec.isMainFunctionComplete());
    CHECK_FALSE(preempted.mainFunctionCompleted);

    std::string payload = serialize(first.exec);

    // The flag travels with the payload: a restored mid-main instance is
    // still incomplete and only flips once the main finishes there
    TestScript second(bytecode);
    restore(second.exec, payload);
    CHECK_FALSE(second.exec.isMainFunctionComplete());

    // The completion is reported on the transition, and only once
    RunResult completed = resumeToCompletion(second.exec, 1.0);
    CHECK(completed.mainFunctionCompleted);
    CHECK(second.exec.isMainFunctionComplete());

    // A payload from a completed main restores with the flag already set
    std::string done_payload = serialize(second.exec);
    TestScript third(bytecode);
    CHECK_FALSE(third.exec.isMainFunctionComplete());
    restore(third.exec, done_payload);
    CHECK(third.exec.isMainFunctionComplete());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor script runtime errors")
{
    TestScript ts(R"(
        local missing = nil
        missing.field = 1
        print("unreachable")
    )");
    ts.loadDefaultState();
    Script& exec = ts.exec;

    // A script that dies inside its main function will never register the rest
    // of its handlers, so the host has to be free to start masking its events
    CHECK_FALSE(exec.isMainFunctionComplete());
    resume(exec, 1.0, HandlerRunStatus::Fault);
    CHECK(exec.isMainFunctionComplete());
    CHECK(exec.getFaultKind() == FaultKind::Runtime);
    CHECK(exec.getFaultString() == "runtime error");
    // Extended info carries the message plus a traceback
    CHECK(exec.getExtendedFaultString().find("attempt to index nil with 'field'") != std::string::npos);
    CHECK(ts.host.printed.empty());
    CHECK_FALSE(exec.isHandlerActive());

    // Clearing the fault and resetting makes the script runnable again
    ts.reset();
    CHECK(exec.getFaultKind() == FaultKind::None);
    CHECK(exec.getExtendedFaultString().empty());
}

// A host that exposes a host_kill() global, standing in for any ll.* call
// that faults the script from the host side (host-initiated kill).
struct KillingTestHost : TestProvisioner
{
    KillingTestHost()
        : TestProvisioner(makeCallbacks())
    {
    }

    static HostCallbacks makeCallbacks()
    {
        HostCallbacks callbacks = TestProvisioner::makeCallbacks();
        callbacks.populateEnvironment = populate_environment;
        return callbacks;
    }

    // Composed rather than overridden: the base population is called through
    // explicitly
    static void populate_environment(IEnvironment& environment, lua_State* L)
    {
        TestProvisioner::populate_environment(environment, L);
        lua_pushcfunction(L, host_kill, "host_kill");
        lua_setglobal(L, "host_kill");
    }

    // This mimics an `ll.*()` function in indra that sets a fault manually.
    static int host_kill(lua_State* L)
    {
        Script* executor = Script::fromLuaState(L);
        REQUIRE(executor != nullptr);
        executor->setFault(FaultKind::Runtime, "killed by host");
        if (executor->getFaultKind() != FaultKind::None)
            lua_killerror(L, executor->getFaultString().c_str());
        return 0;
    }
};

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor host kill aborts the handler")
{
    std::string bytecode = Luau::compile(R"(
        pcall(host_kill)
        print("still alive")
    )");
    KillingTestHost host;
    std::shared_ptr<Script> script = host.provisionScript(makeImageConfig(bytecode), makeScriptConfig());
    REQUIRE(script != nullptr);
    REQUIRE(script->loadDefaultState());

    resume(*script, 1.0, HandlerRunStatus::Fault);
    CHECK(script->getFaultKind() == FaultKind::Runtime);
    CHECK(script->getFaultString() == "killed by host");
    CHECK_FALSE(script->isHandlerActive());
    // The kill preempted everything after the host_kill() call
    CHECK(host.printed.empty());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor does not autocall a state_entry global")
{
    // For SLua the main function *is* state_entry; a global of that name must
    // never be invoked on top of it.
    TestScript ts(R"(
        function state_entry()
            print("SHOULD NOT RUN")
        end
    )");
    ts.loadDefaultState();

    resume(ts.exec);
    CHECK(ts.host.printed.empty());

    ts.reset();
    resume(ts.exec);
    CHECK(ts.host.printed.empty());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor reclaims batched allocations")
{
    // Each batch is garbage by the time the next one is built, so a script that
    // allocates far more than its limit in total still fits if the GC keeps up.
    TestScript ts(R"(
        for batch = 1, 40 do
            local parts = {}
            for i = 1, 200 do
                parts[i] = string.rep("x", 100) .. tostring(i)
            end
        end
        print("survived")
    )");
    ts.loadDefaultState();

    resume(ts.exec);
    CHECK(ts.exec.getFaultKind() == FaultKind::None);
    checkCapture(ts.host.printed, {"survived"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor coroutines survive preemption")
{
    TestScript ts(R"(
        local co = coroutine.create(function(n)
            local total = 0
            for i = 1, n do
                total += i
                if i % 10 == 0 then
                    coroutine.yield(total)
                end
            end
            return total
        end)

        local last = 0
        while coroutine.status(co) ~= "dead" do
            local ok, value = coroutine.resume(co, 50)
            assert(ok, "coroutine resume failed")
            if value then last = value end
        end
        print(`{last}`)
    )");
    ts.loadDefaultState();

    // Preempt repeatedly while a user coroutine is mid-flight
    ts.host.clock_step = 0.001;
    int preemptions = 0;
    resumeToCompletion(ts.exec, 0.003, HandlerRunStatus::Ok, &preemptions);
    CHECK(preemptions >= 1);
    checkCapture(ts.host.printed, {"1275"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor hides engine threads from coroutine.running")
{
    TestScript script(R"(
        in_main = coroutine.running() == nil and 1 or 0
        in_coro = coroutine.wrap(function()
            return coroutine.running() ~= nil and 1 or 0
        end)()
    )");
    script.start();
    // The staged main runs on the handler thread, which the bodged
    // coroutine.running() hides; a user coroutine still sees itself
    CHECK(readIntGlobal(script.exec, "in_main") == 1);
    CHECK(readIntGlobal(script.exec, "in_coro") == 1);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor failed LSL constructor leaves no instance")
{
    // Compiled LSL should never error during construction, but the engine
    // cannot assume the bytecode is well-behaved. A failed load must not leave
    // a live instance behind: restoreState() is documented as valid after a
    // failed load, and refuses to run over a live instance.
    std::string bytecode = Luau::compile(R"(
        error("constructor boom")
    )");
    TestScript ts(bytecode, true);

    CHECK_FALSE(ts.exec.loadDefaultState());
    CHECK(ts.exec.getFaultKind() == FaultKind::Runtime);
    CHECK(!ts.exec.hasInstance());
}

#ifdef LUAU_USE_TAILSLIDE

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor LSL lifecycle and reset")
{
    TestScript ts(R"(
        integer counter = 5;
        default {
            state_entry() {
                counter = counter + 1;
                if (counter == 6) {
                    print("six");
                } else {
                    print("other");
                }
            }
        }
    )", true);
    ts.loadDefaultState();
    Script& exec = ts.exec;

    CHECK(exec.hasLSLEventHandler(0, "state_entry"));
    CHECK_FALSE(exec.hasLSLEventHandler(0, "touch_start"));
    // An event with no handler behind it reports that nothing ran, rather than
    // looking like a handler that completed
    dispatch(exec, "touch_start", HandlerRunStatus::NotRun);
    // The LSL constructor already ran during reset; no handler is staged
    CHECK_FALSE(exec.isHandlerActive());
    CHECK(exec.isMainFunctionComplete());

    dispatch(exec, "state_entry");

    // Globals persist between handler invocations of the same instance
    dispatch(exec, "state_entry");

    // A reset re-forks a pristine instance and reruns the constructor
    ts.reset();
    dispatch(exec, "state_entry");

    checkCapture(ts.host.printed, {"six", "other", "six"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor LSL state change")
{
    TestScript ts(R"(
        default {
            state_entry() {
                state other;
            }
        }
        state other {
            state_entry() {
                print("in_other");
            }
        }
    )", true);
    ts.loadDefaultState();
    Script& exec = ts.exec;

    RunResult result = dispatch(exec, "state_entry", HandlerRunStatus::StateChange);
    CHECK(result.newState != 0);
    CHECK_FALSE(exec.isHandlerActive());

    // State register bookkeeping is the host's job; dispatch into the new state
    REQUIRE(exec.hasLSLEventHandler(result.newState, "state_entry"));
    dispatch(exec, result.newState, "state_entry");
    checkCapture(ts.host.printed, {"in_other"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor state change survives serialization")
{
    std::string bytecode = compileLSL(R"(
        default {
            state_entry() {
                state other;
            }
        }
        state other {
            state_entry() {
                print("in_other");
            }
        }
    )");

    TestScript first(bytecode, true);
    first.loadDefaultState();

    RunResult result = dispatch(first.exec, "state_entry", HandlerRunStatus::StateChange);
    int new_state = result.newState;

    // The host owns the state registers, so it serializes with the pending
    // change recorded on its side and dispatches into the new state after
    // restoring.
    std::string payload = serialize(first.exec);

    TestScript second(bytecode, true);
    restore(second.exec, payload);

    REQUIRE(second.exec.hasLSLEventHandler(new_state, "state_entry"));
    dispatch(second.exec, new_state, "state_entry");
    checkCapture(second.host.printed, {"in_other"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor serialize mid-handler roundtrip")
{
    std::string bytecode = compileLSL(R"(
        default {
            state_entry() {
                integer i = 0;
                while (i < 200) {
                    i = i + 1;
                }
                print("done");
            }
        }
    )");

    TestScript first(bytecode, true);
    first.loadDefaultState();

    // Preempt the handler mid-loop
    first.host.clock_step = 0.001;
    dispatch(first.exec, "state_entry", HandlerRunStatus::Preempted, nullptr, nullptr, 0.005);
    REQUIRE(first.exec.isHandlerActive());
    CHECK(first.host.printed.empty());

    std::string payload = serialize(first.exec);
    CHECK(!payload.empty());

    // Rehydrate the suspended handler in a second engine built from the same
    // bytecode and run it to completion there
    TestScript second(bytecode, true);
    restore(second.exec, payload);
    REQUIRE(second.exec.isHandlerActive());

    resumeToCompletion(second.exec, 1.0);
    CHECK_FALSE(second.exec.isHandlerActive());
    checkCapture(second.host.printed, {"done"});
    // The original instance never reached the print
    CHECK(first.host.printed.empty());
}

#endif // LUAU_USE_TAILSLIDE

TEST_SUITE_END();
