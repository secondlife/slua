// ServerLua: Implementation for lstrbuf.h
#include "lstrbuf.h"
#include "lobject.h"
#include "lmem.h"
#include "lapi.h"
#include "lstring.h"
#include "lgc.h"

#include <string>

// Recover the owning userdata's memcat from the GC header.
// The struct lives at Udata::data, so we back-calculate to the Udata*.
static uint8_t strbuf_memcat(const lua_YieldSafeStrBuf* s)
{
    auto* u = (Udata*)((char*)s - offsetof(Udata, data));
    return u->memcat;
}

void strbuf_init(lua_State* L, lua_YieldSafeStrBuf* s, size_t len)
{
    size_t size = len ? len + 1 : STRBUF_DEFAULT_SIZE;

    s->buf = (char*)luaM_new_(L, size, strbuf_memcat(s));
    s->size = uint32_t(size);
    s->length = 0;

    strbuf_ensure_null(s);
}

void strbuf_free(lua_State* L, lua_YieldSafeStrBuf* s)
{
    if (s->buf)
    {
        luaM_free_(L, s->buf, s->size, strbuf_memcat(s));
        s->buf = nullptr;
    }
}

// Hard limit on buffer size.
#define STRBUF_MAX_SIZE 204800

static size_t calculate_new_size(lua_YieldSafeStrBuf* s, size_t len)
{
    // Room for NULL terminator
    size_t reqsize = len + 1;

    // Overflow check: len+1 wrapped
    if (reqsize < len)
        return STRBUF_MAX_SIZE + 1;

    // If shrinking, use exact size
    if (s->size > reqsize)
        return reqsize;

    size_t newsize = s->size;

    // Exponential doubling with overflow guard
    while (newsize < reqsize)
    {
        if (newsize >= SIZE_MAX / 2)
        {
            newsize = reqsize;
            break;
        }
        newsize *= 2;
    }

    return newsize;
}

void strbuf_resize(lua_State* L, lua_YieldSafeStrBuf* s, size_t len)
{
    if (len > STRBUF_MAX_SIZE)
        luaL_error(L, "string buffer exceeded maximum size");

    size_t newsize = calculate_new_size(s, len);

    s->buf = (char*)luaM_realloc_(L, s->buf, s->size, newsize, strbuf_memcat(s));
    s->size = uint32_t(newsize);
}

void strbuf_append_string(lua_State* L, lua_YieldSafeStrBuf* s, const char* str)
{
    size_t len = strlen(str);
    strbuf_ensure_empty_length(L, s, len);
    memcpy(s->buf + s->length, str, len);
    s->length += uint32_t(len);
}

// Copy buffer contents to an untracked C++ string, nil out the stack slot,
// then create the Lua string into the same slot. This ensures the strbuf
// and the resulting Lua string are never on the stack simultaneously,
// avoiding a spike in the apparent memory cost charged to the user thread.
void strbuf_tostring_inplace(lua_State* L, int idx, bool free_storage)
{
    // idx must be a real stack index; we compute a base-relative offset below
    LUAU_ASSERT(!lua_ispseudo(idx));
    auto* s = (lua_YieldSafeStrBuf*)lua_touserdata(L, idx);
    std::string storage(s->buf, s->length);

    if (free_storage)
        strbuf_free(L, s);

    // Resolve idx to a base-relative offset so we can survive stack reallocs
    StkId base = L->base;
    ptrdiff_t offset = const_cast<TValue*>(luaA_toobject(L, idx)) - base;

    setnilvalue(base + offset);

    luaC_threadbarrier(L);
    // luaS_newlstr may trigger GC which can realloc the stack
    TString* ts = luaS_newlstr(L, storage.data(), storage.size());
    setsvalue(L, L->base + offset, ts);
}

// Register the GC destructor for UTAG_STRBUF userdata.
void lstrbuf_setup(lua_State* L)
{
    lua_setuserdatadtor(
        L,
        UTAG_STRBUF,
        [](lua_State* L, void* ud)
        {
            auto* b = (lua_YieldSafeStrBuf*)ud;
            strbuf_free(L, b);
        }
    );
}

// Pushes a new lua_YieldSafeStrBuf userdata onto the stack.
lua_YieldSafeStrBuf* lstrbuf_push(lua_State* L)
{
    lua_newuserdatatagged(L, sizeof(lua_YieldSafeStrBuf), UTAG_STRBUF);
    auto* b = (lua_YieldSafeStrBuf*)lua_touserdata(L, -1);
    memset(b, 0, sizeof(lua_YieldSafeStrBuf));
    strbuf_init(L, b, 0);
    return b;
}
