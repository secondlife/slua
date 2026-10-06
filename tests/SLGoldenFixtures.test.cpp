// Tests that ensure we don't bungle handling of old script states / bytecode.
// We keep around fixtures with a set of example scripts and their associated
// states.
//
// Each scenario drives a fresh script into some resting state and says what a
// script restored from that state must still be able to do. With
// LUAU_REGENERATE_FIXTURES set, the regenerate case writes each scenario's
// asset and state, named for the current format versions, into the
// scenario's directory. The load case restores
// every committed state this build claims to read and runs its scenario's
// verify against it, so a state written by an older build has to keep working.
//
// TODO: the other direction, an older reader loading states this build wrote,
// can only be checked by running the previous release's test binary against
// this tree's fixtures. That needs a runtime override for the fixture dir,
// unknown scenarios skipped rather than failed, and a way to mark a fixture as
// expected to be refused (a block written BLOCK_REQUIRED).
// Perhaps the mere presence of an unknown BLOCK_REQUIRED block would be enough,
// given that we can parse those out ourselves.
#include "SLExecutorFixture.h"

#include "Luau/FileUtils.h"
#include "Luau/ParseResult.h"

#include "lstate.h"

#include "doctest.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace Luau;
using namespace Luau::Executor;

namespace
{
struct GoldenScenario
{
    // The scenario's directory under the fixture dir, an `-lsl` suffix makes us use LSL mode.
    const char* name;
    // Drives a fresh script into the state the fixture captures
    void (*arrange)(TestScript&);
    // What a payload restored from that fixture has to still be able to do
    void (*verify)(TestScript&);
};
}

// The phase of every region of every suspended yieldable C frame on a thread,
// outermost frame and root region first
using YieldShape = std::vector<std::vector<uint8_t>>;

static YieldShape readYieldShape(lua_State* thread)
{
    YieldShape shape;
    for (CallInfo* ci = thread->base_ci + 1; ci <= thread->ci; ++ci)
    {
        if (!ttisfunction(ci->func) || !clvalue(ci->func)->isC || ci->base >= thread->top)
            continue;
        if (!ttisuserdata(ci->base) || uvalue(ci->base)->tag != UTAG_YIELD_STATE)
            continue;

        Udata* u = uvalue(ci->base);
        const uint8_t* bytes = (const uint8_t*)u->data;
        size_t size = u->len;
        std::vector<uint8_t> phases;
        // Past the version byte, each region is a u16 length, its slots with
        // the phase first, and the innermost flag
        size_t off = sizeof(uint8_t);
        for (;;)
        {
            REQUIRE(off + sizeof(uint16_t) <= size);
            uint16_t length;
            memcpy(&length, bytes + off, sizeof(uint16_t));
            off += sizeof(uint16_t);
            REQUIRE(length >= 2);
            REQUIRE(off + length <= size);
            phases.push_back(bytes[off]);
            if (bytes[off + length - 1])
                break;
            off += length;
        }
        shape.push_back(std::move(phases));
    }
    return shape;
}

static std::string formatYieldShape(const YieldShape& shape)
{
    std::string out = "{";
    for (size_t i = 0; i < shape.size(); ++i)
    {
        out += i ? ",{" : "{";
        for (size_t j = 0; j < shape[i].size(); ++j)
            out += (j ? "," : "") + std::to_string(shape[i][j]);
        out += "}";
    }
    return out + "}";
}

// The handler thread, then each coroutine in the script's `parked` table,
// against the shapes a scenario expects
static void checkYieldShapes(TestScript& ts, const std::vector<YieldShape>& expected)
{
    lua_State* instance = ts.exec.getInstanceState();
    std::vector<lua_State*> threads;
    threads.push_back(lua_tothread(instance, 1));

    lua_getglobal(instance, "parked");
    REQUIRE(lua_istable(instance, -1));
    int count = lua_objlen(instance, -1);
    for (int i = 1; i <= count; ++i)
    {
        lua_rawgeti(instance, -1, i);
        threads.push_back(lua_tothread(instance, -1));
        lua_pop(instance, 1);
    }
    lua_pop(instance, 1);

    REQUIRE(threads.size() == expected.size());
    for (size_t i = 0; i < threads.size(); ++i)
    {
        REQUIRE(threads[i] != nullptr);
        YieldShape actual = readYieldShape(threads[i]);
        INFO("thread ", i, " expected ", formatYieldShape(expected[i]), " actual ", formatYieldShape(actual));
        CHECK(actual == expected[i]);
    }
}

