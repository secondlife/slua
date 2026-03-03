// ServerLua: gsub comparison fuzzer.
// Verifies that the yieldable gsub produces identical results to the
// base (non-yieldable) gsub for string-only arguments. The yieldable
// version is exercised in a coroutine that lua_break's at every
// interrupt opportunity.
#include "lua.h"
#include "lualib.h"
#include "Luau/Common.h"

#include <chrono>
#include <cstdio>
#include <fuzzer/FuzzedDataProvider.h>

static lua_State* LP = nullptr;
static lua_State* LYield = nullptr;

// Stack layout after init:
//   [1] = base gsub (non-yieldable)
//   [2] = yieldable gsub (with continuation)
//   [3] = LYield (thread reference, prevents GC)
static constexpr int BASE = 3;

static constexpr size_t kMaxAlloc = 8 * 1024 * 1024;
static bool runningYieldable = false;

static std::chrono::time_point<std::chrono::system_clock> deadline;
static std::chrono::time_point<std::chrono::system_clock> yieldable_deadline;

static int memoryLimitCallback(lua_State* L, size_t osize, size_t nsize)
{
    if (nsize > kMaxAlloc)
    {
        LUAU_ASSERT(!runningYieldable && "yieldable gsub exceeded allocation limit");
        return 1;
    }
    return 0;
}

// Base interrupt: error on timeout only.
static void base_interrupt(lua_State* L, int gc)
{
    if (gc >= 0)
        return;
    if (std::chrono::system_clock::now() > deadline)
    {
        lua_checkstack(L, 1);
        luaL_error(L, "timeout");
    }
}

// Yieldable interrupt: lua_break at every opportunity until deadline.
static void yieldable_interrupt(lua_State* L, int gc)
{
    if (gc >= 0)
        return;
    if (std::chrono::system_clock::now() < yieldable_deadline)
        lua_break(L);
}

static void initLuaState()
{
    if (LP)
        return;

    LP = luaL_newstate();
    luaL_openlibs(LP);

    lua_callbacks(LP)->beforeallocate = memoryLimitCallback;

    // Build a table with base (non-yieldable) pattern-matching functions.
    lua_newtable(LP);
    luaopen_string_base(LP);
    lua_getfield(LP, -1, "gsub");
    lua_remove(LP, -2);
    // Stack: [1] = base gsub

    // Get the yieldable gsub from the string library.
    lua_getglobal(LP, "string");
    lua_getfield(LP, -1, "gsub");
    lua_remove(LP, -2);
    // Stack: [1] = base gsub, [2] = yieldable gsub

    // Create a reusable coroutine thread.
    LYield = lua_newthread(LP);
    // Stack: [1] = base gsub, [2] = yieldable gsub, [3] = thread
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    initLuaState();

    FuzzedDataProvider fdp(Data, Size);
    std::string source = fdp.ConsumeRandomLengthString();
    std::string pattern = fdp.ConsumeRandomLengthString();
    std::string replacement = fdp.ConsumeRemainingBytesAsString();

    // --- Run base gsub with 50ms timeout ---
    lua_callbacks(LP)->interrupt = base_interrupt;
    deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(50);

    lua_pushvalue(LP, 1);
    lua_pushlstring(LP, source.data(), source.size());
    lua_pushlstring(LP, pattern.data(), pattern.size());
    lua_pushlstring(LP, replacement.data(), replacement.size());
    int base_status = lua_pcall(LP, 3, 2, 0);

    if (base_status != 0)
    {
        // Timeout or pattern error — skip this input.
        lua_settop(LP, BASE);
        return 0;
    }

    // Save base results.
    size_t base_result_len;
    const char* base_result = lua_tolstring(LP, BASE + 1, &base_result_len);
    int base_count = lua_tointeger(LP, BASE + 2);

    // --- Run yieldable gsub in coroutine with lua_break at every interrupt ---
    runningYieldable = true;
    yieldable_deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(50);
    lua_callbacks(LP)->interrupt = yieldable_interrupt;

    lua_resetthread(LYield);
    lua_pushvalue(LP, 2);
    lua_xmove(LP, LYield, 1);
    lua_pushlstring(LYield, source.data(), source.size());
    lua_pushlstring(LYield, pattern.data(), pattern.size());
    lua_pushlstring(LYield, replacement.data(), replacement.size());

    int status = lua_resume(LYield, nullptr, 3);
    while (status == LUA_BREAK || status == LUA_YIELD)
        status = lua_resume(LYield, nullptr, 0);

    // Yieldable must succeed since base succeeded.
    if (status != LUA_OK)
    {
        const char* err = lua_tostring(LYield, -1);
        fprintf(stderr, "MISMATCH: base succeeded, yieldable failed (status=%d)\n", status);
        fprintf(stderr, "  error: %s\n", err ? err : "(no message)");
        fprintf(stderr, "  source[%zu]: ", source.size());
        for (size_t i = 0; i < source.size(); i++)
            fprintf(stderr, "%02x ", (unsigned char)source[i]);
        fprintf(stderr, "\n  pattern[%zu]: ", pattern.size());
        for (size_t i = 0; i < pattern.size(); i++)
            fprintf(stderr, "%02x ", (unsigned char)pattern[i]);
        fprintf(stderr, "\n  replacement[%zu]: ", replacement.size());
        for (size_t i = 0; i < replacement.size(); i++)
            fprintf(stderr, "%02x ", (unsigned char)replacement[i]);
        fprintf(stderr, "\n");
        LUAU_ASSERT(!"yieldable gsub errored but base gsub succeeded");
        lua_settop(LP, BASE);
        return 0;
    }

    // Compare results.
    size_t yield_result_len;
    const char* yield_result = lua_tolstring(LYield, 1, &yield_result_len);
    int yield_count = lua_tointeger(LYield, 2);

    LUAU_ASSERT(base_count == yield_count);
    LUAU_ASSERT(base_result_len == yield_result_len);
    LUAU_ASSERT(memcmp(base_result, yield_result, base_result_len) == 0);

    runningYieldable = false;
    lua_settop(LP, BASE);
    return 0;
}
