// Standalone analogue of indra's lscriptharness.cpp: runs one script through
// the full Executor stack (memory limiting, quanta preemption, punishment) so
// scheduling overhead that a bare REPL never exercises can be measured.
#include "lua.h"
#include "lualib.h"
#include "llsl.h"

#include "Luau/Common.h"
#include "Luau/Compiler.h"
#include "Luau/Executor.h"
#include "Luau/FileUtils.h"
#include "Luau/Flags.h"
#include "Luau/LSLBuiltins.h"
#include "Luau/Script.h"

#ifdef LUAU_USE_TAILSLIDE
#include "Luau/LSLCompiler.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <optional>
#include <string>

using namespace Luau::Executor;


static int harness_sleep(lua_State* L)
{
    double duration = luaL_checknumber(L, 1);
    Script* script = Script::fromLuaState(L);
    if (script != nullptr && duration > 0.0)
    {
        script->setSleep(script->getSleep() + (float)duration);
    }
    return 0;
}

static int harness_getusedmemory(lua_State* L)
{
    Script* script = Script::fromLuaState(L);
    luaSL_pushnativeinteger(L, script != nullptr ? script->getUsedMemory() : 0);
    return 1;
}

static int harness_getfreememory(lua_State* L)
{
    Script* script = Script::fromLuaState(L);
    luaSL_pushnativeinteger(L, script != nullptr ? script->getMemoryLimit() - script->getUsedMemory() : 0);
    return 1;
}

static void set_ll_function(lua_State* L, const char* table, const char* name, lua_CFunction fn)
{
    lua_getglobal(L, table);
    if (lua_istable(L, -1))
    {
        lua_pushcfunction(L, fn, name);
        lua_rawsetfield(L, -2, name);
    }
    lua_pop(L, 1);
}

static void populate_environment(IEnvironment& environment, lua_State* L)
{
    luaSL_set_constant_globals(L);

    for (const char* table : {"ll", "llcompat"})
    {
        set_ll_function(L, table, "Sleep", harness_sleep);
        set_ll_function(L, table, "GetUsedMemory", harness_getusedmemory);
        set_ll_function(L, table, "GetFreeMemory", harness_getfreememory);
    }

    // Pull in the test-lib helpers (StringLength, GetSubString, OwnerSay, ...).
    luaopen_ll(L, true);
    lua_pop(L, 1);
}

// Script-visible wall clock backing os.clock() and the timer managers
static double script_clock(lua_State* L)
{
    return lua_clock();
}

static void log_to_stderr(LogLevel level, const char* source, const char* message)
{
    static const char* level_names[] = {"DEBUG", "INFO", "WARN"};
    fprintf(stderr, "[%s] %s: %s\n", level_names[(int)level], source, message);
}

static void displayHelp(const char* argv0)
{
    printf("Usage: %s [options] script\n", argv0);
    printf("\n");
    printf("Simple script harness that implements script-preemption and memory limits,\n");
    printf("most useful for benchmarking and such..\n");
    printf("\n");
    printf("Options:\n");
    printf("  --quanta=<usecs>: time slice per run window (default 200)\n");
    printf("  -O<n>: compile with optimization level n (default 1)\n");
    printf("  --fflags=<list>: comma-separated fast flag settings (name=true/false),\n");
}

