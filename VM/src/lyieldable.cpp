// ServerLua: Implementation for lyieldable.h
// Slot destructors are inline in the header (only use memcpy).
#include "lyieldable.h"

using Luau::SlotManager;
using Luau::PrimitiveSlot;

#include "lstate.h"
#include "lgc.h"
#include "ludata.h"
#include "llsl.h"
#include "lualib.h"

// Script error for a yield buffer the resume path can't trust
void SlotManager::corrupt() const
{
    luaL_error(L, "corrupt yield state");
}

// Script error for a chain whose regions don't fit the uint16 offsets
void SlotManager::tooLarge() const
{
    luaL_error(L, "yield state too large");
}

// Root constructor. On init, pushes nil at position 1.
// On resume, reads the yield state userdata at position 1.
SlotManager::SlotManager(lua_State* L, bool is_init, uint8_t abi_version)
    : L(L)
    // The stack top leads the root region
    , requiredSize(sizeof(uint16_t))
    , bufferStackOffset(L->base - L->stack)
    , initMode(is_init)
    , abiVersion(abi_version)
{
    if (is_init)
    {
        lua_pushnil(L);
        // Insert at position 1, shifting args right. Args are now at 2+.
        lua_insert(L, 1);
    }
    else
    {
        // Position 1 has the yield state written by the previous flushForYield().
        TValue* slot = L->stack + bufferStackOffset;
        if (!ttisuserdata(slot) || uvalue(slot)->tag != UTAG_YIELD_STATE)
            corrupt();
        Udata* u = uvalue(slot);
        bufferData = u->data;
        bufferSize = u->len;
        // Generally should not happen, since the only way it could happen is if
        // Ares' version check isn't doing its job, but let's be extra sure.
        if ((uint8_t)bufferData[0] > abi_version)
        {
            TString* name = clvalue(L->ci->func)->c.debugname;
            luaL_error(L, "yield state of %s was saved by a newer version", name ? getstr(name) : "?");
        }

        readStoredLength();

        if (storedLength < sizeof(uint16_t) + sizeof(uint8_t))
            corrupt();
        uint16_t top;
        memcpy(&top, bufferData + readBaseOffset, sizeof(uint16_t));
        // Drop whatever resume pushed above the frame
        if (top != 0)
        {
            if (top > lua_gettop(L))
                corrupt();
            lua_settop(L, top);
        }
    }

    callbacks = &L->global->cb;

    // Make sure we always have at least LUA_MINSTACK when we come back,
    // yield can shrink the stack down.
    int needed = LUA_MINSTACK - lua_gettop(L);
    if (needed > 0)
        luaL_checkstack(L, needed, "yieldable frame");
}

void SlotManager::flushForCall(int nargs, int nresults)
{
    LUAU_ASSERT(nresults >= 0);
    flushForYield(false);

    ptrdiff_t frame_size = (L->top - nargs - 1 + nresults) - (L->stack + bufferStackOffset);
    LUAU_ASSERT(frame_size >= 1 && frame_size < UINT16_MAX);
    uint16_t top = (uint16_t)frame_size;
    memcpy(bufferData + Luau::kYieldStateHeader, &top, sizeof(uint16_t));
}

void SlotManager::clearYielding()
{
    for (SlotManager* mgr = this; mgr; mgr = mgr->parent)
        mgr->yielding = false;
}

// Creates a yield state userdata at position 1 sized for the entire chain.
// Sets bufferData and yielding on all managers so slot destructors
// write on unwind.
void SlotManager::flushForYield(bool own_frame)
{
    size_t totalSize = baseOffset + requiredSize;

    uint16_t top = 0;
    if (own_frame)
    {
        // lua_yield moves L->base past the frame's values, lua_break leaves both alone
        StkId frame_top = L->status == LUA_YIELD ? L->base : L->top;
        ptrdiff_t frame_size = frame_top - (L->stack + bufferStackOffset);
        // lua_checkstack keeps C frames within LUAI_MAXCSTACK
        static_assert(LUAI_MAXCSTACK < UINT16_MAX, "C frame size must fit the u16 stack top");
        LUAU_ASSERT(frame_size >= 1 && frame_size < UINT16_MAX);
        top = (uint16_t)frame_size;
    }

    TValue* slot = L->stack + bufferStackOffset;
    Udata* u;

    if (ttisuserdata(slot) && uvalue(slot)->tag == UTAG_YIELD_STATE && uvalue(slot)->len >= (int)totalSize)
    {
        // Reuse userdata from previous yield
        u = uvalue(slot);
    }
    else
    {
        // First yield (slot is nil) or userdata too small — allocate.
        luaC_checkGC(L);
        luaC_threadbarrier(L);
        {
            // NB: We are very careful to allocate this under a non-user memcat so it
            // isn't "charged" against the user for memory purposes. Since these only
            // get allocated by the yielding mechanism, we would end up in a situation
            // where one could non-deterministically OoM due to a pre-emption yield
            // being injected at a particularly inopportune point.
            MemcatGuard guard(L, 0);
            u = luaU_newudata(L, totalSize, UTAG_YIELD_STATE);
        }
        // Recompute slot — GC or allocation may have reallocated the stack.
        slot = L->stack + bufferStackOffset;
        setuvalue(L, slot, u);
    }

    // Propagate to all managers in chain by walking up to root.
    // yielding is set here, after allocation, so that if luaU_newudata
    // throws OOM the flag remains false and slot destructors safely no-op
    // during stack unwinding.
    char* buf = u->data;
    memset(buf, 0, totalSize);
    buf[0] = (char)abiVersion;
    memcpy(buf + Luau::kYieldStateHeader, &top, sizeof(uint16_t));
    for (SlotManager* mgr = this; mgr; mgr = mgr->parent)
    {
        mgr->yielding = true;
        mgr->bufferData = buf;
    }
}
