// ServerLua: Yield-safe string buffer backed by Luau's memory allocator.
// Based on strbuf.c/h from cjson
//
// Lives inside a UTAG_STRBUF tagged userdata for GC-rooted lifetime.
// Allocating operations require lua_State* for luaM_realloc_ tracking.
// Non-allocating operations (unsafe appends, length queries) do not.
#pragma once

#include "lua.h"
#include "lualib.h"
#include "llsl.h"

#include <cstddef>
#include <cstring>

struct lua_YieldSafeStrBuf
{
    char* buf;
    uint32_t size;
    uint32_t length;
};

#ifndef STRBUF_DEFAULT_SIZE
#define STRBUF_DEFAULT_SIZE 128
#endif

// --- Allocating functions (require lua_State*) ---

LUAI_FUNC void luaYB_init(lua_State* L, lua_YieldSafeStrBuf* s, size_t len);
LUAI_FUNC void luaYB_free(lua_State* L, lua_YieldSafeStrBuf* s);
LUAI_FUNC void luaYB_resize(lua_State* L, lua_YieldSafeStrBuf* s, size_t len);
LUAI_FUNC void luaYB_appendstr(lua_State* L, lua_YieldSafeStrBuf* s, const char* str);

// Replace the strbuf userdata at stack index `idx` with its Lua string equivalent.
// The strbuf and the resulting string are never on the stack simultaneously.
// If free_storage is true, the buffer's tracked heap allocation is freed first,
// reducing peak tracked memory to max(strbuf, string) instead of strbuf + string.
LUAI_FUNC void luaYB_tostring(lua_State* L, int idx, bool free_storage);

// --- Non-allocating inline functions ---

static inline void luaYB_reset(lua_YieldSafeStrBuf* s)
{
    s->length = 0;
}

static inline int luaYB_allocated(lua_YieldSafeStrBuf* s)
{
    return s->buf != NULL;
}

// Returns bytes remaining, reserving space for a NULL terminator.
static inline size_t luaYB_space(lua_YieldSafeStrBuf* s)
{
    return s->size - s->length - 1;
}

static inline void luaYB_ensure(lua_State* L, lua_YieldSafeStrBuf* s, size_t len)
{
    if (len > luaYB_space(s))
        luaYB_resize(L, s, s->length + len);
}

static inline char* luaYB_ptr(lua_YieldSafeStrBuf* s)
{
    return s->buf + s->length;
}

static inline void luaYB_setlen(lua_YieldSafeStrBuf* s, int len)
{
    s->length = len;
}

static inline void luaYB_extend(lua_YieldSafeStrBuf* s, size_t len)
{
    s->length += len;
}

static inline size_t luaYB_len(lua_YieldSafeStrBuf* s)
{
    return s->length;
}

static inline void luaYB_appendchar(lua_State* L, lua_YieldSafeStrBuf* s, const char c)
{
    luaYB_ensure(L, s, 1);
    s->buf[s->length++] = c;
}

static inline void luaYB_appendchar_unsafe(lua_YieldSafeStrBuf* s, const char c)
{
    s->buf[s->length++] = c;
}

static inline void luaYB_appendmem(lua_State* L, lua_YieldSafeStrBuf* s, const char* c, size_t len)
{
    luaYB_ensure(L, s, len);
    memcpy(s->buf + s->length, c, len);
    s->length += len;
}

static inline void luaYB_appendmem_unsafe(lua_YieldSafeStrBuf* s, const char* c, size_t len)
{
    memcpy(s->buf + s->length, c, len);
    s->length += len;
}

static inline void luaYB_ensurenull(lua_YieldSafeStrBuf* s)
{
    s->buf[s->length] = 0;
}

static inline char* luaYB_data(lua_YieldSafeStrBuf* s, size_t* len)
{
    if (len)
        *len = s->length;

    return s->buf;
}

// Push buffer contents as an immutable Lua string.
static inline void luaYB_pushresult(lua_State* L, lua_YieldSafeStrBuf* s)
{
    lua_pushlstring(L, s->buf, s->length);
}

// Pop a string from the top of the stack and append it.
// Raises a type error if the top value is not string-coercible.
static inline void luaYB_addvalue(lua_State* L, lua_YieldSafeStrBuf* s)
{
    size_t len;
    const char* str = lua_tolstring(L, -1, &len);
    if (!str)
        luaL_typeerror(L, -1, "string");
    luaYB_appendmem(L, s, str, len);
    lua_pop(L, 1);
}

// --- Userdata lifecycle ---

// Registers the UTAG_STRBUF GC destructor. Call once during VM setup.
LUAI_FUNC void luaYB_setup(lua_State* L);

// Pushes a new lua_YieldSafeStrBuf userdata onto the stack and returns a pointer to it.
LUAI_FUNC lua_YieldSafeStrBuf* luaYB_push(lua_State* L);
