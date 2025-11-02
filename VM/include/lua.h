// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
// This code is based on Lua 5.x implementation licensed under MIT License; see lua_LICENSE.txt for details
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <istream>
#include <ostream>
#include <unordered_set>

#include "luaconf.h"



// option for multiple returns in `lua_pcall' and `lua_call'
#define LUA_MULTRET (-1)

// ServerLua: LSL support. Should only have non script-specific entries since
// this will be shared across all scripts within a particular VM!
#define LUA_SL_IDENTIFIER 0xd34d0000
#define LUA_SL_IDENTIFIER_MASK 0xFFff0000
#define LUA_LSL_IDENTIFIER 0xd34df007

struct lua_State;

typedef bool (*lua_eventHandlerRegistrationCallback)(lua_State *L, const char *event_name, bool registered);
typedef void (*lua_setTimerEventCallback)(lua_State *L, double interval);
// Clock provider should return monotonic stopwatch time in seconds.
// Non-monotonic clocks may cause timer delays proportional to backward jumps.
typedef double (*lua_clockProvider)(lua_State *L);
// This is strictly for future use by a CSPRNG hook where we need
// _secure_ random values.
typedef bool (*lua_randomProvider)(lua_State *L, uint8_t* dst, size_t len);

// This is meant to be shared between instances of the same script and should
// NOT have any instance-specific data on it!
typedef struct lua_SLRuntimeState
{
    unsigned int slIdentifier = LUA_LSL_IDENTIFIER;
    int uuidWeakTab = -1;
    int uuidCompressedWeakTab = -1;
    lua_eventHandlerRegistrationCallback eventHandlerRegistrationCb = nullptr;
    lua_setTimerEventCallback setTimerEventCb = nullptr;
    // stopwatch timer
    lua_clockProvider clockProvider = nullptr;
    lua_clockProvider performanceClockProvider = nullptr;
    lua_randomProvider randomProvider = nullptr;
} lua_SLRuntimeState;

#define LU_TAG_LSL_INTEGER 100


/*
** pseudo-indices
*/
#define LUA_REGISTRYINDEX (-LUAI_MAXCSTACK - 2000)
#define LUA_ENVIRONINDEX (-LUAI_MAXCSTACK - 2001)
// ServerLua: So we can get the "real" globals without any user additions
//  Some cases care about a value's index relative to `LUA_GLOBALSINDEX` specifically,
//  so we sandwich our new value in the middle here and shift `LUA_GLOBALSINDEX`'s
//  value up by one.
#define LUA_BASEGLOBALSINDEX (-LUAI_MAXCSTACK - 2002)
#define LUA_GLOBALSINDEX (-LUAI_MAXCSTACK - 2003)
#define lua_upvalueindex(i) (LUA_GLOBALSINDEX - (i))
#define lua_ispseudo(i) ((i) <= LUA_REGISTRYINDEX)

// thread status; 0 is OK
enum lua_Status
{
    LUA_OK = 0,
    LUA_YIELD,
    LUA_ERRRUN,
    LUA_ERRSYNTAX, // legacy error code, preserved for compatibility
    LUA_ERRMEM,
    LUA_ERRERR,
    LUA_BREAK, // yielded for a debug breakpoint
};

enum lua_CoStatus
{
    LUA_CORUN = 0, // running
    LUA_COSUS,     // suspended
    LUA_CONOR,     // 'normal' (it resumed another coroutine)
    LUA_COFIN,     // finished
    LUA_COERR,     // finished with error
};

typedef struct lua_State lua_State;

typedef int (*lua_CFunction)(lua_State* L);
typedef int (*lua_Continuation)(lua_State* L, int status);

/*
** prototype for memory-allocation functions
*/

typedef void* (*lua_Alloc)(void* ud, void* ptr, size_t osize, size_t nsize);

// non-return type
#define l_noret void LUA_NORETURN

/*
** basic types
*/
#define LUA_TNONE (-1)

/*
 * WARNING: if you change the order of this enumeration,
 * grep "ORDER TYPE"
 */
// clang-format off
enum lua_Type
{
    LUA_TNIL = 0,     // must be 0 due to lua_isnoneornil
    LUA_TBOOLEAN = 1, // must be 1 due to l_isfalse

    LUA_TLIGHTUSERDATA,
    LUA_TNUMBER,
    LUA_TVECTOR,

