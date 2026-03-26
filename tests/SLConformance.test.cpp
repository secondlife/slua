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
#include "lyieldablemacros.h"
#include "../VM/src/lstate.h"
#include "../VM/src/mono_strings.h"
#include "../VM/src/lapi.h"
#include "../VM/src/lgc.h"
#include "../VM/src/ltable.h"

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
    const Instruction* last_break_pc = nullptr;
    int interrupt_call_count = 0;

    // Memory limiting state for reachability-based accounting
    lua_OpaqueGCObjectSet free_objects;
    size_t max_mem = 0;
    size_t actual_size = 0;
    size_t approximate_size = 0;

    // Flags for interrupt check testing
    bool interrupt_should_clear = false;
    bool callback_ran = false;
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

// Returns (array_size, hash_size) for a table - for testing table sizing behavior
static int lua_table_sizes(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    LuaTable* t = hvalue(luaA_toobject(L, 1));
    lua_pushinteger(L, t->sizearray);
    lua_pushinteger(L, t->node == &luaH_dummynode ? 0 : sizenode(t));
    return 2;
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
    luaopen_sl(GL, true);
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

    // Create a new writable global table for current thread
    luaL_sandboxthread(L);

    luaSL_createeventmanager(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "LLEvents");

    luaSL_createtimermanager(L);
    lua_setglobal(L, "LLTimers");

    std::string chunkname = "=" + std::string(name);

    Luau::BytecodeBuilder bcb;
    compileOrThrow(bcb, source);
    std::string bytecode = bcb.getBytecode();

    lua_setmemcat(L, LUA_FIRST_USER_MEMCAT);
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

    // Ensure closures created during execution have correct memcat
    lua_setmemcat(L, LUA_FIRST_USER_MEMCAT);

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

static int give_quaternion(lua_State *L)
{
    luaSL_pushquaternion(L, 1.0, 2.0, 3.0, 4.0);
    return 1;
}

TEST_CASE("Push Quaternion")
{
    runConformance("quaternion.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, give_quaternion, "give_quaternion");
        lua_setglobal(L, "give_quaternion");
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

#define require_weak_uuid_counts(L, start_idx, num_irregular, num_compressed) \
do { \
REQUIRE_EQ(get_num_table_keys(L, (start_idx)), (num_irregular)); \
REQUIRE_EQ(get_num_table_keys(L, (start_idx) + 1), (num_compressed)); \
} while (0)

// Helper function to create a weak table
// mode: "k" for weak keys, "v" for weak values, "kv" for both
// Returns the stack index of the created table
static int create_weak_table(lua_State *L, const char* mode)
{
    lua_newtable(L);
    int table_idx = lua_gettop(L);

    // Create weak metatable
    lua_newtable(L);
    lua_pushstring(L, mode);
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, table_idx);

    return table_idx;
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
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);

    // Test that UUIDs have value semantics in user weak tables (memcat > 1)
    lua_setmemcat(L, LUA_FIRST_USER_MEMCAT);

    // Create a weak-keyed table
    int user_weak_table_idx = create_weak_table(L, "k");

    // Push two different instances of the same UUID as keys (they'll be interned to same pointer)
    luaSL_pushuuidstring(L, "12345678-9abc-def0-1234-56789abcdef0");
    GCObject* first_uuid_ptr = luaA_toobject(L, -1)->value.gc;
    lua_pushstring(L, "first");
    lua_settable(L, user_weak_table_idx);

    luaSL_pushuuidstring(L, "12345678-9abc-def0-1234-56789abcdef0");
    GCObject* second_uuid_ptr = luaA_toobject(L, -1)->value.gc;
    lua_pushstring(L, "second");
    lua_settable(L, user_weak_table_idx);

    // Verify these are the same instance (interning)
    REQUIRE_EQ(first_uuid_ptr, second_uuid_ptr);

    // Table should have 1 entry (same UUID key)
    REQUIRE_EQ(get_num_table_keys(L, user_weak_table_idx), 1);

    lua_gc(L, LUA_GCCOLLECT, 0);

    // Test with a UUID that's not referenced anywhere else
    int user_weak_table2_idx = create_weak_table(L, "k");

    // Push UUID and store it
    luaSL_pushuuidstring(L, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    lua_pushvalue(L, -1); // Duplicate for later
    lua_pushstring(L, "value");
    lua_settable(L, user_weak_table2_idx);

    // Table should have 1 entry
    REQUIRE_EQ(get_num_table_keys(L, user_weak_table2_idx), 1);

    // Pop the duplicate UUID, run GC
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);

    require_weak_uuid_counts(L, weak_idx, 0, 2);

    // In a memcat > 1 weak table, the UUID should NOT be collected (value semantics)
    REQUIRE_EQ(get_num_table_keys(L, user_weak_table2_idx), 1);

    // Verify we can still look it up
    luaSL_pushuuidstring(L, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    lua_gettable(L, user_weak_table2_idx);
    REQUIRE(lua_isstring(L, -1));
    REQUIRE_EQ(std::string(lua_tostring(L, -1)), "value");
    lua_pop(L, 1);

    // Test UUIDs as values in weak-value tables (memcat > 1)
    lua_gc(L, LUA_GCCOLLECT, 0);

    int user_weak_value_table_idx = create_weak_table(L, "v");

    // Store UUID as a value
    lua_pushstring(L, "test_key");
    luaSL_pushuuidstring(L, "bbbbbbbb-cccc-dddd-eeee-ffffffffffff");
    lua_settable(L, user_weak_value_table_idx);

    // Table should have 1 entry
    REQUIRE_EQ(get_num_table_keys(L, user_weak_value_table_idx), 1);

    lua_gc(L, LUA_GCCOLLECT, 0);

    // UUID value should NOT be collected (value semantics in memcat > 1)
    REQUIRE_EQ(get_num_table_keys(L, user_weak_value_table_idx), 1);

    // Should have 3 UUIDs in interning table now (2 from keys + 1 from value)
    require_weak_uuid_counts(L, weak_idx, 0, 3);

    // Verify we can still look up the value
    lua_pushstring(L, "test_key");
    lua_gettable(L, user_weak_value_table_idx);
    REQUIRE(lua_isuserdata(L, -1));
    // Verify it's the correct UUID value
    size_t len;
    const char* str = luaL_tolstring(L, -1, &len);
    REQUIRE_EQ(std::string(str), "bbbbbbbb-cccc-dddd-eeee-ffffffffffff");
    lua_pop(L, 2); // pop string and UUID
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
static bool must_break = false;

static int breakable(lua_State *L)
{
    must_break = true;
    ++breakable_count;
    luau_interruptoncalltail(L);
    return LUA_OK;
}

static int breakcheck_index(lua_State *L)
{
    // this should have already been cleared
    CHECK(!L->global->calltailinterruptcheck);
    // Should have already done it!
    CHECK(!must_break);
    // return a function that does exactly nothing, so we can test LOP_NAMECALL
    lua_pushcfunction(L, lua_silence, "nothing");
    return 1;
}

TEST_CASE("Can break at tail of LOP_CALL")
{
    must_break = false;
    breakable_count = 0;
    auto state = runConformance("breakcheck.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, breakable, "breakable");
        lua_setglobal(L, "breakable");
        lua_pushcfunction(L, breakcheck_index, "breakcheck_index");
        lua_setglobal(L, "breakcheck_index");
        lua_pushcfunction(L, lua_silence, "nothing");
        lua_setglobal(L, "nothing");
        lua_callbacks(L)->interrupt = [](lua_State *L, int gc) {
            if (gc >= 0)
                return;

            auto *ud = ((SLTestRuntimeState*)L->userdata);
            ud->interrupt_call_count++;

            auto may_yield = luaSL_may_interrupt(L);
            if (may_yield != YieldableStatus::OK && must_break)
            {
                LUAU_ASSERT(!"Unexpected may_interrupt return code");
            }

            // Get the current PC to check if this is a re-trigger after resume
            const Instruction* current_pc = L->ci->savedpc;
            bool is_tail_call = L->global->calltailinterruptcheck;

            // If this is the same PC as the last break AND it's not a tail call check,
            // then this is a re-trigger after resume, so skip it
            if (current_pc == ud->last_break_pc && !is_tail_call && !must_break)
            {
                // This is a re-trigger after resume, skip it
                return;
            }

            if (must_break || may_yield == YieldableStatus::OK)
            {
                ud->break_num++;
                must_break = false;
                ud->last_break_pc = current_pc;
                lua_break(L);
            }
        };
    });
    // Make sure the function actually ran
    CHECK_EQ(breakable_count, 3);
    // Expected breaks: 3 breakable() calls (each with call + tail = 6),
    // LLEvents:on call, nothing(m[1]) call, m:foo call, return = 10 total
    // Note: NAMECALL is no longer preemptible - only the following CALL interrupts
    CHECK_EQ(((SLTestRuntimeState*)state->userdata)->break_num, 10);
    // Total interrupts: 10 breaks + 6 re-triggers (CALL only, not tail checks)
    // + 5 non-breaking (setmetatable, 2 NAMECALL __index, m[1] __index, m[2] __newindex) = 21
    CHECK_EQ(((SLTestRuntimeState*)state->userdata)->interrupt_call_count, 21);
}

TEST_CASE("String comparisons work with a non-default memcat")
{
    runConformance("string_equality.lua", nullptr, [](lua_State *L) {
        lua_setmemcat(L, LUA_FIRST_USER_MEMCAT);
    });
}

TEST_CASE("Integer bitwise operations")
{
    runConformance("integer_bitwise.lua");
}

TEST_CASE("bit32.s32")
{
    runConformance("bit32_s32.lua");
}

// ServerLua: shared interrupt infrastructure for lljson yield tests
static bool jsonInterruptEnabled = false;
static int jsonYieldCount = 0;

static void setupJsonInterruptInfra(lua_State* L)
{
    jsonInterruptEnabled = false;
    jsonYieldCount = 0;

    lua_pushcfunction(
        L,
        [](lua_State* L) -> int
        {
            jsonYieldCount = 0;
            return 0;
        },
        "clear_check_count"
    );
    lua_setglobal(L, "clear_check_count");

    lua_pushcfunction(
        L,
        [](lua_State* L) -> int
        {
            lua_pushinteger(L, jsonYieldCount);
            return 1;
        },
        "get_check_count"
    );
    lua_setglobal(L, "get_check_count");

    lua_pushcfunction(
        L,
        [](lua_State* L) -> int
        {
            jsonInterruptEnabled = true;
            return 0;
        },
        "enable_check_interrupt"
    );
    lua_setglobal(L, "enable_check_interrupt");

    lua_callbacks(L)->interrupt = [](lua_State* L, int gc)
    {
        if (gc != LUA_INTERRUPT_LLLIB)
            return;
        if (!jsonInterruptEnabled)
            return;
        jsonYieldCount++;
        lua_yield(L, 0);
    };
}

TEST_CASE("lljson")
{
    runConformance("lljson.lua", nullptr, setupJsonInterruptInfra);
}

TEST_CASE("lljson_replacer")
{
    runConformance("lljson_replacer.lua", nullptr, setupJsonInterruptInfra);
    runConformance("lljson_typedjson.lua", nullptr, setupJsonInterruptInfra);
}

TEST_CASE("llbase64")
{
    runConformance("llbase64.lua");
}

TEST_CASE("llcompat")
{
    runConformance("llcompat.lua");
}

TEST_CASE("dangerouslyexecuterequiredmodule")
{
    runConformance("dangerouslyexecuterequiredmodule.lua");
}

TEST_CASE("dangerouslyexecuterequiredmodule with breaks")
{
    runConformance("dangerouslyexecuterequiredmodule_breaks.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, lua_break, "breaker");
        lua_setglobal(L, "breaker");
    });
}

TEST_CASE("Fake require()")
{
    runConformance("fake_require.lua");
}

TEST_CASE("SL Ares")
{
    runConformance("sl_ares.lua");
}

TEST_CASE("Ares Scavenger")
{
    runConformance("ares_scavenger.lua");
}

static const luaL_Reg test_ll_events_lib[] = {
    {"AdjustDamage", [](lua_State *L) {return 0;}},
    {nullptr, nullptr}
};

TEST_CASE("LLEvents")
{
    runConformance("llevents.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, lua_break, "breaker");
        lua_setglobal(L, "breaker");
        auto sl_state = LUAU_GET_SL_VM_STATE(L);
        sl_state->eventHandlerRegistrationCb = [](lua_State *L, const char *event_name, bool enabled) {
            if (!strcmp(event_name, "disallowed"))
            {
                // reject any attempt to work with an event named "disallowed".
                return false;
            }
            return true;
        };

        luaL_register_noclobber(L, LUA_LLLIBNAME, test_ll_events_lib);
    });
}

