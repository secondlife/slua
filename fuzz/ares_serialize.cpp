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

using StateRef = std::unique_ptr<lua_State, void (*)(lua_State*)>;


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    std::string source((const char*)Data, Size);

    lua_State *initialLuaState = luaL_newstate();
    StateRef globalState(initialLuaState, lua_close);
    lua_State* L = globalState.get();

    luaL_openlibs(L);

    // Register a few global functions for conformance tests
    std::vector<luaL_Reg> funcs = {};
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

    // Protect core libraries and metatables from modification
    luaL_sandbox(L);

    // Create a new writable global table for current thread
    luaL_sandboxthread(L);

    std::string chunkname = "=" + std::string("fuzz");

    lua_CompileOptions opts = {};
    opts.optimizationLevel = 1;
    opts.debugLevel = 1;

    size_t bytecodeSize = 0;
    char * bytecode = luau_compile(source.data(), source.size(), &opts, &bytecodeSize);
    int result = luau_load(L, chunkname.c_str(), bytecode, bytecodeSize, 0);
    free(bytecode);

    int status = (result == 0) ? lua_resume(L, nullptr, 0) : LUA_ERRSYNTAX;

    if (status == 0)
    {
        // must have returned exactly 1 value
        if (lua_gettop(L) != 1)
            return 1;
        printf("YAY\n");
        std::ostringstream foo;
        lua_newtable(L);
        lua_insert(L, -2);
        eris_dump(L, &foo);
    }
    return 0;
}
