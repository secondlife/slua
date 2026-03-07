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

void luaYB_init(lua_State* L, lua_YieldSafeStrBuf* s, size_t len)
{
    size_t size = len ? len + 1 : STRBUF_DEFAULT_SIZE;

    s->buf = (char*)luaM_new_(L, size, strbuf_memcat(s));
    s->size = uint32_t(size);
    s->length = 0;

    luaYB_ensurenull(s);
}

void luaYB_free(lua_State* L, lua_YieldSafeStrBuf* s)
{
    if (s->buf)
    {
        luaM_free_(L, s->buf, s->size, strbuf_memcat(s));
        s->buf = nullptr;
    }
}

// Growth strategy for memory-constrained scripts (128KB per-script limit):
// Double while small, then snap to fixed increments.
// Sequence from 128: 128 → 256 → 512 → 1024 → 2048 → 4096 → 6144 → ...
static constexpr size_t STRBUF_GROWTH_LIMIT = 2048;

void luaYB_resize(lua_State* L, lua_YieldSafeStrBuf* s, size_t len)
{
    size_t reqsize = len + 1; // room for NULL terminator
    size_t newsize = s->size;

    // Small: double up to the growth limit
    while (newsize < reqsize && newsize < STRBUF_GROWTH_LIMIT)
        newsize *= 2;

    // Large: snap to the nearest increment above reqsize
    if (newsize < reqsize)
        newsize = ((reqsize + STRBUF_GROWTH_LIMIT - 1) / STRBUF_GROWTH_LIMIT) * STRBUF_GROWTH_LIMIT;

    s->buf = (char*)luaM_realloc_(L, s->buf, s->size, newsize, strbuf_memcat(s));
    s->size = uint32_t(newsize);
}

void luaYB_appendstr(lua_State* L, lua_YieldSafeStrBuf* s, const char* str)
{
    size_t len = strlen(str);
    luaYB_ensure(L, s, len);
    memcpy(s->buf + s->length, str, len);
    s->length += uint32_t(len);
}

// Copy buffer contents to an untracked C++ string, nil out the stack slot,
// then create the Lua string into the same slot. This ensures the strbuf
// and the resulting Lua string are never on the stack simultaneously,
// avoiding a spike in the apparent memory cost charged to the user thread.
void luaYB_tostring(lua_State* L, int idx, bool free_storage)
{
    // idx must be a real stack index; we compute a base-relative offset below
    LUAU_ASSERT(!lua_ispseudo(idx));
    auto* s = (lua_YieldSafeStrBuf*)lua_touserdata(L, idx);
    std::string storage(s->buf, s->length);

    if (free_storage)
        luaYB_free(L, s);

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
void luaYB_setup(lua_State* L)
{
    lua_setuserdatadtor(
        L,
        UTAG_STRBUF,
        [](lua_State* L, void* ud)
        {
            auto* b = (lua_YieldSafeStrBuf*)ud;
            luaYB_free(L, b);
        }
    );
}

// Pushes a new lua_YieldSafeStrBuf userdata onto the stack.
lua_YieldSafeStrBuf* luaYB_push(lua_State* L)
{
    lua_newuserdatatagged(L, sizeof(lua_YieldSafeStrBuf), UTAG_STRBUF);
    auto* b = (lua_YieldSafeStrBuf*)lua_touserdata(L, -1);
    memset(b, 0, sizeof(lua_YieldSafeStrBuf));
    luaYB_init(L, b, 0);
    return b;
}
