/*
Ares - Heavy-duty persistence for Luau - Based on Eris, based on Pluto
Copyright (c) 2022 by Harold Cindy
Copyright (c) 2013-2015 by Florian Nuecke.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/* The API still uses the "eris" prefix throughout, and uses the old whitespace
 * style to make diffing against Eris proper easier.
 */

/* Standard library headers. */
#include <cstdio>
#include <cstring>
#include <ostream>
#include <istream>
#include <sstream>
#include <vector>

/* Mark us as part of the Lua core to get access to what we need. */
#define eris_c
#define LUA_CORE

/* Public Lua headers. */
#include "lua.h"
#include "lualib.h"

/* Internal Lua headers. */
#include "lbuffer.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"

/* Eris header. */
#include "ares.h"
#include "lapi.h"
#include "ltable.h"
#include "ludata.h"
#include "llsl.h"
#include "lljson.h"
#include "Luau/Bytecode.h"

/*
** {===========================================================================
** Default settings.
** ============================================================================
*/

/* Note that these are the default settings. They can be changed either from C
 * by calling ares_g|set_setting() or from Lua using ares.settings(). */

/* Whether to persist debug information such as line numbers and upvalue and
 * local variable names. */
static const bool kWriteDebugInformation = true;

/* Generate a human-readable "path" that is shown together with error messages
 * to indicate where in the object the error occurred. For example:
 * ares.persist({false, bad = setmetatable({}, {__persist = false})})
 * Will produce: main:1: attempt to persist forbidden table (root.bad)
 * This can be used for debugging, but is disabled per default due to the
 * processing and memory overhead this introduces. */
static const bool kGeneratePath = false;

/* The maximum object complexity. This is the number of allowed recursions when
 * persisting or unpersisting an object, for example for nested tables. This is
 * used to avoid segfaults when writing or reading user data. */
static const lua_Unsigned kMaxComplexity = 10000;

/*
** ============================================================================
** Lua internals interfacing.
** ============================================================================
*/

/* Lua internals we use. We define these as macros to make it easier to swap
 * them out, should the need ever arise. For example, the later Pluto versions
 * copied these function to own files (presumably to allow building it as an
 * extra shared library). These should be all functions we use that are not
 * declared in lua.h or lauxlib.h. If there are some still directly in the code
 * they were missed and should be replaced with a macro added here instead. */
/* I'm quite sure we won't ever want to do this, because Eris needs a slightly
 * patched Lua version to be able to persist some of the library functions,
 * anyway: it needs to put the continuation C functions in the perms table. */
/* ldebug.h */
#define eris_ci_func ci_func
/* ldo.h */
#define eris_incr_top incr_top
#define eris_savestack savestack
#define eris_restorestack restorestack
#define eris_reallocstack luaD_reallocstack
/* lfunc.h */
#define eris_newproto luaF_newproto
#define eris_newLclosure luaF_newLclosure
#define eris_newupval luaF_newupval
#define eris_findupval luaF_findupval
/* lgc.h */
#define eris_barrierproto luaC_barrierproto
/* lmem.h */
#define eris_reallocvector(L, b, on, n, e) luaM_reallocarray((L), (b), (on), (n), e, (L)->activememcat)
/* lobject.h */
#define eris_ttypenv(o) ((o)->tt)
#define eris_clLvalue clLvalue
#define eris_setnilvalue setnilvalue
#define eris_setclLvalue setclLvalue
#define eris_setobj setobj
#define eris_setsvalue2n setsvalue
/* lstate.h */
#define eris_isLua isLua
#define eris_gch gch
#define eris_gco2uv gco2uv
#define eris_obj2gco obj2gco
#define eris_extendCI luaE_extendCI
/* lstring. h */
#define eris_newlstr luaS_newlstr

/* These are required for cross-platform support, since the size of TValue may
 * differ, so the byte offset used by savestack/restorestack in Lua it is not a
 * valid measure. */
#define eris_savestackidx(L, p) ((p) - (L)->stack)
#define eris_restorestackidx(L, n) ((L)->stack + (n))

/* CodeGen doesn't live in Luau.VM, so we need to dynamically pass a compile
 * callback so that we aren't tightly bound to Luau.CodeGen at compile time. */
static void (*sAresCodeGenCompile)(lua_State *, int) = nullptr;

// Constant size so that 32-bit and 64-bit systems can load each other's serialized code
typedef uint64_t ares_size_t;

/*
** ============================================================================
** Language strings for errors.
** ============================================================================
*/

#define ERIS_ERR_CFUNC "attempt to persist a light C function (%p) \"%s\""
#define ERIS_ERR_CFUNC_UPVALS "attempt to persist non-permanent C function "\
                              "with upvals (%p) \"%s\""
#define ERIS_ERR_COMPLEXITY "object too complex"
#define ERIS_ERR_HOOK "cannot persist yielded hooks"
#define ERIS_ERR_METATABLE "bad metatable, not nil or table"
#define ERIS_ERR_NOFUNC "attempt to persist unknown function type"
#define ERIS_ERR_READ "could not read data"
#define ERIS_ERR_SPER_FUNC "%s did not return a function"
#define ERIS_ERR_SPER_LOAD "bad unpersist function (%s expected, returned %s)"
#define ERIS_ERR_SPER_PROT "attempt to persist forbidden table"
#define ERIS_ERR_SPER_TYPE "%d not nil, boolean, or function"
#define ERIS_ERR_SPER_UFUNC "invalid restore function"
#define ERIS_ERR_SPER_UPERM "bad permanent value (%s expected, got %s)"
#define ERIS_ERR_SPER_UPERMNIL "bad permanent value (no value)"
#define ERIS_ERR_STACKBOUNDS "stack index out of bounds"
#define ERIS_ERR_TABLE "bad table value, got a nil value"
#define ERIS_ERR_TABLE_COUNT "table had a different key count after "\
                                "deserialize, expected %d, got %d"
#define ERIS_ERR_THREAD "cannot persist currently running thread"
#define ERIS_ERR_THREADCI "invalid callinfo"
#define ERIS_ERR_UPVAL_IDX "invalid upvalue index %d"
#define ERIS_ERR_THREADCTX "bad C continuation function"
#define ERIS_ERR_THREADERRF "invalid errfunc"
#define ERIS_ERR_THREADPC "saved program counter out of bounds"
#define ERIS_ERR_TRUNC_INT "int value would get truncated"
#define ERIS_ERR_TRUNC_SIZE "size_t value would get truncated"
#define ERIS_ERR_TYPE_FLOAT "unsupported lua_Number type"
#define ERIS_ERR_TYPE_INT "unsupported int type"
#define ERIS_ERR_TYPE_SIZE "unsupported size_t type"
#define ERIS_ERR_TYPEP "trying to persist unknown type %d"
#define ERIS_ERR_TYPEU "trying to unpersist unknown type %d"
#define ERIS_ERR_UCFUNC "bad C closure (C function expected, got %s)"
#define ERIS_ERR_UCFUNCNULL "bad C closure (C function expected, got null)"
#define ERIS_ERR_USERDATA "attempt to literally persist userdata"
#define ERIS_ERR_WRITE "could not write data"
#define ERIS_ERR_REF "invalid reference #%d. this usually means a special "\
                      "persistence callback of a table referenced said table "\
                      "(directly or indirectly via an upvalue)."
#define ERIS_ERR_INVAL_PC "Tried to serialize thread yielded at invalid point"

/*
** ============================================================================
** Constants, settings, types and forward declarations.
** ============================================================================
*/

/* The "type" we write when we persist a value via a replacement from the
 * permanents table. This is just an arbitrary number, but it must be outside
 * the range Lua uses for its types (> LUA_TOTALTAGS). */
#define ERIS_PERMANENT (LUA_TDEADKEY + 1)
/* The "type" we use to reference something from the (ephemeral) reftable */
#define ERIS_REFERENCE (ERIS_PERMANENT + 1)

/* Avoids having to write the nullptr all the time, plus makes it easier adding
 * a custom error message should you ever decide you want one. */
#define eris_checkstack(L, n) luaL_checkstack(L, n, nullptr)

/* Validates that unpersisted data has the expected type. Raises an error on mismatch. */
#define eris_checktype(info, idx, expected_type) \
  do { \
    if (lua_type((info)->L, (idx)) != (expected_type)) { \
      eris_error((info), "malformed data: expected %s, got %s", \
                 kTypenames[(expected_type)], kTypenames[lua_type((info)->L, (idx))]); \
    } \
  } while(0)

/* Validates that unpersisted userdata has the expected tag. Raises an error on mismatch. */
#define eris_checkuserdatatag(info, idx, expected_tag) \
  do { \
    if (lua_userdatatag((info)->L, (idx)) != (expected_tag)) { \
      eris_error((info), "malformed data: expected userdata tag %d, got %d", \
                 (int)(expected_tag), (int)lua_userdatatag((info)->L, (idx))); \
    } \
  } while(0)

/* Used for internal consistency checks, for debugging. These are true asserts
 * in the sense that they should never fire, even for bad inputs. */
#if !defined(NDEBUG) || defined(LUAU_ENABLE_ASSERT)
#define eris_assert(c) LUAU_ASSERT(c)
#define eris_ifassert(e) e
#else
#define eris_assert(c) ((void)0)
#define eris_ifassert(e) ((void)0)
#endif

/* State information when persisting an object. */
typedef struct PersistInfo {
  std::ostream *writer;
  void *ud;
  bool writeDebugInfo;
  bool persistingCFunc;
} PersistInfo;

typedef uint8_t lu_byte;

/* State information when unpersisting an object. */
typedef struct UnpersistInfo {
  std::istream * reader;
  size_t sizeof_int;
  size_t sizeof_size_t;
  size_t vector_components;
  uint32_t version;
} UnpersistInfo;

/* Info shared in persist and unpersist. */
typedef struct Info {
  lua_State *L;
  lua_Unsigned level;
  int refcount; /* int because rawseti/rawgeti takes an int. */
  lua_Unsigned maxComplexity;
  bool generatePath;
  bool persisting;
  bool anyProtoNative;
  /* Which one it really is will always be clear from the context. */
  union {
    PersistInfo pi;
    UnpersistInfo upi;
  } u;
} Info;

typedef enum eris_CIKind {
    ERIS_CI_KIND_NONE = 0,
    ERIS_CI_KIND_LUA = 1,
    ERIS_CI_KIND_C = 2,
} eris_CIKind;

/* Type names, used for error messages. */
static const char *const kTypenames[] = {
  "nil", "boolean", "lightuserdata", "number", "string",
  "table", "function", "userdata", "thread", "proto", "upval",
  "deadkey", "permanent"
};

/* Setting names as used in ares.settings / ares_g|set_setting. Also, the
 * addresses of these static variables are used as keys in the registry of Lua
 * states to save the current values of the settings (as light userdata). */
static const char kSettingGeneratePath[] = "path";
static const char kSettingWriteDebugInfo[] = "debug";
static const char kSettingMaxComplexity[] = "maxrec";
static const char kForkerPermsTable[] = "forkerpermstable";
static const char kForkerUPermsTable[] = "forkerupermstable";
static const char kForkerBaseThread[] = "forkerbasethread";
static const char kForkerBaseState[] = "forkerbasestate";

/* Header we prefix to persisted data for a quick check when unpersisting. */
static char const kHeader[] = { 'A', 'R', 'E', 'S' };
#define HEADER_LENGTH sizeof(kHeader)

/* Floating point number used to check compatibility of loaded data. */
static const lua_Number kHeaderNumber = (lua_Number)-1.234567890;

/* Version number for the file format. */
static const uint32_t kCurrentVersion = 1;
/* Old format magic bytes (0x08, 0x1B, 0xDE, 0x83 in little-endian). */
static const uint32_t kOldMagicBytes = 0x83DE1B08;

/* Stack indices of some internal values/tables, to avoid magic numbers. */
#define PERMIDX 1
#define REFTIDX 2
#define BUFFIDX 3
#define PATHIDX 4

/* Table indices for upvalue tables, keeping track of upvals to open. */
#define UVTOCL 1
#define UVTONU 2
#define UVTVAL 3
#define UVTREF 4

// Stack indices for forkserver threads
#define FS_STATE_IDX 1

// Internal lightuserdata tags for Ares. These are
// intentionally above the regular lightuserdata limit
// since they're an internal detail for deserialization state.
#define LUTAG_ARES_START 150
#define LUTAG_ARES_BOXED_NIL (LUTAG_ARES_START)
#define LUTAG_ARES_UPREF (LUTAG_ARES_START + 1)

/* }======================================================================== */

/*
** {===========================================================================
** Utility functions.
** ============================================================================
*/

/* Temporarily disable GC collections while this object is live, restoring GC
 * parameters when it goes out of scope.
 */
struct ScopedDisableGC {
    explicit ScopedDisableGC(lua_State *L):
            state(L), threshold(L->global->GCthreshold) {
        state->global->GCthreshold = SIZE_MAX;
    };

    ~ScopedDisableGC() {
        state->global->GCthreshold = threshold;
    }

    lua_State *state;
    size_t threshold;
};

static const char* path(Info *info);

static int
allocate_ref_idx(Info *info) {
  int ref = ++(info->refcount);
#if defined(ERIS_DEBUG_REFERENCES)
  fprintf(stderr, "allocated %d at %s\n", ref, path(info));
  lua_pop(info->L, 1);
#endif
  return ref;
}

/* Pushes an object into the reference table when unpersisting. This creates an
 * entry pointing from the id the object is referenced by to the object. */
static int
registerobject(Info *info) {                          /* perms reftbl ... obj */
  const int reference = allocate_ref_idx(info);
  eris_checkstack(info->L, 1);
  lua_pushvalue(info->L, -1);                     /* perms reftbl ... obj obj */
  lua_rawseti(info->L, REFTIDX, reference);           /* perms reftbl ... obj */
  return reference;
}

/** ======================================================================== */

/* Pushes a TString* onto the stack if it holds a value, nil if it is nullptr. */
static void
pushtstring(lua_State* L, TString *ts) {                               /* ... */
  if (ts) {
    eris_setsvalue2n(L, L->top, ts);
    eris_incr_top(L);                                              /* ... str */
  }
  else {
    lua_pushnil(L);                                                /* ... nil */
  }
}

/* Creates a copy of the string on top of the stack and sets it as the value
 * of the specified TString**. */
static void
copytstring(lua_State* L, TString **ts) {
  if (lua_type(L, -1) == LUA_TNIL) {
    *ts = nullptr;
  } else {
    size_t length;
    const char* value = lua_tolstring(L, -1, &length);
    *ts = eris_newlstr(L, value, length);
  }
}

/** ======================================================================== */

// Get an opaque, stable identifier to an upvalue that will be valid for
// the duration of the persist() call.
static void *eris_getupvalueid_safe(Info *info, int funcindex, int n) {
  void *uv_id = lua_getupvalueid(info->L, funcindex, n);
  // We have to be careful about addresses that are on the stack that
  // persist() is running on. We may need to realloc() our stack as part
  // of serializing, which will invalidate any existing UpValue addresses.
  // Sniff out addresses within our stack, and key them off of a
  // stack-relative address instead.
  if (uv_id >= info->L->stack && uv_id < info->L->top) {
    return (void*)(eris_savestackidx(info->L, (StkId)uv_id));
  }
  return uv_id;
}

/** ======================================================================== */

// Get a pointer to the "real" globals base table, not the sandboxed proxy table.
// This is important for thread serialization where we want to serialize any
// mutations the thread may have done to its sandboxed globals, but we want to
// preserve the "proxy-ness" of the thread's global table.
LuaTable *eris_getglobalsbase(lua_State *L) {
  lua_State *GL = lua_mainthread(L);
  LuaTable * base_gt = GL->gt;
  // Looks like the global thread has sandboxed globals, we have to do a bit
  // of spelunking to get a reference to the base globals table.
  if (base_gt->metatable && !base_gt->readonly) {
    eris_assert(base_gt->metatable->readonly);
    // The real table should live in the metatable's __index
    TString *index_key = eris_newlstr(L, "__index", strlen("__index"));
    auto *gt_index_val = luaH_getstr(base_gt->metatable, index_key);
    if (ttistable(gt_index_val)) {
        base_gt = hvalue(gt_index_val);
    }
  }
  return base_gt;
}

/** ======================================================================== */

/* Pushes the specified segment to the current path, if we're generating one.
 * This supports formatting strings using Lua's formatting capabilities. */
static void
pushpath(Info *info, const char* fmt, ...) {     /* perms reftbl var path ... */
  if (!info->generatePath) {
    return;
  }
  else {
    va_list argp;
    eris_checkstack(info->L, 1);
    va_start(argp, fmt);
    lua_pushvfstring(info->L, fmt, argp);    /* perms reftbl var path ... str */
    va_end(argp);
    lua_rawseti(info->L, PATHIDX, lua_objlen(info->L, PATHIDX) + 1);
  }                                              /* perms reftbl var path ... */
}

/* Pops the last added segment from the current path if we're generating one. */
static void
poppath(Info *info) {                            /* perms reftbl var path ... */
  if (!info->generatePath) {
    return;
  }
  eris_checkstack(info->L, 1);
  lua_pushnil(info->L);                      /* perms reftbl var path ... nil */
  lua_rawseti(info->L, PATHIDX, lua_objlen(info->L, PATHIDX));
}                                                /* perms reftbl var path ... */

/* Concatenates all current path segments into one string, pushes it and
 * returns it. This is relatively inefficient, but it's for errors only and
 * keeps the stack small, so it's better this way. */
static const char*
path(Info *info) {                               /* perms reftbl var path ... */
  if (!info->generatePath) {
    lua_pushstring(info->L, "");
    return lua_tostring(info->L, -1);
  }
  eris_checkstack(info->L, 3);
  lua_pushstring(info->L, "");               /* perms reftbl var path ... str */
  lua_pushnil(info->L);                  /* perms reftbl var path ... str nil */
  while (lua_next(info->L, PATHIDX)) {   /* perms reftbl var path ... str k v */
    lua_insert(info->L, -2);             /* perms reftbl var path ... str v k */
    lua_insert(info->L, -3);             /* perms reftbl var path ... k str v */
    lua_concat(info->L, 2);                /* perms reftbl var path ... k str */
    lua_insert(info->L, -2);               /* perms reftbl var path ... str k */
  }                                          /* perms reftbl var path ... str */
  return lua_tostring(info->L, -1);
}