    LUA_TSTRING, // all types above this must be value types, all types below this must be GC types - see iscollectable

    LUA_TTABLE,
    LUA_TFUNCTION,
    LUA_TUSERDATA,
    LUA_TTHREAD,
    LUA_TBUFFER,

    // values below this line are used in GCObject tags but may never show up in TValue type tags
    LUA_TPROTO,
    LUA_TUPVAL,
    LUA_TDEADKEY,

    // the count of TValue type tags
    LUA_T_COUNT = LUA_TPROTO
};
// clang-format on

// type of numbers in Luau
typedef double lua_Number;

// type for integer functions
typedef int lua_Integer;

// unsigned integer type
typedef unsigned lua_Unsigned;

// ServerLua: opaque set of GC object pointers for per-script memory accounting
typedef std::unordered_set<void*> lua_OpaqueGCObjectSet;

/*
** state manipulation
*/
LUA_API lua_State* lua_newstate(lua_Alloc f, void* ud);
LUA_API void lua_close(lua_State* L);
LUA_API lua_State* lua_newthread(lua_State* L);
LUA_API lua_State* lua_mainthread(lua_State* L);
LUA_API void lua_resetthread(lua_State* L);
LUA_API int lua_isthreadreset(lua_State* L);

/*
** basic stack manipulation
*/
LUA_API int lua_absindex(lua_State* L, int idx);
LUA_API int lua_gettop(lua_State* L);
LUA_API void lua_settop(lua_State* L, int idx);
LUA_API void lua_pushvalue(lua_State* L, int idx);
LUA_API void lua_remove(lua_State* L, int idx);
LUA_API void lua_insert(lua_State* L, int idx);
LUA_API void lua_replace(lua_State* L, int idx);
LUA_API int lua_checkstack(lua_State* L, int sz);
LUA_API void lua_rawcheckstack(lua_State* L, int sz); // allows for unlimited stack frames

LUA_API void lua_xmove(lua_State* from, lua_State* to, int n);
LUA_API void lua_xpush(lua_State* from, lua_State* to, int idx);

/*
** access functions (stack -> C)
*/

LUA_API int lua_isnumber(lua_State* L, int idx);
LUA_API int lua_isstring(lua_State* L, int idx);
LUA_API int lua_iscfunction(lua_State* L, int idx);
LUA_API int lua_isLfunction(lua_State* L, int idx);
LUA_API int lua_iscallable(lua_State* L, int idx);
LUA_API int lua_isuserdata(lua_State* L, int idx);
LUA_API int lua_type(lua_State* L, int idx);
LUA_API const char* lua_typename(lua_State* L, int tp);

LUA_API int lua_equal(lua_State* L, int idx1, int idx2);
LUA_API int lua_rawequal(lua_State* L, int idx1, int idx2);
LUA_API int lua_lessthan(lua_State* L, int idx1, int idx2);

LUA_API double lua_tonumberx(lua_State* L, int idx, int* isnum);
LUA_API int lua_tointegerx(lua_State* L, int idx, int* isnum);
LUA_API unsigned lua_tounsignedx(lua_State* L, int idx, int* isnum);
LUA_API const float* lua_tovector(lua_State* L, int idx);
LUA_API int lua_toboolean(lua_State* L, int idx);
LUA_API const char* lua_tolstring(lua_State* L, int idx, size_t* len);
LUA_API const char* lua_tostringatom(lua_State* L, int idx, int* atom);
LUA_API const char* lua_tolstringatom(lua_State* L, int idx, size_t* len, int* atom);
LUA_API const char* lua_namecallatom(lua_State* L, int* atom);
LUA_API int lua_objlen(lua_State* L, int idx);
LUA_API lua_CFunction lua_tocfunction(lua_State* L, int idx);
LUA_API void* lua_tolightuserdata(lua_State* L, int idx);
LUA_API void* lua_tolightuserdatatagged(lua_State* L, int idx, int tag);
LUA_API void* lua_touserdata(lua_State* L, int idx);
LUA_API void* lua_touserdatatagged(lua_State* L, int idx, int tag);
LUA_API int lua_userdatatag(lua_State* L, int idx);
LUA_API int lua_lightuserdatatag(lua_State* L, int idx);
LUA_API lua_State* lua_tothread(lua_State* L, int idx);
LUA_API void* lua_tobuffer(lua_State* L, int idx, size_t* len);
LUA_API const void* lua_topointer(lua_State* L, int idx);

