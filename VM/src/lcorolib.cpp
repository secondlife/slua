// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
// This code is based on Lua 5.x implementation licensed under MIT License; see lua_LICENSE.txt for details
#define lcorolib_c
#include "lualib.h"

#include "ldebug.h"
#include "lfunc.h"
#include "lapi.h"
#include "lgc.h"
#include "lobject.h"
#include "lstate.h"
#include "lvm.h"
#include "llsl.h"

#include <vector>

#define CO_STATUS_ERROR -1
#define CO_STATUS_BREAK -2

static const char* const statnames[] = {"running", "suspended", "normal", "dead", "dead"}; // dead appears twice for LUA_COERR and LUA_COFIN

static int costatus(lua_State* L)
{
    lua_State* co = lua_tothread(L, 1);
    luaL_argexpected(L, co, 1, "thread");
    lua_pushstring(L, statnames[lua_costatus(L, co)]);
    return 1;
}

static int auxresume(lua_State* L, lua_State* co, int narg)
{
    // error handling for edge cases
    if (co->status != LUA_YIELD && co->status != LUA_BREAK)
    {
        int status = lua_costatus(L, co);
        if (status != LUA_COSUS)
        {
            lua_pushfstring(L, "cannot resume %s coroutine", statnames[status]);
            return CO_STATUS_ERROR;
        }
    }

    if (narg)
    {
        if (!lua_checkstack(co, narg))
            luaL_error(L, "too many arguments to resume");
        lua_xmove(L, co, narg);
    }
    else
    {
        // coroutine might be completely full already
        if ((co->top - co->base) > LUAI_MAXCSTACK)
            luaL_error(L, "too many arguments to resume");
    }

    co->singlestep = L->singlestep;

    int status = lua_resume(co, L, narg);
    if (status == 0 || status == LUA_YIELD)
    {
        int nres = cast_int(co->top - co->base);
        if (nres)
        {
            // +1 accounts for true/false status in resumefinish
            if (nres + 1 > LUA_MINSTACK && !lua_checkstack(L, nres + 1))
                luaL_error(L, "too many results to resume");
            lua_xmove(co, L, nres); // move yielded values
        }
        return nres;
    }
    else if (status == LUA_BREAK)
    {
        return CO_STATUS_BREAK;
    }
    else
    {
        lua_xmove(co, L, 1); // move error message
        return CO_STATUS_ERROR;
    }
}

static int interruptThread(lua_State* L, lua_State* co)
{
    // notify the debugger that the thread was suspended
    if (L->global->cb.debuginterrupt)
        luau_callhook(L, L->global->cb.debuginterrupt, co);

    return lua_break(L);
}

static int auxresumecont(lua_State* L, lua_State* co)
{
    if (co->status == 0 || co->status == LUA_YIELD)
    {
        int nres = cast_int(co->top - co->base);
        if (!lua_checkstack(L, nres + 1))
            luaL_error(L, "too many results to resume");
        lua_xmove(co, L, nres); // move yielded values
        return nres;
    }
    else
    {
        lua_rawcheckstack(L, 2);
        lua_xmove(co, L, 1); // move error message
        return CO_STATUS_ERROR;
    }
}

static int coresumefinish(lua_State* L, int r)
{
    if (r < 0)
    {
        lua_pushboolean(L, 0);
        lua_insert(L, -2);
        return 2; // return false + error message
    }
    else
    {
        lua_pushboolean(L, 1);
        lua_insert(L, -(r + 1));
        return r + 1; // return true + `resume' returns
    }
}

static int coresumey(lua_State* L)
{
    lua_State* co = lua_tothread(L, 1);
    luaL_argexpected(L, co, 1, "thread");
    int narg = cast_int(L->top - L->base) - 1;
    int r = auxresume(L, co, narg);

    if (r == CO_STATUS_BREAK)
        return interruptThread(L, co);

    return coresumefinish(L, r);
}

static int coresumecont(lua_State* L, int status)
{
    lua_State* co = lua_tothread(L, 1);
    luaL_argexpected(L, co, 1, "thread");

    // ServerLua: This continuation might be for a thread that was in break state, try resuming it
    int r;
    if (co->status == LUA_BREAK)
    {
        // We don't call auxresumecont in this case because it does basically the same
        // thing as auxresume already does at the end.
        r = auxresume(L, co, 0);
    }
    else
        r = auxresumecont(L, co);

    // if coroutine still hasn't yielded after the break, break current thread again
    if (co->status == LUA_BREAK)
        return interruptThread(L, co);

    return coresumefinish(L, r);
}