/* Generates an error message with the appended path, if available. */
static l_noret
eris_error(Info *info, const char *fmt, ...) {                         /* ... */
    va_list argp;
    eris_checkstack(info->L, 5);

    luaL_where(info->L, 1);                                     /* ... where */
    va_start(argp, fmt);
    lua_pushvfstring(info->L, fmt, argp);                    /* ... where str */
    va_end(argp);
    if (info->generatePath) {
      lua_pushstring(info->L, " (");                    /* ... where str " (" */
      path(info);                                 /* ...  where str " (" path */
      lua_pushstring(info->L, ")");            /* ... where str " (" path ")" */
      lua_concat(info->L, 5);                                      /* ... msg */
    }
    else {
      lua_concat(info->L, 2);                                      /* ... msg */
    }
    lua_error(info->L);
}

/** ======================================================================== */

/* Tries to get a setting from the registry. */
static bool
get_setting(lua_State *L, void *key) {                                 /* ... */
  eris_checkstack(L, 1);
  lua_pushlightuserdata(L, key);                                   /* ... key */
  lua_gettable(L, LUA_REGISTRYINDEX);                        /* ... value/nil */
  if (lua_isnil(L, -1)) {                                          /* ... nil */
    lua_pop(L, 1);                                                     /* ... */
    return false;
  }                                                              /* ... value */
  return true;
}

/* Stores a setting in the registry (or removes it if the value is nil). */
static void
set_setting(lua_State *L, void *key) {                           /* ... value */
  eris_checkstack(L, 2);
  lua_pushlightuserdata(L, key);                             /* ... value key */
  lua_insert(L, -2);                                         /* ... key value */
  lua_settable(L, LUA_REGISTRYINDEX);                                  /* ... */
}

/* Used as a callback for luaL_opt to check boolean setting values. */
static bool
checkboolean(lua_State *L, int narg) {                       /* ... bool? ... */
  if (!lua_isboolean(L, narg)) {                                /* ... :( ... */
    luaL_argerror(L, narg, lua_pushfstring(L,
      "boolean expected, got %s", lua_typename(L, lua_type(L, narg))));
    return false;
  }                                                           /* ... bool ... */
  return lua_toboolean(L, narg);
}

/* }======================================================================== */

/*
** {===========================================================================
** Persist and unpersist.
** ============================================================================
*/

/* I have macros and I'm not afraid to use them! These are highly situational
 * and assume an Info* named 'info' is available. */

/* Writes a raw memory block with the specified size. */
#define WRITE_RAW(value, size) { \
  info->u.pi.writer->write((char*)(value), (size)); \
  if (info->u.pi.writer->fail()) \
    eris_error(info, ERIS_ERR_WRITE); } while(0)

/* Writes a single value with the specified type. */
#define WRITE_VALUE(value, type) write_##type(info, value)

/* Writes a typed array with the specified length. */
#define WRITE(value, length, type) { \
    int _i; for (_i = 0; _i < length; ++_i) WRITE_VALUE((value)[_i], type); } while(0)

/** ======================================================================== */

/* Reads a raw block of memory with the specified size. */
#define READ_RAW(value, size) {\
  info->u.upi.reader->read((char *)(value), (size)); \
  if (info->u.upi.reader->fail())        \
    eris_error(info, ERIS_ERR_READ); } while(0)

/* Reads a single value with the specified type. */
#define READ_VALUE(type) read_##type(info)

/* Reads a typed array with the specified length. */
#define READ(value, length, type) { \
    int _i; for (_i = 0; _i < (length); ++_i) (value)[_i] = READ_VALUE(type); }

/* For safely reallocing a vector, only setting the size field after
 * the realloc succeeds. This prevents insanity if the alloc raises and the
 * destructor for the container triggers. */
#define SAFE_ALLOC_VECTOR(L, b, on, size_type, size_field, e) do { \
size_type _size_val = READ_VALUE(size_type); \
eris_reallocvector((L), (b), (on), _size_val, e); \
size_field = _size_val; \
} while (0)

/** ======================================================================== */

static void
write_uint8_t(Info *info, uint8_t value) {
  WRITE_RAW(&value, sizeof(uint8_t));
}

static void
write_uint32_t(Info *info, uint32_t value) {
  write_uint8_t(info, (uint8_t)value);
  write_uint8_t(info, (uint8_t)(value >> 8));
  write_uint8_t(info, (uint8_t)(value >> 16));
  write_uint8_t(info, (uint8_t)(value >> 24));
}

static void
write_uint64_t(Info *info, uint64_t value) {
  write_uint8_t(info, (uint8_t)value);
  write_uint8_t(info, (uint8_t)(value >> 8));
  write_uint8_t(info, (uint8_t)(value >> 16));
  write_uint8_t(info, (uint8_t)(value >> 24));
  write_uint8_t(info, (uint8_t)(value >> 32));
  write_uint8_t(info, (uint8_t)(value >> 40));
  write_uint8_t(info, (uint8_t)(value >> 48));
  write_uint8_t(info, (uint8_t)(value >> 56));
}

static void
write_int32_t(Info *info, int32_t value) {
  write_uint32_t(info, (uint32_t)value);
}

static void
write_float32(Info *info, float value) {
  uint32_t rep;
  memcpy(&rep, &value, sizeof(float));
  write_uint32_t(info, rep);
}

static void
write_float64(Info *info, double value) {
  uint64_t rep;
  memcpy(&rep, &value, sizeof(double));
  write_uint64_t(info, rep);
}

/* Note regarding the following: any decent compiler should be able
 * to reduce these to just the write call, since sizeof is constant. */

static void
write_int(Info *info, int value) {
  if (sizeof(int) <= sizeof(int32_t)) {
    write_int32_t(info, value);
  }
  else {
    eris_error(info, ERIS_ERR_TYPE_INT);
  }
}

static void
write_ares_size_t(Info *info, ares_size_t value) {
  if (sizeof(size_t) <= sizeof(uint64_t)) {
    write_uint64_t(info, (uint64_t)value);
  }
  else {
    eris_error(info, ERIS_ERR_TYPE_SIZE);
  }
}

static void
write_lua_Number(Info *info, lua_Number value) {
  if (sizeof(lua_Number) == sizeof(uint64_t)) {
    write_float64(info, value);
  }
  else {
    eris_error(info, ERIS_ERR_TYPE_FLOAT);
  }
}

/* Note that Lua only ever uses 32 bits of the Instruction type, so we can
 * assert that there will be no truncation, even if the underlying type has
 * more bits (might be the case on some 64 bit systems). */

static void
write_Instruction(Info *info, Instruction value) {
  if (sizeof(Instruction) == sizeof(uint32_t)) {
    write_uint32_t(info, value);
  }
  else {
    auto pvalue = (uint32_t)value;
    /* Lua only uses 32 bits for its instructions. */
    eris_assert((Instruction)pvalue == value);
    write_uint32_t(info, pvalue);
  }
}

/** ======================================================================== */

static uint8_t
read_uint8_t(Info *info) {
  uint8_t value;
  READ_RAW(&value, sizeof(uint8_t));
  return value;
}

static uint16_t
read_uint16_t(Info *info) {
  auto value = (uint16_t)read_uint8_t(info);
  value |= (uint16_t)read_uint8_t(info) << 8;
  return value;
}

static uint32_t
read_uint32_t(Info *info) {
  auto value = (uint32_t)read_uint8_t(info);
  value |= (uint32_t)read_uint8_t(info) << 8;
  value |= (uint32_t)read_uint8_t(info) << 16;
  value |= (uint32_t)read_uint8_t(info) << 24;
  return value;
}

static uint64_t
read_uint64_t(Info *info) {
  auto value = (uint64_t)read_uint8_t(info);
  value |= (uint64_t)read_uint8_t(info) << 8;
  value |= (uint64_t)read_uint8_t(info) << 16;
  value |= (uint64_t)read_uint8_t(info) << 24;
  value |= (uint64_t)read_uint8_t(info) << 32;
  value |= (uint64_t)read_uint8_t(info) << 40;
  value |= (uint64_t)read_uint8_t(info) << 48;
  value |= (uint64_t)read_uint8_t(info) << 56;
  return value;
}

static int16_t
read_int16_t(Info *info) {
  return (int16_t)read_uint16_t(info);
}

static int32_t
read_int32_t(Info *info) {
  return (int32_t)read_uint32_t(info);
}

static int64_t
read_int64_t(Info *info) {
  return (int64_t)read_uint64_t(info);
}

static float
read_float32(Info *info) {
  float value;
  uint32_t rep = read_uint32_t(info);
  memcpy(&value, &rep, sizeof(float));
  return value;
}

static double
read_float64(Info *info) {
  double value;
  uint64_t rep = read_uint64_t(info);
  memcpy(&value, &rep, sizeof(double));
  return value;
}

/* Note regarding the following: unlike with writing the sizeof check will be
 * impossible to optimize away, since it depends on the input; however, the
 * truncation check may be optimized away in the case where the read data size
 * equals the native one, so reading data written on the same machine should be
 * reasonably quick. Doing a (rather rudimentary) benchmark this did not have
 * any measurable impact on performance. */

static int
read_int(Info *info) {
  int value;
  if (info->u.upi.sizeof_int == sizeof(int16_t)) {
    int16_t pvalue = read_int16_t(info);
    value = (int)pvalue;
    if ((int32_t)value != pvalue) {
      eris_error(info, ERIS_ERR_TRUNC_INT);
    }
  }
  else if (info->u.upi.sizeof_int == sizeof(int32_t)) {
    int32_t pvalue = read_int32_t(info);
    value = (int)pvalue;
    if ((int32_t)value != pvalue) {
      eris_error(info, ERIS_ERR_TRUNC_INT);
    }
  }
  else if (info->u.upi.sizeof_int == sizeof(int64_t)) {
    int64_t pvalue = read_int64_t(info);
    value = (int)pvalue;
    if ((int64_t)value != pvalue) {
      eris_error(info, ERIS_ERR_TRUNC_INT);
    }
  }
  else {
    eris_error(info, ERIS_ERR_TYPE_INT);
    value = 0; /* not reached */
  }
  return value;
}

static ares_size_t
read_ares_size_t(Info *info) {
  ares_size_t value;
  if (info->u.upi.sizeof_size_t <= sizeof(uint64_t)) {
    return read_uint64_t(info);
  }
  else {
    eris_error(info, ERIS_ERR_TYPE_SIZE);
    value = 0; /* not reached */
  }
  return value;
}

static lua_Number
read_lua_Number(Info *info) {
  if (sizeof(lua_Number) == sizeof(uint64_t)) {
    return read_float64(info);
  }
  else {
    eris_error(info, ERIS_ERR_TYPE_FLOAT);
    return 0; /* not reached */
  }
}

static Instruction
read_Instruction(Info *info) {
  return (Instruction)read_uint32_t(info);
}

/** ======================================================================== */

/* Forward declarations for recursively called top-level functions. */
static void persist_keyed(Info*, int type);
static void persist(Info*);
static void unpersist(Info*);

/*
** ============================================================================
** Simple types.
** ============================================================================
*/

static void
p_boolean(Info *info) {                                           /* ... bool */
  // Have to use this rather than toboolean so that we keep the full int value
  WRITE_VALUE(bvalue(info->L->top - 1), int32_t);
}

static void
u_boolean(Info *info) {                                                /* ... */
  eris_checkstack(info->L, 1);
  // Have to use this rather than pushboolean so that we keep the full int value
  setbvalue(info->L->top, READ_VALUE(int32_t));                   /* ... bool */
  eris_incr_top(info->L);

  eris_checktype(info, -1, LUA_TBOOLEAN);
}

/** ======================================================================== */

static void
p_pointer(Info *info) {                                         /* ... ludata */
  WRITE_VALUE((uint8_t)lua_lightuserdatatag(info->L, -1), uint8_t);
  WRITE_VALUE((ares_size_t)lua_touserdata(info->L, -1), ares_size_t);
}

static void
u_pointer(Info *info) {                                                /* ... */
  eris_checkstack(info->L, 1);
  uint8_t tag = READ_VALUE(uint8_t);
  void *ptr = (void*)READ_VALUE(ares_size_t);
  lua_pushlightuserdatatagged(info->L, ptr, tag);    /* ... ludata */

  eris_checktype(info, -1, LUA_TLIGHTUSERDATA);
}

/** ======================================================================== */

static void
p_number(Info *info) {                                             /* ... num */
  WRITE_VALUE(lua_tonumber(info->L, -1), lua_Number);
}

static void
u_number(Info *info) {                                                 /* ... */
  eris_checkstack(info->L, 1);
  lua_pushnumber(info->L, READ_VALUE(lua_Number));                 /* ... num */

  eris_checktype(info, -1, LUA_TNUMBER);
}

/** ======================================================================== */

static void
p_vector(Info *info) {                                             /* ... vec */
  const float *f = lua_tovector(info->L, -1);
  for (size_t i=0; i<LUA_VECTOR_SIZE; ++i) {
    WRITE_VALUE(*(f + i), float32);
  }
}

static void
u_vector(Info *info) {                                                 /* ... */
  if (info->u.upi.vector_components > LUA_VECTOR_SIZE) {
    eris_error(info, ERIS_ERR_TRUNC_SIZE);
  }

  eris_checkstack(info->L, 1);
  // Vectors are _specifically_ 32-bit floats.
  float v[LUA_VECTOR_SIZE];
  for (size_t i=0; i<LUA_VECTOR_SIZE; ++i) {
    v[i] = read_float32(info);
  }

#if LUA_VECTOR_SIZE == 4
  lua_pushvector(info->L, v[0], v[1], v[2], v[3]);                 /* ... vec */
#else
  lua_pushvector(info->L, v[0], v[1], v[2]);                       /* ... vec */
#endif

  eris_checktype(info, -1, LUA_TVECTOR);
}


/** ======================================================================== */

static void
p_string(Info *info) {                                             /* ... str */
  size_t length;
  const char *value = lua_tolstring(info->L, -1, &length);
  WRITE_VALUE(length, ares_size_t);
  WRITE_RAW(value, length);
}

static void
u_string(Info *info) {                                                 /* ... */
  eris_checkstack(info->L, 2);
  {
    /* TODO Can we avoid this copy somehow? (Without it getting too nasty) */
    const size_t length = (size_t)READ_VALUE(ares_size_t);
    char *value = (char*)lua_newuserdata(info->L, length * sizeof(char)); /* ... tmp */
    READ_RAW(value, length);
    lua_pushlstring(info->L, value, length);                   /* ... tmp str */
    lua_replace(info->L, -2);                                      /* ... str */
  }
  registerobject(info);

  eris_checktype(info, -1, LUA_TSTRING);
}


/** ======================================================================== */


static void
p_buffer(Info *info) {                                            /* ... buf */
  const TValue * buf_tv = luaA_toobject(info->L, -1);
  eris_assert(ttisbuffer(buf_tv));
  Buffer *buf = bufvalue(buf_tv);
  WRITE_VALUE(buf->len, ares_size_t);
  WRITE_RAW(buf->data, buf->len);
}

static void
u_buffer(Info *info) {                                                /* ... */
  eris_checkstack(info->L, 2);
  {
    const size_t length = (size_t)READ_VALUE(ares_size_t);
    char *buf_data = (char *)lua_newbuffer(info->L, length);      /* ... buf */
    READ_RAW(buf_data, length);
  }
  registerobject(info);

  eris_checktype(info, -1, LUA_TBUFFER);
}

/*
** ============================================================================
** Tables and userdata.
** ============================================================================
*/

static void
p_metatable(Info *info) {                                          /* ... obj */
  eris_checkstack(info->L, 1);
  pushpath(info, "@metatable");
  if (!lua_getmetatable(info->L, -1)) {                        /* ... obj mt? */
    lua_pushnil(info->L);                                      /* ... obj nil */
  }                                                         /* ... obj mt/nil */
  persist(info);                                            /* ... obj mt/nil */
  lua_pop(info->L, 1);                                             /* ... obj */
  poppath(info);
}

static void
u_metatable(Info *info) {                                          /* ... tbl */
  eris_checkstack(info->L, 1);
  pushpath(info, "@metatable");
  unpersist(info);                                         /* ... tbl mt/nil? */
  if (lua_istable(info->L, -1)) {                               /* ... tbl mt */
    lua_setmetatable(info->L, -2);                                 /* ... tbl */
  }
  else if (lua_isnil(info->L, -1)) {                           /* ... tbl nil */
    lua_pop(info->L, 1);                                           /* ... tbl */
  }
  else {                                                            /* tbl :( */
    eris_error(info, ERIS_ERR_METATABLE);
  }
  poppath(info);
}

/** ======================================================================== */

static void p_table(Info *info) {                                  /* ... tbl */
  eris_checkstack(info->L, 3);

  // write the original array and hash sizes so that we can ensure consistent
  // iteration order when the table is deserialized
  const TValue *table_tv = luaA_toobject(info->L, -1);
  eris_assert(ttistable(table_tv));
  LuaTable* table = hvalue(table_tv);
  WRITE_VALUE(table->readonly, uint8_t);
  WRITE_VALUE(table->safeenv, uint8_t);
  WRITE_VALUE(table->sizearray, int);
  WRITE_VALUE(sizenode(table), int);

  int table_size = sizenode(table) + table->sizearray;
  /* Persist all key / value pairs. */
  for (int i = 0; i < table_size; ++i) {
    TValue key;
    TValue value;
    if (i < table->sizearray) {
      setnvalue(&key, i + 1);
      setobj(info->L, &value, &table->array[i]);
    } else {
      const LuaNode *node;
      int node_iter_idx = i - table->sizearray;
      if (ghaveiterorder(table)) {
        // We're enforcing iteration order, figure out the node index to use
        // in iteration order.
        node_iter_idx = table->iterorder[node_iter_idx].node_idx;
        LUAU_ASSERT(node_iter_idx <= sizenode(table) && node_iter_idx >= ITERORDER_EMPTY);
        if (node_iter_idx != ITERORDER_EMPTY) {
          node = &table->node[node_iter_idx];
        } else {
          // This slot wasn't filled in the original iteration order, just use
          // the dummynode since it's a nil:nil node anyway.
          node = &luaH_dummynode;
        }
      } else {
        node = &table->node[node_iter_idx];
      }

      // We can't guarantee that these will be filled correctly until or unless
      // we get a lot smarter about how we deserialize tables. The order in
      // which you `nil` keys has an effect on which keys will still be present
      // with `nil` values, and which will have their keys `nil`ed implicitly.
      // That's not to mention the pitfalls with serializing GC-able dead keys.
      if (ttype(&node->key) == LUA_TDEADKEY || ttisnil(&node->val)) {
        setnilvalue(&key);
        setnilvalue(&value);
      } else {
        getnodekey(info->L, &key, node);
        setobj(info->L, &value, &node->val);
      }
    }

    luaA_pushobject(info->L, &key);                              /* ... tbl k */

    if (info->generatePath) {
      if (lua_type(info->L, -1) == LUA_TSTRING) {
        const char *str_key = lua_tostring(info->L, -1);
        pushpath(info, ".%s", str_key);
      }
      else {
        const char *str_key = luaL_tolstring(info->L, -1, nullptr);
                                                             /* ... tbl k str */
        pushpath(info, "[%s]", str_key);
        lua_pop(info->L, 1);                                     /* ... tbl k */
      }
    }

    persist(info);                                               /* ... tbl k */
    lua_pop(info->L, 1);                                           /* ... tbl */
    luaA_pushobject(info->L, &value);                            /* ... tbl v */
    persist(info);                                               /* ... tbl v */
    lua_pop(info->L, 1);                                           /* ... tbl */
    poppath(info);
  }

  p_metatable(info);
}