/*
** push functions (C -> stack)
*/
LUA_API void lua_pushnil(lua_State* L);
LUA_API void lua_pushnumber(lua_State* L, double n);
LUA_API void lua_pushinteger(lua_State* L, int n);
LUA_API void lua_pushunsigned(lua_State* L, unsigned n);
#if LUA_VECTOR_SIZE == 4
LUA_API void lua_pushvector(lua_State* L, float x, float y, float z, float w);
#else
LUA_API void lua_pushvector(lua_State* L, float x, float y, float z);
#endif
LUA_API void lua_pushlstring(lua_State* L, const char* s, size_t l);
LUA_API void lua_pushstring(lua_State* L, const char* s);
LUA_API const char* lua_pushvfstring(lua_State* L, const char* fmt, va_list argp);
LUA_API LUA_PRINTF_ATTR(2, 3) const char* lua_pushfstringL(lua_State* L, const char* fmt, ...);
LUA_API void lua_pushcclosurek(lua_State* L, lua_CFunction fn, const char* debugname, int nup, lua_Continuation cont);
LUA_API void lua_pushboolean(lua_State* L, int b);
LUA_API int lua_pushthread(lua_State* L);

LUA_API void lua_pushlightuserdatatagged(lua_State* L, void* p, int tag);
LUA_API void* lua_newuserdatatagged(lua_State* L, size_t sz, int tag);
LUA_API void* lua_newuserdatataggedwithmetatable(lua_State* L, size_t sz, int tag); // metatable fetched with lua_getuserdatametatable
LUA_API void* lua_newuserdatadtor(lua_State* L, size_t sz, void (*dtor)(void*));

LUA_API void* lua_newbuffer(lua_State* L, size_t sz);

/*
** get functions (Lua -> stack)
*/
LUA_API int lua_gettable(lua_State* L, int idx);
LUA_API int lua_getfield(lua_State* L, int idx, const char* k);
LUA_API int lua_rawgetfield(lua_State* L, int idx, const char* k);
LUA_API int lua_rawget(lua_State* L, int idx);
LUA_API int lua_rawgeti(lua_State* L, int idx, int n);
LUA_API void lua_createtable(lua_State* L, int narr, int nrec);

LUA_API void lua_setreadonly(lua_State* L, int idx, int enabled);
LUA_API int lua_getreadonly(lua_State* L, int idx);
LUA_API void lua_setsafeenv(lua_State* L, int idx, int enabled);

LUA_API int lua_getmetatable(lua_State* L, int objindex);
LUA_API void lua_getfenv(lua_State* L, int idx);

/*
** set functions (stack -> Lua)
*/
LUA_API void lua_settable(lua_State* L, int idx);
LUA_API void lua_setfield(lua_State* L, int idx, const char* k);
LUA_API void lua_rawsetfield(lua_State* L, int idx, const char* k);
LUA_API void lua_rawset(lua_State* L, int idx);
LUA_API void lua_rawseti(lua_State* L, int idx, int n);
LUA_API int lua_setmetatable(lua_State* L, int objindex);
LUA_API int lua_setfenv(lua_State* L, int idx);

/*
** `load' and `call' functions (load and run Luau bytecode)
*/
LUA_API int luau_load(lua_State* L, const char* chunkname, const char* data, size_t size, int env);
// ServerLua: Do an interrupt check on the tail of the current LOP_CALL instruction handler.
LUA_API void luau_interruptoncalltail(lua_State *L);
LUA_API void lua_call(lua_State* L, int nargs, int nresults);
LUA_API int lua_pcall(lua_State* L, int nargs, int nresults, int errfunc);
LUA_API int lua_cpcall(lua_State* L, lua_CFunction func, void* ud);

/*
** coroutine functions
*/
LUA_API int lua_yield(lua_State* L, int nresults);
LUA_API int lua_break(lua_State* L);
LUA_API int lua_resume(lua_State* L, lua_State* from, int narg);
LUA_API int lua_resumeerror(lua_State* L, lua_State* from);
LUA_API int lua_status(lua_State* L);
LUA_API int lua_isyieldable(lua_State* L);
LUA_API void* lua_getthreaddata(lua_State* L);
LUA_API void lua_setthreaddata(lua_State* L, void* data);
LUA_API int lua_costatus(lua_State* L, lua_State* co);

