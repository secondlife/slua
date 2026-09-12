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
#include "Luau/LSLBuiltins.h"
#include "Luau/ParseResult.h"

#include "doctest.h"
#include "ScopedFlags.h"
#include "SLExecutorFixture.h"

// For the handler thread's stack and CallInfo capacities, which the API surface
// has no reason to expose
#include "../VM/src/lgc.h"
#include "../VM/src/lstate.h"

#ifdef LUAU_USE_TAILSLIDE
#include "Luau/LSLCompiler.h"
#endif

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Luau;
using namespace Luau::Executor;

LUAU_FASTFLAG(SLuaEagerWeakClear)

// Wraps bytes the caller supplies, which no compiler need have produced, and
// can declare a charged size the payload doesn't match. Tests that just want a
// working asset use compileTestAsset() instead.
static TestAsset makeAsset(const std::string& bytecode, uint32_t charged = 0)
{
    BytecodeHeader header;
    header.chargedBytecodeSize = charged;

    TestAsset asset;
    writeBytecodeHeader(asset.bytes, header);
    asset.bytes += bytecode;
    return asset;
}

// Builds an image the test expects to load
static std::shared_ptr<IImage> makeImage(TestProvisioner& host, const std::shared_ptr<IEnvironment>& env, const TestAsset& asset)
{
    std::shared_ptr<IImage> image = host.buildImage(env, asset);
    REQUIRE(image != nullptr);
    REQUIRE(image->isValid());
    return image;
}