static void u_table(Info *info) {                                      /* ... */
  eris_checkstack(info->L, 4);

  lua_newtable(info->L);                                           /* ... tbl */
  eris_ifassert(const int top = lua_gettop(info->L));

  /* Preregister table for handling of cycles (keys, values or metatable). */
  registerobject(info);

  bool read_only = READ_VALUE(uint8_t);
  bool safe_env = READ_VALUE(uint8_t);
  int array_size = READ_VALUE(int);
  int hash_size = READ_VALUE(int);

  /* Maintain a vector of keys in the order they were parsed so any existing
   * iterators won't be invalidated */
  std::vector<std::pair<TValue, TValue>> ordered_keys;

  /* Unpersist all key / value pairs. */
  int total_elems = array_size + hash_size;
  for (int i=0; i<total_elems; ++i) {
    pushpath(info, "@key");
    unpersist(info);                                       /* ... tbl key/nil */
    poppath(info);

    if (info->generatePath) {
      if (lua_type(info->L, -1) == LUA_TSTRING) {
        const char *key = lua_tostring(info->L, -1);
        pushpath(info, ".%s", key);
      }
      else {
        const char *key = luaL_tolstring(info->L, -1, nullptr);
                                                           /* ... tbl key str */
        pushpath(info, "[%s]", key);
        lua_pop(info->L, 1);                                   /* ... tbl key */
      }
    }

    unpersist(info);                                   /* ... tbl key? value? */

    // Store the keys and values for later insertion into the Table, it's fine to keep
    // these around here after popping since any GCable objects will also have a strong
    // reference in our reference table.
    ordered_keys.emplace_back(*luaA_toobject(info->L, -2), *luaA_toobject(info->L, -1));

    lua_pop(info->L, 2);                                           /* ... tbl */
    poppath(info);
  }

  eris_assert(top == lua_gettop(info->L));

  // Actually put things into the table
  LuaTable *table = hvalue(luaA_toobject(info->L, -1));

  // Resize the array and hash portions so things will end up in the same place as the
  // original Table
  // Array resize has to happen first because it'll force the hash to be 0-sized if it
  // notices nothing is in it.
  luaH_resizearray(info->L, table, array_size);
  luaH_resizehash(info->L, table, hash_size);

  // Make sure the resize happened correctly
  eris_assert(array_size == table->sizearray);
  eris_assert(hash_size == sizenode(table));

  // push a table to (hopefully not) dedupe keys and store their expected iter index
  lua_newtable(info->L); /* tbl pos_tbl */

  int non_nil_nodes = 0;
  // For whatever reason, reverse insertion order is more likely to chain nodes in the
  // same way as the original Table's hash.
  for (int kv_idx = (int)ordered_keys.size() - 1; kv_idx >= 0; --kv_idx) {
    const auto &kv_it = &ordered_keys[kv_idx];
    if (ttisnil(&kv_it->first)) {
      // we only keep these nil keys for iteration order preservation, we can't
      // actually insert them into a Table!
      continue;
    }

    if (kv_idx >= array_size) {
      ++non_nil_nodes;
      // keep track of the key's original iter pos within the hash
      luaA_pushobject(info->L, &kv_it->first);         /* ... tbl pos_tbl key */
      lua_pushinteger(info->L, (int)(kv_idx - array_size));
                                                   /* ... tbl pos_tbl key pos */
      lua_rawset(info->L, -3);                             /* ... tbl pos_tbl */
    }

    // We still want to assign keys with nil values though, keys with tombstones are
    // distinct from keys that aren't present and may affect chaining.
    luaA_pushobject(info->L, &kv_it->first);           /* ... tbl pos_tbl key */
    luaA_pushobject(info->L, &kv_it->second);      /* ... tbl pos_tbl key val */

    lua_rawset(info->L, -4);                               /* ... tbl pos_tbl */
  }

  eris_assert(top + 1 == lua_gettop(info->L));

  // If array has changed size then things are badly broken and we can't maintain
  // iterators
  eris_assert(array_size == table->sizearray);
  // hash size may actually have grown due to bucketing differences, but it
  // better not have shrunk.
  int actual_hash_size = sizenode(table);
  eris_assert(hash_size <= actual_hash_size);

  // If our hashtable ends up with fewer entries than it originally had, then our
  // table somehow shrank during deserialization (possibly due to a perms table issue.)
  lua_pushnil(info->L);                           /* ... tbl pos_tbl key(nil) */
  int new_non_nil_nodes = 0;
  while(lua_next(info->L, -2)) {                       /* ... tbl pos_tbl key */
                                                   /* ... tbl pos_tbl key val */
    ++new_non_nil_nodes;
    lua_pop(info->L, 1);                               /* ... tbl pos_tbl key */
  }
                                                           /* ... tbl pos_tbl */
  if (new_non_nil_nodes != non_nil_nodes)
    eris_error(info, ERIS_ERR_TABLE_COUNT, non_nil_nodes, new_non_nil_nodes);

  eris_assert(top + 1 == lua_gettop(info->L));

  // If this table has a hash component we need to be careful about iteration order
  bool override_iterorder = false;
  if (table->lsizenode) {
    if (hash_size != actual_hash_size) {
      // If bucketing changed, then we obviously need to override.
      override_iterorder = true;
    } else {
      // We don't need to enforce an iteration order if things are in the order we
      // want them to be in anyway, verify that first.
      for (int i=0; i<hash_size; ++i) {
        const TValue *expected = &ordered_keys[i + array_size].first;
        if (!luaO_rawequalKey(gkey(gnode(table, i)), expected)) {
          override_iterorder = true;
          break;
        }
      }
    }
  }

  if (override_iterorder) {
    // force iteration to happen in a particular order until the table is mutated in
    // a way that would invalidate the order.
    luaH_overrideiterorder(info->L, table, true);

    for(int node_idx = 0; node_idx<actual_hash_size; ++node_idx) {
      LuaNode *node = &table->node[node_idx];
      // Lua will never stop iteration on a nil node, so nil nodes don't need to
      // point to their nil equivalent within the hash. They can just point to nothing.
      if (ttisnil(gkey(node))) {
        continue;
      }

      // Find the key's original iter pos
      TValue key_val;
      getnodekey(info->L, &key_val, node);
      luaA_pushobject(info->L, &key_val);              /* ... tbl pos_tbl key */
      lua_rawget(info->L, -2);                         /* ... tbl pos_tbl val */
      // if the lookup failed then something is seriously wrong.
      eris_checktype(info, -1, LUA_TNUMBER);

      int iteridx = lua_tointeger(info->L, -1);
      lua_pop(info->L, 1);                                 /* ... tbl pos_tbl */

      table->iterorder[node_idx].node_to_iterorder_idx = iteridx;
      table->iterorder[iteridx].node_idx = (int)(node_idx);
    }
  }

  lua_pop(info->L, 1);                                             /* ... tbl */

  eris_ifassert(int cur_top = lua_gettop(info->L));
  eris_assert(top == cur_top);

  u_metatable(info);                                               /* ... tbl */

  // Set these last so we don't trigger any errors
  if (read_only)
    lua_setreadonly(info->L, -1, true);
  if (safe_env)
    lua_setsafeenv(info->L, -1, true);
}

/** ======================================================================== */

static void p_userdata(Info *info) {                               /* ... udata */
  eris_checkstack(info->L, 2);
  eris_ifassert(const int top = lua_gettop(info->L));
  uint8_t utag = lua_userdatatag(info->L, -1);
  const size_t size = lua_objlen(info->L, -1);
  const void *value = lua_touserdata(info->L, -1);
  WRITE_VALUE(utag, uint8_t);
  switch(utag) {
    case UTAG_PROXY:
    case UTAG_QUATERNION:
      WRITE_VALUE(size, ares_size_t);
      WRITE_RAW(value, size);
      break;
    case UTAG_UUID:
    {
        luaL_tolstring(info->L, -1, nullptr);                /* ... udata str */
        persist(info);                                       /* ... udata str */
        lua_pop(info->L, 1);                                     /* ... udata */
        break;
    }
    case UTAG_DETECTED_EVENT:
    {
        const auto *detected_event = (lua_DetectedEvent*)value;
        WRITE_VALUE(detected_event->index, int32_t);
        WRITE_VALUE(detected_event->valid, uint8_t);
        WRITE_VALUE(detected_event->can_change_damage, uint8_t);
        break;
    }
    case UTAG_LLEVENTS:
    {
        const auto *llevents = (lua_LLEvents*)value;
        lua_getref(info->L, llevents->listeners_tab_ref);
                                                        /* ... udata handlers */
        persist(info);
        lua_pop(info->L, 1);                                     /* ... udata */
        break;
    }
    case UTAG_LLTIMERS:
    {
        const auto *lltimers = (lua_LLTimers*)value;
        lua_getref(info->L, lltimers->timers_tab_ref);
                                                          /* ... udata timers */
        persist(info);
        lua_pop(info->L, 1);                                     /* ... udata */
        lua_getref(info->L, lltimers->llevents_ref);
                                                        /* ... udata llevents */
        persist(info);
        lua_pop(info->L, 1);                                     /* ... udata */
        lua_getref(info->L, lltimers->timer_wrapper_ref);
                                                         /* ... udata wrapper */
        persist(info);
        lua_pop(info->L, 1);                                     /* ... udata */
        break;
    }
    default:
      eris_error(info, "Unknown userdata type %d", utag);
      break;
  }
  p_metatable(info);                                             /* ... udata */
  eris_assert(top == lua_gettop(info->L));
}

static void u_userdata(Info *info) {                                   /* ... */
  eris_checkstack(info->L, 3);
  eris_ifassert(const int top = lua_gettop(info->L));
  {
    uint8_t utag = READ_VALUE(uint8_t);
    switch(utag) {
      case UTAG_PROXY:
      {
          size_t size = READ_VALUE(ares_size_t);
          void* value = lua_newuserdatatagged(info->L, size, utag);
          /* ... udata */
          READ_RAW(value, size);
          registerobject(info);
          break;
      }
      case UTAG_QUATERNION:
      {
          size_t size = READ_VALUE(ares_size_t);
          void* value = lua_newuserdatataggedwithmetatable(info->L, size, utag);
                                                                 /* ... udata */
          READ_RAW(value, size);
          registerobject(info);
          break;
      }
      case UTAG_UUID:
      {
        // Because we have an inner, wrapped string reference we need to reserve
        // the idx for the outer UUID first, since we saw it first.
        int reference = allocate_ref_idx(info);

        // Deserialize the wrapped string
        unpersist(info);                                           /* ... str */
        eris_checktype(info, -1, LUA_TSTRING);
        size_t len;
        auto *str_val = luaL_checklstring(info->L, -1, &len);
        luaSL_pushuuidlstring(info->L, str_val, len);         /* ... str uuid */
        lua_replace(info->L, -2);                                 /* ... uuid */

        // Manually put the UUID in the references table at the correct reference index
        lua_pushvalue(info->L, -1);               /* perms reftbl ... obj obj */
        lua_rawseti(info->L, REFTIDX, reference);     /* perms reftbl ... obj */
        break;
      }
      case UTAG_DETECTED_EVENT:
      {
          auto *detected_event = (lua_DetectedEvent*)lua_newuserdatataggedwithmetatable(
              info->L,
              sizeof(lua_DetectedEvent),
              UTAG_DETECTED_EVENT
          );
                                                                 /* ... udata */
          memset(detected_event, 0, sizeof(lua_DetectedEvent));
          detected_event->index = READ_VALUE(int32_t);
          detected_event->valid = (bool)READ_VALUE(uint8_t);
          detected_event->can_change_damage = (bool)READ_VALUE(uint8_t);
          registerobject(info);
          break;
      }
      case UTAG_LLEVENTS:
      {
          // Create userdata with safe initial values FIRST to handle cycles
          auto *llevents = (lua_LLEvents*)lua_newuserdatataggedwithmetatable(
              info->L,
              sizeof(lua_LLEvents),
              UTAG_LLEVENTS
          );
                                                              /* ... llevents */
          llevents->listeners_tab_ref = -1;
          llevents->listeners_tab = nullptr;

          // Register immediately to handle cycles (listeners_tab may reference this)
          registerobject(info);

          // Unpersist listeners_tab and store ref IMMEDIATELY to prevent leaks
          unpersist(info);                           /* ... llevents handlers_tab */
          eris_checktype(info, -1, LUA_TTABLE);
          llevents->listeners_tab_ref = lua_ref(info->L, -1);
          llevents->listeners_tab = hvalue(luaA_toobject(info->L, -1));
          lua_pop(info->L, 1);                                    /* ... llevents */

          break;
      }
      case UTAG_LLTIMERS:
      {
          // Create userdata with safe initial values FIRST to handle cycles
          auto *lltimers = (lua_LLTimers*)lua_newuserdatataggedwithmetatable(
              info->L,
              sizeof(lua_LLTimers),
              UTAG_LLTIMERS
          );
                                                              /* ... lltimers */
          lltimers->timers_tab_ref = -1;
          lltimers->llevents_ref = -1;
          lltimers->timer_wrapper_ref = -1;
          lltimers->timers_tab = nullptr;

          // Register immediately to handle cycles (timer_wrapper has LLTimers as upvalue)
          registerobject(info);

          // Unpersist timers_tab and store ref IMMEDIATELY to prevent leaks
          unpersist(info);                            /* ... lltimers timers_tab */
          eris_checktype(info, -1, LUA_TTABLE);
          lltimers->timers_tab_ref = lua_ref(info->L, -1);
          lltimers->timers_tab = hvalue(luaA_toobject(info->L, -1));
          lua_pop(info->L, 1);                                     /* ... lltimers */

          // Unpersist llevents and store ref IMMEDIATELY
          unpersist(info);                             /* ... lltimers llevents */
          eris_checkuserdatatag(info, -1, UTAG_LLEVENTS);
          lltimers->llevents_ref = lua_ref(info->L, -1);
          lua_pop(info->L, 1);                                     /* ... lltimers */

          // Unpersist timer_wrapper and store ref IMMEDIATELY
          unpersist(info);                        /* ... lltimers timer_wrapper */
          eris_checktype(info, -1, LUA_TFUNCTION);
          lltimers->timer_wrapper_ref = lua_ref(info->L, -1);
          lua_pop(info->L, 1);                                     /* ... lltimers */

          break;
      }
      default:
        eris_error(info, "Unknown userdata tag %d", utag);
        break;
    }
  }
  u_metatable(info);
  eris_assert(top + 1 == lua_gettop(info->L));
  eris_checktype(info, -1, LUA_TUSERDATA);
}

/*
** ============================================================================
** Closures and threads.
** ============================================================================
*/

/* We track the actual upvalues themselves by pushing their "id" (meaning a
 * pointer to them) as lightuserdata to the reftable. This is safe because
 * lightuserdata will not normally end up in there, because simple value types
 * are always persisted directly (because that'll be just as large, memory-
 * wise as when pointing to the first instance). Same for protos. */

