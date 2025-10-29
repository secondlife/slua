// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#include "luacodegen.h"

#include "Luau/BuiltinDefinitions.h"
#include "Luau/DenseHash.h"
#include "Luau/ModuleResolver.h"
#include "Luau/TypeInfer.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Frontend.h"
#include "Luau/Compiler.h"
#include "Luau/CodeGen.h"
#include "Luau/BytecodeSummary.h"

#include "doctest.h"
#include "ScopedFlags.h"
#include "llsl.h"
#include "../VM/src/lstate.h"
#include "../VM/src/mono_strings.h"
#include "../VM/src/lapi.h"
#include "../VM/src/lgc.h"

#include <fstream>
#include <string>
#include <vector>
#include <math.h>

extern bool verbose;
extern bool codegen;
extern int optimizationLevel;

LUAU_FASTINT(CodegenHeuristicsInstructionLimit)
LUAU_DYNAMIC_FASTFLAG(LuauCodegenTrackingMultilocationFix)
LUAU_FASTFLAG(LuauCodegenDetailedCompilationResult)


using StateRef = std::unique_ptr<lua_State, void (*)(lua_State*)>;

typedef struct SLTestRuntimeState : lua_SLRuntimeState
{
    int yield_num = 0;
    int break_num = 0;
    int skip_next_break = 0;

    // Memory limiting state for reachability-based accounting
    lua_OpaqueGCObjectSet free_objects;
    size_t max_mem = 0;
    size_t actual_size = 0;
    size_t approximate_size = 0;
} RuntimeState;


static void lua_test_state_closer(lua_State *L) {
    if (L->userdata)
        delete (RuntimeState *)L->userdata;
    L->userdata = nullptr;
    lua_close(L);
}

static int lua_silence(lua_State* L)
{
    return 0;
}

static void userthread_callback(lua_State *LP, lua_State *L)
{
    if (LP == nullptr)
        return;
    L->userdata = LP->userdata;
}

static void checkStatus(lua_State *L, int status)
{
    if (status != LUA_OK)
    {
        std::string error;
        if (status == LUA_YIELD)
        {
            error = "thread yielded unexpectedly";
        }
        else if (status == LUA_BREAK)
        {
            error = "thread used BREAK unexpectedly";
        }
        else
        {
            if (lua_type(L, -1) == LUA_TSTRING)
            {
                error = lua_tostring(L, -1);
            }
            else
            {
                error = "Unknown error";
            }
        }
        error += "\nstacktrace:\n";
        error += lua_debugtrace(L);

        lua_dumpstack(L);

        FAIL(error);
    }
}

static std::string getSourceFilePath(std::string name)
{
#ifdef LUAU_CONFORMANCE_SOURCE_DIR
    std::string path = LUAU_CONFORMANCE_SOURCE_DIR;
    path += "/";
    path += name;
#else
    std::string path = __FILE__;
    path.erase(path.find_last_of("\\/"));
    path += "/conformance/";
    path += name;
#endif
    return path;
}

static int lua_collectgarbage(lua_State* L)
{
    static const char* const opts[] = {"stop", "restart", "collect", "count", "isrunning", "step", "setgoal", "setstepmul", "setstepsize", nullptr};
    static const int optsnum[] = {
        LUA_GCSTOP, LUA_GCRESTART, LUA_GCCOLLECT, LUA_GCCOUNT, LUA_GCISRUNNING, LUA_GCSTEP, LUA_GCSETGOAL, LUA_GCSETSTEPMUL, LUA_GCSETSTEPSIZE
    };

    int o = luaL_checkoption(L, 1, "collect", opts);
    int ex = luaL_optinteger(L, 2, 0);
    int res = lua_gc(L, optsnum[o], ex);
    switch (optsnum[o])
    {
    case LUA_GCSTEP:
    case LUA_GCISRUNNING:
    {
        lua_pushboolean(L, res);
        return 1;
    }
    default:
    {
        lua_pushnumber(L, res);
        return 1;
    }
    }
}

