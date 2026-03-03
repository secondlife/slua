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

// Root constructor. On init, pushes nil at position 1.
// On resume, reads the serialized opaque userdata at position 1.
SlotManager::SlotManager(lua_State* L, bool is_init)
    : L(L)
    , bufferStackOffset(L->base - L->stack)
    , initMode(is_init)
{
    if (is_init)
    {
        lua_pushnil(L);
        // Insert at position 1, shifting args right. Args are now at 2+.
        lua_insert(L, 1);
    }
    else
    {
        // Position 1 has the opaque userdata written by the previous flushForYield().
        TValue* slot = L->stack + bufferStackOffset;
        if (!ttisuserdata(slot) || uvalue(slot)->tag != UTAG_OPAQUE_BUFFER)
            luaL_error(L, "corrupt yield state");
        Udata* u = uvalue(slot);
        bufferData = u->data;
        bufferSize = u->len;
        if (bufferData[0] != 0)
            luaL_error(L, "unsupported yield buffer version");
    }

    callbacks = &L->global->cb;

    // Make sure we always have at least LUA_MINSTACK when we come back,
    // yield can shrink the stack down.
    int needed = LUA_MINSTACK - lua_gettop(L);
    if (needed > 0)
        lua_checkstack(L, needed);
}

// Creates an opaque userdata at position 1 sized for the entire chain.
// Sets bufferData and yielding on all managers so slot destructors
// write on unwind.
void SlotManager::flushForYield()
{
    size_t totalSize = baseOffset + requiredSize;

    TValue* slot = L->stack + bufferStackOffset;
    Udata* u;

    if (ttisuserdata(slot) && uvalue(slot)->tag == UTAG_OPAQUE_BUFFER && uvalue(slot)->len >= (int)totalSize)
    {
        // Reuse userdata from previous yield
        u = uvalue(slot);
    }
    else
    {
        // First yield (slot is nil) or userdata too small — allocate.
        luaC_checkGC(L);
        luaC_threadbarrier(L);
        u = luaU_newudata(L, totalSize, UTAG_OPAQUE_BUFFER);
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
    for (SlotManager* mgr = this; mgr; mgr = mgr->parent)
    {
        mgr->yielding = true;
        mgr->bufferData = buf;
    }
}