/*
** garbage-collection function and options
*/

enum lua_GCOp
{
    // stop and resume incremental garbage collection
    LUA_GCSTOP,
    LUA_GCRESTART,

    // run a full GC cycle; not recommended for latency sensitive applications
    LUA_GCCOLLECT,

    // return the heap size in KB and the remainder in bytes
    LUA_GCCOUNT,
    LUA_GCCOUNTB,

    // return 1 if GC is active (not stopped); note that GC may not be actively collecting even if it's running
    LUA_GCISRUNNING,

    /*
    ** perform an explicit GC step, with the step size specified in KB
    **
    ** garbage collection is handled by 'assists' that perform some amount of GC work matching pace of allocation
    ** explicit GC steps allow to perform some amount of work at custom points to offset the need for GC assists
    ** note that GC might also be paused for some duration (until bytes allocated meet the threshold)
    ** if an explicit step is performed during this pause, it will trigger the start of the next collection cycle
    */
    LUA_GCSTEP,

    /*
    ** tune GC parameters G (goal), S (step multiplier) and step size (usually best left ignored)
    **
    ** garbage collection is incremental and tries to maintain the heap size to balance memory and performance overhead
    ** this overhead is determined by G (goal) which is the ratio between total heap size and the amount of live data in it
    ** G is specified in percentages; by default G=200% which means that the heap is allowed to grow to ~2x the size of live data.
    **
    ** collector tries to collect S% of allocated bytes by interrupting the application after step size bytes were allocated.
    ** when S is too small, collector may not be able to catch up and the effective goal that can be reached will be larger.
    ** S is specified in percentages; by default S=200% which means that collector will run at ~2x the pace of allocations.
    **
    ** it is recommended to set S in the interval [100 / (G - 100), 100 + 100 / (G - 100))] with a minimum value of 150%; for example:
    ** - for G=200%, S should be in the interval [150%, 200%]
    ** - for G=150%, S should be in the interval [200%, 300%]
    ** - for G=125%, S should be in the interval [400%, 500%]
    */
    LUA_GCSETGOAL,
    LUA_GCSETSTEPMUL,
    LUA_GCSETSTEPSIZE,
};

LUA_API int lua_gc(lua_State* L, int what, int data);

/*
** memory statistics
** all allocated bytes are attributed to the memory category of the running thread (0..LUA_MEMORY_CATEGORIES-1)
*/

LUA_API void lua_setmemcat(lua_State* L, int category);
// ServerLua: For getting the memcat
LUA_API int lua_getmemcat(lua_State* L);
LUA_API size_t lua_totalbytes(lua_State* L, int category);

/*
** miscellaneous functions
*/

LUA_API l_noret lua_error(lua_State* L);

LUA_API int lua_next(lua_State* L, int idx);
LUA_API int lua_rawiter(lua_State* L, int idx, int iter);

LUA_API void lua_concat(lua_State* L, int n);

LUA_API uintptr_t lua_encodepointer(lua_State* L, uintptr_t p);

LUA_API double lua_clock();

LUA_API void lua_setuserdatatag(lua_State* L, int idx, int tag);

typedef void (*lua_Destructor)(lua_State* L, void* userdata);

LUA_API void lua_setuserdatadtor(lua_State* L, int tag, lua_Destructor dtor);
LUA_API lua_Destructor lua_getuserdatadtor(lua_State* L, int tag);

// alternative access for metatables already registered with luaL_newmetatable
// used by lua_newuserdatataggedwithmetatable to create tagged userdata with the associated metatable assigned
LUA_API void lua_setuserdatametatable(lua_State* L, int tag);
LUA_API void lua_getuserdatametatable(lua_State* L, int tag);

LUA_API void lua_setlightuserdataname(lua_State* L, int tag, const char* name);
LUA_API const char* lua_getlightuserdataname(lua_State* L, int tag);

LUA_API void lua_clonefunction(lua_State* L, int idx);

LUA_API void lua_cleartable(lua_State* L, int idx);
LUA_API void lua_clonetable(lua_State* L, int idx);

LUA_API lua_Alloc lua_getallocf(lua_State* L, void** ud);