static int test_integer_call(lua_State *L)
{
    luaL_checkany(L, 1);
    lua_settop(L, 1);
    lua_pushunsigned(L, (unsigned int)LSLIType::LST_INTEGER);
    return lsl_cast(L);
}

// Helper callback that enforces reachability-based memory limits
// Reads max_mem and free_objects from the RuntimeState
static int memoryLimitCallback(lua_State *L, size_t osize, size_t nsize)
{
    L = lua_mainthread(L);
    RuntimeState* state = (RuntimeState*)L->userdata;

    // This is a net shrink in memory, not relevant for our limiting logic. We can't assume that
    // memory being freed has anything to do with us, given that the GC can work on things unrelated
    // to the currently executing task.
    if (osize >= nsize)
        return 0;

    size_t net_gain = nsize - osize;

    // Figure out the actual current size of the heap if we didn't already have it,
    // or the alloc would push us over the limit given the current approximate size.
    if (state->actual_size == 0 || (state->approximate_size + net_gain > state->max_mem))
    {
        state->approximate_size = state->actual_size = lua_userthreadsize(L, &state->free_objects);

        if (state->actual_size + net_gain > state->max_mem)
            return 1;
    }

    state->approximate_size += net_gain;

    return 0;
}

static StateRef runConformance(const char* name, void (*yield)(lua_State* L) = nullptr, void (*setup)(lua_State* L) = nullptr,
    lua_State* initialLuaState = nullptr, lua_CompileOptions* options = nullptr)
{
    std::string path = getSourceFilePath(name);
    std::fstream stream(path, std::ios::in | std::ios::binary);
    INFO(path);
    REQUIRE(stream);

    std::string source(std::istreambuf_iterator<char>(stream), {});

    stream.close();

    if (!initialLuaState)
        initialLuaState = luaL_newstate();
    StateRef globalState(initialLuaState, lua_test_state_closer);
    lua_State* GL = globalState.get();

    // Store some data so we know which yield number we're on
    RuntimeState * runtime_state = new RuntimeState;
    // This is an SL vm, not an LSL VM
    runtime_state->slIdentifier = LUA_SL_IDENTIFIER;
    GL->userdata = runtime_state;

    luaL_openlibs(GL);
    luaopen_sl(GL);
    luaopen_ll(GL, true);
    lua_pop(GL, 1);
    luaopen_cjson(GL);
    lua_pop(GL, 1);
    luaopen_llbase64(GL);
    lua_pop(GL, 1);

    // Register a few global functions for conformance tests
    std::vector<luaL_Reg> funcs = {
        {"collectgarbage", lua_collectgarbage},
        {"integer", test_integer_call},
    };

    // "null" terminate the list of functions to register
    funcs.push_back({nullptr, nullptr});

    lua_pushvalue(GL, LUA_GLOBALSINDEX);
    luaL_register(GL, nullptr, funcs.data());
    lua_setglobal(GL, "_G");

    // In some configurations we have a larger C stack consumption which trips some conformance tests
#if defined(LUAU_ENABLE_ASAN) || defined(_NOOPT) || defined(_DEBUG)
    lua_pushboolean(GL, true);
    lua_setglobal(GL, "limitedstack");
#endif

    // Thread should be pristine
    LUAU_ASSERT(lua_gettop(GL) == 0);

    lua_callbacks(GL)->userthread = userthread_callback;

    // Extra test-specific setup
    if (setup)
        setup(GL);

    *runtime_state = *runtime_state;

    // Protect core libraries and metatables from modification
    luaL_sandbox(GL);
    lua_fixallcollectable(GL);

    // Create thread and runtime structures in memcat 0 (system allocations)
    lua_setmemcat(GL, 0);
    lua_State *L = lua_newthread(GL);
    lua_setthreaddata(L, runtime_state);

    // Create a new writable global table for current thread
    luaL_sandboxthread(L);

    luaSL_createeventmanager(L);
    lua_setglobal(L, "LLEvents");

    luaSL_createtimermanager(L);
    lua_setglobal(L, "LLTimers");

    std::string chunkname = "=" + std::string(name);

    Luau::BytecodeBuilder bcb;
    compileOrThrow(bcb, source);
    std::string bytecode = bcb.getBytecode();

    lua_setmemcat(L, 2);
    int result = luau_load(L, chunkname.c_str(), bytecode.c_str(), bytecode.length(), 0);
    if (result)
    {
        FAIL(result);
        return globalState;
    }

    lua_fixallcollectable(L);
    runtime_state->free_objects = lua_collectfreeobjects(L);

    // Make sure all of our globals were fixable. If that's the case,
    // _G itself should be marked fixed as well.
    lua_getglobal(GL, "_G");
    REQUIRE(isfixed(gcvalue(luaA_toobject(GL, -1))));
    lua_pop(GL, 1);

    int status;
    do
    {
        status = lua_resume(L, nullptr, 0);
        if (yield && status == LUA_YIELD)
        {
            yield(L);
        }
    } while(status == LUA_BREAK || (yield && status == LUA_YIELD));

    checkStatus(L, status);

    REQUIRE(lua_gettop(L));
    REQUIRE(lua_isstring(L, -1));
    CHECK_EQ(std::string(lua_tostring(L, -1)), std::string("OK"));

    extern void luaC_validate(lua_State * L); // internal function, declared in lgc.h - not exposed via lua.h
    luaC_validate(L);

    return globalState;
}

