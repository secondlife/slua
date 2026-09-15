// Tests that ensure we don't bungle handling of old script states / bytecode.
// We keep around fixtures with a set of example scripts and their associated
// states.
//
// Each scenario drives a fresh script into some resting state and says what a
// script restored from that state must still be able to do. With
// LUAU_REGENERATE_FIXTURES set, the regenerate case writes every scenario's
// asset and state under the current format versions. The load case restores
// every committed state this build claims to read and runs its scenario's
// verify against it, so a state written by an older build has to keep working.
#include "SLExecutorFixture.h"

#include "Luau/FileUtils.h"
#include "Luau/ParseResult.h"

#include "doctest.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace Luau;
using namespace Luau::Executor;

namespace
{
struct GoldenScenario
{
    // The source file beside the fixtures, whose extension is the flavor
    const char* name;
    // Drives a fresh script into the state the fixture captures
    void (*arrange)(TestScript&);
    // What a payload restored from that fixture has to still be able to do
    void (*verify)(TestScript&);
};
}

static const GoldenScenario kGoldenScenarios[] = {
    {"between-handlers.lua",
        [](TestScript& ts)
        {
            ts.start();
        },
        [](TestScript& ts)
        {
            dispatch(ts.exec, LSLEvent::MovingStart);
            checkCapture(ts.host.printed, {"counter ok", "table ok", "buffer ok", "vector ok", "coroutine ok", "upvalue ok"});
        }},
    {"yielded-main.lua",
        [](TestScript& ts)
        {
            ts.loadDefaultState();
            ts.host.clock_step = 0.001;
            resume(ts.exec, 0.005, HandlerRunStatus::Preempted);
        },
        [](TestScript& ts)
        {
            // Main was suspended mid-loop, so it has to finish before anything
            // can be dispatched over it
            REQUIRE(ts.exec.isHandlerActive());
            resumeToCompletion(ts.exec, 1.0);
            CHECK(ts.exec.isMainFunctionComplete());
            dispatch(ts.exec, LSLEvent::MovingStart);
            checkCapture(ts.host.printed, {"done ok"});
        }},
    {"yielded-handler.lua",
        [](TestScript& ts)
        {
            ts.start();
            dispatch(ts.exec, LSLEvent::Timer, HandlerRunStatus::Preempted);
        },
        [](TestScript& ts)
        {
            REQUIRE(ts.exec.isHandlerActive());
            resumeToCompletion(ts.exec, 1.0);
            dispatch(ts.exec, LSLEvent::MovingStart);
            checkCapture(ts.host.printed, {"counter ok"});
        }},
    {"errored-handler.lua",
        [](TestScript& ts)
        {
            ts.start();
            dispatch(ts.exec, LSLEvent::Timer, HandlerRunStatus::Fault);
        },
        [](TestScript& ts)
        {
            // A faulted script can't report on itself, so this one is checked
            // from the outside, then reset to show the image behind it is fine
            CHECK(ts.exec.getFaultKind() == FaultKind::Runtime);
            CHECK_FALSE(ts.exec.getFaultString().empty());

            ts.reset();
            resume(ts.exec);
            dispatch(ts.exec, LSLEvent::MovingStart);
            checkCapture(ts.host.printed, {"ran after reset"});
        }},
#ifdef LUAU_USE_TAILSLIDE
    {"between-handlers.lsl",
        [](TestScript& ts)
        {
            ts.start();
            dispatch(ts.exec, LSLEvent::StateEntry);
        },
        [](TestScript& ts)
        {
            CHECK(ts.exec.getCurrentState() == 0);
            CHECK(ts.exec.getCurrentEvents() == 0);
            dispatch(ts.exec, LSLEvent::MovingStart);
            checkCapture(ts.host.printed, {"counter ok"});
        }},
    {"state-change-pending.lsl",
        [](TestScript& ts)
        {
            ts.start();
            dispatch(ts.exec, LSLEvent::StateEntry, HandlerRunStatus::StateChange);
        },
        [](TestScript& ts)
        {
            // Captured between the `state` statement and the host committing
            // it, so the departing state's state_exit is still owed
            Script& exec = ts.exec;
            REQUIRE(exec.isStateChangePending());
            CHECK(exec.getCurrentEvents() == LSLEventBit::StateExit);
            dispatch(exec, LSLEvent::StateExit);
            exec.nextState();
            dispatch(exec, LSLEvent::StateEntry);
            dispatch(exec, LSLEvent::MovingStart);
            checkCapture(ts.host.printed, {"counter ok"});
        }},
    {"yielded-handler.lsl",
        [](TestScript& ts)
        {
            ts.start();
            ts.host.clock_step = 0.001;
            dispatch(ts.exec, LSLEvent::StateEntry, HandlerRunStatus::Preempted, nullptr, nullptr, 0.005);
        },
        [](TestScript& ts)
        {
            REQUIRE(ts.exec.isHandlerActive());
            resumeToCompletion(ts.exec, 1.0);
            dispatch(ts.exec, LSLEvent::MovingStart);
            checkCapture(ts.host.printed, {"done ok"});
        }},
    {"errored-main.lsl",
        [](TestScript& ts)
        {
            ts.loadDefaultState();
            resume(ts.exec, 1.0, HandlerRunStatus::Fault);
        },
        [](TestScript& ts)
        {
            CHECK(ts.exec.getFaultKind() == FaultKind::OutOfMemory);
            CHECK_FALSE(ts.exec.getFaultString().empty());
        }},
#endif
};