int main(int argc, char** argv)
{
    const char* script_path = nullptr;
    double quanta_usec = 200.0;
    int optimization_level = 1;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            displayHelp(argv[0]);
            return 0;
        }
        else if (strncmp(argv[i], "--quanta=", 9) == 0)
        {
            quanta_usec = atof(argv[i] + 9);
            if (quanta_usec <= 0.0)
            {
                fprintf(stderr, "Error: --quanta must be a positive number of usecs.\n");
                return 1;
            }
        }
        else if (strncmp(argv[i], "--fflags=", 9) == 0)
        {
            setLuauFlags(argv[i] + 9);
        }
        else if (strncmp(argv[i], "-O", 2) == 0)
        {
            int level = atoi(argv[i] + 2);
            if (level < 0 || level > 2)
            {
                fprintf(stderr, "Error: Optimization level must be between 0 and 2 inclusive.\n");
                return 1;
            }
            optimization_level = level;
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "Error: Unrecognized option '%s'.\n\n", argv[i]);
            displayHelp(argv[0]);
            return 1;
        }
        else if (script_path != nullptr)
        {
            fprintf(stderr, "Error: Only one script path may be given.\n\n");
            displayHelp(argv[0]);
            return 1;
        }
        else
        {
            script_path = argv[i];
        }
    }

    if (script_path == nullptr)
    {
        displayHelp(argv[0]);
        return 1;
    }

    Luau::setRequiredSLuaFlags();
    luauSL_init_global_builtins(nullptr);
    Luau::Executor::logCallback() = log_to_stderr;

    std::optional<std::string> source = readFile(script_path);
    if (!source)
    {
        fprintf(stderr, "Error: Could not open %s\n", script_path);
        return 1;
    }

    const bool is_lsl = strstr(script_path, ".lsl") != nullptr;
    std::string bytecode;
    if (is_lsl)
    {
#ifdef LUAU_USE_TAILSLIDE
        bytecode = compileLSL(*source);
#else
        fprintf(stderr, "No LSL support, do a Tailslide-enabled build\n");
        return 1;
#endif
    }
    else
    {
        Luau::CompileOptions copts = {};
        copts.optimizationLevel = optimization_level;
        copts.debugLevel = 1;
        copts.typeInfoLevel = 1;
        copts.libraryMemberConstantCb = &luauSL_lookup_constant_cb;
        bytecode = Luau::compile(*source, copts);
    }

    HostCallbacks callbacks;
    callbacks.clockProvider = script_clock;
    callbacks.populateEnvironment = populate_environment;
    // quantaClockProvider stays null so we exercise the engine's default

    Provisioner<> provisioner(callbacks);

    ImageConfig image_config;
    image_config.bytecode = bytecode.data();
    image_config.bytecodeSize = bytecode.size();
    image_config.chargedBytecodeSize = bytecode.size();
    image_config.isLSL = is_lsl;
    image_config.chunkname = is_lsl ? "=lsl_script" : "=lua_script";
    image_config.name = script_path;

    // Built stepwise rather than through provisionScript() so compile and
    // load errors can be printed.
    std::shared_ptr<IImage> image = provisioner.buildImage(provisioner.createEnvironment(is_lsl, 0), image_config);
    if (image == nullptr || !image->isValid())
    {
        fprintf(stderr, "Compile error:\n%s\n", image != nullptr ? image->getError().c_str() : "image build refused");
        return 1;
    }

    ScriptConfig script_config;
    script_config.scriptId = script_path;
    std::shared_ptr<Script> script = provisioner.instantiateScript(image, script_config);
    if (script == nullptr)
    {
        fprintf(stderr, "Error: failed to instantiate script\n");
        return 1;
    }

    // For LSL this also runs the constructor
    if (!script->loadDefaultState())
    {
        fprintf(stderr, "Load error: %s\n", script->getExtendedFaultString().empty() ? script->getFaultString().c_str() : script->getExtendedFaultString().c_str());
        return 1;
    }

    double quanta = quanta_usec * 1e-6;
    double accum_sleep = 0.0;
    size_t slices = 0;
    double start = lua_clock();

    // state_entry is implicit in Lua, but LSL needs it specifically dispatched.
    bool dispatch_state_entry = is_lsl;
    int lsl_state = 0;
    RunResult result;
    for (;;)
    {
        // Fake the sleep, just collect it so we know the accumulated
        // sleep across the entire script run.
        if (script->getSleep() > 0.0f)
        {
            accum_sleep += script->getSleep();
            script->setSleep(0.0f);
        }

        script->beginRunWindow(quanta);
        if (dispatch_state_entry)
        {
            result = script->callEventHandler(lsl_state, "state_entry", nullptr);
            dispatch_state_entry = false;
        }
        else
        {
            result = script->resumeEventHandler();
        }
        script->endRunWindow();
        ++slices;

        if (result.status == HandlerRunStatus::Preempted)
            continue;
        if (result.status == HandlerRunStatus::StateChange)
        {
            // TODO: Do state_exit too... meh.
            lsl_state = result.newState;
            dispatch_state_entry = true;
            continue;
        }

        // Anything else means we're done.
        break;
    }

    // Bank sleep the final slice left behind
    if (script->getSleep() > 0.0f)
        accum_sleep += script->getSleep();

    double runtime = lua_clock() - start;
    fprintf(stderr, "Runtime: %f, Accum. Sleep: %f, Time Slices: %zu\n", runtime, accum_sleep, slices);

    if (result.status == HandlerRunStatus::Fault)
    {
        auto &fault_str = script->getExtendedFaultString().empty() ? script->getFaultString() : script->getExtendedFaultString();
        fprintf(stderr, "Fault: %s\n", fault_str.c_str());
        return 1;
    }
    if (result.status == HandlerRunStatus::Refused)
    {
        fprintf(stderr, "Error: engine refused to run the handler\n");
        return 1;
    }

    return 0;
}