static double test_clock_time = 0.0;
static double last_timer_interval = -1.0;
TEST_CASE("LLTimers")
{
    test_clock_time = 0.0;
    last_timer_interval = -1.0;
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

        // Provide a function to read the last timer interval
        lua_pushcfunction(L, [](lua_State *L) {
            lua_pushnumber(L, last_timer_interval);
            return 1;
        }, "get_last_interval");
        lua_setglobal(L, "get_last_interval");

        auto sl_state = LUAU_GET_SL_VM_STATE(L);
        // Set up clock callback to return our test time
        sl_state->performanceClockProvider = sl_state->clockProvider = [](lua_State *L) {
            return test_clock_time;
        };
        // Set up timer event callback to capture the last interval
        sl_state->setTimerEventCb = [](lua_State *L, double interval) {
            // In real usage, this would schedule a timer event
            // For tests, we manually call _tick() and capture the interval
            last_timer_interval = interval;
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
            sl_state->performanceClockProvider = sl_state->clockProvider = [](lua_State *L) {
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

// Helper functions for interrupt check testing
static int reset_interrupt_test(lua_State* L)
{
    RuntimeState* state = (RuntimeState*)L->userdata;
    state->interrupt_should_clear = true;
    state->callback_ran = false;
    return 0;
}

static int check_callback_ran(lua_State* L)
{
    RuntimeState* state = (RuntimeState*)L->userdata;
    lua_pushboolean(L, state->callback_ran);
    return 1;
}

static int test_callback(lua_State* L)
{
    RuntimeState* state = (RuntimeState*)L->userdata;

    if (state->interrupt_should_clear)
    {
        luaL_error(L, "Interrupt handler did not run before callback!");
    }

    state->callback_ran = true;
    lua_pushstring(L, "ok");
    return 1;
}

static int test_sort_callback(lua_State* L)
{
    RuntimeState* state = (RuntimeState*)L->userdata;

    if (state->interrupt_should_clear)
    {
        luaL_error(L, "Interrupt handler did not run before sort comparison!");
    }

    state->callback_ran = true;
    // `table.sort()` gets very angry if we don't do something that looks
    // like sorting _eventually_
    lua_pushboolean(L, lua_lessthan(L, 1, 2));
    return 1;
}

TEST_CASE("Metamethods and library callbacks receive interrupt checks")
{
    runConformance("metamethod_and_callback_interrupts.lua", nullptr, [](lua_State* L) {
        // Set up interrupt handler that clears the flag for metamethod (-3) and library callback (-4) interrupts
        lua_callbacks(L)->interrupt = [](lua_State* L, int gc) {
            switch(gc)
            {
            case LUA_INTERRUPT_METAMETHOD:
            case LUA_INTERRUPT_LLLIB:
            case LUA_INTERRUPT_STDLIB:
                break;
            default:
                return;
            }

            RuntimeState* state = (RuntimeState*)L->userdata;
            state->interrupt_should_clear = false;
        };

        // Register helper functions
        lua_pushcfunction(L, reset_interrupt_test, "reset_interrupt_test");
        lua_setglobal(L, "reset_interrupt_test");

        lua_pushcfunction(L, check_callback_ran, "check_callback_ran");
        lua_setglobal(L, "check_callback_ran");

        lua_pushcfunction(L, test_callback, "test_callback");
        lua_setglobal(L, "test_callback");

        lua_pushcfunction(L, test_sort_callback, "test_sort_callback");
        lua_setglobal(L, "test_sort_callback");
    });
}

TEST_CASE("Table Sizing")
{
    runConformance("table_sizing.lua", nullptr, [](lua_State *L) {
        lua_pushcfunction(L, lua_table_sizes, "table_sizes");
        lua_setglobal(L, "table_sizes");
        lua_pushcfunction(L, [](lua_State *L) {lua_setmemcat(L, luaL_checkinteger(L, 1)); return 0;}, "change_memcat");
        lua_setglobal(L, "change_memcat");
    });
}

// Yieldable C function test using lyieldable.h framework.
// Takes a callback and a count n, calls callback(i) for i=1..n,
// accumulates return values and returns the sum.
DEFINE_YIELDABLE(test_yieldable_sum, 0)
{
    YIELDABLE_RETURNS_DEFAULT;

    // If you want to be able to yield at this function call level,
    // you need to define names for all the places you might yield.
    // So we know where to jump to when we resume.
    enum class Phase : uint8_t {
        DEFAULT = 0,
        CALL_CALLBACK = 1,
    };

    // All slots must be finalized before we do any init code.
    SlotManager slots(L, is_init);

    // Phase storage is explicit
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, i, 1);
    DEFINE_SLOT(int32_t, n, 0);
    DEFINE_SLOT(int32_t, accumulator, 0);

    slots.finalize();

    // Args always start at position 2.
    if (is_init)
    {
        luaL_checktype(L, 2, LUA_TFUNCTION);
        n = luaL_checkinteger(L, 3);
    }

    // Denotes where we can actually begin yielding. Any setup should
    // be done by this point
    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(CALL_CALLBACK);
    YIELD_DISPATCH_END();

    for (; i <= n; ++i)
    {
        // Push callback and argument
        lua_pushvalue(L, 2);
        lua_pushinteger(L, i);
        YIELD_CALL(L, 1, 1, CALL_CALLBACK);
        // Accumulate the return value
        accumulator += lua_tointeger(L, -1);
        lua_pop(L, 1);
    }

    lua_pushinteger(L, accumulator);
    return 1;
}

// Helper function with chained SlotManager. Calls callback(i) for
// i=start..start+count-1, accumulates results into the parent's accumulator
// slot (passed by reference).
static void test_yieldable_inner(lua_State* L, SlotManager& parentSlots, int32_t& accumulator)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        CALL_CALLBACK = 1,
    };

    SlotManager slots(parentSlots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, i, 0);
    DEFINE_SLOT(int32_t, end, 0);
    slots.finalize();

    if (slots.isInit())
    {
        // Args: [2: callback] [3: start] [4: count]
        i = luaL_checkinteger(L, 3);
        end = i + luaL_checkinteger(L, 4);
    }

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(CALL_CALLBACK);
    YIELD_DISPATCH_END();

    for (; i < end; ++i)
    {
        lua_pushvalue(L, 2);
        lua_pushinteger(L, i);
        YIELD_CALL(L, 1, 1, CALL_CALLBACK);
        accumulator += lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
}

// Outer yieldable function that delegates to test_yieldable_inner.
// Takes (callback, start, count) and returns accumulated sum.
// Calls callback(0) first (a yieldable call in the parent) before
// delegating to the helper — this proves that slots.isInit() in the
// helper returns true even when the parent has already yielded & resumed.
DEFINE_YIELDABLE(test_yieldable_chained, 0)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        PRE_CALL = 1,
        HELPER_CALL = 2,
    };

    SlotManager slots(L, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, accumulator, 0);
    slots.finalize();

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(PRE_CALL);
    YIELD_DISPATCH(HELPER_CALL);
    YIELD_DISPATCH_END();

    // Call callback(0) in the parent before entering the helper.
    // The result is added to accumulator, but callback(0) typically
    // returns 0 so it doesn't affect the sum.
    lua_pushvalue(L, 2);
    lua_pushinteger(L, 0);
    YIELD_CALL(L, 1, 1, PRE_CALL);
    accumulator += lua_tointeger(L, -1);
    lua_pop(L, 1);

    YIELD_HELPER(L, HELPER_CALL, test_yieldable_inner(L, slots, accumulator));

    lua_pushinteger(L, accumulator);
    return 1;
}