static std::string goldenFixtureDir()
{
#ifdef LUAU_CONFORMANCE_SOURCE_DIR
    std::string dir = LUAU_CONFORMANCE_SOURCE_DIR;
#else
    std::string dir = __FILE__;
    dir.erase(dir.find_last_of("\\/"));
    dir += "/conformance";
#endif
    return dir + "/fixtures/exec";
}

static std::string goldenFixtureBase(const GoldenScenario& scenario)
{
    return "exec" + std::to_string(kScriptStateFingerprint.major) + "." + std::to_string(kScriptStateFingerprint.minor) + "-ares" +
           std::to_string(ARES_FORMAT_MAJOR) + "." + std::to_string(ARES_FORMAT_MINOR) + "-" + scenario.name;
}

static std::string readBinaryFile(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream);
    return std::string(std::istreambuf_iterator<char>(stream), {});
}

static void writeBinaryFile(const std::string& path, const std::string& data)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    REQUIRE(stream);
    stream.write(data.data(), data.size());
    REQUIRE(stream);
}

// The file name without its directory or final extension
static std::string fileStem(const std::string& path)
{
    size_t start = path.find_last_of("/\\");
    start = start == std::string::npos ? 0 : start + 1;
    size_t dot = path.rfind('.');
    if (dot == std::string::npos || dot < start)
        dot = path.size();
    return path.substr(start, dot - start);
}

// The scenario a fixture belongs to, from the name its stem ends with
static const GoldenScenario* findGoldenScenario(const std::string& stem)
{
    size_t ares = stem.find("-ares");
    if (ares == std::string::npos)
        return nullptr;

    size_t name_start = stem.find('-', ares + 1);
    if (name_start == std::string::npos)
        return nullptr;

    std::string name = stem.substr(name_start + 1);
    for (const GoldenScenario& scenario : kGoldenScenarios)
    {
        if (name == scenario.name)
            return &scenario;
    }
    return nullptr;
}

// The extension a scenario's name ends with is its flavor
static bool isLSLScenario(std::string_view name)
{
    return name.size() > 4 && name.substr(name.size() - 4) == ".lsl";
}

// Compiling is the one place a bad source throws, and doctest is built without
// exceptions (tests/main.cpp), so letting that escape would abort the run
// instead of saying which file is wrong.
static bool compileGoldenScenario(const std::string& dir, const GoldenScenario& scenario, TestAsset& asset)
{
    const std::string source = dir + "/" + scenario.name;
    const std::string text = readBinaryFile(source);
    try
    {
        asset = compileTestAsset(text.c_str(), isLSLScenario(scenario.name));
        return true;
    }
    catch (const ParseErrors& e)
    {
        std::string message = source;
        for (const ParseError& error : e.getErrors())
            message += "\n  " + std::to_string(error.getLocation().begin.line + 1) + ": " + error.what();
        FAIL_CHECK(message);
    }
    catch (const CompileError& e)
    {
        std::string message = source;
        message += "\n  " + std::to_string(e.getLocation().begin.line + 1) + ": " + e.what();
        FAIL_CHECK(message);
    }
    return false;
}