// Okay so this is probably overkill, but we also want to test some of our lyieldable stuff.
// The best way we can do that is by ensuring that we yield at a variety of points in the
// various functions, and save the state. The problem is trying to ensure we can deterministically
// pause at the correct point in that C++ function stack. Counting interrupts is very brittle,
// so instead we just force the functions to constantly flush their yield buffers and inspect
// them to see if they're in the stack state we want to test.
//
// Naturally, all of these Phase enums are kind of internal implementation details of each
// of these functions, so just keep a record of them here as well. Nasty as hell, but I'm
// okay with it if it makes this testable.
namespace phase
{
// lllevents.cpp llevents_handle_event, llltimers.cpp lltimers_tick
constexpr uint8_t HANDLE_EVENT_CALL_HANDLER = 2;
constexpr uint8_t TICK_CALL_HANDLER = 2;
// lyieldstrlib.cpp str_find_match_body, str_gsub_body, yieldable_gmatch_aux
constexpr uint8_t FIND_MATCH_CALL = 1;
constexpr uint8_t FIND_PLAIN_YIELD = 2;
constexpr uint8_t GSUB_MATCH_CALL = 1;
constexpr uint8_t GSUB_REPL_CALL = 2;
constexpr uint8_t GMATCH_MATCH_CALL = 1;
// lyieldstrlib.cpp iterative_match_helper, the check at the top of its loop
constexpr uint8_t MATCH_MAIN_YIELD = 1;
// ltablib.cpp tsort, sort_rec, tfind
constexpr uint8_t SORT = 1;
constexpr uint8_t SORT_REC_CMP_UL = 2;
constexpr uint8_t TFIND_LOOP = 1;
// lua_cjson.cpp json_encode_common, json_append_data, json_append_array
constexpr uint8_t ENCODE_APPEND_DATA = 1;
constexpr uint8_t APPEND_DATA_ARRAY_AUTO = 5;
constexpr uint8_t APPEND_ARRAY_REPLACER_CALL = 4;
// lua_cjson.cpp json_decode_common, json_process_value, json_parse_array_context
constexpr uint8_t DECODE_PROCESS_VALUE = 1;
constexpr uint8_t PROCESS_VALUE_ARRAY = 2;
constexpr uint8_t PARSE_ARRAY_REVIVER_CALL = 4;
}

// Handler thread first, then the coroutines in the order the source parks them
static const std::vector<YieldShape> kYieldedStdlibShapes = {
    // _handleEvent -> _tick -> the timer function, which calls preempt()
    {{phase::HANDLE_EVENT_CALL_HANDLER}, {phase::TICK_CALL_HANDLER}},
    {{phase::FIND_PLAIN_YIELD}},
    {{phase::FIND_MATCH_CALL, phase::MATCH_MAIN_YIELD}},
    {{phase::GSUB_MATCH_CALL, phase::MATCH_MAIN_YIELD}},
    {{phase::GMATCH_MATCH_CALL, phase::MATCH_MAIN_YIELD}},
    {{phase::TFIND_LOOP}},
    {{phase::GSUB_REPL_CALL}},
    {{phase::SORT, phase::SORT_REC_CMP_UL}},
    {{phase::ENCODE_APPEND_DATA, phase::APPEND_DATA_ARRAY_AUTO, phase::APPEND_ARRAY_REPLACER_CALL}},
    {{phase::DECODE_PROCESS_VALUE, phase::PROCESS_VALUE_ARRAY, phase::PARSE_ARRAY_REVIVER_CALL}},
};

