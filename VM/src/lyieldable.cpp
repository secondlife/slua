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

// Root constructor. On init, pushes nil at position 1.
// On resume, reads the yield state userdata at position 1.
SlotManager::SlotManager(lua_State* L, bool is_init, uint8_t abi_version)
    : L(L)
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
    }

    callbacks = &L->global->cb;

    // Make sure we always have at least LUA_MINSTACK when we come back,
    // yield can shrink the stack down.
    int needed = LUA_MINSTACK - lua_gettop(L);
    if (needed > 0)
        lua_checkstack(L, needed);
}

// Creates a yield state userdata at position 1 sized for the entire chain.
// Sets bufferData and yielding on all managers so slot destructors
// write on unwind.
void SlotManager::flushForYield()
{
    size_t totalSize = baseOffset + requiredSize;

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
        u = luaU_newudata(L, totalSize, UTAG_YIELD_STATE);
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
    for (SlotManager* mgr = this; mgr; mgr = mgr->parent)
    {
        mgr->yielding = true;
        mgr->bufferData = buf;
    }
}
