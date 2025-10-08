// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
// This code is based on Lua 5.x implementation licensed under MIT License; see lua_LICENSE.txt for details
#include "lgc.h"

#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ludata.h"
#include "lbuffer.h"
#include "llsl.h"

#include <algorithm>
#include <string.h>

#include <unordered_set>
#include <queue>

// Helper functions to traverse child objects for reachable user alloc traversal
typedef struct ReachableContext
{
    std::queue<GCObject*> queue;
    std::unordered_set<GCObject*> visited;
} ReachableContext;

static void enqueueobj(ReachableContext* ctx, GCObject* obj)
{
    // Note that this is strictly for _user_ allocations that we haven't seen before!
    if (!obj)
        return;

    // We allow traversing threads even if they have a system memcat so long as their _active_
    // memcat is a user memcat.
    bool eligible_thread = (obj->gch.tt == LUA_TTHREAD && gco2th(obj)->activememcat >= 2);
    if ((eligible_thread || obj->gch.memcat >= 2) && ctx->visited.insert(obj).second)
    {
        ctx->queue.push(obj);
    }
}

static void traversetable(ReachableContext* ctx, LuaTable* h)
{
    // Traverse metatable
    if (h->metatable)
        enqueueobj(ctx, obj2gco(h->metatable));

    // Traverse array elements
    for (int i = 0; i < h->sizearray; ++i)
    {
        if (iscollectable(&h->array[i]))
            enqueueobj(ctx, gcvalue(&h->array[i]));
    }

    // Traverse hash node keys and values
    if (h->node != &luaH_dummynode)
    {
        int sizenode = 1 << h->lsizenode;
        for (int i = 0; i < sizenode; ++i)
        {
            const LuaNode& n = h->node[i];
            // We don't care if the key is still there
            // if the value is `nil`. It'll be collected eventually.
            if (!ttisnil(&n.val))
            {
                // Traverse key
                if (iscollectable(&n.key))
                    enqueueobj(ctx, gcvalue(&n.key));
                // Traverse value
                if (iscollectable(&n.val))
                    enqueueobj(ctx, gcvalue(&n.val));
            }
        }
    }
}

static void traverseclosure(ReachableContext* ctx, Closure* cl)
{
    // Traverse environment
    enqueueobj(ctx, obj2gco(cl->env));

    if (cl->isC)
    {
        // Traverse C closure upvalues
        for (int i = 0; i < cl->nupvalues; ++i)
        {
            if (iscollectable(&cl->c.upvals[i]))
                enqueueobj(ctx, gcvalue(&cl->c.upvals[i]));
        }
    }
    else
    {
        // Traverse proto
        enqueueobj(ctx, obj2gco(cl->l.p));

        // Traverse Lua closure upvalues
        for (int i = 0; i < cl->nupvalues; ++i)
        {
            if (iscollectable(&cl->l.uprefs[i]))
                enqueueobj(ctx, gcvalue(&cl->l.uprefs[i]));
        }
    }
}

static void traverseudata(ReachableContext* ctx, Udata* u)
{
    // Traverse metatable
    if (u->metatable)
        enqueueobj(ctx, obj2gco(u->metatable));

    // Traverse UUID string reference if applicable
    if (u->tag == UTAG_UUID)
        enqueueobj(ctx, obj2gco(((lua_LSLUUID*)&u->data)->str));
}

static void traversethread(ReachableContext* ctx, lua_State* th)
{
    // Traverse globals table
    enqueueobj(ctx, obj2gco(th->gt));

    // We don't traverse th->namecall, it's not a user alloc anyway.

    // Traverse stack elements
    for (StkId o = th->stack; o < th->top; ++o)
    {
        if (iscollectable(o))
            enqueueobj(ctx, gcvalue(o));
    }

    // Traverse open upvalues
    for (UpVal* uv = th->openupval; uv; uv = uv->u.open.threadnext)
        enqueueobj(ctx, obj2gco(uv));
}