TEST_SUITE_BEGIN("SLConformance");

// Figure out which yield number this is for the current testcase
#define YIELD_NUM(L) (((RuntimeState *)(L)->userdata)->yield_num)

TEST_CASE("UUID Table Keys")
{
    runConformance("uuid_table_keys.lua");
}

static int give_uuid(lua_State *L)
{
    luaSL_pushuuidstring(L, "12345678-9abc-def0-1234-56789abcdef0");
    bool compressed = false;
    const char *uuid = luaSL_checkuuid(L, -1, &compressed);
    REQUIRE(compressed);
    // Check the in-memory form of the UUID
    REQUIRE(memcmp(uuid, "\x12\x34\x56\x78\x9a\xbc\xde\xf0\x12\x34\x56\x78\x9a\xbc\xde\xf0", 16) == 0);
    return 1;
}

TEST_CASE("Push UUID string")
{
    runConformance("uuid.lua", nullptr, [](lua_State *L) {
            lua_pushcfunction(L, give_uuid, "give_uuid");
            lua_setglobal(L, "give_uuid");
    });
}

static int get_num_table_keys(lua_State *L, int idx)
{
    lua_pushnil(L);
    int num = 0;
    while(lua_next(L, idx))
    {
        ++num;
        lua_pop(L, 1);
    }
    return num;
}

static void require_weak_uuid_counts(lua_State *L, int start_idx, int num_irregular, int num_compressed)
{
    REQUIRE_EQ(get_num_table_keys(L, start_idx), num_irregular);
    REQUIRE_EQ(get_num_table_keys(L, start_idx + 1), num_compressed);
}