// Recursive helper: calls callback(n), accumulates result, then recurses with n-1.
// Stack layout: [1: nil/buf] [2: callback] [3: n]
static void test_yieldable_recursive_sum(lua_State* L, SlotManager& parentSlots, int32_t& accumulator)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        CALL_CALLBACK = 1,
        RECURSE = 2,
    };

    SlotManager slots(parentSlots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, n, 0);
    slots.finalize();

    if (slots.isInit())
    {
        n = luaL_checkinteger(L, 3);
    }

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(CALL_CALLBACK);
    YIELD_DISPATCH(RECURSE);
    YIELD_DISPATCH_END();

    if (n > 0)
    {
        // Call callback(n) — callback is at position 2
        lua_pushvalue(L, 2);
        lua_pushinteger(L, n);
        YIELD_CALL(L, 1, 1, CALL_CALLBACK);
        accumulator += lua_tointeger(L, -1);
        lua_pop(L, 1);

        // Recurse with n-1
        lua_pushinteger(L, n - 1);
        lua_replace(L, 3);
        YIELD_HELPER(L, RECURSE, test_yieldable_recursive_sum(L, slots, accumulator));
    }
}

// Outer yieldable: takes (callback, n), returns sum of callback(i) for i=n..1.
DEFINE_YIELDABLE(test_yieldable_recursive, 0)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        HELPER_CALL = 1,
    };

    SlotManager slots(L, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, accumulator, 0);
    slots.finalize();

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(HELPER_CALL);
    YIELD_DISPATCH_END();

    YIELD_HELPER(L, HELPER_CALL, test_yieldable_recursive_sum(L, slots, accumulator));

    lua_pushinteger(L, accumulator);
    return 1;
}