static int auxwrapfinish(lua_State* L, int r)
{
    if (r < 0)
    {
        if (lua_isstring(L, -1))
        {                     // error object is a string?
            luaL_where(L, 1); // add extra info
            lua_insert(L, -2);
            lua_concat(L, 2);
        }
        lua_error(L); // propagate error
    }
    return r;
}

static int auxwrapy(lua_State* L)
{
    lua_State* co = lua_tothread(L, lua_upvalueindex(1));
    int narg = cast_int(L->top - L->base);
    int r = auxresume(L, co, narg);

    if (r == CO_STATUS_BREAK)
        return interruptThread(L, co);

    return auxwrapfinish(L, r);
}

static int auxwrapcont(lua_State* L, int status)
{
    lua_State* co = lua_tothread(L, lua_upvalueindex(1));

    // ServerLua: This continuation might be for a thread that was in break state, try resuming it
    int r;
    if (co->status == LUA_BREAK)
    {
        // We don't call auxresumecont in this case because it does basically the same
        // thing as auxresume already does at the end.
        r = auxresume(L, co, 0);
    }
    else
        r = auxresumecont(L, co);

    // if coroutine still hasn't yielded after the break, break current thread again
    if (co->status == LUA_BREAK)
        return interruptThread(L, co);

    return auxwrapfinish(L, r);
}

static int cocreate(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_State* NL = lua_newthread(L);
    lua_xpush(L, NL, 1); // push function on top of NL
    return 1;
}

static int cowrap(lua_State* L)
{
    cocreate(L);

    lua_pushcclosurek(L, auxwrapy, "<wrapped>", 1, auxwrapcont);
    return 1;
}

static int coyield(lua_State* L)
{
    int nres = cast_int(L->top - L->base);
    return lua_yield(L, nres);
}

static int corunning(lua_State* L)
{
    if (lua_pushthread(L))
        lua_pushnil(L); // main thread is not a coroutine
    return 1;
}

static int coyieldable(lua_State* L)
{
    lua_pushboolean(L, lua_isyieldable(L));
    return 1;
}

static int coclose(lua_State* L)
{
    lua_State* co = lua_tothread(L, 1);
    luaL_argexpected(L, co, 1, "thread");

    int status = lua_costatus(L, co);
    if (status != LUA_COFIN && status != LUA_COERR && status != LUA_COSUS)
        luaL_error(L, "cannot close %s coroutine", statnames[status]);

    if (co->status == LUA_OK || co->status == LUA_YIELD)
    {
        lua_pushboolean(L, true);
        lua_resetthread(co);
        return 1;
    }
    else
    {
        lua_pushboolean(L, false);

        if (co->status == LUA_ERRMEM)
            lua_pushstring(L, LUA_MEMERRMSG);
        else if (co->status == LUA_ERRERR)
            lua_pushstring(L, LUA_ERRERRMSG);
        else if (lua_gettop(co))
            lua_xmove(co, L, 1); // move error message

        lua_resetthread(co);
        return 2;
    }
}

// ServerLua: For mimicking sandboxing semantics of `require()`
static int auxsandboxedfinish(lua_State* L, lua_State* co, int r)
{
    if (r >= 0 && co->status == LUA_YIELD)
        luaL_error(L, "attempt to yield from sandboxed require");

    if (r == CO_STATUS_BREAK)
        return interruptThread(L, co);

    if (r < 0)
    {
        if (lua_isstring(L, -1))
        {
            luaL_where(L, 1);
            lua_insert(L, -2);
            lua_concat(L, 2);
        }
        lua_error(L);
    }

    lua_remove(L, 2);
    lua_remove(L, 1);
    return r;
}

