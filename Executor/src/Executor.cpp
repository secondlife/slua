#include "Luau/Executor.h"
#include "Luau/Script.h"

#include <utility>

#include "lualib.h"
#include "llsl.h"

#include "lstate.h"

namespace Luau
{
namespace Executor
{

static const luaL_Reg kSafeLuaLibs[] = {
    {"", luaopen_base},
    {LUA_COLIBNAME, luaopen_coroutine},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_OSLIBNAME, luaopen_os},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {LUA_DBLIBNAME, luaopen_debug},
    {LUA_UTF8LIBNAME, luaopen_utf8},
    {LUA_BITLIBNAME, luaopen_bit32},
    // Erm, this is basically never safe to expose directly.
    // {LUA_ERISLIBNAME, luaopen_eris},
    {LUA_BUFFERLIBNAME, luaopen_buffer},
    {LUA_VECLIBNAME, luaopen_vector},
    {LUA_CJSONLIBNAME, luaopen_cjson},
    {LUA_LLBASE64LIBNAME, luaopen_llbase64},
    {NULL, NULL},
};

// These have to be removed so that people can't mess with globals dynamically
// and de-optimize the script.
static const char* const kGlobalsToRemove[] = {
    "getfenv",
    "setfenv",
};

// A replacement for `coroutine.running()` that won't return handles for the
// handler thread or the instance root. Mostly to stop people manually
// `resume()`ing or `close()`ing them.
static int lcorolib_running(lua_State* L)
{
    Script* script = Script::fromLuaState(L);
    if (script && (L == script->getHandlerState() || L == script->getInstanceState()))
    {
        // You're not allowed to touch these
        lua_pushnil(L);
        return 1;
    }

    if (lua_pushthread(L))
        lua_pushnil(L);
    return 1;
}

// A replacement for `gcinfo()` that reports the calling script's memory
// rather than the whole VM's, which would leak co-resident scripts' usage.
static int gcinfo(lua_State* L)
{
    // User code only ever runs on script threads
    Script* script = Script::fromLuaState(L);
    if (script == nullptr)
        return 0;

    // Luau expects this to be in KB with no fractional part
    // Let the fractional component through since bytes matter for us :)
    lua_pushnumber(L, (double)script->getUsedMemory() / 1024);
    return 1;
}

// Basically `luaL_openlibs()`, but the specific subset we consider safe.
static void open_safe_libs(lua_State* L)
{
    lua_checkstack(L, 10);
    for (const luaL_Reg* lib = kSafeLuaLibs; lib->func; lib++)
    {
        lua_pushcfunction(L, lib->func, NULL);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }

    for (const char* to_remove : kGlobalsToRemove)
    {
        lua_pushnil(L);
        lua_setglobal(L, to_remove);
    }

    lua_pushcfunction(L, gcinfo, "gcinfo");
    lua_setglobal(L, "gcinfo");

    // Now replace `coroutine.running()` with our bodged version.
    lua_getglobal(L, "coroutine");
    lua_pushcfunction(L, lcorolib_running, "lcorolib_running");
    lua_rawsetfield(L, -2, "running");
    lua_pop(L, 1);
}

Environment::Environment(IProvisioner& provisioner, bool is_lsl, uint32_t api_version)
    : mProvisioner(provisioner)
    , mIsLSL(is_lsl)
    , mAPIVersion(api_version)
{
    mRuntimeState.slIdentifier = is_lsl ? LUA_LSL_IDENTIFIER : LUA_SL_IDENTIFIER;
}

Environment::~Environment()
{
    if (mBaseState)
    {
        lua_close(mBaseState);
    }
}

lua_State* Environment::openVM()
{
    lua_State* L = luaL_newstate();
    lua_setthreaddata(L, &getRuntimeState());

    open_safe_libs(L);

    // Both of these return their tables, pop them.
    luaopen_sl(L, false);
    if (mIsLSL)
    {
        luaopen_lsl(L);
        lua_pop(L, 1);
    }
    luaopen_ll(L, false);
    lua_pop(L, 1);
    LUAU_ASSERT(lua_gettop(L) == 0);

    const PopulateEnvironmentCallback populate = getProvisioner().getCallbacks().populateEnvironment;
    if (populate != nullptr)
        populate(*this, L);
    LUAU_ASSERT(lua_gettop(L) == 0);

    // Protect core libraries and metatables from modification
    luaL_sandbox(L);

    // Register constants we need for serialization
    eris_register_perms(L, true);
    eris_register_perms(L, false);

    lua_fixallcollectable(L);

    return L;
}

Image::Image(std::shared_ptr<IEnvironment> environment, const ImageConfig& config)
    : mEnvironment(std::move(environment))
    , mIsLSL(config.isLSL)
    , mAPIVersion(config.apiVersion)
    , mChargedBytecodeSize(config.chargedBytecodeSize)
    , mName(config.name ? config.name : "")
{
}

Image::~Image()
{
    if (mForkerState)
    {
        lua_unref(mEnvironment->getBaseState(), mForkerRef);
    }
}

void Image::build(const ImageConfig& config)
{
    lua_State* L = mEnvironment->getBaseState();

    if (config.bytecode == nullptr || config.bytecodeSize == 0)
    {
        mError = "Invalid bytecode";
        return;
    }
    LUAU_ASSERT(lua_gettop(L) == 0);

    // Make sure we create the thread with a user memcat!
    lua_State* clone_base = nullptr;
    {
        MemcatGuard guard{L, kUserMemcat};
        clone_base = lua_newthread(L);
    }
    // Uses the active memcat on `clone_base` to create the globals table.
    luaL_sandboxthread(clone_base);

    int result;
    {
        // Protos and their constituent constants need to be loaded with the user memcat.
        // This works out because even if things are interned cross-script, we know which
        // user memcat objects should be considered "free" for the user, generally constants.
        MemcatGuard guard{clone_base, kUserMemcat};
        result = luau_load(clone_base, config.chunkname ? config.chunkname : "=lua_script", config.bytecode, config.bytecodeSize, 0);
    }
    if (result != 0)
    {
        const char* error_str = lua_tostring(clone_base, -1);
        mError = error_str ? error_str : "unknown error";
        logWarn(logSource(), "Failed to load Luau bytecode: %s", mError.c_str());
        // Drop the clone thread, the environment stays usable
        lua_pop(L, 1);
        return;
    }

    // pristine pre-fork objects
    mFreeObjects = lua_collectfreeobjects(clone_base);
    lua_State* forker = eris_make_forkserver(clone_base);
    // Anchor the forkserver in the registry
    mForkerRef = lua_ref(L, -1);
    // And now we're fine to pop the forker and clone.
    lua_pop(L, 2);
    LUAU_ASSERT(lua_gettop(L) == 0);

    // Assigned last so isValid() only flips once the whole build succeeded
    mForkerState = forker;
}

Instance Image::forkInstance(lua_SLRuntimeState* owner, const std::string* blob)
{
    lua_State* base = mEnvironment->getBaseState();

    if (blob != nullptr)
    {
        // Push the serialized state for the forker to rehydrate from
        lua_pushlstring(mForkerState, blob->data(), blob->size());
    }

    // `owner` is passed down so any newly-created threads in here get associated with `owner`. Generally
    // `owner` is a `Script` requesting an instance due to a load or reset.
    lua_State* thread = eris_fork_thread(mForkerState, blob == nullptr, kUserMemcat, owner);

    if (thread == nullptr)
    {
        const char* error_str = lua_tostring(mForkerState, -1);
        if (blob == nullptr)
        {
            // Forking the pristine instance failing is an engine bug rather
            // than anything a script can provoke.
            logWarn(logSource(), "Failed to deserialize default state: %s", error_str ? error_str : "unknown error");
            lua_dumpstack(mForkerState);
        }
        else
        {
            logWarn(logSource(), "Failed to parse script state %s", error_str ? error_str : "unknown error");
        }
        lua_pop(mForkerState, 1);
        return Instance();
    }

    LUAU_ASSERT(lua_tothread(base, -1) == thread);
    // We got something we can use, anchor it to the registry and make an Instance out of it.
    Instance instance(thread, base, lua_ref(base, -1));
    lua_pop(base, 1);

    return instance;
}

bool Image::serializeInstance(const Instance& instance, std::string& out)
{
    // The string-serialized state ends up on the forker thread
    int start_forker_top = lua_gettop(mForkerState);
    int status = eris_serialize_thread(mForkerState, instance.thread());

    if (status != LUA_OK)
    {
        // Top of the stack should be the error message
        const char* error_str = status == LUA_ERRRUN ? lua_tostring(mForkerState, -1) : nullptr;
        if (error_str != nullptr)
        {
            logWarn(logSource(), "Failed to serialize script state %s", error_str);
        }
        else
        {
            logWarn(logSource(), "Failed to serialize script state with error code %d", status);
        }
        // Pop off anything that might have been pushed on by an error handler
        lua_settop(mForkerState, start_forker_top);
        return false;
    }

    size_t data_len;
    const char* data = lua_tolstring(mForkerState, -1, &data_len);
    if (data == nullptr)
    {
        logWarn(logSource(), "Serialized script state is not a string");
        lua_settop(mForkerState, start_forker_top);
        return false;
    }
    out.assign(data, data_len);
    // Get rid of the string copy managed by Lua
    lua_pop(mForkerState, 1);
    return true;
}

} // namespace Executor
} // namespace Luau