// ServerLua: GC additions
LUA_API void lua_fixallcollectable(lua_State *L);
LUA_API void lua_graphheap(lua_State *L, const char *out);
LUA_API void lua_dumpgc(lua_State *L, const char *out);
LUA_API void lua_gcvalidate(lua_State *L);
// Add utility for marking something on the stack as being un-collectable
// without using the reftable. The reftable is for things that might be collectible
// at some point in the future determined by the embedder, this is for things that
// will _never_ be collectible until the VM shuts down.
LUA_API void lua_fixvalue(lua_State* L, int idx);
// Whether the object is fixed or not a collectable type
LUA_API bool lua_iscollectable(lua_State *L, int idx);
// Get how much "real" memory is used by pages.
LUA_API int lua_totalmemoverhead(lua_State *L);
// Gets the total size of all user-allocated objects reachable from a user thread.
// If free_objects is provided, objects in that set will be excluded from the size calculation.
LUA_API size_t lua_userthreadsize(lua_State *L, const lua_OpaqueGCObjectSet* free_objects);
// Collects all memcat 2+ objects reachable from a user thread into a set.
// Intended to be called immediately after luau_load() to capture bytecode constants.
LUA_API lua_OpaqueGCObjectSet lua_collectfreeobjects(lua_State *L);


/*
** reference system, can be used to pin objects
*/
#define LUA_NOREF -1
#define LUA_REFNIL 0

LUA_API int lua_ref(lua_State* L, int idx);
LUA_API void lua_unref(lua_State* L, int ref);

#define lua_getref(L, ref) lua_rawgeti(L, LUA_REGISTRYINDEX, (ref))

/*
** ===============================================================
** some useful macros
** ===============================================================
*/
#define lua_tonumber(L, i) lua_tonumberx(L, i, NULL)
#define lua_tointeger(L, i) lua_tointegerx(L, i, NULL)
#define lua_tounsigned(L, i) lua_tounsignedx(L, i, NULL)

#define lua_pop(L, n) lua_settop(L, -(n)-1)

#define lua_newtable(L) lua_createtable(L, 0, 0)
#define lua_newuserdata(L, s) lua_newuserdatatagged(L, s, 0)

#define lua_strlen(L, i) lua_objlen(L, (i))

#define lua_isfunction(L, n) (lua_type(L, (n)) == LUA_TFUNCTION)
#define lua_istable(L, n) (lua_type(L, (n)) == LUA_TTABLE)
#define lua_islightuserdata(L, n) (lua_type(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L, n) (lua_type(L, (n)) == LUA_TNIL)
#define lua_isboolean(L, n) (lua_type(L, (n)) == LUA_TBOOLEAN)
#define lua_isvector(L, n) (lua_type(L, (n)) == LUA_TVECTOR)
#define lua_isthread(L, n) (lua_type(L, (n)) == LUA_TTHREAD)
#define lua_isbuffer(L, n) (lua_type(L, (n)) == LUA_TBUFFER)
#define lua_isnone(L, n) (lua_type(L, (n)) == LUA_TNONE)
#define lua_isnoneornil(L, n) (lua_type(L, (n)) <= LUA_TNIL)

#define lua_pushliteral(L, s) lua_pushlstring(L, "" s, (sizeof(s) / sizeof(char)) - 1)
#define lua_pushcfunction(L, fn, debugname) lua_pushcclosurek(L, fn, debugname, 0, NULL)
#define lua_pushcclosure(L, fn, debugname, nup) lua_pushcclosurek(L, fn, debugname, nup, NULL)
#define lua_pushlightuserdata(L, p) lua_pushlightuserdatatagged(L, p, 0)

#define lua_setglobal(L, s) lua_setfield(L, LUA_GLOBALSINDEX, (s))
#define lua_getglobal(L, s) lua_getfield(L, LUA_GLOBALSINDEX, (s))

#define lua_tostring(L, i) lua_tolstring(L, (i), NULL)

#define lua_pushfstring(L, fmt, ...) lua_pushfstringL(L, fmt, ##__VA_ARGS__)


/*
** =======================================================================
** Ares persistence
** =======================================================================
*/