static const GoldenScenario kGoldenScenarios[] = {
    {"between-handlers",
        [](TestScript& ts)
        {
            ts.start();
        },
        [](TestScript& ts)
        {
            dispatch(ts.exec, LSLEvent::MovingStart);
            checkCapture(ts.host.printed, {"counter ok", "table ok", "buffer ok", "vector ok", "coroutine ok", "upvalue ok"});
        }},
    {"yielded-main",
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
    {"yielded-handler",
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
    // Every yieldable C function suspended mid-call at once, each in its own
    // coroutine, the handler itself inside _tick. The shapes pin where.
    {"yielded-stdlib",
        [](TestScript& ts)
        {
            ts.start();
            ts.host.script_clock = 1.0;
            dispatch(ts.exec, LSLEvent::Timer, HandlerRunStatus::Preempted);
            checkYieldShapes(ts, kYieldedStdlibShapes);
        },
        [](TestScript& ts)
        {
            REQUIRE(ts.exec.isHandlerActive());
            checkYieldShapes(ts, kYieldedStdlibShapes);
            resumeToCompletion(ts.exec, 1.0);
            dispatch(ts.exec, LSLEvent::MovingStart);
            checkCapture(ts.host.printed, {"yieldables ok"});
        }},
    {"errored-handler",
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
    {"between-handlers-lsl",
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
    {"state-change-pending-lsl",
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
    {"yielded-handler-lsl",
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
    {"errored-main-lsl",
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
    return dir + "/exec";
}

// The format versions this build writes, which name its files in each
// scenario's directory
static std::string goldenFixtureVersion()
{
    return "exec" + std::to_string(kScriptStateFingerprint.major) + "." + std::to_string(kScriptStateFingerprint.minor) + "-ares" +
           std::to_string(ARES_FORMAT_MAJOR) + "." + std::to_string(ARES_FORMAT_MINOR);
}

static std::string goldenFixtureBase(const std::string& dir, const GoldenScenario& scenario)
{
    return dir + "/" + scenario.name + "/" + goldenFixtureVersion();
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

static bool isLSLScenario(std::string_view name)
{
    return name.size() > 4 && name.substr(name.size() - 4) == "-lsl";
}

// Compiling is the one place a bad source throws, and doctest is built without
// exceptions (tests/main.cpp), so letting that escape would abort the run
// instead of saying which file is wrong.
static bool compileGoldenScenario(const std::string& dir, const GoldenScenario& scenario, TestAsset& asset)
{
    const bool lsl = isLSLScenario(scenario.name);
    const std::string source = dir + "/" + scenario.name + (lsl ? "/source.lsl" : "/source.lua");
    const std::string text = readBinaryFile(source);
    try
    {
        asset = compileTestAsset(text.c_str(), lsl);
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

    std::string base = goldenFixtureBase(dir, scenario);
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
        const std::string base = goldenFixtureBase(dir, scenario);
        CAPTURE(base);
        REQUIRE(isFile(base + ".state"));
        REQUIRE(isFile(base + ".sluac"));
    }

    std::vector<std::pair<const GoldenScenario*, std::string>> loadable;
    for (const GoldenScenario& scenario : kGoldenScenarios)
    {
#ifndef LUAU_USE_TAILSLIDE
        // No LSL compiler in this build, so those scenarios can't run
        if (isLSLScenario(scenario.name))
            continue;
#endif
        // Yucky yucky lambda :(
        auto collect = [&](const std::string& path)
        {
            if (hasFileExtension(path, {".state"}))
                loadable.emplace_back(&scenario, path);
        };
        REQUIRE(traverseDirectory(dir + "/" + scenario.name, collect));
        REQUIRE_FALSE(loadable.empty());
    }

    for (const auto& [scenario, state_path] : loadable)
    {
        const std::string subcase_name = std::string(scenario->name) + "/" + fileStem(state_path);
        SUBCASE(subcase_name.c_str())
        {
            const std::string asset_path = state_path.substr(0, state_path.rfind('.')) + ".sluac";
            REQUIRE(isFile(asset_path));

            TestAsset asset{readBinaryFile(asset_path)};

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
