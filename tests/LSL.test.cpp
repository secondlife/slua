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
#include "../Compiler/src/BuiltinFolding.h"
#include "llsl.h"
#include "../VM/src/lstate.h"
#include "../VM/src/mono_strings.h"
#include "../VM/src/lapi.h"
#include "Luau/LSLCompiler.h"

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


static int yielding_print(lua_State* L)
{
    assert(lua_gettop(L) == 1);
    luaL_checkany(L, 1);
    return lua_yield(L, 1);
}

using StateRef = std::unique_ptr<lua_State, void (*)(lua_State*)>;

typedef struct LSLTestRuntimeState : lua_SLRuntimeState
{
    int yield_num = 0;
} RuntimeState;

static void lua_test_state_closer(lua_State *L) {
    if (L->userdata)
        delete (RuntimeState *)L->userdata;
    L->userdata = nullptr;
    auto *GL = L->global->constsstate;
    lua_close(L);
    if (GL)
        lua_test_state_closer(GL);
}

static void checkStatus(lua_State *L, int status)
{
    if (status != 0)
    {
        std::string error;
        if (status == LUA_YIELD)
        {
            error = "thread yielded unexpectedly";
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

static StateRef runConformance(const char* name, int32_t (*yield)(lua_State* L) = nullptr, void (*setup)(lua_State* L) = nullptr, lua_CompileOptions* options = nullptr)
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

    std::fstream stream(path, std::ios::in | std::ios::binary);
    INFO(path);
    REQUIRE(stream);

    std::string source(std::istreambuf_iterator<char>(stream), {});

    stream.close();

    StateRef globalState(luaL_newstate(), lua_test_state_closer);
    lua_State* L = globalState.get();

    auto *runtime_state = new RuntimeState;
    L->userdata = runtime_state;

    luaL_openlibs(L);
    luaopen_sl(L);
    luaopen_lsl(L);
    lua_pop(L, 1);
    luaopen_ll(L, true);
    lua_pop(L, 1);

    // Register a few global functions for conformance tests
    std::vector<luaL_Reg> funcs = {
        // Use print() as a bespoke yield-to-interpreter call
        {"print", yielding_print}
    };

    // "null" terminate the list of functions to register
    funcs.push_back({nullptr, nullptr});

    lua_pushvalue(L, LUA_GLOBALSINDEX);
    luaL_register(L, nullptr, funcs.data());
    lua_pop(L, 1);

    // In some configurations we have a larger C stack consumption which trips some conformance tests
#if defined(LUAU_ENABLE_ASAN) || defined(_NOOPT) || defined(_DEBUG)
    lua_pushboolean(L, true);
    lua_setglobal(L, "limitedstack");
#endif

    // Thread should be pristine
    LUAU_ASSERT(lua_gettop(L) == 0);

    // Extra test-specific setup
    if (setup)
        setup(L);

    // Protect core libraries and metatables from modification
    luaL_sandbox(L);

    lua_fixallcollectable(L);

    // Create a new writable global table for current thread
    luaL_sandboxthread(L);

    std::string chunkname = "=" + std::string(name);

    Luau::BytecodeBuilder bcb;
    compileLSLOrThrow(bcb, source);
    std::string bytecode = bcb.getBytecode();

    int result = luau_load(L, chunkname.c_str(), bytecode.c_str(), bytecode.length(), 0);

    int status = (result == 0) ? lua_resume(L, nullptr, 0) : LUA_ERRSYNTAX;

    // Run the script "constructor" (main function)
    while (status == LUA_YIELD || status == LUA_BREAK)
    {
        status = lua_resume(L, nullptr, 0);
    }

    checkStatus(L, status);

    // Now run the default state's state_entry if that succeeded
    // We create a new thread for this so we can tear it down if we
    // need to switch states without killing the current script
    auto *Lhandler = lua_newthread(L);
    lua_setthreaddata(Lhandler, lua_getthreaddata(L));
    lua_getglobal(Lhandler, "_e0/state_entry");
    status = lua_resume(Lhandler, nullptr, 0);

    int state = 0;
    while (yield && (status == LUA_YIELD || status == LUA_BREAK))
    {
        int next_state = yield(Lhandler);
        if (next_state != -1 && state != next_state)
        {
            state = next_state;
            // State changed, destroy the old handler thread and create a new one calling
            // the new state's state_entry(). Note that this isn't strictly correct, since
            // we don't call state_exit(), but whatever.
            lua_pop(L, 1);
            Lhandler = lua_newthread(L);
            lua_setthreaddata(Lhandler, lua_getthreaddata(L));
            lua_getglobal(Lhandler, (std::string("_e") + std::to_string(state) + std::string("/state_entry")).c_str());
        }
        else
        {
            // Keep track of how many times we've yielded so we can have "stepped" yield functions
            ++runtime_state->yield_num;
        }
        status = lua_resume(Lhandler, nullptr, 0);
    }

    extern void luaC_validate(lua_State * L); // internal function, declared in lgc.h - not exposed via lua.h
    luaC_validate(L);

    if (status == 0)
    {
        // Do nothing for now.
    }
    else
    {
        checkStatus(Lhandler, status);
    }

    return globalState;
}

TEST_SUITE_BEGIN("LSLConformance");

// Figure out which yield number this is for the current testcase
#define YIELD_NUM(L) (((RuntimeState *)(L)->userdata)->yield_num)

TEST_CASE("Cast")
{
    auto state = runConformance("cast.lsl", [](lua_State *L)
    {
        CHECK_EQ(luaL_checkstring(L, 1), std::string("5.000000hello!"));
        return -1;
    });
    CHECK_EQ(YIELD_NUM(state.get()), 1);
}

TEST_CASE("PostPreIncrDecr")
{
    auto state = runConformance("incr_decr.lsl", [](lua_State *L)
    {
        double expected;
        switch(YIELD_NUM(L))
        {
            case 0: expected = 2.0; break;
            case 1: expected = 2.0; break;
            case 2: expected = 1.0; break;
            case 3: expected = 3.0; break;
            case 4: expected = 2.0; break;
            case 5: expected = 2.0; break;
            case 6: expected = 1.0; break;
            case 7: expected = 3.0; break;
            default: assert(0);
        }
        CHECK_EQ(luaL_checknumber(L, 1), expected);
        return -1;
    });
    CHECK_EQ(YIELD_NUM(state.get()), 8);
}

TEST_CASE("LValue Assignment")
{
    auto state = runConformance("lval_to_lval.lsl", [](lua_State *L)
    {
            double expected;
            switch(YIELD_NUM(L))
            {
                case 0: expected = 2.0; break;
                case 1: expected = 4.0; break;
                case 2: expected = 4.0; break;
                case 3: expected = 6.0; break;
                case 4: expected = 2.0; break;
                case 5: expected = 6.0; break;
                case 6: expected = 2.0; break;
                case 7: expected = 10.0; break;
                case 8: expected = 10.0; break;
                default: assert(0);
            }
            CHECK_EQ(luaL_checknumber(L, 1), expected);
            return -1;
    });
    CHECK_EQ(YIELD_NUM(state.get()), 9);
}

TEST_CASE("Builtin function calls")
{
    auto state = runConformance("builtin.lsl", [](lua_State *L)
    {
        CHECK_EQ(std::string(luaL_checkstring(L, 1)), std::string("foobarbaz"));
        return -1;
    });
    CHECK_EQ(YIELD_NUM(state.get()), 1);
}

TEST_CASE("Addition")
{
    auto state = runConformance("add.lsl", [](lua_State *L)
    {
        if (YIELD_NUM(L) == 0)
            CHECK_EQ(luaL_checknumber(L, 1), 3.0);
        else
            CHECK_EQ(std::string(luaL_checkstring(L, 1)), std::string("foobar"));
        return -1;
    });
    CHECK_EQ(YIELD_NUM(state.get()), 2);
}

TEST_CASE("List Compare")
{
    auto state = runConformance("list_compare.lsl", [](lua_State *L)
    {
        int32_t actual = luaL_checkinteger(L, 1);
        int32_t expected = 0;
        switch(YIELD_NUM(L))
        {
            case 0: expected = 2; break;
            case 1: expected = -2; break;
            case 2: expected = 3; break;
            case 3: expected = 0; break;
            case 4: expected = 1; break;
            case 5: expected = 2; break;
            default: assert(0);
        }
        CHECK_EQ(actual, expected);
        return -1;
    });
    CHECK_EQ(YIELD_NUM(state.get()), 6);
}

TEST_CASE("Boolean And Or")
{
    auto state = runConformance("boolean_and_or.lsl", [](lua_State *L)
        {
            CHECK_EQ(lua_type(L, 1), LUA_TLIGHTUSERDATA);
            int32_t actual = luaL_checkinteger(L, 1);
            int32_t expected = 0;
            switch(YIELD_NUM(L))
            {
                case 0: expected = 0; break;
                case 1: expected = 0; break;
                case 2: expected = 0; break;
                case 3: expected = 1; break;
                case 4: expected = 1; break;
                case 5: expected = 1; break;
                default: assert(0);
            }
            CHECK_EQ(actual, expected);
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 6);
}

TEST_CASE("List Concat")
{
    auto state = runConformance("list_concat.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "123456"; break;
                case 1: expected = "123foobar"; break;
                case 2: expected = "foobar123"; break;
                default: assert(0);
            }
            CHECK_EQ(actual, std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 3);
}

TEST_CASE("Vector Cast")
{
    auto state = runConformance("vector_cast.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "<1.00000, 2.00000, 3.00000>"; break;
                case 1: expected = "<1.000000, 2.000000, 3.000000>"; break;
                default: assert(0);
            }
            CHECK_EQ(actual, std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 2);
}

TEST_CASE("Bitwise")
{
    auto state = runConformance("bitwise.lsl", [](lua_State *L)
        {
            int32_t actual = luaL_checkinteger(L, 1);
            int32_t expected = 0;
            switch(YIELD_NUM(L))
            {
                case 0: expected = 2; break;
                case 1: expected = 0xFFaaAAaa; break;
                case 2: expected = 0x000000F0; break;
                case 3: expected = 2; break;
                case 4: expected = 3; break;
                case 5: expected = 6; break;
                case 6: expected = 0xF; break;
                default: assert(0);
            }
            CHECK_EQ(actual, expected);
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 7);
}

TEST_CASE("Comparison Operators")
{
    auto state = runConformance("comparison.lsl", [](lua_State *L)
        {
            CHECK_EQ(lua_type(L, 1), LUA_TLIGHTUSERDATA);
            int32_t actual = luaL_checkinteger(L, 1);
            int32_t expected = 0;
            switch(YIELD_NUM(L))
            {
                case 0: expected = 1; break;
                case 1: expected = 0; break;
                case 2: expected = 1; break;
                case 3: expected = 1; break;
                case 4: expected = 0; break;
                case 5: expected = 0; break;
                case 6: expected = 1; break;
                case 7: expected = 1; break;
                case 8: expected = 1; break;
                default: assert(0);
            }
            CHECK_EQ(actual, expected);
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 9);
}

TEST_CASE("If Statement")
{
    auto state = runConformance("if.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "yes"; break;
                case 1: expected = "no"; break;
                case 2: expected = "yes"; break;
                default: assert(0);
            }
            CHECK_EQ(std::string(actual), std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 3);
}

TEST_CASE("For statement")
{
    auto state = runConformance("for.lsl", [](lua_State *L)
        {
            CHECK_EQ(lua_type(L, 1), LUA_TLIGHTUSERDATA);
            int32_t actual = luaL_checkinteger(L, 1);
            int32_t expected = 0;
            switch(YIELD_NUM(L))
            {
                case 0: expected = 0; break;
                case 1: expected = 1; break;
                case 2: expected = 2; break;
                default: assert(0);
            }
            CHECK_EQ(actual, expected);
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 3);
}

TEST_CASE("Do while statement")
{
    auto state = runConformance("do_while.lsl", [](lua_State *L)
        {
            int32_t actual = luaL_checkinteger(L, 1);
            int32_t expected = 0;
            switch(YIELD_NUM(L))
            {
                case 0: expected = 0; break;
                case 1: expected = 1; break;
                case 2: expected = 2; break;
                default: assert(0);
            }
            CHECK_EQ(actual, expected);
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 3);
}

TEST_CASE("While statement")
{
    auto state = runConformance("while.lsl", [](lua_State *L)
        {
            int32_t actual = luaL_checkinteger(L, 1);
            int32_t expected = 0;
            switch(YIELD_NUM(L))
            {
                case 0: expected = 1; break;
                case 1: expected = 2; break;
                default: assert(0);
            }
            CHECK_EQ(actual, expected);
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 2);
}

TEST_CASE("Mandelbrot bench")
{
    auto state = runConformance("mandelbrot.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "P4"; break;
                case 1: expected = "10 10"; break;
                case 2: expected = "0x00000100078007C03FC0FF803FC007C007800100"; break;
                default: assert(0);
            }
            CHECK_EQ(std::string(actual), std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 3);
}

TEST_CASE("Jumps")
{
    auto state = runConformance("jump.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "yay!"; break;
                case 1: expected = "2"; break;
                default: assert(0);
            }
            CHECK_EQ(actual, std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 2);
}

TEST_CASE("Vectors")
{
    auto state = runConformance("vectors.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "<1.00000, 2.00000, 3.00000>"; break;
                default: assert(0);
            }
            CHECK_EQ(actual, std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 1);
}

TEST_CASE("Quaternions")
{
    auto state = runConformance("quaternions.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "<1.00000, 2.00000, 3.00000, 4.00000>"; break;
                case 1: expected = "<1.00000, 2.00000, 3.00000, 4.00000>"; break;
                default: assert(0);
            }
            CHECK_EQ(actual, std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 2);
}

TEST_CASE("Accessors")
{
    auto state = runConformance("accessors.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "3.000000"; break;
                case 1: expected = "4.000000"; break;
                default: assert(0);
            }
            CHECK_EQ(actual, std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 2);
}

TEST_CASE("Mutate Accessors")
{
    auto state = runConformance("mutate_accessors.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "5.000000"; break;
                case 1: expected = "<5.00000, 2.00000, 3.00000>"; break;
                case 2: expected = "8.000000"; break;
                case 3: expected = "<1.00000, 2.00000, 3.00000, 8.00000>"; break;
                default: assert(0);
            }
            CHECK_EQ(actual, std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 4);
}

TEST_CASE("Accessor incr / decr")
{
    auto state = runConformance("coord_incr.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "2.000000"; break;
                case 1: expected = "3.000000"; break;
                case 2: expected = "<2.00000, 2.00000, 4.00000>"; break;
                case 3: expected = "2.000000"; break;
                case 4: expected = "3.000000"; break;
                case 5: expected = "<2.00000, 2.00000, 4.00000>"; break;
                default: assert(0);
            }
            CHECK_EQ(actual, std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 6);
}

TEST_CASE("Float precision")
{
    runConformance("float_precision.lsl");
}

TEST_CASE("List operations")
{
    runConformance("list_operations.lsl");
}

TEST_CASE("Unicode")
{
    runConformance("unicode.lsl");
}


TEST_CASE("CheckSLHelpers")
{
    auto state = runConformance("check_sl_helpers.lsl", [](lua_State *L)
        {
            bool compressed = false;
            switch(YIELD_NUM(L))
            {
                case 0: CHECK_EQ(luaL_checkunsigned(L, 1), 2); break;
                case 1:
                    CHECK_EQ(luaSL_checkuuid(L, 1, &compressed), std::string("str_as_key"));
                    CHECK(!compressed);
                    break;
                case 2:
                    CHECK_EQ(luaSL_checkuuid(L, 1, &compressed), std::string("key_as_key"));
                    CHECK(!compressed);
                    break;
                case 3: {
                    const float *quat = luaSL_checkquaternion(L, 1);
                    float expected[] = {1.0f, 2.0f, 3.0f, 4.0f};
                    CHECK(!memcmp((const void *)quat, (const void *)&expected, sizeof(expected)));
                    break;
                }
                case 4: CHECK_EQ(luaL_checkunsigned(L, 1), 1); break;
                case 5: CHECK_EQ(luaL_checkunsigned(L, 1), 0); break;
                case 6: CHECK_EQ(luaL_checkunsigned(L, 1), 1); break;
                case 7: CHECK_EQ(luaL_checkunsigned(L, 1), 0); break;
                case 8: CHECK_EQ(luaL_checkunsigned(L, 1), 1); break;
                case 9: CHECK_EQ(luaL_checkunsigned(L, 1), 0); break;
                case 10:
                    CHECK_EQ(luaSL_checkuuid(L, 1, &compressed), std::string("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01"));
                    CHECK(compressed);
                    break;
                case 11:
                    // We can't treat this as compressed because it's actually a string
                    CHECK_EQ(luaSL_checkuuid(L, 1, &compressed), std::string("00000000-0000-0000-0000-000000000001"));
                    CHECK(!compressed);
                    break;
                default: assert(0);
            }
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 12);
}

TEST_CASE("Conformance Test 1")
{
    auto state = runConformance("conformance1.lsl", [](lua_State *L)
        {
            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "All tests passed"; break;
                default: assert(0);
            }
            CHECK_EQ(actual, std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 1);
}

TEST_CASE("Conformance Test 2")
{
    auto state = runConformance("conformance2.lsl", [](lua_State *L)
        {
            // Probably a state change
            if (lua_type(L, 1) == LUA_TNUMBER)
            {
                return luaL_checkinteger(L, 1);
            }

            const char *actual = luaL_checkstring(L, 1);
            const char *expected = nullptr;
            switch(YIELD_NUM(L))
            {
                case 0: expected = "Test succeeded"; break;
                default: assert(0);
            }
            CHECK_EQ(std::string(actual), std::string(expected));
            return -1;
        });
    CHECK_EQ(YIELD_NUM(state.get()), 1);
}

static int sInjectedYields = 0;
static int sYieldAttempts = 0;

TEST_CASE("Conformance Test 2 ForceYield")
{
    runConformance("conformance2.lsl", [](lua_State *L)
        {
            // Yielded nothing, this must be a yield forced by
            // the scheduler via interrupt.
            if (lua_gettop(L) == 0)
                return -1;

            // Probably a state change
            if (lua_type(L, 1) == LUA_TNUMBER)
            {
                return luaL_checkinteger(L, 1);
            }

            const std::string actual = luaL_checkstring(L, 1);
            const char *expected = "Test succeeded";
            CHECK_EQ(std::string(actual), std::string(expected));
            return -1;
        }, [](lua_State *L) {
            lua_callbacks(L)->interrupt = [](lua_State* L, int gc)
            {
                // We're only interrupting for GC, not interesting.
                if (gc >= 0)
                    return;
                if (luaSL_may_interrupt(L) != YieldableStatus::OK)
                    return;

                ++sYieldAttempts;
                // Only do this every other chance
                if (sYieldAttempts % 2)
                {
                    // inject a yield so we can resume later.
                    lua_yield(L, 0);
                    ++sInjectedYields;
                }
            };
        });
    CHECK(sInjectedYields > 20);
}

static bool sAresDidSucceed = false;

#define assertHasPrintGlobal(_L)     lua_getglobal((_L), "print"); \
    LUAU_ASSERT(lua_isfunction((_L), -1)); \
    lua_pop((_L), 1);

LuaTable *eris_getglobalsbase(lua_State *L);

static void userthread_callback(lua_State *LP, lua_State *L)
{
    if (LP == nullptr)
        return;
    L->userdata = LP->userdata;
}

void runAresYieldTest(const std::string &name)
{
    sAresDidSucceed = false;
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

    std::fstream stream(path, std::ios::in | std::ios::binary);
    INFO(path);
    REQUIRE(stream);

    std::string source(std::istreambuf_iterator<char>(stream), {});

    stream.close();

    StateRef globalState(luaL_newstate(), lua_test_state_closer);
    lua_State* GL = globalState.get();

    // Store some data so we know which yield number we're on
    RuntimeState *runtime_state = new RuntimeState;
    GL->userdata = runtime_state;

    lua_callbacks(GL)->userthread = userthread_callback;

    luaL_openlibs(GL);
    luaopen_sl(GL);
    luaopen_lsl(GL);
    lua_pop(GL, 1);
    luaopen_ll(GL, true);
    lua_pop(GL, 1);

    // Register a few global functions for conformance tests
    std::vector<luaL_Reg> funcs = {
        // Use print() as a bespoke yield-to-interpreter call
        {"print", yielding_print}
    };

    // "null" terminate the list of functions to register
    funcs.push_back({nullptr, nullptr});

    lua_pushvalue(GL, LUA_GLOBALSINDEX);
    luaL_register(GL, nullptr, funcs.data());
    lua_pop(GL, 1);

    // Done writing to globals
    luaL_sandbox(GL);

    eris_register_perms(GL, true);
    eris_register_perms(GL, false);

    lua_fixallcollectable(GL);

    assertHasPrintGlobal(GL);

    // Thread should be pristine
    LUAU_ASSERT(lua_gettop(GL) == 0);

    // Spawn a new thread to load our script into
    // TODO: remove `getfenv()` and `setfenv()` to preserve our sanity.
    lua_State* L = lua_newthread(GL);
    luaL_sandboxthread(L);

    assertHasPrintGlobal(L);

    std::string chunkname = "=" + std::string(name);

    Luau::BytecodeBuilder bcb;
    compileLSLOrThrow(bcb, source);
    std::string bytecode = bcb.getBytecode();

    int result = luau_load(L, chunkname.c_str(), bytecode.c_str(), bytecode.length(), 0);

    REQUIRE(result == 0);

    lua_State *Lforker = eris_make_forkserver(L);
    // Don't need the original thread on the main stack anymore
    lua_remove(GL, -2);

    assertHasPrintGlobal(Lforker);

    lua_State *Lchild = eris_fork_thread(Lforker, true, 2);

    CHECK_EQ(eris_getglobalsbase(Lchild), GL->gt);
    assertHasPrintGlobal(GL);
    assertHasPrintGlobal(Lforker);
    assertHasPrintGlobal(Lchild);

    // Run the script "constructor" / set the global variables.
    int status;
    do
    {
        status = lua_resume(Lchild, nullptr, 0);
    } while (status == LUA_YIELD || status == LUA_BREAK);

    checkStatus(Lchild, status);

    // Run the default state_entry next.
    auto *Lhandler = lua_newthread(Lchild);
    lua_getglobal(Lhandler, "_e0/state_entry");
    LUAU_ASSERT(lua_type(Lhandler, -1) == LUA_TFUNCTION);

    checkStatus(Lhandler, lua_status(Lhandler));

    assertHasPrintGlobal(Lhandler);

    extern void luaC_validate(lua_State * L); // internal function, declared in lgc.h - not exposed via lua.h
    do
    {
        status = lua_resume(Lhandler, nullptr, 0);
        luaC_validate(Lhandler);
        if (status == LUA_YIELD)
        {
            std::string actual = luaL_checkstring(Lhandler, 1);
            // If the yield was anything other than test success or an explicit "yield"
            // something is very wrong.
            if (actual == "Test succeeded")
            {
                sAresDidSucceed = true;
            }
            else
            {
                CHECK_EQ(actual, "yield");
            }

            int old_handler_depth = lua_stackdepth(Lhandler);

            // serialized state string is now on the Lforker stack
            eris_serialize_thread(Lforker, Lchild);

            // Pop a copy of the script state off the end of the stack
            size_t serialized_state_size;
            const char *serialized_state_data = luaL_checklstring(Lforker, -1, &serialized_state_size);
            std::string serialized_state {serialized_state_data, serialized_state_size};
            // Pop it so we can simulate re-adding it
            lua_pop(Lforker, 1);

            // Pop the old thread off the main stack
            lua_pop(GL, 1);

            // Do a full GC to keep us honest
            lua_gc(Lforker, LUA_GCCOLLECT, 0);

            // Put the script state back on the forker thread
            lua_pushlstring(Lforker, serialized_state.c_str(), serialized_state_size);

            // spawn a new thread from the serialized version still on the stack
            Lchild = eris_fork_thread(Lforker, false, 2);
            if (Lchild == nullptr) {
                fprintf(stderr, "Failed to spawn child: %s\n", lua_tostring(Lforker, -1));
                LUAU_ASSERT(false);
            }
            // thread is now on the end of GL's stack
            Lhandler = lua_tothread(Lchild, 1);

            // Would be nice to look at `gettop()` too, but gettop()
            // is expected to return `0` in `old` because it's in a yield
            // handler where nothing was yielded.
            LUAU_ASSERT(lua_stackdepth(Lhandler) == old_handler_depth);

            // Check that the GC state is still sane
            luaC_validate(Lchild);
            luaC_validate(Lforker);
        }
        else if (status == LUA_ERRRUN)
        {
            checkStatus(Lhandler, status);
        }
        else
        {
            REQUIRE_EQ(status, LUA_OK);
        }
    } while(status == LUA_YIELD);

    luaC_validate(L);
    CHECK_EQ(sAresDidSucceed, true);
}

TEST_CASE("Simple LSL Ares Test")
{
    runAresYieldTest("simple_ares.lsl");
}

TEST_CASE("Simple LSL Key Serialization")
{
    runAresYieldTest("ares_type.lsl");
}

TEST_CASE("Extensive LSL Ares Test")
{
    runAresYieldTest("conformance2_yield.lsl");
}


static std::string mono_to_lower_string(const std::string &str)
{
    return to_lower_mono(str.c_str(), str.length());
}

static std::string mono_to_upper_string(const std::string &str)
{
    return to_upper_mono(str.c_str(), str.length());
}

TEST_CASE("Mono Strings")
{
    // upper end of BMP without case change (3-byte)
    CHECK_EQ("\uffed", mono_to_lower_string("\uffed"));
    CHECK_EQ("\uffed", mono_to_upper_string("\uffed"));

    // four-byte UTF-8 test
    CHECK_EQ("\xF0\x90\x8D\x8A", mono_to_lower_string("\xF0\x90\x8D\x8A"));
    CHECK_EQ("\xF0\x90\x8D\x8A", mono_to_upper_string("\xF0\x90\x8D\x8A"));

    // Two-byte UTF-8
    CHECK_EQ("ü", mono_to_lower_string("Ü"));
    CHECK_EQ("Ü", mono_to_upper_string("ü"));

    // fullwidth A <-> fullwidth a, upper end of BMP (3-byte)
    CHECK_EQ("\uff41", mono_to_lower_string("\uff21"));
    CHECK_EQ("\uff21", mono_to_upper_string("\uff41"));

    // Truncated UTF-8
    CHECK_EQ("?", mono_to_upper_string("\xF0\x90\x8D"));
    CHECK_EQ("?", mono_to_lower_string("\xF0\x90\x8D"));

    // Embedded null
    // TODO: is this behavior actually desired?
    CHECK_EQ("<>", mono_to_upper_string(std::string("<\x00>", 3)));
    CHECK_EQ("<>", mono_to_lower_string(std::string("<\x00>", 3)));
}

TEST_SUITE_END();