// Same, in a fresh environment of its own
static std::shared_ptr<IImage> makeImage(TestProvisioner& host, const TestAsset& asset)
{
    return makeImage(host, host.createEnvironment(false, 0), asset);
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

// Runs `asset` to completion on a throwaway script and returns its serialized
// payload; the restoreState() tests all start from one of these.
static std::string donorPayload(const TestAsset& asset)
{
    TestScript donor(asset);
    donor.start();
    return serialize(donor.exec);
}

// Delivers a timer tick the way the sim would: advance the script clock, then
// dispatch a `timer` event and run the callbacks to completion.
static void tickTimers(TestScript& ts, double script_time)
{
    ts.host.script_clock = script_time;
    RunResult result = dispatchRaw(ts.exec, LSLEvent::Timer);
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

TEST_SUITE_BEGIN("SLExecutor");

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor invalid bytecode")
{
    std::string bytecode = "definitely not luau bytecode";
    TestProvisioner host;
    std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);
    std::shared_ptr<IImage> image = host.buildImage(env, makeAsset(bytecode));
    CHECK_FALSE(image->isValid());
    // The load error is surfaced for the host's compile-error reporting
    CHECK_FALSE(image->getError().empty());
    CHECK(host.instantiateScript(image, makeScriptConfig()) == nullptr);
    CHECK(host.provisionScript(makeAsset(bytecode), makeScriptConfig()) == nullptr);

    // A failed load leaves the environment usable
    makeImage(host, env, compileTestAsset(R"(
        counter = 1
    )"));
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor a failed compile never becomes an asset")
{
    // Throwing rather than returning something asset-shaped means there is
    // nothing a caller could store by forgetting to check it
    CHECK_THROWS_AS(
        Luau::compileAssetOrThrow(R"(
            local x = = 5
        )"),
        Luau::ParseErrors
    );

    BytecodeHeader parsed;
    size_t bytecode_start = 0;
    TestAsset good = compileTestAsset(R"(
        counter = 1
    )");
    REQUIRE(readBytecodeHeader(good.bytes.data(), good.bytes.size(), parsed, bytecode_start));
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
    std::shared_ptr<IImage> image = host.buildImage(env, makeAsset("definitely not luau bytecode"));
    REQUIRE_FALSE(image->isValid());
    REQUIRE(captured.size() == 2);
    CHECK(captured[1].level == LogLevel::Warn);
    CHECK(captured[1].source == "test_script");
    CHECK(captured[1].message == "Failed to load Luau bytecode: " + image->getError());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor provisioner topologies")
{
    const char* source = R"(
        counter = 1
    )";
    TestAsset asset = compileTestAsset(source);

    SUBCASE("cross-provisioner refusals")
    {
        TestProvisioner host;
        std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);
        std::shared_ptr<IImage> image = makeImage(host, env, asset);

        // A provisioner never builds into or instantiates from another
        // provisioner's tiers
        TestProvisioner other_host;
        CHECK(other_host.buildImage(env, asset) == nullptr);
        CHECK(other_host.instantiateScript(image, makeScriptConfig()) == nullptr);

        // Nor does an image build into an environment of another flavor. The
        // flavor is the asset's now, so that's a bad image rather than a refusal
        std::shared_ptr<IImage> mismatched = host.buildImage(env, compileTestAsset(source, false, 1));
        REQUIRE(mismatched != nullptr);
        CHECK_FALSE(mismatched->isValid());
    }

    SUBCASE("one environment hosts multiple images")
    {
        TestAsset asset2 = compileTestAsset(R"(
            counter = 2
        )");
        TestProvisioner host;
        std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);
        std::weak_ptr<IEnvironment> env_watch = env;
        std::shared_ptr<IImage> image1 = makeImage(host, env, asset);
        std::shared_ptr<IImage> image2 = makeImage(host, env, asset2);

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
        TestAsset mutable_asset = compileTestAsset(R"(
            counter = (counter or 0) + 1
        )");
        TestProvisioner host;
        std::shared_ptr<IImage> image = makeImage(host, mutable_asset);
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
        std::shared_ptr<IImage> image = makeImage(host, asset);
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
        std::shared_ptr<Script> first = host.provisionScript(asset, makeScriptConfig());
        std::shared_ptr<Script> second = host.provisionScript(asset, makeScriptConfig());
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        CHECK(&first->getEnvironment() != &second->getEnvironment());
    }
}

// A host that shares one image per distinct asset by composing the public
// factories; a real host would key on its asset identity instead.
struct CachingTestHost : TestProvisioner
{
    std::map<std::string, std::shared_ptr<IImage>> image_cache;
    // One environment per flavor, chosen off the asset header the way a host
    // has to before it can build an image
    std::map<std::pair<bool, uint32_t>, std::shared_ptr<IEnvironment>> environment_cache;

    std::shared_ptr<IEnvironment> environmentFor(const TestAsset& asset)
    {
        BytecodeHeader header;
        size_t bytecode_start = 0;
        REQUIRE(readBytecodeHeader(asset.bytes.data(), asset.bytes.size(), header, bytecode_start));

        std::shared_ptr<IEnvironment>& environment = environment_cache[{header.isLSL, header.apiVersion}];
        if (environment == nullptr)
            environment = createEnvironment(header.isLSL, header.apiVersion);
        return environment;
    }

    std::shared_ptr<Script> provision(const TestAsset& asset)
    {
        std::shared_ptr<IImage>& image = image_cache[asset.bytes];
        if (image == nullptr)
            image = buildImage(environmentFor(asset), asset);
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
    TestAsset asset = compileTestAsset(R"(
        counter = (counter or 0) + 1
    )");
    TestAsset asset2 = compileTestAsset(R"(
        counter = 2
    )");

    CachingTestHost host;
    std::shared_ptr<Script> first = host.provision(asset);
    std::shared_ptr<Script> second = host.provision(asset);
    std::shared_ptr<Script> other = host.provision(asset2);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(other != nullptr);

    CHECK(&first->getImage() == &second->getImage());
    CHECK(&first->getEnvironment() == &second->getEnvironment());
    CHECK(&first->getImage() != &other->getImage());

    // The cache owns its entries: releasing the scripts leaves the image
    // resident, and only the host's sweep drops it
    std::weak_ptr<IImage> watch = host.image_cache[asset.bytes];
    first.reset();
    second.reset();
    CHECK_FALSE(watch.expired());
    // The other asset is still in use, so the sweep leaves it alone
    CHECK(host.releaseUnusedImages() == 1);
    CHECK(watch.expired());

    std::shared_ptr<Script> again = host.provision(asset);
    REQUIRE(again != nullptr);
    CHECK(host.image_cache.count(asset.bytes) == 1);

#ifdef LUAU_USE_TAILSLIDE
    // An asset of another flavor lands in its own environment, and the
    // build refuses to put it anywhere else
    TestAsset lsl = compileTestAsset(R"(
        default {
            state_entry() {}
        }
    )", true);
    std::shared_ptr<Script> lsl_script = host.provision(lsl);
    REQUIRE(lsl_script != nullptr);
    CHECK(&lsl_script->getEnvironment() != &again->getEnvironment());
    CHECK(host.environment_cache.size() == 2);

    std::shared_ptr<IImage> misplaced = host.buildImage(host.environmentFor(asset), lsl);
    REQUIRE(misplaced != nullptr);
    CHECK_FALSE(misplaced->isValid());
#endif
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor co-resident scripts account memory independently")
{
    // Parks a large table in a global on demand, so one instance can grow
    // while its neighbour in the same VM sits idle
    TestAsset asset = compileTestAsset(R"(
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
    std::shared_ptr<IImage> image = makeImage(host, asset);
    std::shared_ptr<Script> first = startScript(host, image);
    std::shared_ptr<Script> second = startScript(host, image);

    int first_baseline = first->getUsedMemory();
    int second_baseline = second->getUsedMemory();

    // Same image, so the bytecode is charged to both in full rather than split
    // between them, and two fresh instances of it start out level
    CHECK(first_baseline >= (int)asset.chargedSize());
    CHECK(second_baseline >= (int)asset.chargedSize());
    CHECK(first_baseline == second_baseline);

    dispatch(*first, LSLEvent::MovingStart);

    // The allocation lands on the script that made it
    CHECK(first->getUsedMemory() > first_baseline + 30000);

    // ... and not on its neighbour. Running a handler on the idle script is
    // what dirties its cached size, so this is a real re-measurement of the
    // instance rather than the number read back above.
    dispatch(*second, LSLEvent::MovingEnd);
    CHECK(second->getUsedMemory() < second_baseline + 1000);

    // gcinfo() is the script-visible face of the same accounting: each script
    // reads its own size in KB, not the shared VM's. The second reading was
    // taken after the hoard existed, so a VM-wide gcinfo would have counted
    // it in both.
    CHECK(readIntGlobal(*first, "reading") > readIntGlobal(*second, "reading") + 25);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor interleaves co-resident scripts")
{
    TestAsset slow_asset = compileTestAsset(R"(
        total = 0
        for i = 1, 20000 do
            total += i
        end
    )");
    TestAsset quick_asset = compileTestAsset(R"(
        counter = 7
    )");

    TestProvisioner host;
    std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);
    std::shared_ptr<Script> slow = host.instantiateScript(makeImage(host, env, slow_asset), makeScriptConfig());
    std::shared_ptr<Script> quick = host.instantiateScript(makeImage(host, env, quick_asset), makeScriptConfig());
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
    dispatch(exec, LSLEvent::MovingStart, HandlerRunStatus::Refused);
    CHECK(ts.host.printed.empty());

    resume(exec);

    dispatch(exec, LSLEvent::MovingStart);
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
    dispatch(ts.exec, LSLEvent::MovingStart, HandlerRunStatus::Preempted, nullptr, nullptr, 0.005);
    REQUIRE(ts.exec.isHandlerActive());

    ts.host.clock_step = 0.0;
    ts.exec.abortHandler();
    CHECK_FALSE(ts.exec.isHandlerActive());

    // Aborting again is a no-op
    ts.exec.abortHandler();

    // The aborted handler's tail never ran, and the thread is clean enough
    // to dispatch on again
    RunResult rerun = dispatch(ts.exec, LSLEvent::MovingStart);
    // Main-function completion was already reported, never again
    CHECK_FALSE(rerun.mainFunctionCompleted);
    CHECK(readIntGlobal(ts.exec, "ran") == 102);
}

TEST_CASE("LSLEvents registry")
{
    // The enum's events are there before any builtins.txt is loaded
    CHECK(lslEventIndex("state_entry") == LSLEvent::StateEntry);
    CHECK(lslEventIndex("final_damage") == LSLEvent::FinalDamage);
    CHECK(lslEventIndex("bogus") == 0);
    CHECK(lslEventIndex(nullptr) == 0);
    CHECK(std::string(lslEventName(LSLEvent::Timer)) == "timer");
    CHECK(lslEventName(0) == nullptr);
    CHECK(lslEventName(LSLEvent::KnownCount) == nullptr);
    CHECK(LSLEventBit::StateEntry == 1);
    CHECK(lslEventBit(0) == 0);
    CHECK(lslEventBit(65) == 0);

    // The embedded builtins.txt agrees with the enum
    luauSL_init_global_builtins(nullptr);
    CHECK(getLSLEventNames().size() >= (size_t)(LSLEvent::KnownCount - 1));
    CHECK(lslEventIndex("timer") == LSLEvent::Timer);

    // A file that moves a known event is refused, and the registry stands
    std::vector<std::string> shuffled = getLSLEventNames();
    std::swap(shuffled[0], shuffled[1]);
    CHECK_FALSE(setLSLEventNames(shuffled));
    CHECK(lslEventIndex("state_entry") == LSLEvent::StateEntry);

    // One that appends is not, and the new event is reachable by index
    std::vector<std::string> extended = getLSLEventNames();
    extended.push_back("brand_new");
    CHECK(setLSLEventNames(extended));
    CHECK(lslEventIndex("brand_new") == (int)extended.size());
    CHECK(std::string(lslEventName((int)extended.size())) == "brand_new");
    luauSL_init_global_builtins(nullptr);
    CHECK(lslEventIndex("brand_new") == 0);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor handler registers")
{
    TestScript ts(R"(
        function LLEvents.timer()
            for i = 1, 100000 do end
        end
    )");
    Script& exec = ts.exec;
    CHECK(exec.getCurrentHandler() == 0);
    CHECK(exec.getStickyHandler() == 0);

    // The staged main is the state_entry handler in flight. For SLua that is
    // literally what it is, so nothing is left pending behind it.
    ts.loadDefaultState();
    CHECK(exec.getCurrentHandler() == LSLEventBit::StateEntry);
    CHECK(exec.getStickyHandler() == LSLEventBit::StateEntry);
    CHECK(exec.getCurrentEvents() == 0);

    resume(exec);
    CHECK(exec.getCurrentHandler() == 0);
    CHECK(exec.getStickyHandler() == LSLEventBit::StateEntry);

    // A dispatch consumes its own bit from the host's pending events, and
    // only its own
    exec.setCurrentEvents(LSLEventBit::Timer | LSLEventBit::TouchStart);
    ts.host.clock_step = 0.001;
    dispatch(exec, LSLEvent::Timer, HandlerRunStatus::Preempted, nullptr, nullptr, 0.005);
    CHECK(exec.getCurrentHandler() == LSLEventBit::Timer);
    CHECK(exec.getStickyHandler() == LSLEventBit::Timer);
    CHECK(exec.getCurrentEvents() == LSLEventBit::TouchStart);

    ts.host.clock_step = 0.0;
    resumeToCompletion(exec, 1.0);
    CHECK(exec.getCurrentHandler() == 0);
    CHECK(exec.getStickyHandler() == LSLEventBit::Timer);
    CHECK(exec.getCurrentEvents() == LSLEventBit::TouchStart);

    // Aborting clears the register the same as completing
    ts.host.clock_step = 0.001;
    dispatch(exec, LSLEvent::Timer, HandlerRunStatus::Preempted, nullptr, nullptr, 0.005);
    ts.host.clock_step = 0.0;
    exec.abortHandler();
    CHECK(exec.getCurrentHandler() == 0);
    CHECK(exec.getStickyHandler() == LSLEventBit::Timer);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor handler registers survive a round trip")
{
    TestAsset asset = compileTestAsset(R"(
        function LLEvents.timer()
            for i = 1, 100000 do end
        end
    )");
    TestScript first(asset);
    first.start();
    first.exec.setCurrentEvents(LSLEventBit::TouchStart);
    first.host.clock_step = 0.001;
    dispatch(first.exec, LSLEvent::Timer, HandlerRunStatus::Preempted, nullptr, nullptr, 0.005);
    std::string payload = serialize(first.exec);

    TestScript second(asset);
    restore(second.exec, payload);
    CHECK(second.exec.getCurrentHandler() == LSLEventBit::Timer);
    CHECK(second.exec.getStickyHandler() == LSLEventBit::Timer);
    CHECK(second.exec.getCurrentEvents() == LSLEventBit::TouchStart);

    resumeToCompletion(second.exec, 1.0);
    CHECK(second.exec.getCurrentHandler() == 0);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor SLua handler set follows registrations")
{
    TestAsset asset = compileTestAsset(R"(
        local function handler() end
        LLEvents:on("touch_start", handler)
        LLEvents:on("timer", handler)
        LLEvents:off("touch_start", handler)
    )");
    TestScript first(asset);
    // Nothing to know until an instance exists
    CHECK_FALSE(first.exec.eventHandlersKnown());
    first.loadDefaultState();
    CHECK(first.exec.getEventHandlers() == 0);
    // Still not: main may register more before it finishes
    CHECK_FALSE(first.exec.eventHandlersKnown());

    resume(first.exec);
    CHECK(first.exec.getEventHandlers() == LSLEventBit::Timer);
    CHECK(first.exec.eventHandlersKnown());

    // Registrations happen at runtime, so nothing but the payload can rebuild
    // the register
    std::string payload = serialize(first.exec);
    TestScript second(asset);
    restore(second.exec, payload);
    CHECK(second.exec.getEventHandlers() == LSLEventBit::Timer);
    CHECK(second.exec.eventHandlersKnown());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor value-bearing yield from a handler faults")
{
    // Preemption never yields, so a yield carrying values off the handler
    // thread can only be the script's own coroutine.yield, and those are
    // reserved for the engine
    TestScript ts(R"(
        function LLEvents.moving_start()
            coroutine.yield(1)
            print("unreachable")
        end
    )");
    ts.start();

    dispatch(ts.exec, LSLEvent::MovingStart, HandlerRunStatus::Fault);
    CHECK(ts.exec.getFaultKind() == FaultKind::UnexpectedYield);
    CHECK_FALSE(ts.exec.isHandlerActive());
    CHECK(ts.host.printed.empty());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor faulted script refuses every handler")
{
    TestAsset asset = compileTestAsset(R"(
        function LLEvents.moving_start()
            print("moved")
        end
        error("boom")
    )");

    SUBCASE("script fault")
    {
        TestScript ts(asset);
        ts.loadDefaultState();
        resume(ts.exec, 1.0, HandlerRunStatus::Fault);

        // The handler was registered before the death, and still never runs
        dispatch(ts.exec, LSLEvent::MovingStart, HandlerRunStatus::Fault);
        CHECK(ts.exec.resumeEventHandler().status == HandlerRunStatus::Fault);
        CHECK(ts.host.printed.empty());
        CHECK(ts.exec.getCurrentHandler() == 0);
    }

    SUBCASE("host fault")
    {
        TestScript ts(R"(
            function LLEvents.moving_start()
                print("moved")
            end
        )");
        ts.start();
        ts.exec.setFault(FaultKind::Runtime, "killed by host");

        dispatch(ts.exec, LSLEvent::MovingStart, HandlerRunStatus::Fault);
        CHECK(ts.host.printed.empty());
        CHECK(ts.exec.getCurrentHandler() == 0);
    }
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
    REQUIRE(exec.callEventHandler(LSLEvent::MovingStart, nullptr).status == HandlerRunStatus::Preempted);
    CHECK(exec.isYieldDue());

    // Finishing that handler and running another one in the same window leaves
    // the flag set: it describes the window, not the last call
    exec.setForceYield(false);
    REQUIRE(exec.resumeEventHandler().status == HandlerRunStatus::Ok);
    CHECK(exec.isYieldDue());
    REQUIRE(exec.callEventHandler(LSLEvent::MovingEnd, nullptr).status == HandlerRunStatus::Ok);
    CHECK(exec.isYieldDue());

    // The flag survives the window's close
    exec.endRunWindow();
    CHECK(exec.isYieldDue());

    // Only a fresh window clears it
    exec.beginRunWindow(1.0);
    CHECK_FALSE(exec.isYieldDue());
    exec.endRunWindow();
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

    // The asset declares a charged size unrelated to what it actually carries,
    // so the engine could swap the bytecode without moving reported memory
    TestAsset asset = makeAsset(bytecode, (uint32_t)(bytecode.size() + 4096));

    ScriptConfig script_config = makeScriptConfig();
    script_config.hostContext = &host_object;
    script_config.memoryLimit = 64 * 1024;

    std::shared_ptr<Script> script = host.provisionScript(asset, script_config);
    REQUIRE(script != nullptr);
    CHECK(script->getHostContext() == &host_object);
    CHECK(script->getMemoryLimit() == 64 * 1024);

    // The declared size is what gets billed, not the bytecode's own length
    REQUIRE(asset.chargedSize() == bytecode.size() + 4096);
    REQUIRE(asset.chargedSize() != asset.bytecodeSize());

    // The charge applies before an instance exists and after it does
    CHECK(script->getUsedMemory() >= (int)asset.chargedSize());
    REQUIRE(script->loadDefaultState());
    resume(*script);
    CHECK(script->getUsedMemory() >= (int)asset.chargedSize());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor weak references die before OoM")
{
    ScopedFastFlag eager_weak_clear{FFlag::SLuaEagerWeakClear, true};

    TestAsset asset = compileTestAsset(R"(
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

    std::shared_ptr<Script> script = host.provisionScript(asset, script_config);
    REQUIRE(script != nullptr);
    REQUIRE(script->loadDefaultState());

    // Keep the real GC from completing a cycle on its own so any weak
    // clearing is attributable to the memory limit callback's heap walk.
    lua_gc(script->getInstanceState(), LUA_GCCOLLECT, 0);
    lua_gc(script->getInstanceState(), LUA_GCSTOP, 0);

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
    SUBCASE("baseline heap walk feeds the exclusion accumulator")
    {
        // Trigger a ton of allocations so the lua_userthreadgc() check kicks
        // in to establish the baseline heap size. That walk's timing depends
        // on engine bookkeeping state, so its cost is excluded.
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

    SUBCASE("limit-pressure heap walks are charged")
    {
        // Walks forced by allocating near the limit are the script's own
        // work, so unlike the baseline walk they bank no exclusion.
        TestScript ts(R"(
            baseline = {}
            preempt()
            local s
            for i = 1, 1000 do
                s = string.rep("x", 200)
            end
            print("churned")
        )");
        ts.loadDefaultState();
        Script& exec = ts.exec;
        lua_State* script = exec.getInstanceState();
        lua_gc(script, LUA_GCSETGOAL, 100000);
        lua_gc(script, LUA_GCCOLLECT, 0);

        ts.host.clock_step = 0.001;
        // The first window's allocation establishes the baseline measurement
        resume(exec, 10.0, HandlerRunStatus::Preempted);
        CHECK(exec.getExcludedTime() > 0.0);

        // Pin the limit just over current use so the churn loop repeatedly
        // forces recalc walks
        exec.setMemoryLimitUnsafe(exec.getUsedMemory() + 4096);
        resume(exec, 10.0, HandlerRunStatus::Ok);
        CHECK(exec.getExcludedTime() == 0.0);
        checkCapture(ts.host.printed, {"churned"});
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

    RunResult result;
    {
        RunWindow window(ts.exec, 1.0);
        ts.exec.setSleep(1.0f);
        result = ts.exec.resumeEventHandler();
    }
    REQUIRE(result.status == HandlerRunStatus::Preempted);
    CHECK(ts.exec.isYieldDue());
    // Sleep-driven preemption carries no punishment
    CHECK(ts.exec.getSleep() == 1.0f);

    resumeToCompletion(ts.exec, 1.0);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor run window guard")
{
    TestScript ts(R"(
        local total = 0
        for i = 1, 100 do
            total += i
        end
        print("finished")
    )");
    ts.loadDefaultState();

    RunResult result;
    {
        RunWindow window(ts.exec, 1.0);
        ts.exec.setForceYield(true);
        result = ts.exec.resumeEventHandler();
    }
    REQUIRE(result.status == HandlerRunStatus::Preempted);
    // Scope exit closed the window: GC unparked, force-yield cleared
    CHECK(ts.exec.getInstanceState()->global->GCthreshold < SIZE_MAX);
    resumeToCompletion(ts.exec, 1.0);
    checkCapture(ts.host.printed, {"finished"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor run window catch-up collects the window's garbage")
{
    TestScript ts(R"(
        for i = 1, 20000 do
            local t = {i, i, i}
        end
        peak_kb = gc_count()
    )");
    ts.loadDefaultState();
    lua_State* instance = ts.exec.getInstanceState();

    // Parked for the whole window, so the heap only grows until the end
    resumeToCompletion(ts.exec, 1.0);
    lua_getglobal(instance, "peak_kb");
    int peak_kb = lua_tointeger(instance, -1);
    lua_pop(instance, 1);
    REQUIRE(peak_kb > 0);

    // The catch-up loop's postcondition, and proof it actually collected
    CHECK(!luaC_needsGC(instance));
    CHECK(lua_gc(instance, LUA_GCCOUNT, 0) < peak_kb);
}

// Real quanta clock, handler put in at the deadline by a watchdog thread or a
// timer signal. The policies below all get the same tests.
static HostCallbacks deadlineCallbacks(InterruptInstallPolicy policy)
{
    HostCallbacks callbacks;
    callbacks.interruptInstallPolicy = policy;
    return callbacks;
}

static const std::vector<InterruptInstallPolicy> kDeadlinePolicies = {
    InterruptInstallPolicy::Threaded,
#if defined(__linux__)
    InterruptInstallPolicy::Signal,
#endif
};

static const char* kBusyLoop = R"(
    local total = 0
    for i = 1, 1000000000 do
        total += i
    end
)";

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor watchdog preempts a busy script")
{
    for (InterruptInstallPolicy policy : kDeadlinePolicies)
    {
        INFO("policy ", (int)policy);
        TestProvisioner host{deadlineCallbacks(policy)};
        std::shared_ptr<Script> script = host.provisionScript(compileTestAsset(kBusyLoop), makeScriptConfig());
        REQUIRE(script != nullptr);
        REQUIRE(script->loadDefaultState());

        // 1e9 iterations can't finish inside 2ms, so preemption is the only way out
        RunResult result = resumeRaw(*script, 0.002);
        REQUIRE(result.status == HandlerRunStatus::Preempted);
        CHECK(script->isYieldDue());
        CHECK(script->getInstanceState()->global->GCthreshold < SIZE_MAX);
        // The fired handler doesn't outlive the window
        CHECK((lua_callbacks(script->getInstanceState())->interrupt == nullptr));
        CHECK(host.getWatchdogStats().fires == 1);
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor watchdog honors a mid-window sleep")
{
    for (InterruptInstallPolicy policy : kDeadlinePolicies)
    {
        INFO("policy ", (int)policy);
        TestProvisioner host{deadlineCallbacks(policy)};
        std::shared_ptr<Script> script = host.provisionScript(compileTestAsset(kBusyLoop), makeScriptConfig());
        REQUIRE(script != nullptr);
        REQUIRE(script->loadDefaultState());

        // Quanta generous enough that only the sleep can preempt us
        RunResult result;
        {
            RunWindow window(*script, 10.0);
            script->setSleep(0.5f);
            result = script->resumeEventHandler();
        }
        REQUIRE(result.status == HandlerRunStatus::Preempted);
        CHECK(script->isYieldDue());
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor watchdog uninstalls a withdrawn force-yield")
{
    for (InterruptInstallPolicy policy : kDeadlinePolicies)
    {
        INFO("policy ", (int)policy);
        TestProvisioner host{deadlineCallbacks(policy)};
        std::shared_ptr<Script> script = host.provisionScript(compileTestAsset(R"(
            local total = 0
            for i = 1, 1000 do
                total += i
            end
        )"), makeScriptConfig());
        REQUIRE(script != nullptr);
        REQUIRE(script->loadDefaultState());

        // The set installs the handler. The first safepoint finds nothing to do
        // and the deadline far away, so it uninstalls itself.
        RunResult result;
        {
            RunWindow window(*script, 10.0);
            script->setForceYield(true);
            script->setForceYield(false);
            result = script->resumeEventHandler();
        }
        REQUIRE(result.status == HandlerRunStatus::Ok);
        CHECK(!script->isYieldDue());
        CHECK((lua_callbacks(script->getInstanceState())->interrupt == nullptr));
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor deadline installers honor the host fire lead")
{
    for (InterruptInstallPolicy policy : kDeadlinePolicies)
    {
        INFO("policy ", (int)policy);
        HostCallbacks callbacks = deadlineCallbacks(policy);
        // A lead longer than the window means the deadline is already inside
        // it when the window opens, so the handler goes in up front and the
        // first safepoint yields.
        callbacks.interruptFireLead = 1.0;
        TestProvisioner host{callbacks};
        std::shared_ptr<Script> script = host.provisionScript(compileTestAsset(kBusyLoop), makeScriptConfig());
        REQUIRE(script != nullptr);
        REQUIRE(script->loadDefaultState());

        RunResult result = resumeRaw(*script, 0.002);
        REQUIRE(result.status == HandlerRunStatus::Preempted);
        CHECK(script->isYieldDue());
        WatchdogStats stats = host.getWatchdogStats();
        CHECK(stats.fires == 1);
        CHECK(stats.fireLead == 1.0);
    }
}

#if defined(__linux__)
TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor signal installer handles a deadline that is already due")
{
    TestProvisioner host{deadlineCallbacks(InterruptInstallPolicy::Signal)};
    std::shared_ptr<Script> script = host.provisionScript(compileTestAsset(kBusyLoop), makeScriptConfig());
    REQUIRE(script != nullptr);
    REQUIRE(script->loadDefaultState());

    // A zero quanta puts the deadline inside the fire lead before the timer
    // could be asked for anything, so the install has to happen inline and
    // the first safepoint yields.
    RunResult result = resumeRaw(*script, 0.0);
    REQUIRE(result.status == HandlerRunStatus::Preempted);
    CHECK(script->isYieldDue());
    CHECK(host.getWatchdogStats().fires == 1);
    CHECK((lua_callbacks(script->getInstanceState())->interrupt == nullptr));
}
#endif

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor memory limit")
{
    TestAsset asset = compileTestAsset(R"(
        local parts = {}
        for i = 1, 400 do
            parts[i] = string.rep("x", 100) .. tostring(i)
        end
        held_parts = parts
        print("allocated")
    )");

    TestScript ts(asset);
    ts.loadDefaultState();
    Script& exec = ts.exec;

    SUBCASE("allocations under the limit succeed and are measured")
    {
        int baseline = exec.getUsedMemory();
        CHECK(baseline >= (int)asset.chargedSize());

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
    dispatch(ts.exec, LSLEvent::LinkMessage, HandlerRunStatus::Ok, [](lua_State* handler, void*) {
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

    // A forced yield makes the first interrupt mandatory, so one reaching the
    // marshalling would kill the script rather than pass unnoticed. The first
    // one that does fire lands in `handleEvent`, which is yieldable. Driven
    // by hand because force-yield can only be set inside a window.
    RunResult result;
    {
        RunWindow window(ts.exec, 1.0);
        ts.exec.setForceYield(true);
        result = ts.exec.callEventHandler(LSLEvent::Dataserver, [](lua_State* handler, void*) {
            luaSL_pushuuidstring(handler, "8d4a1e26-3f2b-4c7d-9a15-6e0b3c8f2d41");
            lua_pushstring(handler, "payload");
        });
    }
    REQUIRE(result.status == HandlerRunStatus::Preempted);
    CHECK(ts.exec.getFaultKind() == FaultKind::None);
    CHECK(ts.host.printed.empty());

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
        dispatch(exec, LSLEvent::TouchStart, HandlerRunStatus::Fault);
        CHECK(exec.getFaultKind() == FaultKind::OutOfMemory);
        CHECK(ts.host.printed.empty());
    }

    SUBCASE("handler teardown after a restore with no memory left")
    {
        // Tearing the handler thread down resets it, and that reset runs
        // between handlers with no protected frame around it. A restored
        // thread must not need an allocation to get back to the reset state.
        TestAsset asset = compileTestAsset(R"(
            held = buffer.create(1024 * 100)
            LLEvents:on("touch_start", function() print("HANDLER RAN") end)
        )");
        TestScript first(asset);
        first.start();

        std::string payload = serialize(first.exec);

        TestScript ts(asset);
        restore(ts.exec, payload);
        Script& exec = ts.exec;
        REQUIRE(exec.getUsedMemory() > 1024 * 100);

        exec.setMemoryLimitUnsafe(exec.getUsedMemory() + 128);
        dispatch(exec, LSLEvent::TouchStart, HandlerRunStatus::Fault);
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
        dispatch(exec, LSLEvent::Dataserver, HandlerRunStatus::Fault, [](lua_State* handler, void*) {
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
constexpr size_t kExpectedDonorPayloadSize = 634;

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor invalid restore")
{
    TestAsset asset = compileTestAsset(R"(
        counter = 1
    )");

    std::string payload = donorPayload(asset);
    TestScript second(asset);

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
        auto scratch = std::make_unique<TestScript>(asset);
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
                // over one trips restoreState()'s assert, so start over
                scratch = std::make_unique<TestScript>(asset);
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

        // The core section's major follows the magic, the class's own follows
        // the class tag
        std::string bad_core_version = payload;
        bad_core_version[4] = (char)(kScriptStateFingerprint.major + 99);
        CHECK_FALSE(second.exec.restoreState(bad_core_version.data(), bad_core_version.size()));

        std::string bad_class_tag = payload;
        bad_class_tag[12] = 'B';
        CHECK_FALSE(second.exec.restoreState(bad_class_tag.data(), bad_class_tag.size()));

        std::string bad_class_version = payload;
        bad_class_version[16] = (char)(kScriptStateFingerprint.major + 99);
        CHECK_FALSE(second.exec.restoreState(bad_class_version.data(), bad_class_version.size()));

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

    // Restoring (or loading the default state) over a live instance is a host
    // sequencing bug and asserts rather than being refused, so there is no
    // subcase for it here.
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor sleep and fault survive a round trip")
{
    // The payload carries the script instance plus the sleep and fault it was
    // holding, so the host stores one opaque blob and cannot drop half of it.
    TestAsset asset = compileTestAsset(R"(
        counter = 1
    )");
    TestScript first(asset);
    first.start();

    first.exec.setSleep(0.25f);
    first.exec.setFault(FaultKind::Runtime, "runtime error", "boom\nstacktrace");

    std::string payload = serialize(first.exec);

    TestScript second(asset);
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
    const char* source = R"(
        counter = 1
    )";
    TestAsset asset = compileTestAsset(source);

    SUBCASE("a state from another flavor is refused")
    {
        // Caught in the envelope rather than somewhere inside Ares
        std::string payload = donorPayload(asset);

        TestScript other(compileTestAsset(source, false, 1));
        CHECK_FALSE(other.exec.restoreState(payload.data(), payload.size()));
        CHECK(other.exec.getFaultKind() == FaultKind::Runtime);
        CHECK_FALSE(other.exec.hasInstance());
    }

    SUBCASE("the persisted traceback is capped")
    {
        TestScript first(asset);
        first.start();
        std::string huge_trace(kMaxPersistedFaultLen * 4, 'x');
        first.exec.setFault(FaultKind::Runtime, "runtime error", huge_trace.c_str());

        std::string payload = serialize(first.exec);

        TestScript second(asset);
        restore(second.exec, payload);
        CHECK(second.exec.getExtendedFaultString().size() == kMaxPersistedFaultLen);
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor memory limit survives a round trip")
{
    TestAsset asset = compileTestAsset(R"(
        counter = 1
    )");
    TestScript first(asset);
    first.start();

    // The limit drifts at runtime (llSetMemoryLimit), so the payload must
    // carry it: a restored script continuing with the instantiate-time
    // default would silently regain headroom the host took away.
    constexpr int kLoweredLimit = 96 * 1024;
    REQUIRE(first.exec.setMemoryLimit(kLoweredLimit));

    std::string payload = serialize(first.exec);

    TestScript second(asset);
    REQUIRE(second.exec.getMemoryLimit() == kDefaultMemoryLimit);
    restore(second.exec, payload);
    CHECK(second.exec.getMemoryLimit() == kLoweredLimit);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor thread capacities survive a round trip")
{
    // Make sure we don't muck up CallInfo arrays with our serialization.
    TestAsset asset = compileTestAsset(R"(
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
        TestScript first(asset);
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

        TestScript second(asset);
        restore(second.exec, payload);

        lua_State* restored = handlerThread(second.exec);
        CHECK(restored->size_ci == size_ci);
        CHECK(restored->stacksize == stacksize);
    }

    SUBCASE("serialized while preempted mid-handler")
    {
        TestScript first(asset);
        first.start();

        dispatch(first.exec, LSLEvent::MovingStart, HandlerRunStatus::Preempted);

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

        TestScript second(asset);
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
    StateFingerprint getStateFingerprint() const override { return {{'S', 'T', 'F', 'L'}, 1, 0}; }

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

// The same host class one release later: one field appended to its section,
// minor bumped. Payloads cross between the two in both directions.
class NewerStatefulScript : public StatefulScript
{
public:
    using StatefulScript::StatefulScript;

    int32_t newer = 0;

protected:
    StateFingerprint getStateFingerprint() const override { return {{'S', 'T', 'F', 'L'}, 1, 1}; }

    bool serializeExtra(ByteWriter& writer) const override
    {
        StatefulScript::serializeExtra(writer);
        writer.writeS32(newer);
        return true;
    }

    bool restoreExtra(ByteReader& reader) override
    {
        if (!StatefulScript::restoreExtra(reader))
            return false;
        // Appended after 1.0, so a 1.0 payload simply doesn't carry it
        if (reader.remaining >= sizeof(int32_t))
            return reader.readS32(newer);
        newer = -1;
        return true;
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
        , raw_asset_size(config.assetSize)
    {
    }

    // Image only retains the charged size, not what was actually handed to it
    size_t raw_asset_size = 0;
};

// Replaces every tier. TestProvisioner's callbacks recover it by downcasting
// getProvisioner() to itself, so a provisioner of other types needs its own.
template<class S>
struct StatefulProvisionerT : Provisioner<S>, FakeQuantaClock
{
    int environments_made = 0;
    int images_made = 0;

    StatefulProvisionerT()
        : Provisioner<S>(makeCallbacks())
    {
    }

    static HostCallbacks makeCallbacks()
    {
        HostCallbacks callbacks;
        callbacks.quantaClockProvider = FakeQuantaClock::read;
        callbacks.interruptInstallPolicy = InterruptInstallPolicy::Resident;
        return callbacks;
    }

    // The only place the downcast happens, since everything this provisioner
    // mints comes through makeScript()
    std::shared_ptr<S> provision(const TestAsset& asset)
    {
        return std::static_pointer_cast<S>(this->provisionScript(asset, makeScriptConfig()));
    }

    std::shared_ptr<S> start(const TestAsset& asset)
    {
        std::shared_ptr<S> script = provision(asset);
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

using StatefulProvisioner = StatefulProvisionerT<StatefulScript>;

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor subclass state survives a round trip")
{
    // Long enough to take a loop back-edge interrupt, short enough to finish
    // inside one window
    TestAsset asset = compileTestAsset(R"(
        local total = 0
        for i = 1, 10000 do
            total += i
        end
        counter = 1
    )");

    StatefulProvisioner host;
    std::shared_ptr<StatefulScript> first = host.start(asset);

    // Every tier is recoverable as the host's own type without the engine
    // knowing any of them exist
    CHECK(host.environments_made == 1);
    CHECK(host.images_made == 1);
    CHECK(static_cast<StatefulEnvironment&>(first->getEnvironment()).flavor == "lua");
    CHECK(static_cast<StatefulImage&>(first->getImage()).raw_asset_size > asset.bytecodeSize());
    // Chaining to ours left the run window intact, so the script still finished
    CHECK(first->interrupts > 0);

    first->counter = 42;
    first->label = "held";

    std::string payload = serialize(*first);

    // The subclass's fields ride in the same payload as the instance, so the
    // host cannot store one without the other
    std::shared_ptr<StatefulScript> second = host.provision(asset);
    REQUIRE(second != nullptr);
    REQUIRE(second->counter == 0);

    restore(*second, payload);
    CHECK(second->counter == 42);
    CHECK(second->label == "held");
    CHECK(readIntGlobal(*second, "counter") == 1);
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor refuses a payload from another script class")
{
    TestAsset asset = compileTestAsset(R"(
        counter = 1
    )");

    StatefulProvisioner host;
    std::string stateful_payload = serialize(*host.start(asset));
    std::string plain_payload = donorPayload(asset);

    // The fingerprint at the head of the payload says whose it is, so neither
    // class gets far enough to fork an instance out of the other's state
    TestScript plain(asset);
    CHECK_FALSE(plain.exec.restoreState(stateful_payload.data(), stateful_payload.size()));
    CHECK(plain.exec.getFaultKind() == FaultKind::Runtime);
    CHECK_FALSE(plain.exec.hasInstance());

    std::shared_ptr<StatefulScript> stateful = host.provision(asset);
    REQUIRE(stateful != nullptr);
    CHECK_FALSE(stateful->restoreState(plain_payload.data(), plain_payload.size()));
    CHECK(stateful->getFaultKind() == FaultKind::Runtime);
    CHECK_FALSE(stateful->hasInstance());
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor subclass payloads cross minor versions")
{
    TestAsset asset = compileTestAsset(R"(
        counter = 1
    )");

    StatefulProvisioner older_host;
    StatefulProvisionerT<NewerStatefulScript> newer_host;

    SUBCASE("an older build skips the field a newer one appended")
    {
        std::shared_ptr<NewerStatefulScript> newer = newer_host.start(asset);
        newer->counter = 42;
        newer->label = "held";
        newer->newer = 7;
        std::string payload = serialize(*newer);

        std::shared_ptr<StatefulScript> older = older_host.provision(asset);
        REQUIRE(older != nullptr);
        restore(*older, payload);
        CHECK(older->counter == 42);
        CHECK(older->label == "held");
        CHECK(readIntGlobal(*older, "counter") == 1);

        // ...and what it writes back is a 1.0 payload the newer build still loads
        std::string rewritten = serialize(*older);
        std::shared_ptr<NewerStatefulScript> again = newer_host.provision(asset);
        REQUIRE(again != nullptr);
        restore(*again, rewritten);
        CHECK(again->counter == 42);
        CHECK(again->newer == -1);
    }

    SUBCASE("a newer build defaults the field an older one never wrote")
    {
        std::shared_ptr<StatefulScript> older = older_host.start(asset);
        older->counter = 5;
        std::string payload = serialize(*older);

        std::shared_ptr<NewerStatefulScript> newer = newer_host.provision(asset);
        REQUIRE(newer != nullptr);
        restore(*newer, payload);
        CHECK(newer->counter == 5);
        CHECK(newer->newer == -1);
        CHECK(readIntGlobal(*newer, "counter") == 1);
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor ares records with unknown trailing bytes restore")
{
    TestAsset asset = compileTestAsset(R"(
        counter = 1
        names = {"a", "b", c = {nested = true}}
        local captured = 10
        function bump()
            captured += 1
            return captured
        end
        function LLEvents.timer()
            counter += bump()
        end
    )");

    TestScript first(asset);
    first.start();

    // The writer pads every ares record with bytes this reader has never seen,
    // as a newer engine appending fields would
    lua_State* instance = first.exec.getInstanceState();
    lua_pushunsigned(instance, 5);
    eris_set_setting(instance, "testpad", -1);
    lua_pop(instance, 1);

    std::string payload = serialize(first.exec);

    TestScript second(asset);
    restore(second.exec, payload);
    CHECK(second.exec.getFaultKind() == FaultKind::None);
    CHECK(readIntGlobal(second.exec, "counter") == 1);

    dispatch(second.exec, LSLEvent::Timer);
    CHECK(readIntGlobal(second.exec, "counter") == 12);

    std::string again;
    CHECK(second.exec.serializeState(again));
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor main function completion")
{
    TestAsset asset = compileTestAsset(R"(
        local total = 0
        for i = 1, 100000 do
            total += i
        end
        done = total
    )");

    TestScript first(asset);
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
    TestScript second(asset);
    restore(second.exec, payload);
    CHECK_FALSE(second.exec.isMainFunctionComplete());

    // The completion is reported on the transition, and only once
    RunResult completed = resumeToCompletion(second.exec, 1.0);
    CHECK(completed.mainFunctionCompleted);
    CHECK(second.exec.isMainFunctionComplete());

    // A payload from a completed main restores with the flag already set
    std::string done_payload = serialize(second.exec);
    TestScript third(asset);
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
    TestAsset asset = compileTestAsset(R"(
        pcall(host_kill)
        print("still alive")
    )");
    KillingTestHost host;
    std::shared_ptr<Script> script = host.provisionScript(asset, makeScriptConfig());
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

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor faulting LSL constructor")
{
    // Compiled LSL should never error during construction, but the engine
    // cannot assume the bytecode is well-behaved. The constructor is staged
    // like any main function, so its death is an ordinary handler fault: the
    // instance stays, faulted, and main reports complete so the host stops
    // waiting on it.
    TestAsset asset = compileTestAsset(R"(
        error("constructor boom")
    )");
    TestScript ts(asset);
    ts.loadDefaultState();
    CHECK_FALSE(ts.exec.isMainFunctionComplete());

    resume(ts.exec, 1.0, HandlerRunStatus::Fault);
    CHECK(ts.exec.getFaultKind() == FaultKind::Runtime);
    CHECK(ts.exec.hasInstance());
    CHECK(ts.exec.isMainFunctionComplete());
    CHECK_FALSE(ts.exec.isHandlerActive());
}

TEST_CASE("BytecodeHeader round trip")
{
    BytecodeHeader header;
    header.isLSL = true;
    header.apiVersion = 3;
    header.stateHandlerMasks = {0x5, 0x8000000000000001ull};
    header.chargedBytecodeSize = 4096;

    std::string asset;
    writeBytecodeHeader(asset, header);
    const std::string bytecode = "not really bytecode";
    asset += bytecode;

    BytecodeHeader parsed;
    size_t bytecode_start = 0;
    REQUIRE(readBytecodeHeader(asset.data(), asset.size(), parsed, bytecode_start));
    CHECK(parsed.isLSL);
    CHECK(parsed.apiVersion == 3);
    CHECK(parsed.stateHandlerMasks == header.stateHandlerMasks);
    CHECK(parsed.chargedBytecodeSize == 4096);
    CHECK(asset.substr(bytecode_start) == bytecode);

    SUBCASE("SLua carries no masks")
    {
        BytecodeHeader slua;
        std::string slua_asset;
        writeBytecodeHeader(slua_asset, slua);
        BytecodeHeader parsed_slua;
        REQUIRE(readBytecodeHeader(slua_asset.data(), slua_asset.size(), parsed_slua, bytecode_start));
        CHECK_FALSE(parsed_slua.isLSL);
        CHECK(parsed_slua.stateHandlerMasks.empty());
        CHECK(bytecode_start == slua_asset.size());
    }

    SUBCASE("bytes a newer writer appends are skipped")
    {
        // Grow the section by seven junk bytes, the way a newer minor would
        std::string padded;
        writeBytecodeHeader(padded, header);
        size_t section_end = padded.size();
        padded += "\xA5\xA5\xA5\xA5\xA5\xA5\xA5";
        padded[12] = (char)(uint8_t)(section_end - 16 + 7);
        padded += bytecode;
        BytecodeHeader parsed_padded;
        REQUIRE(readBytecodeHeader(padded.data(), padded.size(), parsed_padded, bytecode_start));
        CHECK(parsed_padded.stateHandlerMasks == header.stateHandlerMasks);
        CHECK(padded.substr(bytecode_start) == bytecode);
    }

    SUBCASE("malformed headers are refused")
    {
        // Layout: tag at 0, major at 4, minor at 8, section length at 12,
        // is_lsl at 16, api_version at 17, num_states at 21
        std::string bad_tag = asset;
        bad_tag[0] = 'B';
        CHECK_FALSE(readBytecodeHeader(bad_tag.data(), bad_tag.size(), parsed, bytecode_start));

        std::string bad_major = asset;
        bad_major[4] = (char)(kBytecodeHeaderFingerprint.major + 1);
        CHECK_FALSE(readBytecodeHeader(bad_major.data(), bad_major.size(), parsed, bytecode_start));

        std::string long_section = asset;
        long_section[15] = (char)0x7F;
        CHECK_FALSE(readBytecodeHeader(long_section.data(), long_section.size(), parsed, bytecode_start));

        std::string many_states = asset;
        many_states[24] = (char)0x7F;
        CHECK_FALSE(readBytecodeHeader(many_states.data(), many_states.size(), parsed, bytecode_start));

        for (size_t len : {size_t(0), size_t(4), size_t(12), size_t(16), size_t(25), size_t(40)})
            CHECK_FALSE(readBytecodeHeader(asset.data(), len, parsed, bytecode_start));
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor image reads its flavor off the asset")
{
    std::string bytecode = Luau::compile(R"(
        counter = 1
    )");
    TestProvisioner host;

    SUBCASE("an unreadable header is a bad image, not a crash")
    {
        std::shared_ptr<IEnvironment> env = host.createEnvironment(false, 0);

        TestAsset asset = makeAsset(bytecode);

        // Anything past the header is a bytecode question, not a header one, so
        // the parse itself says where to stop cutting
        BytecodeHeader parsed;
        size_t header_len = 0;
        REQUIRE(readBytecodeHeader(asset.bytes.data(), asset.bytes.size(), parsed, header_len));

        for (size_t len = 0; len < header_len; ++len)
        {
            CAPTURE(len);
            ImageConfig config = asset;
            config.assetSize = len;

            std::shared_ptr<IImage> image = host.buildImage(env, config);
            REQUIRE(image != nullptr);
            CHECK_FALSE(image->isValid());
        }

        // And provisionScript() refuses outright, since it can't even pick an
        // environment without the header
        ImageConfig truncated = asset;
        truncated.assetSize = 8;
        CHECK(host.provisionScript(truncated, makeScriptConfig()) == nullptr);
    }

    SUBCASE("a zero charged size charges the bytecode, not the asset")
    {
        std::shared_ptr<IImage> honest = host.buildImage(host.createEnvironment(false, 0), makeAsset(bytecode));
        REQUIRE(honest != nullptr);
        REQUIRE(honest->isValid());
        // Header growth must never move this number
        CHECK(honest->getChargedBytecodeSize() == bytecode.size());

        std::shared_ptr<IImage> declared = host.buildImage(host.createEnvironment(false, 0), makeAsset(bytecode, 4096));
        REQUIRE(declared != nullptr);
        REQUIRE(declared->isValid());
        CHECK(declared->getChargedBytecodeSize() == 4096);
    }
}

#ifdef LUAU_USE_TAILSLIDE

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor LSL handler masks reach the image")
{
    TestAsset asset = compileTestAsset(R"(
        default {
            state_entry() {
                print("default");
            }
        }
        state busy {
            state_entry() {
                print("busy");
            }
            touch_start(integer num) {
                print("touched");
            }
        }
        state idle {
            timer() {
                print("tick");
            }
        }
    )", true);

    TestScript ts(asset);

    // One mask per state, in state order, so a host can decide what to
    // dispatch without standing an instance up first. Bit (index - 1), index
    // being the event's position in builtins.txt: state_entry 1, touch_start 3,
    // timer 12.
    const std::vector<uint64_t>& masks = ts.exec.getImage().getStateHandlerMasks();
    REQUIRE(masks.size() == 3);
    CHECK(masks[0] == 0x1);
    CHECK(masks[1] == 0x5);
    CHECK(masks[2] == 0x800);
}

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

    // The constructor is staged like the SLua main, not run at load, and
    // nothing can be dispatched over it. Unlike SLua's main it is not the
    // state_entry handler, so that one is still pending behind it.
    CHECK(exec.isHandlerActive());
    CHECK_FALSE(exec.isMainFunctionComplete());
    CHECK(exec.getCurrentHandler() == LSLEventBit::StateEntry);
    CHECK(exec.getCurrentEvents() == LSLEventBit::StateEntry);
    // The asset says what the state handles, so there is nothing to wait for
    CHECK(exec.eventHandlersKnown());
    CHECK(exec.getEventHandlers() == LSLEventBit::StateEntry);
    dispatch(exec, LSLEvent::StateEntry, HandlerRunStatus::Refused);
    RunResult constructed = resume(exec);
    CHECK(constructed.mainFunctionCompleted);
    CHECK(exec.isMainFunctionComplete());
    CHECK_FALSE(exec.isHandlerActive());
    CHECK(exec.getCurrentHandler() == 0);
    CHECK(exec.getCurrentEvents() == LSLEventBit::StateEntry);

    // An event with no handler behind it reports that nothing ran, rather than
    // looking like a handler that completed
    dispatch(exec, LSLEvent::TouchStart, HandlerRunStatus::NotRun);
    CHECK(exec.getCurrentHandler() == 0);
    CHECK(exec.getStickyHandler() == LSLEventBit::TouchStart);

    dispatch(exec, LSLEvent::StateEntry);
    CHECK(exec.getCurrentEvents() == 0);

    // Globals persist between handler invocations of the same instance
    dispatch(exec, LSLEvent::StateEntry);

    // A reset re-forks a pristine instance and stages the constructor again,
    // with the registers back at their defaults
    ts.reset();
    CHECK(exec.getCurrentHandler() == LSLEventBit::StateEntry);
    CHECK(exec.getStickyHandler() == LSLEventBit::StateEntry);
    CHECK(exec.getCurrentEvents() == LSLEventBit::StateEntry);
    resume(exec);
    dispatch(exec, LSLEvent::StateEntry);

    checkCapture(ts.host.printed, {"six", "other", "six"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor LSL state change")
{
    TestScript ts(R"(
        default {
            state_entry() {
                state other;
            }
            state_exit() {
                print("leaving");
            }
        }
        state other {
            state_entry() {
                print("in_other");
            }
            timer() {
            }
        }
    )", true);
    ts.start();
    Script& exec = ts.exec;

    RunResult result = dispatch(exec, LSLEvent::StateEntry, HandlerRunStatus::StateChange);
    REQUIRE(result.newState == 1);
    CHECK_FALSE(exec.isHandlerActive());
    CHECK(exec.getCurrentHandler() == 0);

    // The change is recorded but not committed, and the departing state's
    // state_exit is pending because it has one
    CHECK(exec.isStateChangePending());
    CHECK(exec.getCurrentState() == 0);
    CHECK(exec.getNextState() == 1);
    CHECK(exec.getCurrentEvents() == LSLEventBit::StateExit);
    CHECK(exec.getEventHandlers() == (LSLEventBit::StateEntry | LSLEventBit::StateExit));

    dispatch(exec, LSLEvent::StateExit);
    CHECK(exec.getCurrentEvents() == 0);

    // Committing swaps in the new state's handlers with its state_entry pending
    CHECK(exec.nextState() == (LSLEventBit::StateEntry | LSLEventBit::Timer));
    CHECK_FALSE(exec.isStateChangePending());
    CHECK(exec.getCurrentState() == 1);
    CHECK(exec.getCurrentEvents() == LSLEventBit::StateEntry);
    CHECK(exec.getEventHandlers() == (LSLEventBit::StateEntry | LSLEventBit::Timer));

    dispatch(exec, LSLEvent::StateEntry);
    CHECK(exec.getCurrentEvents() == 0);
    checkCapture(ts.host.printed, {"leaving", "in_other"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor LSL state change with no state_exit")
{
    TestScript ts(R"(
        default {
            state_entry() {
                state other;
            }
        }
        state other {
            timer() {
                print("in_other");
            }
        }
    )", true);
    ts.start();
    Script& exec = ts.exec;

    // Nothing pends for a handler the departing state lacks
    dispatch(exec, LSLEvent::StateEntry, HandlerRunStatus::StateChange);
    CHECK(exec.isStateChangePending());
    CHECK(exec.getCurrentEvents() == 0);

    // Nor does state_entry pend for a state that doesn't handle it
    CHECK(exec.nextState() == LSLEventBit::Timer);
    CHECK(exec.getCurrentEvents() == 0);
    dispatch(exec, LSLEvent::Timer);
    checkCapture(ts.host.printed, {"in_other"});
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor state change survives serialization")
{
    TestAsset asset = compileTestAsset(R"(
        default {
            state_entry() {
                state other;
            }
            state_exit() {
                print("leaving");
            }
        }
        state other {
            state_entry() {
                print("in_other");
            }
        }
    )", true);

    TestScript first(asset);
    first.start();
    dispatch(first.exec, LSLEvent::StateEntry, HandlerRunStatus::StateChange);
    std::string payload = serialize(first.exec);

    SUBCASE("the uncommitted change restores intact")
    {
        TestScript second(asset);
        restore(second.exec, payload);
        Script& exec = second.exec;
        CHECK(exec.isStateChangePending());
        CHECK(exec.getCurrentState() == 0);
        CHECK(exec.getNextState() == 1);
        CHECK(exec.getCurrentEvents() == LSLEventBit::StateExit);

        dispatch(exec, LSLEvent::StateExit);
        exec.nextState();
        dispatch(exec, LSLEvent::StateEntry);
        checkCapture(second.host.printed, {"leaving", "in_other"});
    }

    SUBCASE("a state the asset lacks is refused")
    {
        // Bytecode can be swapped behind a script's back; a payload heading
        // into a state the replacement doesn't have is caught before the fork
        TestScript fewer_states(compileTestAsset(R"(
            default {
                state_entry() {
                    print("only state");
                }
            }
        )", true));
        CHECK_FALSE(fewer_states.exec.restoreState(payload.data(), payload.size()));
        CHECK(fewer_states.exec.getFaultKind() != FaultKind::None);
        CHECK_FALSE(fewer_states.exec.hasInstance());
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor serialize mid-handler roundtrip")
{
    TestAsset asset = compileTestAsset(R"(
        default {
            state_entry() {
                integer i = 0;
                while (i < 200) {
                    i = i + 1;
                }
                print("done");
            }
        }
    )", true);

    TestScript first(asset);
    first.start();

    // Preempt the handler mid-loop
    first.host.clock_step = 0.001;
    dispatch(first.exec, LSLEvent::StateEntry, HandlerRunStatus::Preempted, nullptr, nullptr, 0.005);
    REQUIRE(first.exec.isHandlerActive());
    CHECK(first.host.printed.empty());

    std::string payload = serialize(first.exec);
    CHECK(!payload.empty());

    // Rehydrate the suspended handler in a second engine built from the same
    // bytecode and run it to completion there
    TestScript second(asset);
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