static void
p_proto(Info *info) {                                            /* ... proto */
  int i;
  const Proto *p = (Proto*)lua_touserdata(info->L, -1);
  eris_checkstack(info->L, 3);

  info->anyProtoNative |= p->execdata != nullptr;

  /* Write function source code */
  pushtstring(info->L, p->source);                       /* ... proto str/nil */
  persist(info);
  lua_pop(info->L, 1);                                           /* ... proto */

  /* Write general information. */
  WRITE_VALUE(p->bytecodeid, int);

  WRITE_VALUE(p->maxstacksize, uint8_t);
  WRITE_VALUE(p->flags, uint8_t);
  WRITE_VALUE(p->numparams, uint8_t);
  WRITE_VALUE(p->nups, uint8_t);
  WRITE_VALUE(p->is_vararg, uint8_t);

  /* Write byte code. */
  WRITE_VALUE(p->sizecode, int);
  WRITE(p->code, p->sizecode, Instruction);

  /* Write constants. */
  WRITE_VALUE(p->sizek, int);
  pushpath(info, ".constants");
  for (i = 0; i < p->sizek; ++i) {
    pushpath(info, "[%d]", i);
    eris_setobj(info->L, info->L->top++, &p->k[i]);      /* ... lcl proto obj */
    persist(info);                                       /* ... lcl proto obj */
    lua_pop(info->L, 1);                                     /* ... lcl proto */
    poppath(info);
  }
  poppath(info);

  /* Write child protos. */
  WRITE_VALUE(p->sizep, int);
  pushpath(info, ".protos");
  for (i = 0; i < p->sizep; ++i) {
    pushpath(info, "[%d]", i);
    lua_pushlightuserdata(info->L, p->p[i]);           /* ... lcl proto proto */
    lua_pushvalue(info->L, -1);                  /* ... lcl proto proto proto */
    persist_keyed(info, LUA_TPROTO);                   /* ... lcl proto proto */
    lua_pop(info->L, 1);                                     /* ... lcl proto */
    poppath(info);
  }
  poppath(info);

  WRITE_VALUE(p->linedefined, int);
  pushtstring(info->L, p->debugname);                    /* ... proto str/nil */
  persist(info);
  lua_pop(info->L, 1);                                           /* ... proto */

  // TODO: write line and debug info, just say we don't have either for now.
  WRITE_VALUE(0, uint8_t);
  WRITE_VALUE(0, uint8_t);
//  /* Write upvalues. */
//  WRITE_VALUE(p->sizeupvalues, int);
//  for (i = 0; i < p->sizeupvalues; ++i) {
////    WRITE_VALUE(p->upvalues[i].instack, uint8_t);
////    WRITE_VALUE(p->upvalues[i].idx, uint8_t);
//    WRITE_VALUE(0, uint8_t);
//    WRITE_VALUE(0, uint8_t);
//  }
//
//  /* If we don't have to persist debug information skip the rest. */
//  WRITE_VALUE(info->u.pi.writeDebugInfo, uint8_t);
//  if (!info->u.pi.writeDebugInfo) {
//    return;
//  }
//
//  /* Write function source code. */
//  pushtstring(info->L, p->source);                    /* ... lcl proto source */
//  persist(info);                                      /* ... lcl proto source */
//  lua_pop(info->L, 1);                                       /* ... lcl proto */
//
//  /* Write line information. */
//  WRITE_VALUE(p->sizelineinfo, int);
//  WRITE(p->lineinfo, p->sizelineinfo, int);
//
//  /* Write locals info. */
//  WRITE_VALUE(p->sizelocvars, int);
//  pushpath(info, ".locvars");
//  for (i = 0; i < p->sizelocvars; ++i) {
//    pushpath(info, "[%d]", i);
//    WRITE_VALUE(p->locvars[i].startpc, int);
//    WRITE_VALUE(p->locvars[i].endpc, int);
//    pushtstring(info->L, p->locvars[i].varname);     /* ... lcl proto varname */
//    persist(info);                                   /* ... lcl proto varname */
//    lua_pop(info->L, 1);                                     /* ... lcl proto */
//    poppath(info);
//  }
//  poppath(info);
//
//  /* Write upvalue names. */
//  pushpath(info, ".upvalnames");
//  for (i = 0; i < p->sizeupvalues; ++i) {
//    pushpath(info, "[%d]", i);
//    pushtstring(info->L, p->upvalues[i]);               /* ... lcl proto name */
//    persist(info);                                      /* ... lcl proto name */
//    lua_pop(info->L, 1);                                     /* ... lcl proto */
//    poppath(info);
//  }
//  poppath(info);
  WRITE_VALUE(p->sizeyieldpoints, int32_t);
  for (i=0; i<p->sizeyieldpoints; ++i)
  {
      WRITE_VALUE(p->yieldpoints[i], int32_t);
  }
}

static void
u_proto(Info *info) {                                            /* ... proto */
  int i, n;
  Proto *p = (Proto*)lua_touserdata(info->L, -1);
  eris_assert(p);

  eris_checkstack(info->L, 2);

  /* Preregister proto for handling of cycles (probably impossible, but
   * maybe via the constants of the proto... not worth taking the risk). */
  registerobject(info);

  /* Read function source code. */
  unpersist(info);                                           /* ... proto str */
  copytstring(info->L, &p->source);
  lua_pop(info->L, 1);                                           /* ... proto */

  /* Read general information. */
  p->bytecodeid = READ_VALUE(int);

  p->maxstacksize = READ_VALUE(uint8_t);
  p->flags = READ_VALUE(uint8_t);
  p->numparams = READ_VALUE(uint8_t);
  p->nups = READ_VALUE(uint8_t);
  p->is_vararg = READ_VALUE(uint8_t);

  /* Read byte code. */
  SAFE_ALLOC_VECTOR(info->L, p->code, 0, int, p->sizecode, Instruction);
  READ(p->code, p->sizecode, Instruction);
  /* codeentry should only differ for JITted protos, and we don't deal in those.
   * If it matters, this'll be fixed up by the call to the codegen later. */
  p->codeentry = p->code;

  /* Read constants. */
  SAFE_ALLOC_VECTOR(info->L, p->k, 0, int, p->sizek, TValue);
  /* Set all values to nil to avoid confusing the GC. */
  for (i = 0, n = p->sizek; i < n; ++i) {
    eris_setnilvalue(&p->k[i]);
  }
  pushpath(info, ".constants");
  for (i = 0, n = p->sizek; i < n; ++i) {
    pushpath(info, "[%d]", i);
    // Import constants aren't supported, but they aren't necessary with
    // how we do constant serialization. We never actually serialize protos
    // themselves.
    unpersist(info);                                         /* ... proto obj */
    eris_setobj(info->L, &p->k[i], info->L->top - 1);
    lua_pop(info->L, 1);                                         /* ... proto */
    poppath(info);
  }
  poppath(info);

  /* Read child protos. */
  SAFE_ALLOC_VECTOR(info->L, p->p, 0, int, p->sizep, Proto*);
  /* Null all entries to avoid confusing the GC. */
  memset(p->p, 0, p->sizep * sizeof(Proto*));
  pushpath(info, ".protos");
  for (i = 0, n = p->sizep; i < n; ++i) {
    Proto *cp;
    pushpath(info, "[%d]", i);
    cp = p->p[i] = eris_newproto(info->L);
    luaC_objbarrier(info->L, p, cp);
    lua_pushlightuserdata(info->L, (void*)p->p[i]);       /* ... proto nproto */
    unpersist(info);                        /* ... proto nproto nproto/oproto */
    cp = (Proto*)lua_touserdata(info->L, -1);
    if (cp != p->p[i]) {                           /* ... proto nproto oproto */
      /* Just overwrite it, GC will clean this up. */
      p->p[i] = cp;
    }
    lua_pop(info->L, 2);                                         /* ... proto */
    poppath(info);
  }
  poppath(info);

  p->linedefined = READ_VALUE(int);
  unpersist(info);                                           /* ... proto str */
  copytstring(info->L, &p->debugname);
  lua_pop(info->L, 1);                                           /* ... proto */

  // TODO: line info
  /* Read line info if any is present. */
  if (READ_VALUE(uint8_t)) {
    eris_error(info, "reading line info isn't supported");
  }

  // TODO: debug info
  /* Read debug information if any is present. */
  if (READ_VALUE(uint8_t)) {
    eris_error(info, "reading debug info isn't supported");
  }
//
//  /* Read debug information if any is present. */
//  if (!READ_VALUE(uint8_t)) {
//    /* Match stack behaviour of alternative branch. */
//    lua_pushvalue(info->L, -1);                            /* ... proto proto */
//    return;
//  }
//
//  /* Read line information. */
//  p->sizelineinfo = READ_VALUE(int);
//  eris_reallocvector(info->L, p->lineinfo, 0, p->sizelineinfo, uint8_t);
//  READ(p->lineinfo, p->sizelineinfo, int);
//  eris_reallocvector(info->L, p->abslineinfo, 0, p->sizelineinfo, uint8_t);
//  READ(p->lineinfo, p->abslineinfo, int);
//
//  /* Read locals info. */
//  p->sizelocvars = READ_VALUE(int);
//  eris_reallocvector(info->L, p->locvars, 0, p->sizelocvars, LocVar);
//  /* Null the variable names to avoid confusing the GC. */
//  for (i = 0, n = p->sizelocvars; i < n; ++i) {
//    p->locvars[i].varname = nullptr;
//  }
//  pushpath(info, ".locvars");
//  for (i = 0, n = p->sizelocvars; i < n; ++i) {
//    pushpath(info, "[%d]", i);
//    p->locvars[i].startpc = READ_VALUE(int);
//    p->locvars[i].endpc = READ_VALUE(int);
//    unpersist(info);                                         /* ... proto str */
//    copytstring(info->L, &p->locvars[i].varname);
//    lua_pop(info->L, 1);                                         /* ... proto */
//    poppath(info);
//  }
//  poppath(info);
//
//  /* Read upvalue names. */
//  pushpath(info, ".upvalnames");
//  for (i = 0, n = p->sizeupvalues; i < n; ++i) {
//    pushpath(info, "[%d]", i);
//    unpersist(info);                                         /* ... proto str */
//    copytstring(info->L, &p->upvalues[i]);
//    lua_pop(info->L, 1);                                         /* ... proto */
//    poppath(info);
//  }
//  poppath(info);
  SAFE_ALLOC_VECTOR(info->L, p->yieldpoints, 0, int, p->sizeyieldpoints, int);
  for (i=0; i<p->sizeyieldpoints; ++i)
  {
      p->yieldpoints[i] = READ_VALUE(int32_t);
  }

  lua_pushvalue(info->L, -1);                              /* ... proto proto */

  eris_assert(lua_type(info->L, -1) == LUA_TLIGHTUSERDATA);
}

/** ======================================================================== */

static void
p_upval(Info *info) {                                              /* ... obj */
  persist(info);                                                   /* ... obj */
}

static void
u_upval(Info *info) {                                                  /* ... */
  eris_checkstack(info->L, 2);

  /* Create the table we use to store the stack location to the upval (1+2),
   * the value of the upval (3) and any references to the upvalue's value (4+).
   * References are stored as two entries each, the actual closure holding the
   * upvalue, and the index of the upvalue in that closure. */
  lua_createtable(info->L, 5, 0);                                  /* ... tbl */
  registerobject(info);
  unpersist(info);                                             /* ... tbl obj */
  // We can't put a literal nil in the table or it'll mess up objlen. Box it.
  if (lua_isnil(info->L, -1)) {
    lua_pushlightuserdatatagged(info->L, nullptr, LUTAG_ARES_BOXED_NIL);
                                                     /* ... tbl obj boxed_nil */
    lua_replace(info->L, -2);                            /* ... tbl boxed_nil */
  }
  lua_rawseti(info->L, -2, UVTVAL);                                /* ... tbl */

  eris_assert(lua_type(info->L, -1) == LUA_TTABLE);
}

/** ======================================================================== */

/* For Lua closures we write the upvalue ID, which is usually the memory
 * address at which it is stored. This is used to tell which upvalues are
 * identical when unpersisting. */
/* In either case we store the upvale *values*, i.e. the actual objects they
 * point to. As in Pluto, we will restore any upvalues of Lua closures as
 * closed as first, i.e. the upvalue will store the TValue itself. When
 * loading a thread containing the upvalue (meaning it's the actual owner of
 * the upvalue) we open it, i.e. we point it to the thread's upvalue list.
 * For C closures, upvalues are always closed. */
static void
p_closure(Info *info) {                              /* perms reftbl ... func */
  int nup;
  Closure *cl = clvalue(luaA_toobject(info->L, -1));
  eris_checkstack(info->L, 2);

  if (info->u.pi.persistingCFunc) {
    /* This is a case where we tried to persist a c function via the permtable
     * but failed, causing us to be re-passed the light userdata we sent into
     * persist_keyed().
     *
     * We cannot persist these. They have to be handled via the permtable. */
    eris_error(info, ERIS_ERR_CFUNC, cl->c.f, cl->c.debugname);
    return;
  }

  /* Mark whether it is a C closure. */
  WRITE_VALUE(cl->isC, uint8_t);

  /* Write the upvalue count first, since we have to know it when creating
   * a new closure when unpersisting. */
  WRITE_VALUE(cl->nupvalues, uint8_t);

  // Technically C closures, like Lua closures have an `env`, but it seems
  // that in practice these are always `GL->gt`, and `getfenv()` is
  // special-cased to lie about the fenv. Don't need to store an `env` for
  // them, then.
  // NOTE: Luau only has "C" and "Lua" closures, no light C functions!
  if (cl->isC) {
    /* If we got here it means that there was no closure instance for this C
     * function in the perms table. That may be the case for dynamically
     * created closures like those returned by `coroutine.wrap()`. As a last
     * resort, try seeing if we have _any_ closure that points to the same
     * underlying C function by pushing its pointer as a lightuserdata.
     *
     * Note that this isn't foolproof as the perms table entry is keyed solely
     * on the function pointer and not any continuation function pointer. In
     * practice this shouldn't matter because a C closure with the same
     * pointer but different continuation pointer seems to be unusual.
     */
    eris_ifassert(const int pre_cfunc_top = lua_gettop(info->L));
    eris_assert(lua_type(info->L, -1) == LUA_TFUNCTION);
    info->u.pi.persistingCFunc = true;
    lua_pushlightuserdata(info->L, (void *)cl->c.f);
                                              /* perms reftbl ... ccl cfunc */
    persist_keyed(info, LUA_TFUNCTION);             /* perms reftbl ... ccl */
    info->u.pi.persistingCFunc = false;
    eris_assert(lua_gettop(info->L) == pre_cfunc_top);
    eris_assert(lua_type(info->L, -1) == LUA_TFUNCTION);

    /* Persist the upvalues. Since for C closures all upvalues are always
     * closed we can just write the actual values. */
    pushpath(info, ".upvalues");
    for (nup = 1; nup <= cl->nupvalues; ++nup) {
      eris_ifassert(const int pre_upval_top = lua_gettop(info->L));
      pushpath(info, "[%d]", nup);
      lua_getupvalue(info->L, -1, nup);         /* perms reftbl ... ccl obj */
      eris_assert(lua_gettop(info->L) == pre_upval_top + 1);
      persist(info);                            /* perms reftbl ... ccl obj */
      eris_assert(lua_gettop(info->L) == pre_upval_top + 1);
      lua_pop(info->L, 1);                          /* perms reftbl ... ccl */
      eris_assert(lua_gettop(info->L) == pre_upval_top);
      poppath(info);
    }
    poppath(info);
    eris_assert(lua_gettop(info->L) == pre_cfunc_top);
    eris_assert(lua_type(info->L, -1) == LUA_TFUNCTION);
  }
  /* Lua function */
  else {               /* perms reftbl ... lcl */
    pushpath(info, ".env");
    /* Persist the environment (globals) table for the closure */
    lua_getfenv(info->L, -1);                   /* perms reftbl ... lcl env */
    persist(info);
    lua_pop(info->L, 1);                            /* perms reftbl ... lcl */
    poppath(info);

    /* Persist the function's prototype. Pass the proto as a parameter to
     * p_proto so that it can access it and register it in the ref table. */
    pushpath(info, ".proto");
    info->anyProtoNative = false;
    lua_pushlightuserdata(info->L, cl->l.p);  /* perms reftbl ... lcl proto */
    lua_pushvalue(info->L, -1);         /* perms reftbl ... lcl proto proto */
    persist_keyed(info, LUA_TPROTO);          /* perms reftbl ... lcl proto */
    lua_pop(info->L, 1);                            /* perms reftbl ... lcl */
    WRITE_VALUE(info->anyProtoNative, uint8_t);
    poppath(info);

    /* Persist the upvalues. We pretend to write these as their own type,
     * to get proper identity preservation. We also pass them as a parameter
     * to p_upval so it can register the upvalue in the reference table. */
    pushpath(info, ".upvalues");
    for (nup = 1; nup <= cl->nupvalues; ++nup) {
      // internally pushes the upvalue's value onto the stack
      const char *name = lua_getupvalue(info->L, -1, nup);
                                             /* perms reftbl ... lcl uv_val */
      // name is unlikely to be useful, but it shouldn't be null
      if (name == nullptr)
        eris_error(info, ERIS_ERR_UPVAL_IDX, nup);

      pushpath(info, "[%d]", nup);

      // strictly used as a key for finding shared upvalue references!
      lua_pushlightuserdatatagged(
          info->L,
          eris_getupvalueid_safe(info, -2, nup),
          LUTAG_ARES_UPREF
      );
                                       /* perms reftbl ... lcl uv_val uv_id */

      persist_keyed(info, LUA_TUPVAL);       /* perms reftbl ... lcl uv_val */
      lua_pop(info->L, 1);                          /* perms reftbl ... lcl */
      poppath(info);
      // stack should be back to normal
      eris_assert(lua_type(info->L, -1) == LUA_TFUNCTION);
    }
    poppath(info);
  }
}