static void traverseproto(ReachableContext* ctx, Proto* p)
{
    // Traverse source string
    if (p->source)
        enqueueobj(ctx, obj2gco(p->source));

    // Traverse debug name string
    if (p->debugname)
        enqueueobj(ctx, obj2gco(p->debugname));

    // Traverse constants
    for (int i = 0; i < p->sizek; ++i)
    {
        if (iscollectable(&p->k[i]))
            enqueueobj(ctx, gcvalue(&p->k[i]));
    }

    // Traverse upvalue names
    for (int i = 0; i < p->sizeupvalues; ++i)
    {
        if (p->upvalues[i])
            enqueueobj(ctx, obj2gco(p->upvalues[i]));
    }

    // Traverse sub-protos
    for (int i = 0; i < p->sizep; ++i)
    {
        if (p->p[i])
            enqueueobj(ctx, obj2gco(p->p[i]));
    }

    // Traverse local variable names
    for (int i = 0; i < p->sizelocvars; ++i)
    {
        if (p->locvars[i].varname)
            enqueueobj(ctx, obj2gco(p->locvars[i].varname));
    }
}

static void traverseupval(ReachableContext* ctx, UpVal* uv)
{
    // Traverse referenced value
    if (iscollectable(uv->v))
        enqueueobj(ctx, gcvalue(uv->v));
}

static void traverseobj(ReachableContext* ctx, GCObject* o)
{
    switch (o->gch.tt)
    {
    case LUA_TSTRING:
        // Strings have no children
        break;

    case LUA_TTABLE:
        traversetable(ctx, gco2h(o));
        break;

    case LUA_TFUNCTION:
        traverseclosure(ctx, gco2cl(o));
        break;

    case LUA_TUSERDATA:
        traverseudata(ctx, gco2u(o));
        break;

    case LUA_TTHREAD:
        traversethread(ctx, gco2th(o));
        break;

    case LUA_TBUFFER:
        // Buffers have no children
        break;

    case LUA_TPROTO:
        // Generally this should never happen since protos will have a memcat of `0`, but just in case.
        traverseproto(ctx, gco2p(o));
        break;

    case LUA_TUPVAL:
        traverseupval(ctx, gco2uv(o));
        break;

    default:
        LUAU_ASSERT(!"Unknown object type in traverseobj");
    }
}

static size_t calctruegcosize(GCObject *obj)
{
    switch (obj->gch.tt)
    {
    case LUA_TSTRING:
        return sizestring(gco2ts(obj)->len);
    case LUA_TTABLE:
    {
        LuaTable* h = gco2h(obj);
        return sizeof(LuaTable) + (h->node == &luaH_dummynode ? 0 : sizenode(h) * sizeof(LuaNode)) + h->sizearray * sizeof(TValue);
    }
    case LUA_TFUNCTION:
    {
        Closure* cl = gco2cl(obj);
        return cl->isC ? sizeCclosure(cl->nupvalues) : sizeLclosure(cl->nupvalues);
    }
    case LUA_TUSERDATA:
        return sizeudata(gco2u(obj)->len);
    case LUA_TTHREAD:
    {
        lua_State* th = gco2th(obj);
        return sizeof(lua_State) + sizeof(TValue) * th->stacksize + sizeof(CallInfo) * th->size_ci;
    }
    case LUA_TBUFFER:
        return sizebuffer(gco2buf(obj)->len);
    case LUA_TPROTO:
    {
        Proto* p = gco2p(obj);
        return sizeof(Proto) +
               (sizeof(Instruction) * p->sizecode) +
               (sizeof(Proto*) * p->sizep) +
               (sizeof(TValue) * p->sizek) +
               (sizeof(uint8_t) * p->sizelineinfo) +
               (sizeof(LocVar) * p->sizelocvars) +
               (sizeof(TString*) * p->sizeupvalues);
    }
    case LUA_TUPVAL:
        return sizeof(UpVal);
    default:
        LUAU_ASSERT(!"Unknown object type");
        return 0;
    }
}