// Drives a fresh script into the scenario's resting state and serializes it
static void writeGoldenFixture(const std::string& dir, const GoldenScenario& scenario)
{
    TestAsset asset;
    if (!compileGoldenScenario(dir, scenario, asset))
        return;

    TestScript ts(asset);
    scenario.arrange(ts);

    std::string base = dir + "/" + goldenFixtureBase(scenario);
    writeBinaryFile(base + ".sluac", asset.bytes);
    writeBinaryFile(base + ".state", serialize(ts.exec));
}

TEST_SUITE_BEGIN("SLExecutor");

// Go through all the scenarios and serialize a version of each one using the
//  current serialization versions. Should be done with each version bump!
//  Separate from the loading case below so a bad scenario names itself, and so
//  the subcases there don't re-run the whole regeneration once each.
TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor golden fixtures regenerate")
{
    if (std::getenv("LUAU_REGENERATE_FIXTURES") == nullptr)
        return;

    const std::string dir = goldenFixtureDir();
    // The fixtures live in the source tree, so there is nothing to create
    REQUIRE(isDirectory(dir));

    for (const GoldenScenario& scenario : kGoldenScenarios)
    {
        SUBCASE(scenario.name)
        {
            writeGoldenFixture(dir, scenario);
        }
    }
}

TEST_CASE_FIXTURE(SLuaFixture, "SLExecutor golden fixtures")
{
    const std::string dir = goldenFixtureDir();

    REQUIRE(isDirectory(dir));

    // Verify we _do_ have a dump on the current version for each scenario.
    for (const GoldenScenario& scenario : kGoldenScenarios)
    {
        const std::string base = dir + "/" + goldenFixtureBase(scenario);
        CAPTURE(base);
        REQUIRE(isFile(base + ".state"));
        REQUIRE(isFile(base + ".sluac"));
    }

    // Every state file this build is expected to load. Collected before the
    // subcases so the skips don't each cost a run of the body.
    std::vector<std::string> loadable;
    auto collect = [&](const std::string& path)
    {
        if (!hasFileExtension(path, {".state"}))
            return;

        const std::string name = fileStem(path);

#ifndef LUAU_USE_TAILSLIDE
        // No LSL compiler in this build, so those scenarios can't run
        const GoldenScenario* scenario = findGoldenScenario(name);
        if (scenario != nullptr && isLSLScenario(scenario->name))
            return;
#endif

        loadable.push_back(path);
    };
    REQUIRE(traverseDirectory(dir, collect));
    REQUIRE_FALSE(loadable.empty());

    // A subcase each, so a scenario that breaks says which one it was rather
    // than taking the rest of them down with it.
    for (const std::string& state_path : loadable)
    {
        const std::string name = fileStem(state_path);
        SUBCASE(name.c_str())
        {
            const std::string asset_path = state_path.substr(0, state_path.size() - 6) + ".sluac";
            REQUIRE(isFile(asset_path));

            // The committed asset says what flavor it is, so the filename
            // doesn't have to, and it goes in whole the way a host would hand
            // it over
            TestAsset asset{readBinaryFile(asset_path)};

            // Get the scenario associated with this state file, and there
            // _better_ be one.
            const GoldenScenario* scenario = findGoldenScenario(name);
            REQUIRE(scenario != nullptr);

            TestScript ts(asset);
            // Start the script and restore the state from the statefile
            restore(ts.exec, readBinaryFile(state_path));
            // Make sure we can serialize it again immediately after we load
            std::string again;
            CHECK(ts.exec.serializeState(again));
            // Check that it actually did the thing we expected once it
            // reloaded and started running.
            scenario->verify(ts);

            // Just as a sanity check, ensure we can serialize it again at the end.
            again = "";
            CHECK(ts.exec.serializeState(again));
        }
    }
}

TEST_SUITE_END();