static void
u_closure(Info *info) {                                                /* ... */
  int nup;
  bool isCClosure = READ_VALUE(uint8_t);
  uint8_t nups = READ_VALUE(uint8_t);
  Closure *cl;

  if (isCClosure) {
    /* Reserve reference for the closure to avoid light C function or its
     * perm table key going first. */
    const int reference = allocate_ref_idx(info);

    eris_checkstack(info->L, nups + 2);

    /* Read the C function from the permanents table. */
    unpersist(info);                                             /* ... cfunc */
    if (!lua_iscfunction(info->L, -1)) {
      eris_error(info, ERIS_ERR_UCFUNC, kTypenames[lua_type(info->L, -1)]);
    }
    cl = clvalue(luaA_toobject(info->L, -1));
    if (!cl->c.f) {
      eris_error(info, ERIS_ERR_UCFUNCNULL);
    }
    // Ok to pop here, there's still a strong reference to the base closure
    // in the perms table.
    lua_pop(info->L, 1);

    /* Now this is a little roundabout, but we want to create the closure
     * before unpersisting the actual upvalues to avoid cycles. So we have to
     * create it with all nil first, then fill the upvalues in afterwards. */
    for (nup = 1; nup <= nups; ++nup) {
      lua_pushnil(info->L);                        /* ... nil[1] ... nil[nup] */
    }
    lua_pushcclosurek(info->L, cl->c.f, cl->c.debugname, nups, cl->c.cont);
                                                                   /* ... ccl */
    /* Create the entry in the reftable. */
    lua_pushvalue(info->L, -1);                   /* perms reftbl ... ccl ccl */
    lua_rawseti(info->L, REFTIDX, reference);         /* perms reftbl ... ccl */

    /* Unpersist actual upvalues. */
    pushpath(info, ".upvalues");
    for (nup = 1; nup <= nups; ++nup) {
      pushpath(info, "[%d]", nup);
      unpersist(info);                                         /* ... ccl obj */
      lua_setupvalue(info->L, -2, nup);                            /* ... ccl */
      poppath(info);
    }
    poppath(info);
                                                                   /* ... ccl */
  }
  else {
    Proto *p;

    eris_checkstack(info->L, 6);

    /* Create closure and anchor it on the stack (avoid collection via GC). */
    p = eris_newproto(info->L);
    // Pre-set this so luaC_validate() / validateclosure() doesn't explode if
    // we error while deserializing the proto. We expect it to get clobbered.
    p->nups = nups;
    // `info->L->gt` almost definitely isn't the proper env for this closure,
    // we'll replace it with the real one later.
    cl = luaF_newLclosure(info->L, nups, info->L->gt, p);
    setclvalue(info->L, info->L->top, cl);                         /* ... lcl */
    incr_top(info->L);

    /* Preregister closure for handling of cycles (upvalues). */
    registerobject(info);

    pushpath(info, ".fenv");
    /* Read env dict, this is generally the proxy table for globals */
    unpersist(info);                                            /* ... lcl gt */
    // Replace the placeholder env we had on the closure
    lua_setfenv(info->L, -2);                                      /* ... lcl */
    poppath(info);

    /* Read prototype. In general, we create protos (and upvalues) before
     * trying to read them and pass a pointer to the instance along to the
     * unpersist function. This way the instance is safely hooked up to an
     * object, so we don't have to worry about it getting GCed. */
    pushpath(info, ".proto");
    /* Push the proto into which to unpersist as a parameter to u_proto. */
    lua_pushlightuserdata(info->L, cl->l.p);                /* ... lcl nproto */
    unpersist(info);                          /* ... lcl nproto nproto/oproto */
    eris_checktype(info, -1, LUA_TLIGHTUSERDATA);
    /* The proto we have now may differ, if we already unpersisted it before.
     * In that case we now have a reference to the originally unpersisted
     * proto so we'll use that. */
    p = (Proto*)lua_touserdata(info->L, -1);
    if (p != cl->l.p) {                              /* ... lcl nproto oproto */
      /* Just overwrite the old one, GC will clean this up. */
      cl->l.p = p;
    }
    lua_pop(info->L, 2);                                           /* ... lcl */
    if (cl->l.p->code == nullptr) {
      eris_error(info, "malformed data: proto has no code");
    }
    if (cl->l.p->nups != nups) {
      eris_error(info, "malformed data: proto upvalue count mismatch");
    }
    cl->stacksize = p->maxstacksize;

    /* Check if any of the inner protos originally had native code */
    bool needCompile = READ_VALUE(uint8_t);
    poppath(info);

    /* Unpersist all upvalues. */
    pushpath(info, ".upvalues");
    for (nup = 1; nup <= nups; ++nup) {
      TValue *upval_cont = &cl->l.uprefs[nup - 1];
      /* Get the actual name of the upvalue, if possible. */
      if (p->upvalues && p->upvalues[nup - 1]) {
        pushpath(info, "[%s]", getstr(p->upvalues[nup - 1]));
      }
      else {
        pushpath(info, "[%d]", nup);
      }

      // upval will be unpersisted as a table describing the upval
      unpersist(info);                                         /* ... lcl tbl */
      eris_checktype(info, -1, LUA_TTABLE);
      lua_rawgeti(info->L, -1, UVTOCL);               /* ... lcl tbl olcl/nil */
      if (lua_isnil(info->L, -1)) {                        /* ... lcl tbl nil */
        // Don't have an existing closure to pull this upval from, create an upval.
        lua_pop(info->L, 1);                                   /* ... lcl tbl */
        lua_pushvalue(info->L, -2);                        /* ... lcl tbl lcl */
        lua_rawseti(info->L, -2, UVTOCL);                      /* ... lcl tbl */
        lua_pushinteger(info->L, nup);                     /* ... lcl tbl nup */
        lua_rawseti(info->L, -2, UVTONU);                      /* ... lcl tbl */
        setupvalue(info->L, upval_cont, luaF_newupval(info->L));
      }
      else {                                              /* ... lcl tbl olcl */
        // This upval was already referenced in another closure, pull it off.
        Closure *ocl;
        int onup;
        eris_checktype(info, -1, LUA_TFUNCTION);
        ocl = clvalue(info->L->top - 1);
        lua_pop(info->L, 1);                                   /* ... lcl tbl */
        lua_rawgeti(info->L, -1, UVTONU);                 /* ... lcl tbl onup */
        eris_checktype(info, -1, LUA_TNUMBER);
        onup = lua_tointeger(info->L, -1);
        lua_pop(info->L, 1);                                   /* ... lcl tbl */
        // _not_ setupvalue(), we want the tvalue pointers to be the same!
        setobj(info->L, upval_cont, &ocl->l.uprefs[onup - 1]);
      }

      if (ttype(upval_cont) != LUA_TUPVAL) {
        eris_error(info, "malformed data: expected upvalue, got %s", kTypenames[ttype(upval_cont)]);
      }
      UpVal *uv = &upval_cont->value.gc->uv;
      luaC_objbarrier(info->L, cl, uv);

      /* Set the upvalue's actual value and add our reference to the upvalue to
       * the list, for reference patching if we have to open the upvalue in
       * u_thread. Either is only necessary if the upvalue is still closed. */
      if (!upisopen(uv)) {
        int i;
        /* Always update the value of the upvalue's value for closed upvalues,
         * even if we re-used one - if we had a cycle, it might have been
         * incorrectly initialized to nil before (or rather, not yet set). */
        lua_rawgeti(info->L, -1, UVTVAL);                  /* ... lcl tbl obj */
        // Check if this was a boxed nil, and replace it with real nil if so
        if (lua_type(info->L, -1) == LUA_TLIGHTUSERDATA) {
          if (lua_lightuserdatatag(info->L, -1) == LUTAG_ARES_BOXED_NIL) {
            lua_pushnil(info->L);                /* ... lcl tbl boxed_nil nil */
            lua_replace(info->L, -2);                      /* ... lcl tbl nil */
          }
        }
        eris_setobj(info->L, &uv->u.value, info->L->top - 1);
        lua_pop(info->L, 1);                                   /* ... lcl tbl */

        lua_pushinteger(info->L, nup);                     /* ... lcl tbl nup */
        lua_pushvalue(info->L, -3);                    /* ... lcl tbl nup lcl */
        if (lua_objlen(info->L, -3) >= UVTVAL) {
          // Got a valid sequence (value already set), insert at the end.
          i = lua_objlen(info->L, -3);
          lua_rawseti(info->L, -3, i + 1);                 /* ... lcl tbl nup */
          lua_rawseti(info->L, -2, i + 2);                     /* ... lcl tbl */
        }
        else {                                         /* ... lcl tbl nup lcl */
          /* Find where to insert. This can happen if we have cycles, in which
           * case the table is not fully initialized at this point, i.e. the
           * value is not in it, yet (we work around that by always setting it,
           * as seen above). */
          for (i = UVTREF;; i += 2) {
            lua_rawgeti(info->L, -3, i);       /* ... lcl tbl nup lcl lcl/nil */
            if (lua_isnil(info->L, -1)) {          /* ... lcl tbl nup lcl nil */
              lua_pop(info->L, 1);                     /* ... lcl tbl nup lcl */
              lua_rawseti(info->L, -3, i);                 /* ... lcl tbl nup */
              lua_rawseti(info->L, -2, i + 1);                 /* ... lcl tbl */
              break;
            }
            else {
              lua_pop(info->L, 1);                     /* ... lcl tbl nup lcl */
            }
          }                                                    /* ... lcl tbl */
        }
      }

      lua_pop(info->L, 1);                                         /* ... lcl */
      poppath(info);
    }
    poppath(info);
    if (needCompile) {
      if (sAresCodeGenCompile == nullptr)
        eris_error(info, "Need codegen initialize");

      sAresCodeGenCompile(info->L, -1);
    }
  }

  eris_checktype(info, -1, LUA_TFUNCTION);
}

/** ======================================================================== */

CLANG_NOOPT static void GCC_NOOPT
p_thread(Info *info) {                                          /* ... thread */
  eris_checkstack(info->L, 3);
  eris_ifassert(const int initial_stack_top = lua_gettop(info->L));
  lua_State* thread = lua_tothread(info->L, -1);
  eris_assert(thread != nullptr);
  size_t level = 0, total = thread->top - thread->stack;
  CallInfo *ci;
  UpVal *uv;

  /* We cannot persist any running threads, because by definition we *are* that
   * running thread. And we use the stack. So yeah, really not a good idea. */
  if (thread == info->L) {
    eris_error(info, ERIS_ERR_THREAD);
    return; /* not reached */
  }

  /* Persist the globals table for the thread */
  lua_getfenv(info->L, -1);                                  /* ... thread gt */
  persist(info);
  lua_pop(info->L, 1);                                          /* ... thread */
  eris_assert(lua_gettop(info->L) == initial_stack_top);
  eris_assert(lua_type(info->L, -1) == LUA_TTHREAD);

  /* Persist the stack. Save the total size and used space first. */
  WRITE_VALUE(thread->stacksize, int);
  WRITE_VALUE(total, ares_size_t);

  /* The Lua stack looks like this:
   * stack ... top ... stack_last
   * Where stack <= top <= stack_last, and "top" actually being the first free
   * element, i.e. there's nothing stored there. So we stop one below that. */
  pushpath(info, ".stack");
  lua_pushnil(info->L);                                     /* ... thread nil */
  /* Since the thread's stack may be re-allocated in the meantime, we cannot
   * use pointer arithmetic here (i.e. o = thread->stack; ...; ++o). Instead we
   * have to keep track of our position in the stack directly (which we do for
   * the path info anyway) and compute the actual address each time.
   */
  for (; level < total; ++level) {
    pushpath(info, "[%d]", level);
    eris_setobj(info->L, info->L->top - 1, thread->stack + level);
                                                            /* ... thread obj */
    persist(info);                                          /* ... thread obj */
    poppath(info);
  }
  lua_pop(info->L, 1);                                          /* ... thread */
  poppath(info);
  eris_assert(lua_gettop(info->L) == initial_stack_top);
  eris_assert(lua_type(info->L, -1) == LUA_TTHREAD);

  /* thread->oldpc always seems to be uninitialized, at least gdb always shows
   * it as 0xbaadf00d when I set a breakpoint here. */

  /* Write general information. */
  WRITE_VALUE(thread->status, uint8_t);
//  WRITE_VALUE(eris_savestackidx(thread,
//    eris_restorestack(thread, thread->errfunc)), size_t);
  // no err func!
  WRITE_VALUE(0, ares_size_t);
  /* These are only used while a thread is being executed or can be deduced:
  WRITE_VALUE(thread->nCcalls, uint16_t);
  WRITE_VALUE(thread->allowhook, uint8_t); */

  /* Hooks are not supported, bloody can of worms, those.
  WRITE_VALUE(thread->hookmask, uint8_t);
  WRITE_VALUE(thread->basehookcount, int);
  WRITE_VALUE(thread->hookcount, int); */

  /* Write call information (stack frames). In 5.2 CallInfo is stored in a
   * linked list that originates in thead.base_ci. Upon initialization the
   * thread.ci is set to thread.base_ci. During thread calls this is extended
   * and always represents the tail of the callstack, though not necessarily of
   * the linked list (which can be longer if the callstack was deeper earlier,
   * but shrunk due to returns). */
  pushpath(info, ".callinfo");
  level = 0;

  // we expect that there's at least 1 CallInfo node, even for finished threads.
  // I'd like to use end_ci here but it looks like it's actually the end of the vector,
  // and not the ci just past the end of the last CI.
  int num_cis = (int)((thread->ci + 1) - thread->base_ci);
  WRITE_VALUE(num_cis, int);
  for (int i=0; i < num_cis; ++i) {
    pushpath(info, "[%d]", level++);
    ci = thread->base_ci + i;
    WRITE_VALUE(eris_savestackidx(thread, ci->func), ares_size_t);
    WRITE_VALUE(eris_savestackidx(thread, ci->top), ares_size_t);
    WRITE_VALUE(eris_savestackidx(thread, ci->base), ares_size_t);
    /* CallInfo.nresults is only set for actual functions */
    WRITE_VALUE(ttisfunction(ci->func) ? ci->nresults : 0, int);
    WRITE_VALUE(ci->flags, uint8_t);

    if (eris_isLua(ci)) {
      WRITE_VALUE(ERIS_CI_KIND_LUA, uint8_t);
      const Closure *lcl = eris_ci_func(ci);

      // PC relative to the start of the code
      int64_t pc_offset = ci->savedpc - lcl->l.p->code;
      // the PC had better be in bounds.
      eris_assert(pc_offset >= 0 && pc_offset < lcl->l.p->sizecode);

      int yield_point = -1;
      for (int j = 0; j< lcl->l.p->sizeyieldpoints; ++j) {
        if (lcl->l.p->yieldpoints[j] == pc_offset) {
            yield_point = j;
            break;
        }
      }
      // We had better have yielded somewhere we think it's legal to yield.
      // We're yielded, how would we have yielded where it's illegal to yield?
      if (yield_point == -1) {
          switch(thread->status) {
            case LUA_OK:
            case LUA_YIELD:
            case LUA_BREAK:
              eris_error(info, ERIS_ERR_INVAL_PC);
              break;
            default:
              // It's fine to just force yield_point to -1 for errored threads,
              // they can never be revived again. It's not clear if this will mess
              // up stacktraces if the bytecode ever changes, but that's the price
              // we pay I guess!
              yield_point = -1;
              break;
          }
      }

      WRITE_VALUE(yield_point, int);
      WRITE_VALUE((int)pc_offset, int);
    }
    else if (ttisfunction(ci->func)) {
      WRITE_VALUE(ERIS_CI_KIND_C, uint8_t);
      // Unlike eris, we don't write a status here. I'm assuming that
      // only _threads_ have statuses now, which I guess makes sense.
      // When would you ever expect them to differ anyway?
      eris_ifassert(const int pre_closure_top = lua_gettop(info->L));
      // Copy the original closure from ci->func to info->L's stack for serialization.
      // The closure is already on the thread's stack at ci->func, and will be
      // loaded from there during unpersist. We expect we're really only
      // serializing a reference here and that the actual closure was serialized
      // when Ares was handling the stack.
      luaA_pushobject(info->L, ci->func);                     /* ... thread func */
      persist(info);
      lua_pop(info->L, 1);                                    /* ... thread */
      eris_assert(lua_gettop(info->L) == pre_closure_top);
    } else {
      WRITE_VALUE(ERIS_CI_KIND_NONE, uint8_t);
      eris_assert(ttisnil(ci->func));
    }
    poppath(info);
  }

  poppath(info);
  eris_assert(lua_gettop(info->L) == initial_stack_top);
  eris_assert(lua_type(info->L, -1) == LUA_TTHREAD);

  pushpath(info, ".openupval");
  lua_pushnil(info->L);                                     /* ... thread nil */
  level = 0;
  for (uv = thread->openupval;
       uv != nullptr;
       uv = uv->u.open.threadnext)
  {
    eris_ifassert(int uv_top = lua_gettop(info->L));
    pushpath(info, "[%d]", level++);
    WRITE_VALUE(eris_savestackidx(thread, uv->v) + 1, ares_size_t);
    eris_setobj(info->L, info->L->top - 1, uv->v);          /* ... thread obj */
    lua_pushlightuserdata(info->L, uv);                  /* ... thread obj id */
    persist_keyed(info, LUA_TUPVAL);                        /* ... thread obj */
    poppath(info);
    eris_assert(uv_top == lua_gettop(info->L));
  }
  // terminate the openupval list
  WRITE_VALUE(0, ares_size_t);
  lua_pop(info->L, 1);                                          /* ... thread */
  poppath(info);
  eris_assert(lua_type(info->L, -1) == LUA_TTHREAD);
}

/* Used in u_thread to validate read stack positions. */
#define validate(stackpos, inclmax) \
  if ((stackpos) < thread->stack || stackpos > (inclmax)) { \
    (stackpos) = thread->stack; \
    eris_error(info, ERIS_ERR_STACKBOUNDS); }

/* I had so hoped to get by without any 'hacks', but I surrender. We mark the
 * thread as incomplete to avoid the GC messing with it while we're building
 * it. Otherwise it may try to shrink its stack. We do this by setting its
 * stack field to null for every call that may trigger a GC run, since that
 * field is what's used to determine whether threads should be shrunk. See
 * lgc.c:699. Some of the locks could probably be joined (since nothing
 * inbetween requires the stack field to be valid), but I prefer to keep the
 * "invalid" blocks as small as possible to make it clearer. Also, locking and
 * unlocking are really just variable assignments, so they're really cheap. */
// #define LOCK(L) (L->stack = nullptr)
// #define UNLOCK(L) (L->stack = stack)
// ServerLua: This is no longer necessary because we pause GC during deserialization.
//  In fact, this may actually break GC validation due to the validation functions
//  not expecting a stack pointing to nullptr.
#define LOCK(L) ((void)0)
#define UNLOCK(L) ((void)0)

