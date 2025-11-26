// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#include "luacodegen.h"

#include "Luau/DenseHash.h"
#include "Luau/ModuleResolver.h"
#include <string>
#include <sstream>
#include <vector>


static lua_State* GL = nullptr;
static lua_State* Lforker = nullptr;
static lua_SLRuntimeState lsl_state;

static const std::string EXAMPLE_SCRIPT = R"(
function foo()
    return bar()
end

local function baz()
    return "yeah"
end

-- Won't run, doesn't matter.
print(baz() .. foo())
)";

static void initLuaState()
{
    if (GL)
        return;

    GL = luaL_newstate();
    luaL_openlibs(GL);

    // Tag as SL VM (required for UUID/quaternion support)
    lsl_state.slIdentifier = LUA_SL_IDENTIFIER;
    lua_setthreaddata(GL, &lsl_state);
    // propagate thread data
    lua_callbacks(GL)->userthread = [](lua_State *LP, lua_State *L) {
        if (!LP)
            return;
        lua_setthreaddata(L, lua_getthreaddata(LP));
    };

    // Register SL types (UUID metatables, quaternion metatables, weak tables)
    luaopen_sl(GL, true);

    // Protect core libraries and metatables from modification
    luaL_sandbox(GL);
    lua_fixallcollectable(GL);
    eris_register_perms(GL, true);
    eris_register_perms(GL, false);

    // Create a new writable global table for current thread
    lua_State *L = lua_newthread(GL);
    luaL_sandboxthread(L);

    // We need to load some bytecode in so we have a thread to fork
    lua_CompileOptions opts = {};
    opts.optimizationLevel = 1;
    opts.debugLevel = 1;

    size_t bytecodeSize = 0;
    char * bytecode = luau_compile(EXAMPLE_SCRIPT.c_str(), EXAMPLE_SCRIPT.length(), &opts, &bytecodeSize);
    int result = luau_load(L, "=fuzzing", bytecode, bytecodeSize, 0);
    free(bytecode);
    LUAU_ASSERT(result == 0);

    Lforker = eris_make_forkserver(L);
    LUAU_ASSERT(Lforker);
    // Don't need the original thread on the main stack anymore
    lua_remove(GL, -2);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    initLuaState();
    int forker_top = lua_gettop(Lforker);
    int base_top = lua_gettop(GL);

    // Push the state data onto the forker state
    lua_pushlstring(Lforker, (const char *)Data, Size);
    // Have the forker create a state with the deserialized state
    if (eris_fork_thread(Lforker, false, 2) != nullptr)
    {
        // The new thread should be on the end of the base stat's stack.
        luaL_checktype(GL, -1, LUA_TTHREAD);
        lua_pop(GL, 1);
    }
    else
    {
        luaL_checktype(Lforker, -1, LUA_TSTRING);
        lua_pop(Lforker, 1);
    }

    LUAU_ASSERT(lua_gettop(GL) == base_top);
    LUAU_ASSERT(lua_gettop(Lforker) == forker_top);
    lua_gc(GL, LUA_GCCOLLECT, 0);

    return 0;
}