#define CHECK_GCO_SIZE(logical_size, real_size) \
    static_assert((logical_size) <= (real_size), "" # real_size " logical size is sensible")

// For cases where we want a "logical" size rather than a "true" size.
// In a lot of ways, the logical size of allocations leaks into the API contract in SL.
// This isn't ideal, since the sizes of structs and pointers can vary based on padding,
// the platform, and the bitness of the platform.
static size_t calcgcosize(GCObject *obj)
{
    // These are either arbitrary or based on 32-bit x86 sizes.
    constexpr size_t BASE_STRING_COST = 16;
    constexpr size_t BASE_BUFFER_COST = 12;
    constexpr size_t BASE_THREAD_COST = 76;
    constexpr size_t BASE_TABLE_COST = 32;
    constexpr size_t BASE_CLOSURE_COST = 20;
    constexpr size_t TVALUE_COST = 16;
    constexpr size_t CALLINFO_COST = 24;
    constexpr size_t LUANODE_COST = TVALUE_COST * 2;
    constexpr size_t POINTER_COST = 4;
    constexpr size_t UPVAL_COST = 24;

    // Make sure that these values are sensible. They should not be _more_ than the
    // actual size of these structs on i686.
    CHECK_GCO_SIZE(UPVAL_COST, sizeof(UpVal));
    CHECK_GCO_SIZE(LUANODE_COST, sizeof(LuaNode));
    CHECK_GCO_SIZE(CALLINFO_COST, sizeof(CallInfo));
    CHECK_GCO_SIZE(BASE_CLOSURE_COST, sizeCclosure(0));
    CHECK_GCO_SIZE(BASE_CLOSURE_COST, sizeLclosure(0));
    CHECK_GCO_SIZE(BASE_TABLE_COST, sizeof(LuaTable));
    CHECK_GCO_SIZE(TVALUE_COST, sizeof(TValue));
    CHECK_GCO_SIZE(BASE_THREAD_COST, sizeof(lua_State));
    CHECK_GCO_SIZE(BASE_BUFFER_COST, sizeof(Buffer));
    CHECK_GCO_SIZE(BASE_STRING_COST, sizeof(TString));

    switch (obj->gch.tt)
    {
    case LUA_TSTRING:
        return BASE_STRING_COST + gco2ts(obj)->len;
    case LUA_TTABLE:
    {
        LuaTable* h = gco2h(obj);
        return BASE_TABLE_COST +
               (h->node == &luaH_dummynode ? 0 : sizenode(h) * LUANODE_COST) +
               (h->sizearray * TVALUE_COST);
    }
    case LUA_TFUNCTION:
    {
        Closure* cl = gco2cl(obj);
        return BASE_CLOSURE_COST + (cl->nupvalues * TVALUE_COST);
    }
    case LUA_TUSERDATA:
        // TODO: specific sizes for each kind of userdata!
        return sizeudata(gco2u(obj)->len);
    case LUA_TTHREAD:
    {
        lua_State* th = gco2th(obj);
        return BASE_THREAD_COST + (TVALUE_COST * th->stacksize) + (CALLINFO_COST * th->size_ci);
    }
    case LUA_TBUFFER:
        return BASE_BUFFER_COST + std::max(gco2buf(obj)->len, 8u);
    case LUA_TPROTO:
    {
        // TODO: These don't show up in practice, but we should probably have something better.
        Proto* p = gco2p(obj);
        return sizeof(Proto) +
               (sizeof(Instruction) * p->sizecode) +
               (POINTER_COST * p->sizep) +
               (TVALUE_COST * p->sizek) +
               (sizeof(uint8_t) * p->sizelineinfo) +
               (sizeof(LocVar) * p->sizelocvars) +
               (POINTER_COST * p->sizeupvalues);
    }
    case LUA_TUPVAL:
        return UPVAL_COST;
    default:
        LUAU_ASSERT(!"Unknown object type");
        return 0;
    }
}

void luaC_enumreachableuserallocs(
    lua_State* L,
    void* context,
    void (*node)(void* context, GCObject* ptr, uint8_t tt, uint8_t memcat, size_t size)
)
{
    ReachableContext ctx;

    ctx.queue.push(obj2gco(L));
    ctx.visited.insert(obj2gco(L));

    while (!ctx.queue.empty())
    {
        GCObject* current = ctx.queue.front();
        ctx.queue.pop();

        // Call the user-provided traversal callback
        if (current->gch.memcat >= 2)
            node(context, current, current->gch.tt, current->gch.memcat, calcgcosize(current));

        // Take any new references the current node has and add them to the queue
        // Even if we don't want to include their size in the calculation, we may still want
        // to traverse them.
        traverseobj(&ctx, current);
    }
}