CLANG_NOOPT static void GCC_NOOPT
u_thread(Info *info) {                                                 /* ... */
  lua_State* thread;
  size_t level;
  StkId stack, o;

  eris_checkstack(info->L, 3);
  thread = lua_newthread(info->L);                              /* ... thread */
  registerobject(info);

  // The created thread's globals table is currently shared with info->L, which
  // isn't what we want. Rehydrate the thread's global environment (including
  // its sandboxed-ness if applicable) and shove it onto `thread->gt`
  pushpath(info, ".gt");
  unpersist(info);                                           /* ... thread gt */
  lua_setfenv(info->L, -2);                                     /* ... thread */
  poppath(info);

  /* Unpersist the stack. Read size first and adjust accordingly. */
  eris_reallocstack(thread, READ_VALUE(int), true);
  stack = thread->stack; /* After the realloc in case the address changes. */
  thread->top = thread->stack + (size_t)READ_VALUE(ares_size_t);
  validate(thread->top, thread->stack_last);

  /* Read the elements one by one. */
  LOCK(thread);
  pushpath(info, ".stack");
  UNLOCK(thread);
  level = 0;
  for (o = stack; o < thread->top; ++o) {
    LOCK(thread);
    pushpath(info, "[%d]", level++);
    unpersist(info);                                        /* ... thread obj */
    UNLOCK(thread);
    eris_setobj(thread, o, info->L->top - 1);
    lua_pop(info->L, 1);                                        /* ... thread */
    LOCK(thread);
    poppath(info);
    UNLOCK(thread);
  }
  LOCK(thread);
  poppath(info);
  UNLOCK(thread);

  /* Read general information. */
  thread->status = READ_VALUE(uint8_t);
  /* size_t _errfunc = */ READ_VALUE(ares_size_t);
  /* These are only used while a thread is being executed or can be deduced:
  thread->nCcalls = READ_VALUE(uint16_t);
  thread->allowhook = READ_VALUE(uint8_t); */

  /* Not supported.
  thread->hookmask = READ_VALUE(uint8_t);
  thread->basehookcount = READ_VALUE(int);
  thread->hookcount = READ_VALUE(int); */

  /* Read call information (stack frames). */
  LOCK(thread);
  pushpath(info, ".callinfo");
  UNLOCK(thread);

  int num_cis = READ_VALUE(int);
  luaD_reallocCI(thread, num_cis);
  thread->ci = thread->base_ci;
  level = 0;
  for (int ci_idx=0; ci_idx<num_cis; ++ci_idx) {
    // Need to add a callinfo if this isn't the first one
    if (ci_idx)
        incr_ci(thread);

    thread->ci->func = eris_restorestackidx(thread, (size_t)READ_VALUE(ares_size_t));
    validate(thread->ci->func, thread->top - 1);
    thread->ci->top = eris_restorestackidx(thread, (size_t)READ_VALUE(ares_size_t));
    validate(thread->ci->top, thread->stack_last);
    thread->ci->base = eris_restorestackidx(thread, (size_t)READ_VALUE(ares_size_t));
    validate(thread->ci->base, thread->top);
    thread->ci->nresults = READ_VALUE(int32_t);
    thread->ci->flags = READ_VALUE(uint8_t);

    // We have to do this later to not run afoul of hardmem tests,
    // otherwise this would be at the top of the loop.
    LOCK(thread);
    pushpath(info, "[%d]", level++);
    UNLOCK(thread);

    auto ci_kind = (eris_CIKind)READ_VALUE(uint8_t);
    if (ci_kind == ERIS_CI_KIND_LUA) {
      Closure *lcl = eris_ci_func(thread->ci);
      int yield_point = READ_VALUE(int);
      int real_pc = READ_VALUE(int);
      int pc_offset;

      // Make sure the yield point makes sense
      switch(thread->status) {
        case LUA_OK:
        case LUA_YIELD:
        case LUA_BREAK:
          if (yield_point < 0 || yield_point >= lcl->l.p->sizeyieldpoints) {
            eris_error(info, ERIS_ERR_THREADPC);
          }
          pc_offset = lcl->l.p->yieldpoints[yield_point];
          break;
      default:
          // it's "okay" if the yield point is nonsense for an errored thread,
          // it will never be run again. Just try to make the real PC sort of make sense
          if (yield_point >= 0 && yield_point < lcl->l.p->sizeyieldpoints)
            pc_offset = lcl->l.p->yieldpoints[yield_point];
          else
            pc_offset = std::min(0, std::max(real_pc, lcl->l.p->sizecode - 1));
          break;
      }
      thread->ci->savedpc = lcl->l.p->code + pc_offset;
      if (thread->ci->savedpc < lcl->l.p->code ||
          thread->ci->savedpc > lcl->l.p->code + lcl->l.p->sizecode)
      {
        thread->ci->savedpc = lcl->l.p->code; /* Just to be safe. */
        eris_error(info, ERIS_ERR_THREADPC);
      }
    } else if (ci_kind == ERIS_CI_KIND_C) {
      // ci->func is a StkIdx, so loading the stack should have loaded this.
      if (!ttisfunction(thread->ci->func)) {
        eris_error(info, "malformed data: expected function in call info");
      }

      // This function _should_ already be on the stack, let's make sure.
      LOCK(thread);
      unpersist(info);                                  /* ... thread func? */
      UNLOCK(thread);
      eris_checktype(info, -1, LUA_TFUNCTION);

      Closure *func_cl = clvalue(thread->ci->func);
      if (clvalue(luaA_toobject(info->L, -1))->c.f != func_cl->c.f) {
        eris_error(info, "malformed data: call info function mismatch");
      }
      // We don't actually use the function for anything, just checking!
      lua_pop(info->L, 1);                                    /* ... thread */
    } else {
      if (ci_kind != ERIS_CI_KIND_NONE) {
        eris_error(info, "malformed data: invalid call info kind");
      }
    }
    LOCK(thread);
    poppath(info);
    UNLOCK(thread);
  }
  if (thread->status == LUA_YIELD) {
//    thread->ci->extra = eris_savestack(thread,
//      eris_restorestackidx(thread, (size_t)READ_VALUE(ares_size_t)));
//    o = eris_restorestack(thread, thread->ci->extra);
//    validate(o, thread->top);
//    if (eris_ttypenv(o) != LUA_TFUNCTION) {
//      eris_error(info, ERIS_ERR_THREADCI);
//    }
  }
  LOCK(thread);
  poppath(info);
  UNLOCK(thread);

  /* Get from context: only zero for dead threads, otherwise one. */
  thread->nCcalls = thread->status != LUA_OK || lua_gettop(thread) != 0;

  /* Proceed to open upvalues. These upvalues will already exist due to the
   * functions using them having been unpersisted (they'll usually be in the
   * stack of the thread). For this reason we store all previous references to
   * the upvalue in a table that is returned when we try to unpersist an
   * upvalue, so that we can adjust these references in here. */
  LOCK(thread);
  pushpath(info, ".openupval");
  UNLOCK(thread);
  level = 0;
  for (;;) {
    UpVal *nuv;
    StkId stk;
    /* Get the position of the upvalue on the stack. As a special value we pass
     * zero to indicate there are no more upvalues. */
    const size_t offset = (size_t)READ_VALUE(ares_size_t);
    if (offset == 0) {
      break;
    }
    LOCK(thread);
    pushpath(info, "[%d]", level);
    UNLOCK(thread);
    stk = eris_restorestackidx(thread, offset - 1);
    validate(stk, thread->top - 1);
    LOCK(thread);
    unpersist(info);                                        /* ... thread tbl */
    UNLOCK(thread);
    eris_checktype(info, -1, LUA_TTABLE);

    /* Create the open upvalue either way. */
    LOCK(thread);
    nuv = eris_findupval(thread, stk);
    UNLOCK(thread);

    /* Then check if we need to patch some references. */
    lua_rawgeti(info->L, -1, UVTREF);               /* ... thread tbl lcl/nil */
    if (!lua_isnil(info->L, -1)) {                      /* ... thread tbl lcl */
      int i, n;
      eris_checktype(info, -1, LUA_TFUNCTION);
      /* Already exists, replace it. To do this we have to patch all the
       * references to the already existing one, which we added to the table in
       * u_closure. */
      lua_pop(info->L, 1);                                  /* ... thread tbl */
      for (i = UVTREF, n = lua_objlen(info->L, -1); i <= n; i += 2) {
        Closure *cl;
        int nup;
        lua_rawgeti(info->L, -1, i);                    /* ... thread tbl lcl */
        cl = clvalue(info->L->top - 1);
        lua_pop(info->L, 1);                                /* ... thread tbl */
        lua_rawgeti(info->L, -1, i + 1);                /* ... thread tbl nup */
        nup = lua_tointeger(info->L, -1);
        lua_pop(info->L, 1);                                /* ... thread tbl */
        /* Open the upvalue by pointing to the stack and register in GC. */
        setupvalue(info->L, &cl->l.uprefs[nup - 1], nuv);
        luaC_objbarrier(info->L, cl, nuv);
      }
    }
    else {                                              /* ... thread tbl nil */
      lua_pop(info->L, 1);                                  /* ... thread tbl */
    }

    /* Store open upvalue in table for future references. */
    LOCK(thread);
    lua_pop(info->L, 1);                                        /* ... thread */
    poppath(info);
    UNLOCK(thread);
  }
  poppath(info);

  luaC_threadbarrier(thread);

  eris_checktype(info, -1, LUA_TTHREAD);
}

#undef UNLOCK
#undef LOCK

#undef validate

/*
** ============================================================================
** Top-level delegator.
** ============================================================================
*/

static void
persist_typed(Info *info, int type) {                 /* perms reftbl ... obj */
  eris_ifassert(const int top = lua_gettop(info->L));
  if (info->level >= info->maxComplexity) {
    eris_error(info, ERIS_ERR_COMPLEXITY);
  }
  ++info->level;

  WRITE_VALUE(type, uint8_t);
  switch(type) {
    case LUA_TBOOLEAN:
      p_boolean(info);
      break;
    case LUA_TLIGHTUSERDATA:
      p_pointer(info);
      break;
    case LUA_TNUMBER:
      p_number(info);
      break;
    case LUA_TVECTOR:
      p_vector(info);
      break;
    case LUA_TSTRING:
      p_string(info);
      break;
    case LUA_TBUFFER:
      p_buffer(info);
      break;
    case LUA_TTABLE:
      p_table(info);
      break;
    case LUA_TFUNCTION:
      p_closure(info);
      break;
    case LUA_TUSERDATA:
      p_userdata(info);
      break;
    case LUA_TTHREAD:
      p_thread(info);
      break;
    case LUA_TPROTO:
      p_proto(info);
      break;
    case LUA_TUPVAL:
      p_upval(info);
      break;
    default:
      eris_error(info, ERIS_ERR_TYPEP, type);
  }                                                   /* perms reftbl ... obj */

  --info->level;
  eris_assert(top == lua_gettop(info->L));
}

/* Second-level delegating persist function, used for cases when persisting
 * data that's stored in the reftable with a key that is not the data itself,
 * namely upvalues and protos. */
static void
persist_keyed(Info *info, int type) {          /* perms reftbl ... obj refkey */
  eris_checkstack(info->L, 2);

  /* Keep a copy of the key for pushing it to the reftable, if necessary. */
  lua_pushvalue(info->L, -1);           /* perms reftbl ... obj refkey refkey */

  /* If the object has already been written, write a reference to it. */
  lua_rawget(info->L, REFTIDX);           /* perms reftbl ... obj refkey ref? */
  if (!lua_isnil(info->L, -1)) {           /* perms reftbl ... obj refkey ref */
    const int reference = lua_tointeger(info->L, -1);
    WRITE_VALUE(ERIS_REFERENCE, uint8_t);
    WRITE_VALUE(reference, int);
    lua_pop(info->L, 2);                              /* perms reftbl ... obj */
    return;
  }                                        /* perms reftbl ... obj refkey nil */
  lua_pop(info->L, 1);                         /* perms reftbl ... obj refkey */

  /* Copy the refkey for the perms check below. */
  lua_pushvalue(info->L, -1);           /* perms reftbl ... obj refkey refkey */

  /* Put the value in the reference table. This creates an entry pointing from
   * the object (or its key) to the id the object is referenced by. */
  lua_pushinteger(info->L, allocate_ref_idx(info));
                                    /* perms reftbl ... obj refkey refkey ref */
  lua_rawset(info->L, REFTIDX);                /* perms reftbl ... obj refkey */

  /* At this point, we'll give the permanents table a chance to play. */
  eris_ifassert(const int pre_permtable_top = lua_gettop(info->L));
  lua_gettable(info->L, PERMIDX);            /* perms reftbl ... obj permkey? */
  eris_assert(lua_gettop(info->L) == pre_permtable_top);
  if (!lua_isnil(info->L, -1)) {              /* perms reftbl ... obj permkey */
    type = lua_type(info->L, -2);
    /* Prepend permanent "type" so that we know it's a permtable key. This will
     * trigger u_permanent when unpersisting. Also write the original type, so
     * that we can verify what we get in the permtable when unpersisting is of
     * the same kind we had when persisting. */
    WRITE_VALUE(ERIS_PERMANENT, uint8_t);
    WRITE_VALUE(type, uint8_t);
    eris_ifassert(const int pre_persist_top = lua_gettop(info->L));
    persist(info);                            /* perms reftbl ... obj permkey */
    eris_assert(lua_gettop(info->L) == pre_persist_top);
    lua_pop(info->L, 1);                              /* perms reftbl ... obj */
    eris_assert(lua_gettop(info->L) == pre_permtable_top - 1);
  }
  else {                                          /* perms reftbl ... obj nil */
    /* No entry in the permtable for this object, persist it directly. */
    lua_pop(info->L, 1);                              /* perms reftbl ... obj */
    persist_typed(info, type);                        /* perms reftbl ... obj */
  }                                                   /* perms reftbl ... obj */
}

/* Top-level delegating persist function. */
static void
persist(Info *info) {                                 /* perms reftbl ... obj */
  /* Grab the object's type. */
  const int type = lua_type(info->L, -1);

  /* If the object is nil, only write its type. */
  if (type == LUA_TNIL) {
    WRITE_VALUE(type, uint8_t);
  }
  /* Write simple values directly, because writing a "reference" would take up
   * just as much space and we can save ourselves work this way. */
  else if (type == LUA_TBOOLEAN ||
           type == LUA_TLIGHTUSERDATA ||
           type == LUA_TNUMBER ||
           type == LUA_TVECTOR)
  {
    persist_typed(info, type);                        /* perms reftbl ... obj */
  }
  /* For all non-simple values we keep a record in the reftable, so that we
   * keep references alive across persisting and unpersisting an object. This
   * has the nice side-effect of saving some space. */
  else {
    eris_checkstack(info->L, 1);
    lua_pushvalue(info->L, -1);                   /* perms reftbl ... obj obj */
    persist_keyed(info, type);                        /* perms reftbl ... obj */
  }
}

/** ======================================================================== */

static void
u_permanent(Info *info) {                                 /* perms reftbl ... */
  const int type = READ_VALUE(uint8_t);
  /* Reserve reference to avoid the key going first. */
  const int reference = allocate_ref_idx(info);
  eris_checkstack(info->L, 1);
  unpersist(info);                                /* perms reftbl ... permkey */
  eris_assert(lua_type(info->L, PERMIDX) == LUA_TTABLE);
  lua_gettable(info->L, PERMIDX);                    /* perms reftbl ... obj? */
  if (lua_isnil(info->L, -1)) {                       /* perms reftbl ... nil */
    /* Since we may need permanent values to rebuild other structures, namely
     * closures and threads, we cannot allow perms to fail unpersisting. */
    eris_error(info, ERIS_ERR_SPER_UPERMNIL);
  }
  else if (lua_type(info->L, -1) != type) {            /* perms reftbl ... :( */
    /* For the same reason that we cannot allow nil we must also require the
     * unpersisted value to be of the correct type. */
    const char *want = kTypenames[type];
    const char *have = kTypenames[lua_type(info->L, -1)];
    eris_error(info, ERIS_ERR_SPER_UPERM, want, have);
  }                                                   /* perms reftbl ... obj */
  /* Create the entry in the reftable. */
  lua_pushvalue(info->L, -1);                     /* perms reftbl ... obj obj */
  lua_rawseti(info->L, REFTIDX, reference);           /* perms reftbl ... obj */
}

static void
unpersist(Info *info) {                                   /* perms reftbl ... */
  eris_ifassert(const int top = lua_gettop(info->L));
  if (info->level >= info->maxComplexity) {
    eris_error(info, ERIS_ERR_COMPLEXITY);
  }
  ++info->level;

  eris_checkstack(info->L, 1);
  {
    const uint8_t type = READ_VALUE(uint8_t);
    switch (type) {
      case LUA_TNIL:
        lua_pushnil(info->L);
        break;
      case LUA_TBOOLEAN:
        u_boolean(info);
        break;
      case LUA_TLIGHTUSERDATA:
        u_pointer(info);
        break;
      case LUA_TNUMBER:
        u_number(info);
        break;
      case LUA_TVECTOR:
        u_vector(info);
        break;
      case LUA_TSTRING:
        u_string(info);
        break;
      case LUA_TBUFFER:
        u_buffer(info);
        break;
      case LUA_TTABLE:
        u_table(info);
        break;
      case LUA_TFUNCTION:
        u_closure(info);
        break;
      case LUA_TUSERDATA:
        u_userdata(info);
        break;
      case LUA_TTHREAD:
        u_thread(info);
        break;
      case LUA_TPROTO:
        u_proto(info);
        break;
      case LUA_TUPVAL:
        u_upval(info);
        break;
      case ERIS_PERMANENT:
        u_permanent(info);
        break;
      case ERIS_REFERENCE: {
        const int reference = READ_VALUE(int);
        lua_rawgeti(info->L, REFTIDX, reference);   /* perms reftbl ud ... obj? */
        if (lua_isnil(info->L, -1)) {                 /* perms reftbl ud ... :( */
          eris_error(info, ERIS_ERR_REF, reference);
        }                                            /* perms reftbl ud ... obj */
        break;
      }
      default:
        eris_error(info, ERIS_ERR_TYPEU, type);
    }                                                  /* perms reftbl ... obj? */
  }

  --info->level;
  eris_assert(top + 1 == lua_gettop(info->L));
}

/*
** {===========================================================================
** Library functions.
** ============================================================================
*/

static void
p_header(Info *info) {
  WRITE_RAW(kHeader, HEADER_LENGTH);
  WRITE_VALUE(kCurrentVersion, uint32_t);
  WRITE_VALUE(sizeof(lua_Number), uint8_t);
  WRITE_VALUE(kHeaderNumber, lua_Number);
  WRITE_VALUE(sizeof(int), uint8_t);
  WRITE_VALUE(sizeof(size_t), uint8_t);
  WRITE_VALUE(LUA_VECTOR_SIZE, uint8_t);
}

static void
u_header(Info *info) {
  char header[HEADER_LENGTH];
  uint8_t number_size;
  uint32_t version_or_magic;

  READ_RAW(header, HEADER_LENGTH);
  if (strncmp(kHeader, header, HEADER_LENGTH) != 0) {
    eris_error(info, "invalid header signature");
  }

  /* Read next 4 bytes - could be version (new format) or old magic bytes */
  version_or_magic = READ_VALUE(uint32_t);

  if (version_or_magic == kOldMagicBytes) {
    /* Old format detected */
    info->u.upi.version = 0;
    /* Seek back 4 bytes so we can re-read the header fields */
    info->u.upi.reader->seekg(-4, std::ios_base::cur);
    if (info->u.upi.reader->fail()) {
      eris_error(info, ERIS_ERR_READ);
    }
  } else {
    /* New format - interpret as version number */
    info->u.upi.version = version_or_magic;
    if (info->u.upi.version > kCurrentVersion) {
      eris_error(info, "unsupported file format version (too new)");
    }
  }

  number_size = READ_VALUE(uint8_t);
  if (number_size != sizeof(lua_Number)) {
    eris_error(info, "incompatible floating point type");
  }
  /* In this case we really do want floating point equality. */
  if (READ_VALUE(lua_Number) != kHeaderNumber) {
    eris_error(info, "incompatible floating point representation");
  }
  info->u.upi.sizeof_int = READ_VALUE(uint8_t);
  info->u.upi.sizeof_size_t = READ_VALUE(uint8_t);
  info->u.upi.vector_components = READ_VALUE(uint8_t);
}

