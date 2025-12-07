// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#include "llsl.h"

#include "Luau/Common.h"
#include "Luau/Frontend.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/LSLBuiltins.h"

#include <string>
#include <memory>

#include <string.h>

// Simple FileResolver for type checking on luau.org/demo
struct DemoFileResolver
    : Luau::FileResolver
{
    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override
    {
        auto it = source.find(name);
        if (it == source.end())
            return std::nullopt;

        return Luau::SourceCode{it->second, Luau::SourceCode::Module};
    }

    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* expr, const Luau::TypeCheckLimits& limits) override
    {
        if (Luau::AstExprGlobal* g = expr->as<Luau::AstExprGlobal>())
            return Luau::ModuleInfo{g->name.value};

        return std::nullopt;
    }

    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override
    {
        return name;
    }

    std::optional<std::string> getEnvironmentForModule(const Luau::ModuleName& name) const override
    {
        return std::nullopt;
    }

    std::unordered_map<Luau::ModuleName, std::string> source;
};

// Simple ConfigResolver for type checking on luau.org/demo that defaults to Strict mode
struct DemoConfigResolver : Luau::ConfigResolver
{
    DemoConfigResolver()
    {
        defaultConfig.mode = Luau::Mode::Strict;
    }

    virtual const Luau::Config& getConfig(const Luau::ModuleName& name, const Luau::TypeCheckLimits& limits) const override
    {
        return defaultConfig;
    }

    Luau::Config defaultConfig;
};

// SL mode state
static lua_SLRuntimeState sl_state;

static void userthread_callback(lua_State* LP, lua_State* L)
{
    if (LP == nullptr)
        return;
    lua_setthreaddata(L, lua_getthreaddata(LP));
}

static void setupState(lua_State* L)
{
    luaL_openlibs(L);

    // SL mode setup
    lua_setthreaddata(L, &sl_state);
    sl_state.slIdentifier = LUA_SL_IDENTIFIER;
    lua_callbacks(L)->userthread = userthread_callback;

    // SL libraries
    luaopen_sl(L, true);
    luaopen_ll(L, true);
    lua_pop(L, 1);

    // JSON and Base64
    luaopen_cjson(L);
    lua_pop(L, 1);
    luaopen_llbase64(L);
    lua_pop(L, 1);

    // Set SL constants on _G
    luaSL_set_constant_globals(L);

    luaL_sandbox(L);
    lua_fixallcollectable(L);
}

static std::string runCode(lua_State* L, const std::string& source)
{
    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(source.data(), source.length(), nullptr, &bytecodeSize);
    lua_setmemcat(L, 0);
    int result = luau_load(L, "=stdin", bytecode, bytecodeSize, 0);
    free(bytecode);

    if (result != 0)
    {
        size_t len;
        const char* msg = lua_tolstring(L, -1, &len);

        std::string error(msg, len);
        lua_pop(L, 1);

        return error;
    }

    lua_setmemcat(L, 2);
    lua_State* T = lua_newthread(L);
    lua_setmemcat(L, 0);

    lua_pushvalue(L, -2);
    lua_remove(L, -3);
    lua_xmove(L, T, 1);

    int status = lua_resume(T, NULL, 0);

    if (status == 0)
    {
        int n = lua_gettop(T);

        if (n)
        {
            luaL_checkstack(T, LUA_MINSTACK, "too many results to print");
            lua_getglobal(T, "print");
            lua_insert(T, 1);
            lua_pcall(T, n, 0, 0);
        }

        lua_pop(L, 1); // pop T
        return std::string();
    }
    else
    {
        std::string error;

        lua_Debug ar;
        if (lua_getinfo(L, 0, "sln", &ar))
        {
            error += ar.short_src;
            error += ':';
            error += std::to_string(ar.currentline);
            error += ": ";
        }

        if (status == LUA_YIELD)
        {
            error += "thread yielded unexpectedly";
        }
        else if (const char* str = lua_tostring(T, -1))
        {
            error += str;
        }

        error += "\nstack backtrace:\n";
        error += lua_debugtrace(T);

        lua_pop(L, 1); // pop T
        return error;
    }
}

extern "C" const char* checkScript(const char* source, int useNewSolver)
{
    static std::string finalCheckResult;
    finalCheckResult.clear();

    try
    {
        DemoFileResolver fileResolver;
        DemoConfigResolver configResolver;
        Luau::FrontendOptions options;

        Luau::Frontend frontend(&fileResolver, &configResolver, options);
        frontend.setLuauSolverMode(useNewSolver ? Luau::SolverMode::New : Luau::SolverMode::Old);
        // Add Luau builtins
        Luau::unfreeze(frontend.globals.globalTypes);
        Luau::registerBuiltinGlobals(frontend, frontend.globals);
        Luau::freeze(frontend.globals.globalTypes);

        // restart
        frontend.clear();
        fileResolver.source.clear();

        fileResolver.source["main"] = source;

        Luau::CheckResult checkResult = frontend.check("main");
        for (const Luau::TypeError& err : checkResult.errors)
        {
            if (!finalCheckResult.empty())
                finalCheckResult += "\n";
            finalCheckResult += std::to_string(err.location.begin.line + 1);
            finalCheckResult += ": ";
            finalCheckResult += Luau::toString(err);
        }
    }
    catch (const std::exception& e)
    {
        finalCheckResult = e.what();
    }

    return finalCheckResult.empty() ? nullptr : finalCheckResult.c_str();
}

extern "C" const char* executeScript(const char* source)
{
    // setup flags
    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
        if (strncmp(flag->name, "Luau", 4) == 0)
            flag->value = true;

    // Initialize SL builtins once from embedded data
    static bool builtins_initialized = false;
    if (!builtins_initialized)
    {
        luauSL_init_global_builtins(nullptr);
        builtins_initialized = true;
    }

    // create new state
    std::unique_ptr<lua_State, void (*)(lua_State*)> globalState(luaL_newstate(), lua_close);
    lua_State* L = globalState.get();

    // setup state
    setupState(L);

    // sandbox thread first (like REPL does)
    luaL_sandboxthread(L);

    // Event and timer managers (after sandbox)
    luaSL_createeventmanager(L);
    lua_ref(L, -1);
    lua_pushvalue(L, -1);  // Duplicate for timer manager (it expects LLEvents on stack)
    lua_setglobal(L, "LLEvents");

    luaSL_createtimermanager(L);
    lua_ref(L, -1);
    lua_setglobal(L, "LLTimers");

    // static string for caching result (prevents dangling ptr on function exit)
    static std::string result;

    // run code + collect error
    result = runCode(L, source);

    return result.empty() ? nullptr : result.c_str();
}