TEST_CASE("UUID interning")
{
    // Do nothing, just set up the state so we can poke it directly
    auto state = runConformance("nothing.lua");
    lua_State *L = lua_tothread(state.get(), 1);
    REQUIRE(L);
    lua_gc(L, LUA_GCCOLLECT, 0);

    auto *runtime_state = LUAU_GET_SL_VM_STATE(L);

    int weak_idx = lua_gettop(L) + 1;
    lua_getref(L, runtime_state->uuidWeakTab);
    lua_getref(L, runtime_state->uuidCompressedWeakTab);
    require_weak_uuid_counts(L, weak_idx, 0, 0);

    for (int i=0; i<2; ++i)
    {
        luaSL_pushuuidstring(L, "foo");
    }

    // These should have the same pointer identity
    REQUIRE_EQ(luaA_toobject(L, -1)->value.gc, luaA_toobject(L, -2)->value.gc);

    require_weak_uuid_counts(L, weak_idx, 1, 0);
    // Pop off the newest copy, run a GC and then check that this assertion still holds.
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
    require_weak_uuid_counts(L, weak_idx, 1, 0);

    // It's okay to release it once there are no more reachable instances
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
    require_weak_uuid_counts(L, weak_idx, 0, 0);

    // Now do the same with a well-formed UUID
    for (int i=0; i<2; ++i)
    {
        luaSL_pushuuidstring(L, "12345678-9abc-def0-1234-56789abcdef0");
    }

    // These should have the same pointer identity
    REQUIRE_EQ(luaA_toobject(L, -1)->value.gc, luaA_toobject(L, -2)->value.gc);

    require_weak_uuid_counts(L, weak_idx, 0, 1);
    // Pop off the newest copy, run a GC and then check that this assertion still holds.
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
    require_weak_uuid_counts(L, weak_idx, 0, 1);

    // It's okay to release it once there are no more reachable instances
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
    require_weak_uuid_counts(L, weak_idx, 0, 0);

    // Push a UUID as well as its binary form. These should result in separate instances
    // even though they have the same underlying string value. one is compressed and
    // one is not, but has the same literal bytes as the backing str.
    luaSL_pushuuidstring(L, "12345678-9abc-def0-1234-56789abcdef0");
    luaSL_pushuuidlstring(L, "\x12\x34\x56\x78\x9a\xbc\xde\xf0\x12\x34\x56\x78\x9a\xbc\xde\xf0", 16);

    require_weak_uuid_counts(L, weak_idx, 1, 1);

    lua_LSLUUID *bin_uuid = (lua_LSLUUID*)lua_touserdatatagged(L, -1, UTAG_UUID);
    lua_LSLUUID *real_uuid = (lua_LSLUUID*)lua_touserdatatagged(L, -2, UTAG_UUID);
    REQUIRE_NE(bin_uuid, real_uuid);
    REQUIRE_EQ(bin_uuid->str, real_uuid->str);

    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
    require_weak_uuid_counts(L, weak_idx, 0, 1);
}

TEST_CASE("UUID interning (GC Fixed)")
{
    // Do nothing, just set up the state so we can poke it directly
    auto state = runConformance("nothing.lua", nullptr, [](lua_State *L) {
        luaSL_pushuuidstring(L, "12345678-9abc-def0-1234-56789abcdef0");
        lua_setglobal(L, "test_uuid");
    });
    lua_State *L = lua_tothread(state.get(), 1);
    REQUIRE(L);

    auto *runtime_state = LUAU_GET_SL_VM_STATE(L);

    int weak_idx = lua_gettop(L) + 1;

    lua_getref(L, runtime_state->uuidWeakTab);
    lua_getref(L, runtime_state->uuidCompressedWeakTab);
    require_weak_uuid_counts(L, weak_idx, 0, 1);

    lua_gc(L, LUA_GCCOLLECT, 0);

    require_weak_uuid_counts(L, weak_idx, 0, 1);

    lua_getglobal(L, "test_uuid");
    luaSL_pushuuidstring(L, "12345678-9abc-def0-1234-56789abcdef0");

    // These should have the same pointer identity
    REQUIRE_EQ(luaA_toobject(L, -1)->value.gc, luaA_toobject(L, -2)->value.gc);

    lua_LSLUUID *first_uuid = (lua_LSLUUID*)lua_touserdatatagged(L, -1, UTAG_UUID);
    lua_LSLUUID *second_uuid = (lua_LSLUUID*)lua_touserdatatagged(L, -2, UTAG_UUID);
    REQUIRE_EQ(first_uuid->str, second_uuid->str);

    require_weak_uuid_counts(L, weak_idx, 0, 1);
    // Pop off the newest copy, run a GC and then check that this assertion still holds.
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
    require_weak_uuid_counts(L, weak_idx, 0, 1);
}