static int dangerouslyexecuterequiredmodule(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    if (!lua_isLfunction(L, 1))
        luaL_error(L, "Lua function expected");

    const TValue* o = luaA_toobject(L, 1);
    Closure* cl = clvalue(o);
    if (cl->nupvalues != 0)
        luaL_error(L, "function with upvalues not allowed");
    // We don't currently have any system-defined Lua closures,
    // but we might at some point.
    if (cl->memcat < 2)
        luaL_error(L, "can't wrap a system lua closure");

    // Intentionally creating this on the main thread so that we
    // don't automatically inherit the globals of the caller.
    // This is somewhat unlike the REPL, since it will inherit
    // mutable globals on the main thread, but that seems to
    // have been a mistake.
    lua_State* GL = lua_mainthread(L);
    lua_State* co = lua_newthread(GL);
    lua_xmove(GL, L, 1);
    luaL_sandboxthread(co);
    lua_setmemcat(co, lua_getmemcat(L));

    // SL needs some special logic for things that don't live on _G
    if (LUAU_IS_SL_VM(L))
    {
        // New thread must inherit SL VM userdata from calling thread
        co->userdata = L->userdata;

        // Limit the globals we let `require()`d inherit to limit shenanigans
        // Some of these objects conventionally live on the user globals object.
        using CopyableGlobalDetails = struct CopyableGlobalDetails {
            const char *name;
            int type;
            int utag;
        };
        static const std::vector<CopyableGlobalDetails> SL_GLOBALS = {
            {"LLEvents", LUA_TUSERDATA, UTAG_LLEVENTS},
            {"LLTimers", LUA_TUSERDATA, UTAG_LLTIMERS},
            // It doesn't make sense to call this function if you don't provide a `require()`
            {"require", LUA_TFUNCTION, -1},
        };
        for (const auto &to_inherit : SL_GLOBALS)
        {
            // Try pristine copy first
            // The stupid name is so that people can't access it through normal means, since
            // this isn't a legal identifier and we don't allow `getfenv()` or `setfenv()`
            // in prod.
            std::string pristine_name = std::string("/$ require_sandbox $ ") + to_inherit.name;
            lua_rawgetfield(L, LUA_GLOBALSINDEX, pristine_name.c_str());

            if (lua_isnil(L, -1))
            {
                lua_pop(L, 1);
                // Fall back to normal version
                // We intentionally look at the globals including `_G` as a fallback in case
                // `require()` and friends were provided for us by the implementation on `_G`.
                lua_getfield(L, LUA_GLOBALSINDEX, to_inherit.name);

                if (lua_isnil(L, -1))
                {
                    lua_pop(L, 1);
                    luaL_errorL(L, "cannot call dangerouslyexecuterequiredmodule() without '%s' global",
                                to_inherit.name);
                }
            }

            lua_xmove(L, co, 1);

            // And it better be something I'd expect to be there.
            if (to_inherit.type != lua_type(co, -1)
                || (to_inherit.type == LUA_TUSERDATA && !lua_touserdatatagged(co, -1, to_inherit.utag)))
            {
                luaL_errorL(
                    L,
                    "cannot call dangerouslyexecuterequiredmodule() with an invalid '%s' global",
                    to_inherit.name
                );
            }

            // Set with normal name
            lua_pushvalue(co, -1);  // Duplicate the value
            lua_rawsetfield(co, LUA_GLOBALSINDEX, to_inherit.name);

            // Set with prefixed name (for nested `require()` calls)
            lua_rawsetfield(co, LUA_GLOBALSINDEX, pristine_name.c_str());
        }
    }

    // Copy the closure that was passed in but give it our new environment
    Proto* p = cl->l.p;
    Closure* newcl = luaF_newLclosure(co, 0, co->gt, p);

    // push it onto the new thread's stack
    setclvalue(co, co->top, newcl);
    incr_top(co);

    int r = auxresume(L, co, 0);
    return auxsandboxedfinish(L, co, r);
}

static int dangerouslyexecuterequiredmodulecont(lua_State* L, int status)
{
    lua_State* co = lua_tothread(L, 2);

    int r;
    if (co->status == LUA_BREAK)
        r = auxresume(L, co, 0);
    else
        r = auxresumecont(L, co);

    return auxsandboxedfinish(L, co, r);
}

static const luaL_Reg co_funcs[] = {
    {"create", cocreate},
    {"running", corunning},
    {"status", costatus},
    {"wrap", cowrap},
    {"yield", coyield},
    {"isyieldable", coyieldable},
    {"close", coclose},
    {NULL, NULL},
};

int luaopen_coroutine(lua_State* L)
{
    luaL_register(L, LUA_COLIBNAME, co_funcs);

    lua_pushcclosurek(L, coresumey, "resume", 0, coresumecont);
    lua_setfield(L, -2, "resume");

    // ServerLua: For mimicking sandboxing semantics of `require()`
    lua_pushcclosurek(L, dangerouslyexecuterequiredmodule, "dangerouslyexecuterequiredmodule", 0, dangerouslyexecuterequiredmodulecont);
    lua_setglobal(L, "dangerouslyexecuterequiredmodule");

    return 1;
}
