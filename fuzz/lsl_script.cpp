// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
// ServerLua: LSL script fuzzer - tests the full LSL compilation and execution pipeline
#include "lua.h"
#include "lualib.h"

#include "Luau/BytecodeBuilder.h"
#include "Luau/LSLCompiler.h"

#include <string>

static lua_State* GL = nullptr;
static lua_SLRuntimeState lsl_state;

static uint64_t startTsc = 0;
// ~10ms at 3GHz = ~30M cycles.
static const uint64_t kTscLimit = 30000000;
static const size_t kMemoryLimit = 256 * 1024; // 256KB

// Read timestamp counter (low overhead)
static inline uint64_t rdtsc()
{
#if defined(__x86_64__) || defined(_M_X64)
    return __rdtsc();
#elif defined(__aarch64__)
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
    return 0;
#endif
}

// Time limit interrupt callback using rdtsc
static void interruptCallback(lua_State* L, int gc)
{
    if (gc >= 0)
        return;
    if (rdtsc() - startTsc > kTscLimit)
    {
        lua_checkstack(L, 1);
        luaL_error(L, "execution timed out");
    }
}

// Track approximate memory usage, only recalc when approaching limit
static size_t approxMemoryUsage = 0;

// Memory limit callback - only recalc the user thread size when needed
static int memoryLimitCallback(lua_State* L, size_t osize, size_t nsize)
{
    if (osize >= nsize)
        return 0; // Allow shrinks

    size_t delta = nsize - osize;

    // Only recalculate actual size when we're getting close to the limit
    if (approxMemoryUsage + delta > kMemoryLimit)
    {
        // This is fine, GL won't be included but it will descend to `L` eventually.
        // Naturally, this would be an issue if we _actually_ had multiple different
        // scripts within the VM.
        approxMemoryUsage = lua_userthreadgc(lua_mainthread(L), nullptr);

        if (approxMemoryUsage + delta > kMemoryLimit)
            return 1; // Reject allocation
    }

    approxMemoryUsage += delta;
    return 0;
}

static void initLuaState()
{
    if (GL)
        return;

    GL = luaL_newstate();
    luaL_openlibs(GL);

    lsl_state.slIdentifier = LUA_LSL_IDENTIFIER;
    lua_setthreaddata(GL, &lsl_state);

    // Propagate thread data to new threads
    lua_callbacks(GL)->userthread = [](lua_State* LP, lua_State* L) {
        if (!LP)
            return;
        lua_setthreaddata(L, lua_getthreaddata(LP));
    };

    // Register SL types (UUID metatables, quaternion metatables, weak tables)
    luaopen_sl(GL, true);

    // Register LSL builtins
    luaopen_lsl(GL);
    lua_pop(GL, 1);

    // Set up callbacks
    lua_callbacks(GL)->interrupt = interruptCallback;
    lua_callbacks(GL)->beforeallocate = memoryLimitCallback;

    // Protect core libraries and metatables from modification
    luaL_sandbox(GL);
    lua_fixallcollectable(GL);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    initLuaState();

    // Try to compile LSL source
    std::string source((const char*)Data, Size);
    Luau::BytecodeBuilder bcb;
    try
    {
        compileLSLOrThrow(bcb, source);
    }
    catch (...)
    {
        // Compilation failed - expected for fuzz inputs
        return 0;
    }

    int baseTop = lua_gettop(GL);

    // Reset memory tracking for this test
    approxMemoryUsage = 0;

    // Make sure our thread is created in a user memcat
    lua_setmemcat(GL, LUA_FIRST_USER_MEMCAT);
    lua_State* L = lua_newthread(GL);
    lua_setmemcat(GL, 0);

    luaL_sandboxthread(L);

    std::string bytecode = bcb.getBytecode();

    int result = luau_load(L, "=fuzz", bytecode.c_str(), bytecode.length(), 0);
    if (result != 0)
    {
        lua_pop(GL, 1);
        lua_gc(GL, LUA_GCCOLLECT, 0);
        LUAU_ASSERT(lua_gettop(GL) == baseTop);
        return 0;
    }

    // Set execution start time
    startTsc = rdtsc();

    lua_getglobal(L, "_e0/state_entry");

    // Run script
    int status = lua_resume(L, nullptr, 0);
    while (status == LUA_YIELD || status == LUA_BREAK)
    {
        if (rdtsc() - startTsc > kTscLimit)
            break;
        status = lua_resume(L, nullptr, 0);
    }

    // Clean up
    lua_pop(GL, 1);
    lua_gc(GL, LUA_GCCOLLECT, 0);

    // Verify stack is balanced
    LUAU_ASSERT(lua_gettop(GL) == baseTop);

    return 0;
}