TEST_CASE("SLFunctions")
{
    runConformance("slfunctions.lua");
}

TEST_CASE("Wrapped Breaks")
{
    runConformance("wrapped_breaks.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, lua_break, "breaker");
        lua_setglobal(L, "breaker");
    });
}

TEST_CASE("Nested Breaks")
{
    runConformance("nested_breaks.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, lua_break, "breaker");
        lua_setglobal(L, "breaker");
    });
}

TEST_CASE("Can interrupt everywhere in simple script")
{
    runConformance("json_encode_sl.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, lua_silence, "print");
        lua_setglobal(L, "print");
        lua_callbacks(L)->interrupt = [](lua_State *L, int gc) {
            if (gc >= 0)
                return;
            auto may_yield = luaSL_may_interrupt(L);
            if (may_yield != YieldableStatus::OK && may_yield != YieldableStatus::BAD_NCALLS) {
                LUAU_ASSERT(!"Unexpected may_interrupt return code");
            }
        };
    });
}

static int breakable_count = 0;

static int breakable(lua_State *L)
{
    ++breakable_count;
    luau_interruptoncalltail(L);
    return LUA_OK;
}

TEST_CASE("Can break at tail of LOP_CALL")
{
    auto state = runConformance("breakcheck.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, breakable, "breakable");
        lua_setglobal(L, "breakable");
        lua_callbacks(L)->interrupt = [](lua_State *L, int gc) {
            if (gc >= 0)
                return;
            auto may_yield = luaSL_may_interrupt(L);
            if (may_yield != YieldableStatus::OK)
            {
                LUAU_ASSERT(!"Unexpected may_interrupt return code");
            }

            auto *ud = ((SLTestRuntimeState*)L->userdata);
            if (ud->skip_next_break)
            {
                // Don't break twice in a row.
                ud->skip_next_break = 0;
                return;
            }

            // Don't set this flag if we're doing the break on the tail of a LOP_CALL
            // This makes sure we'll still break for the following LOP_RETURN.
            // You can't actually check this flag in user code, but it's necessary for
            // our test to make sure our interrupt behavior is correct.
            if (!L->global->calltailinterruptcheck)
                ud->skip_next_break = 1;

            ud->break_num++;
            lua_break(L);
        };
    });
    // Make sure the function actually ran
    CHECK_EQ(breakable_count, 1);
    // We should have broken 3 times, once at the start of LOP_CALL,
    // one at the end, and one at the LOP_RETURN.
    CHECK_EQ(((SLTestRuntimeState*)state->userdata)->break_num, 3);
}

TEST_CASE("String comparisons work with a non-default memcat")
{
    runConformance("string_equality.lua", nullptr, [](lua_State *L) {
        lua_setmemcat(L, 2);
    });
}

TEST_CASE("Integer bitwise operations")
{
    runConformance("integer_bitwise.lua");
}

TEST_CASE("lljson")
{
    runConformance("lljson.lua");
}

TEST_CASE("llbase64")
{
    runConformance("llbase64.lua");
}

TEST_CASE("llcompat")
{
    runConformance("llcompat.lua");
}

TEST_CASE("SL Ares")
{
    runConformance("sl_ares.lua");
}

static bool may_call_handle_event = true;
TEST_CASE("LLEvents")
{
    runConformance("llevents.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, lua_break, "breaker");
        lua_setglobal(L, "breaker");
        lua_pushcfunction(L, [](lua_State *L) {
            may_call_handle_event = lua_toboolean(L, 1);
            return 0;
        }, "set_may_call_handle_event");
        lua_setglobal(L, "set_may_call_handle_event");
        auto sl_state = LUAU_GET_SL_VM_STATE(L);
        sl_state->eventHandlerRegistrationCb = [](lua_State *L, const char *event_name, bool enabled) {
            if (!strcmp(event_name, "disallowed"))
            {
                // reject any attempt to work with an event named "disallowed".
                return false;
            }
            return true;
        };
        sl_state->mayCallHandleEventCb = [](lua_State *L) { return may_call_handle_event; };
    });
}