#if defined(eris_c)
/* Utility macro for populating the perms table with internal C functions. */
# define eris_persist_static(lib, fn) eris_persist_static_cont(lib, fn, doesntmatter)
# define eris_persist_static_cont(lib, fn, cont)\
    extern lua_CFunction __perm_##lib##_##fn;   \
    extern lua_Continuation __perm_##lib##_##fn##_cont; \
    if (forUnpersist) {\
      lua_pushstring(L, "__eris." #lib "_" #fn);      \
      /* upvalues on the closure are irrelevant since we'll never actually call it */ \
      lua_pushcclosurek(L, *__perm_##lib##_##fn, #fn, 0, *__perm_##lib##_##fn##_cont);\
    }\
    else {\
      lua_pushcclosurek(L, *__perm_##lib##_##fn, #fn, 0, *__perm_##lib##_##fn##_cont);\
      lua_pushstring(L, "__eris." #lib "_" #fn);\
    }\
    lua_rawset(L, -3);

// Can just use the same wrapper here.
# define eris_persist_cont(lib, fn, cont) eris_persist_static_cont(lib, fn, cont)
#else
/* Base macros for exporting C functions to eris with configurable linkage. */
# define eris_persist_base(lib, fn, prefix)\
    prefix int fn (lua_State *L);          \
    lua_CFunction __perm_##lib##_##fn = fn;     \
    lua_Continuation __perm_##lib##_##fn##_cont = nullptr;
# define eris_persist_base_cont(lib, fn, cont, prefix)\
    prefix int fn (lua_State *L);          \
    prefix int cont (lua_State *L, int idx);\
    lua_CFunction __perm_##lib##_##fn = fn;     \
    lua_Continuation __perm_##lib##_##fn##_cont = cont;

/* Static linkage macros (for internal-only functions). */
# define eris_persist_static(lib, fn) eris_persist_base(lib, fn, static)
# define eris_persist_static_cont(lib, fn, cont) eris_persist_base_cont(lib, fn, cont, static)

/* External linkage macros (for functions that need to be referenced across compilation units). */
# define eris_persist_cont(lib, fn, cont) eris_persist_base_cont(lib, fn, cont, )
#endif

#if defined(eris_c)
/* Functions in Lua libraries used to access C functions we need to add to the
 * permanents table to fully support yielded coroutines. */
static void populateperms(lua_State *L, bool forUnpersist)
{
#endif
#if defined(eris_c) || defined(lstrlib_c)
    eris_persist_static(strlib, gmatch_aux)
#endif
#if defined(eris_c) || defined(lbaselib_c)
    // these aren't continuations, these are upvalues that may end up on
    // the call stack in their own right, but can't be called directly!
    eris_persist_static(baselib, luaB_inext)
    eris_persist_static(baselib, luaB_next)
#endif
#if defined(eris_c) || defined(lcorolib_c)
    // closure created by coroutine.wrap()
    eris_persist_static_cont(corolib, auxwrapy, auxwrapcont)
#endif
#if defined(eris_c) || defined(lllevents_c)
    eris_persist_static_cont(llevents, llevents_handle_event_init, llevents_handle_event_cont)
    eris_persist_static_cont(llevents, llevents_once_wrapper, llevents_once_wrapper_cont)
    eris_persist_static(llevents, timer_wrapper_guard)
#endif
#if defined(eris_c) || defined(llltimers_c)
    eris_persist_static_cont(llltimers, lltimers_tick_init, lltimers_tick_cont)
    eris_persist_cont(llltimers, timer_event_wrapper, timer_event_wrapper_cont)
#endif
#if defined(eris_c)
}
#endif

#undef eris_persist_static
#undef eris_persist_static_cont
#undef eris_persist_base
#undef eris_persist_base_cont
#undef eris_persist_cont

LUA_API lua_State *eris_make_forkserver(lua_State *Lsrc);

LUA_API lua_State *eris_fork_thread(lua_State *Lforker, uint8_t default_state, uint8_t memcat);
LUA_API int eris_serialize_thread(lua_State *Lforker, lua_State *L);
LUA_API void eris_set_compile_func(void (*compile_func)(lua_State*, int));
LUA_API void eris_dump(lua_State* L, std::ostream *writer);
LUA_API void eris_set_setting(lua_State *L, const char *name, int value);
LUA_API void eris_populate_perms(lua_State *L, bool for_unpersist);
LUA_API void eris_register_perms(lua_State *L, bool for_unpersist);

/**
 * This provides an interface to Eris' unpersist functionality for reading
 * in an arbitrary way, using a reader.
 *
 * When called, the stack in 'L' must look like this:
 * 1: perms:table
 *
 * 'reader' is the reader stream used to read all data
 *
 * The result of the operation will be pushed onto the stack.
 *
 * [-0, +1, e]
 */
LUA_API void eris_undump(lua_State* L, std::istream *reader);

/*
** {======================================================================
** Debug API
** =======================================================================
*/

typedef struct lua_Debug lua_Debug; // activation record

// Functions to be called by the debugger in specific events
typedef void (*lua_Hook)(lua_State* L, lua_Debug* ar);

LUA_API int lua_stackdepth(lua_State* L);
LUA_API int lua_getinfo(lua_State* L, int level, const char* what, lua_Debug* ar);
LUA_API int lua_getargument(lua_State* L, int level, int n);
LUA_API const char* lua_getlocal(lua_State* L, int level, int n);
LUA_API const char* lua_setlocal(lua_State* L, int level, int n);
LUA_API const char* lua_getupvalue(lua_State* L, int funcindex, int n);
LUA_API const char* lua_setupvalue(lua_State* L, int funcindex, int n);
// Ares: get a pointer to the actual upvalue address rather than
// pushing its value onto the stack
LUA_API void* lua_getupvalueid(lua_State* L, int funcindex, int n);

LUA_API void lua_singlestep(lua_State* L, int enabled);
LUA_API int lua_breakpoint(lua_State* L, int funcindex, int line, int enabled);

typedef void (*lua_Coverage)(void* context, const char* function, int linedefined, int depth, const int* hits, size_t size);

LUA_API void lua_getcoverage(lua_State* L, int funcindex, void* context, lua_Coverage callback);

// Warning: this function is not thread-safe since it stores the result in a shared global array! Only use for debugging.
LUA_API const char* lua_debugtrace(lua_State* L);

// ServerLua: for debugging!
LUA_API void lua_dumpstack(lua_State *L);

struct lua_Debug
{
    const char* name;      // (n)
    const char* what;      // (s) `Lua', `C', `main', `tail'
    const char* source;    // (s)
    const char* short_src; // (s)
    int linedefined;       // (s)
    int currentline;       // (l)
    unsigned char nupvals; // (u) number of upvalues
    unsigned char nparams; // (a) number of parameters
    char isvararg;         // (a)
    void* userdata;        // only valid in luau_callhook

    char ssbuf[LUA_IDSIZE];
};

// }======================================================================

/* Callbacks that can be used to reconfigure behavior of the VM dynamically.
 * These are shared between all coroutines.
 *
 * Note: interrupt is safe to set from an arbitrary thread but all other callbacks
 * can only be changed when the VM is not running any code */
struct lua_Callbacks
{
    void* userdata; // arbitrary userdata pointer that is never overwritten by Luau

    void (*interrupt)(lua_State* L, int gc);  // gets called at safepoints (loop back edges, call/ret, gc) if set
    void (*panic)(lua_State* L, int errcode); // gets called when an unprotected error is raised (if longjmp is used)

    void (*userthread)(lua_State* LP, lua_State* L); // gets called when L is created (LP == parent) or destroyed (LP == NULL)
    int16_t (*useratom)(const char* s, size_t l);    // gets called when a string is created; returned atom can be retrieved via tostringatom

    void (*debugbreak)(lua_State* L, lua_Debug* ar);     // gets called when BREAK instruction is encountered
    void (*debugstep)(lua_State* L, lua_Debug* ar);      // gets called after each instruction in single step mode
    void (*debuginterrupt)(lua_State* L, lua_Debug* ar); // gets called when thread execution is interrupted by break in another thread
    void (*debugprotectederror)(lua_State* L);           // gets called when protected call results in an error

    void (*onallocate)(lua_State* L, size_t osize, size_t nsize); // gets called when memory is allocated
    // gets called before memory is allocated, return non-zero to fail the alloc. Only called for allocs in user memcats.
    int (*beforeallocate)(lua_State* L, size_t osize, size_t nsize);
};
typedef struct lua_Callbacks lua_Callbacks;

LUA_API lua_Callbacks* lua_callbacks(lua_State* L);

/******************************************************************************
 * Copyright (c) 2019-2023 Roblox Corporation
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************/