// stack effect -2
extern "C" void register_perm_checked(lua_State *L, int perms_idx, const char *context) {
    perms_idx = lua_absindex(L, perms_idx);

    // Check if this key is already registered
    lua_pushvalue(L, -2);
    lua_rawget(L, perms_idx);

    if (!lua_isnil(L, -1)) {
        const char *existing_name = lua_tostring(L, -1);
        const char *new_name = lua_tostring(L, -2);
        luaL_error(L, "Duplicate permanent object in %s: already registered as '%s', attempting to register as '%s'",
                  context,
                  existing_name ? existing_name : "(unknown)",
                  new_name ? new_name : "(unknown)");
    }
    lua_pop(L, 1);

    lua_rawset(L, perms_idx);
}

// Forward declaration
static void scavenge_general_perms_internal(lua_State *L, bool forUnpersist, const std::string& prefix, int perms_idx= -1);

/// Scans and registers a metatable (expects metatable already on stack)
static void scan_metatable(lua_State *L, bool forUnpersist, const std::string& prefix, int perms_idx) {
                                                      // ... perms obj_table mt

    // Register the metatable as a permanent
    std::string mt_name = prefix + "/mt";

    if (forUnpersist) {
        lua_pushstring(L, mt_name.c_str());      // ... perms obj_table mt name
        lua_pushvalue(L, -2);                    // ... perms obj_table mt name mt
        eris_assert(lua_type(L, -2) == LUA_TSTRING);
        eris_assert(lua_type(L, -1) == LUA_TTABLE);
    } else {
        lua_pushvalue(L, -1);                    // ... perms obj_table mt mt
        lua_pushstring(L, mt_name.c_str());      // ... perms obj_table mt mt name
        eris_assert(lua_type(L, -1) == LUA_TSTRING);
        eris_assert(lua_type(L, -2) == LUA_TTABLE);
    }
    register_perm_checked(L, perms_idx, mt_name.c_str());
                                                      // ... perms obj_table mt

    // Recursively scan the metatable's contents
    scavenge_general_perms_internal(L, forUnpersist, mt_name, perms_idx);
                                                      // ... perms obj_table mt
}

static void scavenge_general_perms_internal(lua_State *L, bool forUnpersist, const std::string& prefix, int perms_idx) {
    if (perms_idx == -1)
        perms_idx = lua_gettop(L) - 1;

    luaL_checktype(L, perms_idx, LUA_TTABLE);
    lua_pushnil(L);                                   /* ... perms glob_tab k */

    while (lua_next(L, -2)) {
        /* ... perms glob_tab k v */
        const auto val_type = lua_type(L, -1);
        const auto key_type = lua_type(L, -2);
        if (key_type != LUA_TSTRING) {
            lua_pop(L, 1);
            continue;
        }
        const char *key = lua_tostring(L, -2);
        // self-referential __index is typical for metatables.
        if (!strcmp("__index", key) && lua_rawequal(L, -1, -3))
        {
            lua_pop(L, 1);
            continue;
        }

        if (val_type == LUA_TFUNCTION) {
            Closure *cl = clvalue(luaA_toobject(L, -1));
            // We only need to be careful about C functions, so let's look for those.
            if (cl->isC) {
                std::string perm_name = prefix + "/" + key;

                if (forUnpersist) {
                    lua_pushstring(L, perm_name.c_str());
                                           /* ... perms glob_tab k v ref_name */
                    lua_pushvalue(L, -2);
                                      /* ... perms glob_tab k v ref_name func */
                    eris_assert(lua_type(L, -2) == LUA_TSTRING);
                    eris_assert(lua_type(L, -1) == LUA_TFUNCTION);
                } else {
                    lua_pushvalue(L, -1);
                                               /* ... perms glob_tab k v func */
                    lua_pushstring(L, perm_name.c_str());
                                      /* ... perms glob_tab k v func ref_name */
                    eris_assert(lua_type(L, -1) == LUA_TSTRING);
                    eris_assert(lua_type(L, -2) == LUA_TFUNCTION);
                }
                register_perm_checked(L, perms_idx, prefix.c_str());
                                                    /* ... perms glob_tab k v */
            }
        }
        else if (val_type == LUA_TTABLE) {
            // Skip _G since it's a self-reference to globals - we're already scanning it
            // (it's already a permanent as globals_base)
            if (prefix == "g" && strcmp(key, "_G") == 0) {
                lua_pop(L, 1);
                continue;
            }

            // When scanning type metatables, skip __index if it points to the global module table
            // (let the global scan register it with the canonical name)
            // I really should just upgrade this repo to C++17...
            if (prefix.find("type/") == 0 && prefix.find("/mt") == prefix.length() - 3 && strcmp(key, "__index") == 0) {
                // Extract type name from "type/TYPENAME/mt" format
                size_t type_start = 5;  // Skip "type/"
                size_t type_end = prefix.find("/mt");
                if (type_end != std::string::npos) {
                    std::string type_name = prefix.substr(type_start, type_end - type_start);

                    // Check if this __index table matches a global table with the same name as the type
                    lua_rawgetfield(L, LUA_BASEGLOBALSINDEX, type_name.c_str());

                    if (lua_rawequal(L, -2, -1)) {
                        // They're the same table - skip registering this, let global scan handle it
                        lua_pop(L, 2);
                        continue;
                    }
                    lua_pop(L, 2); // Pop global_module and base_globals
                }
            }

            // Register the table itself as a permanent
            std::string perm_name = prefix + "/" + key;

            if (forUnpersist) {
                lua_pushstring(L, perm_name.c_str());
                                           /* ... perms glob_tab k v ref_name */
                lua_pushvalue(L, -2);
                                      /* ... perms glob_tab k v ref_name table */
                eris_assert(lua_type(L, -2) == LUA_TSTRING);
                eris_assert(lua_type(L, -1) == LUA_TTABLE);
            } else {
                lua_pushvalue(L, -1);
                                               /* ... perms glob_tab k v table */
                lua_pushstring(L, perm_name.c_str());
                                      /* ... perms glob_tab k v table ref_name */
                eris_assert(lua_type(L, -1) == LUA_TSTRING);
                eris_assert(lua_type(L, -2) == LUA_TTABLE);
            }
            register_perm_checked(L, perms_idx, prefix.c_str());
                                                    /* ... perms glob_tab k v */

            // Scan the table's metatable if it has one
            if (lua_getmetatable(L, -1)) {          /* ... perms glob_tab k v mt */
                scan_metatable(L, forUnpersist, perm_name, perms_idx);
                lua_pop(L, 1);                      /* ... perms glob_tab k v */
            }

            // Recurse into table to find nested objects (only at top level to avoid deep nesting)
            if (prefix == "g") {
                scavenge_general_perms_internal(L, forUnpersist, perm_name, perms_idx);
            }
        }
        // Pop the value
        lua_pop(L, 1);                                /* ... perms glob_tab k */
        eris_assert(lua_type(L, -2) == LUA_TTABLE);
    }
}

/// Looks through the base globals for all objects to register as permanents.
/// Registers tables, C closures, and other objects reachable from globals.
static void scavenge_global_perms(lua_State *L, bool forUnpersist) {
    eris_ifassert(const int top = lua_gettop(L));
    // Push the real, underlying globals table onto the stack
    TValue gt_tv;
    sethvalue(L, &gt_tv, eris_getglobalsbase(L));
    luaA_pushobject(L, &gt_tv);                         /* ... perms glob_tab */

    scavenge_general_perms_internal(L, forUnpersist, "g");

                                                        /* ... perms glob_tab */
    lua_pop(L, 1);                                               /* ... perms */

    eris_assert(top == lua_gettop(L));
}

static void scavenge_sl_vm_internals(lua_State *L, bool forUnpersist) {
    if (!LUAU_IS_SL_VM(L))
        return;
    eris_ifassert(const int top = lua_gettop(L));
    int perms_idx = lua_gettop(L);

    // Mark the methods on internal userdata functions as internal
    for (int i=0; i<LUA_UTAG_LIMIT; ++i) {
        LuaTable *udata_mt = L->global->udatamt[i];
        if (udata_mt == nullptr)
            continue;

        // Do it by tag number in case names change
        std::string mt_prefix = "udata/" + std::to_string(i);

        TValue udata_mt_tv;
        sethvalue(L, &udata_mt_tv, udata_mt);
        luaA_pushobject(L, &udata_mt_tv);           /* ... perms udata_mt */

        scan_metatable(L, forUnpersist, mt_prefix, perms_idx);
        lua_pop(L, 1);                                       /* ... perms */
    }

    // Scan type metatables
    for (int type_idx = 0; type_idx < LUA_T_COUNT; type_idx++) {
        if (!L->global->mt[type_idx]) {
            continue;
        }

        TValue mt_tv;
        sethvalue(L, &mt_tv, L->global->mt[type_idx]);
        luaA_pushobject(L, &mt_tv);

        std::string mt_name = std::string("type/") + lua_typename(L, type_idx);

        scan_metatable(L, forUnpersist, mt_name, perms_idx);
        lua_pop(L, 1);
    }

    eris_assert(top == lua_gettop(L));
}

static void store_cfunc_perms(lua_State *L) {
  eris_ifassert(const int top = lua_gettop(L));
  int perms_idx = lua_gettop(L);
  // We need to do special things to handle permanent indexes for C functions,
  // there can be multiple closures for the same C function and their closure
  // objects will be considered unequal.

  // scan for c closures and store their function pointers
  lua_newtable(L);                                   /* ... perms new_perms */
  lua_pushnil(L);                                  /* ... perms new_perms k */
  while (lua_next(L, perms_idx)) {               /* ... perms new_perms k v */
    if (lua_type(L, -2) == LUA_TFUNCTION) {
      Closure *cl = clvalue(luaA_toobject(L, -2));
                                              /* ... perms new_perms k_cl v */
      if (cl->isC) {
        lua_pushvalue(L, -3);          /* ... perms new_perms k v new_perms */
        // NB: technically this should be keyed on the
        //  function + continuation function pair. I suppose it's technically
        //  possible for multiple closures to be defined with the same
        //  function pointer, but distinct continuation pointers, although I
        //  don't know if that ever actually happens!
        lua_pushlightuserdata(L, (void*)cl->c.f);
                               /* ... perms new_perms k_cl v new_perms k_id */
        lua_pushvalue(L, -3);
                             /* ... perms new_perms k_cl v new_perms k_id v */
        lua_rawset(L, -3);          /* ... perms new_perms k_cl v new_perms */
        lua_pop(L, 1);                        /* ... perms new_perms k_cl v */
      }
    }
    lua_pop(L, 1);                                 /* ... perms new_perms k */
    eris_assert(lua_type(L, -2) == LUA_TTABLE);
  }

  // update the perms table with the function pointer -> id mappings
  lua_pushnil(L);                                  /* ... perms new_perms k */
  while (lua_next(L, -2)) {                      /* ... perms new_perms k v */
    lua_pushvalue(L, -2);                      /* ... perms new_perms k v k */
    lua_pushvalue(L, -2);                    /* ... perms new_perms k v k v */
    // assign to the old perms table
    lua_settable(L, perms_idx);                  /* ... perms new_perms k v */
    lua_pop(L, 1);                                 /* ... perms new_perms k */
    eris_assert(lua_type(L, perms_idx) == LUA_TTABLE);
  }
  lua_pop(L, 1);                                               /* ... perms */
  eris_assert(top == lua_gettop(L));
}

void eris_populate_perms(lua_State *L, bool for_unpersist) {
    eris_ifassert(int old_top = lua_gettop(L));
    luaL_checktype(L, -1, LUA_TTABLE);

    // Let's check if we have cached perms data first.
    const char *name = "eris_perms";
    if (for_unpersist)
        name = "eris_uperms";
    if (lua_rawgetfield(L, LUA_REGISTRYINDEX, name) == LUA_TTABLE) {
        // Just copy the cached data into the table we were passed in.
        for (int index = 0; index = lua_rawiter(L, -1, index), index >= 0;) {
            lua_rawset(L, -4);
        }
        lua_pop(L, 1);
    } else {
        // No cache!
        lua_pop(L, 1);

        populateperms(L, for_unpersist);
        if (!for_unpersist) {
            // Populate fallback table only from statically registered closures
            store_cfunc_perms(L);
        }
        scavenge_global_perms(L, for_unpersist);
        scavenge_sl_vm_internals(L, for_unpersist);

        // Store a perm for the underlying globals object
        if (for_unpersist) {
            lua_pushstring(L, "globals_base");
            luaC_threadbarrier(L);
            sethvalue(L, L->top, eris_getglobalsbase(L));
            eris_incr_top(L);
            lua_rawset(L, -3);
        } else {
            luaC_threadbarrier(L);
            sethvalue(L, L->top, eris_getglobalsbase(L));
            eris_incr_top(L);
            lua_pushstring(L, "globals_base");
            lua_rawset(L, -3);
        }
    }
    eris_assert(lua_gettop(L) == old_top);
}

void eris_register_perms(lua_State *L, bool for_unpersist) {
    eris_ifassert(int old_top = lua_gettop(L));
    const char *name = "eris_perms";
    if (for_unpersist)
        name = "eris_uperms";
    // Clear out any existing value so we don't just read into the cache
    lua_pushnil(L);
    lua_rawsetfield(L, LUA_REGISTRYINDEX, name);

    // Now build the new cache
    lua_newtable(L);
    eris_populate_perms(L, for_unpersist);
    lua_setreadonly(L, -1, true);
    lua_rawsetfield(L, LUA_REGISTRYINDEX, name);
    eris_assert(lua_gettop(L) == old_top);
}

static void
unchecked_persist(lua_State *L, std::ostream *writer) {
  // pause GC for the duration of serialization - some objects we're creating aren't rooted
  // Also prevents beforeallocate callbacks from being invoked during setup
  ScopedDisableGC _disable_gc(L);

  eris_ifassert(int old_top=lua_gettop(L));
  Info info;                                            /* perms buff rootobj */
  info.L = L;
  info.level = 0;
  info.refcount = 0;
  info.maxComplexity = kMaxComplexity;
  info.generatePath = kGeneratePath;
  info.persisting = true;
  info.u.pi.writer = writer;
  info.u.pi.writeDebugInfo = kWriteDebugInformation;
  info.u.pi.persistingCFunc = false;

  eris_checkstack(L, 6);

  if (get_setting(L, (void*)&kSettingMaxComplexity)) {
                                                  /* perms buff rootobj value */
    info.maxComplexity = lua_tounsigned(L, -1);
    lua_pop(L, 1);                                      /* perms buff rootobj */
  }
  if (get_setting(L, (void*)&kSettingGeneratePath)) {
                                                  /* perms buff rootobj value */
    info.generatePath = lua_toboolean(L, -1);
    lua_pop(L, 1);                                      /* perms buff rootobj */
  }
  if (get_setting(L, (void*)&kSettingWriteDebugInfo)) {
                                                  /* perms buff rootobj value */
    info.u.pi.writeDebugInfo = lua_toboolean(L, -1);
    lua_pop(L, 1);                                      /* perms buff rootobj */
  }

  lua_newtable(L);                               /* perms buff rootobj reftbl */
  lua_insert(L, REFTIDX);                        /* perms reftbl buff rootobj */
  if (info.generatePath) {
    lua_newtable(L);                        /* perms reftbl buff rootobj path */
    lua_insert(L, PATHIDX);                 /* perms reftbl buff path rootobj */
    pushpath(&info, "root");
  }

  // Copy the perms table first, we're going to mutate it.
  lua_clonetable(L, PERMIDX);
  lua_replace(L, PERMIDX);

  /* Populate perms table with Lua internals. */
  lua_pushvalue(L, PERMIDX);         /* perms reftbl buff path? rootobj perms */
  eris_populate_perms(L, false);
  lua_pop(L, 1);                           /* perms reftbl buff path? rootobj */

#if HARDSTACKTESTS
  // Arrange the stack to make it more likely that we hit any lua_checkstack() misuse
  int pre_pad_top = lua_gettop(L);
  lua_checkstack(L, LUA_MINSTACK);
  while (lua_gettop(L) != LUA_MINSTACK - 1) {
    lua_pushnil(L);
  }
  // A reference to the root obj needs to end back up on top
  lua_pushvalue(L, pre_pad_top);
  eris_assert(lua_gettop(L) == LUA_MINSTACK);
#endif

  p_header(&info);
  persist(&info);                          /* perms reftbl buff path? rootobj */

#if HARDSTACKTESTS
  lua_pop(L, LUA_MINSTACK - pre_pad_top);
  eris_assert(lua_gettop(L) == pre_pad_top);
#endif

  if (info.generatePath) {                  /* perms reftbl buff path rootobj */
    lua_remove(L, PATHIDX);                      /* perms reftbl buff rootobj */
  }                                              /* perms reftbl buff rootobj */
  lua_remove(L, REFTIDX);                               /* perms buff rootobj */
  eris_assert(lua_gettop(L) == old_top);
}