static double test_clock_time = 0.0;
TEST_CASE("LLTimers")
{
    test_clock_time = 0.0;
    runConformance("lltimers.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, lua_break, "breaker");
        lua_setglobal(L, "breaker");

        // Provide a setclock function to control time in tests
        lua_pushcfunction(L, [](lua_State *L) {
            test_clock_time = luaL_checknumber(L, 1);
            return 0;
        }, "setclock");
        lua_setglobal(L, "setclock");

        // Provide a getclock function to read time in tests
        lua_pushcfunction(L, [](lua_State *L) {
            lua_pushnumber(L, test_clock_time);
            return 1;
        }, "getclock");
        lua_setglobal(L, "getclock");

        auto sl_state = LUAU_GET_SL_VM_STATE(L);
        // Set up clock callback to return our test time
        sl_state->clockProvider = [](lua_State *L) {
            return test_clock_time;
        };
        // Set up timer event callback (no-op for tests)
        sl_state->setTimerEventCb = [](lua_State *L, double interval) {
            // In real usage, this would schedule a timer event
            // For tests, we manually call _tick()
        };
    });
}

TEST_CASE("LLEvents and LLTimers interrupt between handlers")
{
    static int yield_count = 0;
    static double test_time = 0.0;
    yield_count = 0;
    test_time = 0.0;

    runConformance(
        "llevents_interrupt.lua",
        // yield handler
        [](lua_State *L) {
            yield_count++;
        },
        // setup handler
        [](lua_State *L) {
            // runConformance already created LLEvents and LLTimers, just configure them
            auto sl_state = LUAU_GET_SL_VM_STATE(L);
            sl_state->eventHandlerRegistrationCb = [](lua_State *L, const char *event_name, bool enabled) {
                return true; // Allow all events for this test
            };
            sl_state->mayCallHandleEventCb = [](lua_State *L) {
                return true; // Allow calling handleEvent
            };
            sl_state->clockProvider = [](lua_State *L) {
                return test_time;
            };
            sl_state->setTimerEventCb = [](lua_State *L, double interval) {
                // No-op for test
            };

            lua_pushcfunction(L, [](lua_State *L) {
                test_time = luaL_checknumber(L, 1);
                return 0;
            }, "setclock");
            lua_setglobal(L, "setclock");

            // Set up interrupt callback that forces yields on handler interrupts only
            // This serves as a check that we're _always_ allowed to interrupt between
            // handlers, even if we can't interrupt within the handlers themselves.
            lua_callbacks(L)->interrupt = [](lua_State *L, int gc) {
                if (gc != -2)
                    return;  // Only yield for handler interrupt checks
                lua_yield(L, 0);
            };
        });

    // Should have 3 event handlers + 3 timer handlers = 6 yields total
    CHECK_EQ(yield_count, 6);
}

TEST_CASE("Table Clone OoM")
{
    runConformance("table_clone_oom.lua", nullptr, [](lua_State *L) {
        RuntimeState* state = (RuntimeState*)L->userdata;
        state->max_mem = 200;

        lua_pushcfunction(L, [](lua_State *L) {lua_setmemcat(L, luaL_checkinteger(L, 1)); return 0;}, "change_memcat");
        lua_setglobal(L, "change_memcat");

        lua_callbacks(L)->beforeallocate = memoryLimitCallback;
    });
}

TEST_CASE("User thread alloc size calculation")
{
    runConformance("responsive_gc.lua", nullptr, [](lua_State *L) {
        RuntimeState* state = (RuntimeState*)L->userdata;
        state->max_mem = 11000;

        lua_pushcfunction(L, [](lua_State *L) {lua_setmemcat(L, luaL_checkinteger(L, 1)); return 0;}, "change_memcat");
        lua_setglobal(L, "change_memcat");

        lua_callbacks(L)->beforeallocate = memoryLimitCallback;
    });
}

TEST_SUITE_END();