// Simple yieldable function that yields via YIELD_CHECK (interrupt-driven).
// Takes n, sums 1..n. No callback — yields happen purely from the interrupt handler.
DEFINE_YIELDABLE(test_yieldable_check_sum, 0)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        CHECK_POINT = 1,
    };

    SlotManager slots(L, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, i, 1);
    DEFINE_SLOT(int32_t, n, 0);
    DEFINE_SLOT(int32_t, accumulator, 0);
    slots.finalize();

    if (is_init)
        n = luaL_checkinteger(L, 2);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(CHECK_POINT);
    YIELD_DISPATCH_END();

    for (; i <= n; ++i)
    {
        YIELD_CHECK(L, CHECK_POINT, -100);
        accumulator += i;
    }

    lua_pushinteger(L, accumulator);
    return 1;
}

// Inner helper with child SlotManager that yields via YIELD_CHECK.
// Sums start..start+count-1 into parent's accumulator.
static void test_yieldable_inner_check(lua_State* L, SlotManager& parentSlots, int32_t& accumulator)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        CHECK_POINT = 1,
    };

    SlotManager slots(parentSlots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, i, 0);
    DEFINE_SLOT(int32_t, end, 0);
    slots.finalize();

    if (slots.isInit())
    {
        // No callback at position 2 (unlike YIELD_CALL version), so
        // start is at position 2, count at position 3
        i = luaL_checkinteger(L, 2);
        end = i + luaL_checkinteger(L, 3);
    }

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(CHECK_POINT);
    YIELD_DISPATCH_END();

    for (; i < end; ++i)
    {
        YIELD_CHECK(L, CHECK_POINT, -100);
        accumulator += i;
    }
}