static void
unchecked_unpersist(lua_State *L, std::istream *reader) {/* perms str? */
  // pause GC for the duration of deserialization - some objects we're creating aren't rooted
  // Also prevents beforeallocate callbacks from being invoked during setup
  ScopedDisableGC _disable_gc(L);

  eris_ifassert(int old_top = lua_gettop(L));
  Info info;
  info.L = L;
  info.level = 0;
  info.refcount = 0;
  info.maxComplexity = kMaxComplexity;
  info.generatePath = kGeneratePath;
  info.persisting = false;
  info.u.upi.reader = reader;

  eris_checkstack(L, 6);

  if (get_setting(L, (void*)&kSettingMaxComplexity)) {
                                                  /* perms buff rootobj value */
    info.maxComplexity = lua_tounsigned(L, -1);
    lua_pop(L, 1);                                      /* perms buff rootobj */
  }
  if (get_setting(L, (void*)&kSettingGeneratePath)) {
                                                 /* perms buff? rootobj value */
    info.generatePath = lua_toboolean(L, -1);
    lua_pop(L, 1);                                     /* perms buff? rootobj */
  }

  lua_newtable(L);                                       /* perms str? reftbl */
  lua_insert(L, REFTIDX);                                /* perms reftbl str? */
  if (info.generatePath) {
    /* Make sure the path is always at index 4, so that it's the same for
     * persist and unpersist. */
    lua_pushnil(L);                                  /* perms reftbl str? nil */
    lua_insert(L, BUFFIDX);                          /* perms reftbl nil str? */
    lua_newtable(L);                            /* perms reftbl nil str? path */
    lua_insert(L, PATHIDX);                     /* perms reftbl nil path str? */
    pushpath(&info, "root");
  }

  // Copy the perms table first, we're going to mutate it.
  lua_clonetable(L, PERMIDX);
  lua_replace(L, PERMIDX);

  /* Populate perms table with Lua internals. */
  lua_pushvalue(L, PERMIDX);            /* perms reftbl nil? path? str? perms */
  eris_populate_perms(L, true);
  lua_pop(L, 1);                              /* perms reftbl nil? path? str? */

#if HARDSTACKTESTS
  // Arrange the stack to make it more likely that we hit any lua_checkstack() misuse
  int pre_pad_top = lua_gettop(L);
  lua_checkstack(L, LUA_MINSTACK);
  while (lua_gettop(L) != LUA_MINSTACK - 1) {
      lua_pushnil(L);
  }
  // A reference to the root obj needs to end back up on top
  lua_pushvalue(L, pre_pad_top);
  eris_assert(lua_gettop(L) == LUA_MINSTACK);
#endif

  u_header(&info);
  unpersist(&info);                   /* perms reftbl nil? path? str? rootobj */

#if HARDSTACKTESTS
  // Slot the top of the stack back in where it should be
  lua_replace(L, pre_pad_top + 1);
  lua_pop(L, LUA_MINSTACK - pre_pad_top - 1);
  eris_assert(lua_gettop(L) == pre_pad_top + 1);
#endif

  if (info.generatePath) {              /* perms reftbl nil path str? rootobj */
    lua_remove(L, PATHIDX);                  /* perms reftbl nil str? rootobj */
    lua_remove(L, BUFFIDX);                      /* perms reftbl str? rootobj */
  }                                              /* perms reftbl str? rootobj */
  lua_remove(L, REFTIDX);                               /* perms str? rootobj */
  eris_assert(lua_gettop(L) == old_top + 1);
}

/** ======================================================================== */

static int
l_persist(lua_State *L) {                             /* perms? rootobj? ...? */

  /* See if we have anything at all. */
  luaL_checkany(L, 1);

  /* If we only have one object we assume it is the root object and that there
   * is no perms table, so we create an empty one for internal use. */
  if (lua_gettop(L) == 1) {                                        /* rootobj */
    eris_checkstack(L, 1);
    lua_newtable(L);                                         /* rootobj perms */
    lua_insert(L, PERMIDX);                                  /* perms rootobj */
  }
  else {
    luaL_checktype(L, 1, LUA_TTABLE);                  /* perms rootobj? ...? */
    luaL_checkany(L, 2);                                /* perms rootobj ...? */
    lua_settop(L, 2);                                        /* perms rootobj */
  }
  eris_checkstack(L, 1);
  lua_pushnil(L);                                       /* perms rootobj buff */
  lua_insert(L, 2);                                     /* perms buff rootobj */


  std::ostringstream writer;
  unchecked_persist(L, &writer);                        /* perms buff rootobj */

  /* Copy the buffer as the result string before removing it, to avoid the data
   * being garbage collected. */
  lua_pushlstring(L, writer.str().c_str(), writer.str().size());
                                                    /* perms buff rootobj str */

  return 1;
}

static int
l_unpersist(lua_State *L) {                               /* perms? str? ...? */

  /* See if we have anything at all. */
  luaL_checkany(L, 1);

  /* If we only have one object we assume it is the root object and that there
   * is no perms table, so we create an empty one for internal use. */
  if (lua_gettop(L) == 1) {                                           /* str? */
    eris_checkstack(L, 1);
    lua_newtable(L);                                            /* str? perms */
    lua_insert(L, PERMIDX);                                     /* perms str? */
  }
  else {
    luaL_checktype(L, 1, LUA_TTABLE);                      /* perms str? ...? */
  }

  size_t buff_len;
  const char *buff = luaL_checklstring(L, 2, &buff_len);
  std::istringstream reader(std::string(buff, buff_len));
  reader.seekg(0);
  lua_settop(L, 2);                                              /* perms str */

  unchecked_unpersist(L, &reader);                       /* perms str rootobj */

  return 1;
}

#define IS(s) strncmp(s, name, length < sizeof(s) ? length : sizeof(s)) == 0

static int
l_settings(lua_State *L) {                                /* name value? ...? */
  size_t length;
  const char *name = luaL_checklstring(L, 1, &length);
  if (lua_isnone(L, 2)) {                                        /* name ...? */
    lua_settop(L, 1);                                                 /* name */
    /* Get the current setting value and return it. */
    if (IS(kSettingWriteDebugInfo)) {
      if (!get_setting(L, (void*)&kSettingWriteDebugInfo)) {
        lua_pushboolean(L, kWriteDebugInformation);
      }
    }
    else if (IS(kSettingGeneratePath)) {
      if (!get_setting(L, (void*)&kSettingGeneratePath)) {
        lua_pushboolean(L, kGeneratePath);
      }
    }
    else if (IS(kSettingMaxComplexity)) {
      if (!get_setting(L, (void*)&kSettingMaxComplexity)) {
        lua_pushunsigned(L, kMaxComplexity);
      }
    }
    else {
      luaL_argerror(L, 1, "no such setting");
      return 0;
    }                                                           /* name value */
    return 1;
  }
  else {                                                   /* name value ...? */
    lua_settop(L, 2);                                           /* name value */
    /* Set a new value for the setting. */
    if (IS(kSettingWriteDebugInfo)) {
      luaL_opt(L, checkboolean, 2, false);
      set_setting(L, (void*)&kSettingWriteDebugInfo);
    }
    else if (IS(kSettingGeneratePath)) {
      luaL_opt(L, checkboolean, 2, false);
      set_setting(L, (void*)&kSettingGeneratePath);
    }
    else if (IS(kSettingMaxComplexity)) {
      luaL_optunsigned(L, 2, 0);
      set_setting(L, (void*)&kSettingMaxComplexity);
    }
    else {
      luaL_argerror(L, 1, "no such setting");
      return 0;
    }                                                                 /* name */
    return 0;
  }
}

#undef IS

/** ======================================================================== */

static luaL_Reg erislib[] = {
  { "persist", l_persist },
  { "unpersist", l_unpersist },
  { "settings", l_settings },
  { nullptr, nullptr }
};

LUA_API int luaopen_eris(lua_State *L) {
  luaL_register(L, LUA_ERISLIBNAME, erislib);
  return 1;
}

/* }======================================================================== */

/*
** {===========================================================================
** Public API functions.
** ============================================================================
*/

LUA_API void
eris_dump(lua_State *L, std::ostream *writer) {     /* perms? rootobj? */
  if (lua_gettop(L) > 2) {
    luaL_error(L, "too many arguments");
  }
  luaL_checktype(L, 1, LUA_TTABLE);                         /* perms rootobj? */
  luaL_checkany(L, 2);                                       /* perms rootobj */
  lua_pushnil(L);                                        /* perms rootobj nil */
  lua_insert(L, -2);                                     /* perms nil rootobj */
  unchecked_persist(L, writer);                          /* perms nil rootobj */
  lua_remove(L, -2);                                         /* perms rootobj */
}

LUA_API void
eris_undump(lua_State *L, std::istream *reader) {                   /* perms? */
  if (lua_gettop(L) > 1) {
    luaL_error(L, "too many arguments");
  }
  luaL_checktype(L, 1, LUA_TTABLE);                                  /* perms */
  unchecked_unpersist(L, reader);                            /* perms rootobj */
}

/** ======================================================================== */

LUA_API int
eris_persist(lua_State *L, int perms, int value) {                    /* ...? */
  perms = lua_absindex(L, perms);
  value = lua_absindex(L, value);
  eris_checkstack(L, 3);
  lua_pushcfunction(L, l_persist, "l_persist");              /* ... l_persist */
  lua_pushvalue(L, perms);                             /* ... l_persist perms */
  lua_pushvalue(L, value);                     /* ... l_persist perms rootobj */
  return lua_pcall(L, 2, 1, 0);                                    /* ... str */
}

LUA_API int
eris_unpersist(lua_State *L, int perms, int value) {                   /* ... */
  perms = lua_absindex(L, perms);
  value = lua_absindex(L, value);
  eris_checkstack(L, 3);
  lua_pushcfunction(L, l_unpersist, "l_unpersist");
  lua_pushvalue(L, perms);                           /* ... l_unpersist perms */
  lua_pushvalue(L, value);                       /* ... l_unpersist perms str */
  return lua_pcall(L, 2, 1, 0);
}

LUA_API void
eris_get_setting(lua_State *L, const char *name) {                     /* ... */
  eris_checkstack(L, 2);
  lua_pushcfunction(L, l_settings, "l_settings");           /* ... l_settings */
  lua_pushstring(L, name);                             /* ... l_settings name */
  lua_call(L, 1, 1);                                             /* ... value */
}

LUA_API void
eris_set_setting(lua_State *L, const char *name, int value) {          /* ... */
  value = lua_absindex(L, value);
  eris_checkstack(L, 3);
  lua_pushcfunction(L, l_settings, "l_settings");           /* ... l_settings */
  lua_pushstring(L, name);                             /* ... l_settings name */
  lua_pushvalue(L, value);                       /* ... l_settings name value */
  lua_call(L, 2, 0);                                                   /* ... */
}

/* }======================================================================== */

/*
** {===========================================================================
** ServerLua serialization functions.
** ============================================================================
*/

static void gatherfunctions(std::vector<Proto*>& results, Proto* proto) {
  if (results.size() <= size_t(proto->bytecodeid))
    results.resize(proto->bytecodeid + 1);

  // Skip protos that we've already compiled in this run: this happens because
  // at -O2, inlined functions get their protos reused
  if (results[proto->bytecodeid])
    return;

  // We need to know where it came from to give it a unique name
  LUAU_ASSERT(proto->source);

  results[proto->bytecodeid] = proto;

  for (int i = 0; i < proto->sizep; i++)
    gatherfunctions(results, proto->p[i]);
}

LUA_API lua_State *
eris_make_forkserver(lua_State *L) {
  lua_State *GL = lua_mainthread(L);
  // We can't clone the main thread!
  eris_assert(GL != L);
  // Using a running script as a forkserver gets nasty, so make sure this
  // isn't one.
  eris_assert(lua_gettop(L) == 1);
  eris_assert(lua_isfunction(L, -1));
  // The thread we're serializing must be a sandboxed thread. We rely on
  // globals being provided through the environment proxy table to ensure
  // efficient serializability of state.
  eris_assert(L->gt->safeenv && GL->gt->readonly);

  // Make a new thread for the forkserver that will be used to serialize `L`
  // and spin up new instances of it on demand
  lua_State *Lforker = lua_newthread(GL);                  /* GL: ... Lforker */
  // Show paths on error
  lua_pushboolean(Lforker, 1);                            /* LForker: enabled */
  eris_set_setting(Lforker, kSettingGeneratePath, -1);
  lua_pop(Lforker, 1);                                            /* LForker: */
  luaL_sandboxthread(Lforker);

  // Add a table for state related to this forker and anchor it to the base of the
  // stack. This effectively works as a forker-local registry.
  lua_newtable(Lforker);                                    /* LForker: state */

  // Collect all protos reachable from the main func (should be all of them)
  std::vector<Proto *> protos = {};
  if (lua_isLfunction(L, -1))
    gatherfunctions(protos, clvalue(luaA_toobject(L, -1))->l.p);

  // build the perm val -> id table for persisting
  lua_newtable(Lforker);                              /* Lforker: state perms */
  for (auto *proto : protos) {
    if (!proto) continue;
    lua_pushlightuserdata(Lforker, proto);      /* Lforker: state perms proto */
    lua_pushfstring(Lforker, "proto/%s/%d", getstr(proto->source), proto->bytecodeid);
                                       /* Lforker: state perms proto proto_id */
    lua_rawset(Lforker, -3);                          /* Lforker: state perms */
  }

  lua_pushlightuserdata(Lforker, (void *)&kForkerPermsTable);
                                                  /* LForker: state perms key */
  lua_pushvalue(Lforker, -2);               /* LForker: state perms key perms */
  lua_rawset(Lforker, FS_STATE_IDX);                  /* LForker: state perms */

  // push the thread to fork
  setthvalue(Lforker, Lforker->top, L);            /* Lforker: state perms th */
  eris_incr_top(Lforker);
  luaC_threadbarrier(Lforker);

  // args stay on stack, but serialized form added
  eris_persist(Lforker, -2, -1);               /* Lforker: state perms th ser */

  // Anchor the base thread we're forking in the registry so it stays alive as
  // long as we do.
  lua_pushlightuserdata(Lforker, (void *)&kForkerBaseThread);
                                           /* LForker: state perms th ser key */
  lua_pushvalue(Lforker, -3);           /* LForker: state perms th ser key th */
  lua_rawset(Lforker, FS_STATE_IDX);           /* LForker: state perms th ser */

  // Store the "base" state for new scripts of this type
  lua_pushlightuserdata(Lforker, (void *)&kForkerBaseState);
                                           /* LForker: state perms th ser key */
  lua_pushvalue(Lforker, -2);          /* LForker: state perms th ser key ser */
  lua_rawset(Lforker, FS_STATE_IDX);           /* LForker: state perms th ser */

  lua_pop(Lforker, 3);                                      /* LForker: state */

  // Make a new table for keys to use when deserializing script states
  lua_newtable(Lforker);                             /* Lforker: state uperms */

  // rebuild the perms table to work for deserialization
  for (auto *proto : protos) {
    if (!proto) continue;
    lua_pushfstring(Lforker, "proto/%s/%d", getstr(proto->source), proto->bytecodeid);
                                            /* Lforker: state uperms proto_id */
    lua_pushlightuserdata(Lforker, proto);
                                      /* Lforker: state uperms proto_id proto */
    lua_rawset(Lforker, -3);                         /* Lforker: state uperms */
  }

  // store the uperms table to the state table
  lua_pushlightuserdata(Lforker, (void *)&kForkerUPermsTable);
                                                 /* LForker: state uperms key */
  lua_pushvalue(Lforker, -2);             /* LForker: state uperms key uperms */
  lua_rawset(Lforker, FS_STATE_IDX);                 /* LForker: state uperms */

  lua_pop(Lforker, 1);                                      /* LForker: state */

  // We don't want to log paths when deserializing, it's expensive, but we need it.
  // TODO: disable this, we need it for now.
  lua_pushboolean(Lforker, 1);                      /* LForker: state enabled */
  eris_set_setting(Lforker, kSettingGeneratePath, -1);
  lua_pop(Lforker, 1);                                      /* LForker: state */

  // only the state table should be left
  LUAU_ASSERT(lua_gettop(Lforker) == 1);

  return Lforker;
}

LUA_API lua_State*
eris_fork_thread(lua_State *Lforker, uint8_t default_state, uint8_t memcat) {
                                               /* Lforker: state default_ser? */
  eris_assert(lua_getmemcat(Lforker) == 0);
  lua_State *GL = lua_mainthread(Lforker);
  // We can't clone the main thread!
  LUAU_ASSERT(GL != Lforker);

  // serialized script state isn't already on the stack, push the default state
  if (default_state) {
    lua_pushlightuserdata(Lforker, (void *)&kForkerBaseState);
    lua_rawget(Lforker, FS_STATE_IDX);           /* Lforker: state serialized */
  }

  eris_ifassert(int start_top = lua_gettop(Lforker));

  lua_pushlightuserdata(Lforker, (void*)&kForkerUPermsTable);
  lua_rawget(Lforker, FS_STATE_IDX);      /* Lforker: state serialized uperms */

  // Make sure any objects we create during deserialization are created with the desired memcat
  lua_setmemcat(Lforker, memcat);

  int status = eris_unpersist(Lforker, -1, -2); /* Lforker: state serialized uperms new_th */

  // Done, set the memcat back to the main one.
  lua_setmemcat(Lforker, 0);

  if (status == LUA_OK) {
    eris_assert(lua_isthread(Lforker, -1));
    eris_assert(lua_getmemcat(lua_tothread(Lforker, -1)) == memcat);
  } else {
    // ServerLua: Make sure the error message is included (error types with messages on stack)
    eris_assert(status == LUA_ERRRUN || status == LUA_ERRKILL);
    eris_assert(lua_gettop(Lforker) == start_top + 2);
    // remove uperms table
    lua_remove(Lforker, -2);
    // remove state string
    lua_remove(Lforker, -2);

    eris_assert(lua_gettop(Lforker) == start_top);
    eris_assert(lua_isstring(Lforker, -1));
    return nullptr;
  }

  // Anchor the reference to the unpersisted thread to the main thread
  lua_xmove(Lforker, GL, 1);
         /* GL: ... Lforker ... new_th */ /* Lforker: state serialized uperms */

  lua_State *Lchild = thvalue(luaA_toobject(GL, -1));
  lua_pop(Lforker, 2);                                      /* Lforker: state */
  return Lchild;
}

LUA_API int
eris_serialize_thread(lua_State *Lforker, lua_State *L) {
  eris_assert(lua_getmemcat(Lforker) == 0);
  eris_assert(lua_gettop(Lforker) == 1);
  eris_assert(lua_mainthread(L) == lua_mainthread(Lforker));
  lua_pushlightuserdata(Lforker, (void *)&kForkerPermsTable);
  lua_rawget(Lforker, FS_STATE_IDX);                  /* LForker: state perms */

  // push the thread to fork
  setthvalue(Lforker, Lforker->top, L);            /* Lforker: state perms th */
  eris_incr_top(Lforker);
  luaC_threadbarrier(Lforker);

  lua_pushboolean(Lforker, 1);                            /* LForker: enabled */
  eris_set_setting(Lforker, kSettingGeneratePath, -1);
  lua_pop(Lforker, 1);

  // args stay on stack, but serialized form added
  int status = eris_persist(Lforker, -2, -1);  /* Lforker: state perms th ser */
  lua_remove(Lforker, -2);
  lua_remove(Lforker, -2);                              /* Lforker: state ser */
  // only state and serialized left on stack
  eris_assert(lua_gettop(Lforker) == 2);
  return status;
}

LUA_API void
eris_set_compile_func(void (*compile_func)(lua_State*, int)) {
  sAresCodeGenCompile = compile_func;
}

/* }======================================================================== */