// Outer function with parent YIELD_CHECK + YIELD_HELPER to child.
// Takes (start, count), returns sum of start..start+count-1.
DEFINE_YIELDABLE(test_yieldable_chained_check, 0)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        PRE_CHECK = 1,
        HELPER_CALL = 2,
    };

    SlotManager slots(L, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, accumulator, 0);
    slots.finalize();

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(PRE_CHECK);
    YIELD_DISPATCH(HELPER_CALL);
    YIELD_DISPATCH_END();

    YIELD_CHECK(L, PRE_CHECK, -100);

    YIELD_HELPER(L, HELPER_CALL, test_yieldable_inner_check(L, slots, accumulator));

    lua_pushinteger(L, accumulator);
    return 1;
}

TEST_CASE("Lyieldable")
{
    runConformance("lyieldable.luau", nullptr, [](lua_State* L) {
        lua_pushcclosurek(L, test_yieldable_sum_v0, "test_yieldable_sum", 0, test_yieldable_sum_v0_k);
        lua_setglobal(L, "yieldable_sum");

        lua_pushcclosurek(L, test_yieldable_chained_v0, "test_yieldable_chained", 0, test_yieldable_chained_v0_k);
        lua_setglobal(L, "yieldable_chained");

        lua_pushcclosurek(L, test_yieldable_recursive_v0, "test_yieldable_recursive", 0, test_yieldable_recursive_v0_k);
        lua_setglobal(L, "yieldable_recursive");
    });
}

static int checkYieldCount = 0;

TEST_CASE("LyieldableCheck")
{
    runConformance(
        "lyieldable_check.luau",
        nullptr,
        [](lua_State* L)
        {
            lua_pushcclosurek(L, test_yieldable_check_sum_v0, "test_yieldable_check_sum", 0, test_yieldable_check_sum_v0_k);
            lua_setglobal(L, "yieldable_check_sum");

            lua_pushcclosurek(L, test_yieldable_chained_check_v0, "test_yieldable_chained_check", 0, test_yieldable_chained_check_v0_k);
            lua_setglobal(L, "yieldable_chained_check");

            // clear_check_count() — resets the yield counter
            lua_pushcfunction(
                L,
                [](lua_State* L) -> int
                {
                    checkYieldCount = 0;
                    return 0;
                },
                "clear_check_count"
            );
            lua_setglobal(L, "clear_check_count");

            // get_check_count() — returns the yield counter
            lua_pushcfunction(
                L,
                [](lua_State* L) -> int
                {
                    lua_pushinteger(L, checkYieldCount);
                    return 1;
                },
                "get_check_count"
            );
            lua_setglobal(L, "get_check_count");

            // Install interrupt that yields on every YIELD_CHECK hit
            lua_callbacks(L)->interrupt = [](lua_State* L, int gc)
            {
                if (gc != -100)
                    return;
                checkYieldCount++;
                lua_yield(L, 0);
            };
        }
    );
}

TEST_SUITE_END();
