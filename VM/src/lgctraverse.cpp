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
#include "lstrbuf.h"

#include <algorithm>
#include <string.h>

#include <unordered_set>
#include <vector>
#include <utility>

LUAU_FASTFLAGVARIABLE(SLuaEagerWeakClear)

static constexpr uint8_t kWeakKey = 1 << 0;
static constexpr uint8_t kWeakValue = 1 << 1;

// Starting capacity for the walk's scratch vectors. Reserving up front keeps a
// walk to roughly one allocation per vector, where allocations were previously
// a big chunk of runtime for large graphs.
static constexpr size_t kWalkReserve = 1024;

// Helper functions to traverse child objects for reachable user alloc traversal
typedef struct ReachableContext
{
    // DFS stack of objects still to traverse.
    std::vector<GCObject*> worklist;
    // Every object we set WALKBIT on this walk, so it can be cleared before we
    // return.
    std::vector<GCObject*> marked;
    global_State* global = nullptr;
    // Only the size/clear walk defers weak sides; the baseline free-object
    // snapshot keeps fully-strong traversal so the exemption set is maximal.
    bool defer_weak = false;
    // Weak tables seen during the walk, with their weakness bits, for the
    // post-walk clear pass.
    std::vector<std::pair<LuaTable*, uint8_t>> weak_tables;

    // Exception-safe way to ensure WALKBIT never persists beyond the scope of
    // our traversal.
    ~ReachableContext()
    {
        for (GCObject* obj : marked)
            resetbit(obj->gch.marked, WALKBIT);
    }
} ReachableContext;

static inline bool mark_seen(ReachableContext* ctx, GCObject* obj)
{
    if (testbit(obj->gch.marked, WALKBIT))
        return false;
    // Record before marking so a throwing push_back never strands a set bit.
    ctx->marked.push_back(obj);
    l_setbit(obj->gch.marked, WALKBIT);
    return true;
}

static void enqueueobj(ReachableContext* ctx, GCObject* obj)
{
    // Note that this is strictly for _user_ allocations that we haven't seen before!
    if (!obj)
        return;

    // We allow traversing threads even if they have a system memcat so long as their _active_
    // memcat is a user memcat.
    bool eligible_thread = (obj->gch.tt == LUA_TTHREAD && gco2th(obj)->activememcat >= LUA_FIRST_USER_MEMCAT);
    if ((eligible_thread || obj->gch.memcat >= LUA_FIRST_USER_MEMCAT) && mark_seen(ctx, obj))
    {
        ctx->worklist.push_back(obj);
    }
}

// Strings and UUIDs act as values in user weak tables (see isobjcleared in
// lgc.cpp): they keep their entries alive and stay charged.
static bool hasweakvaluesemantics(GCObject* o)
{
    return o->gch.tt == LUA_TSTRING || (o->gch.tt == LUA_TUSERDATA && gco2u(o)->tag == UTAG_UUID);
}

static void traversetable(ReachableContext* ctx, LuaTable* h)
{
    // Traverse metatable
    if (h->metatable)
        enqueueobj(ctx, obj2gco(h->metatable));

    // Weak sides of weak tables are deferred to the post-walk clear pass, so
    // objects reachable only through them are neither charged nor kept.
    // Fixed tables may be shared across forked instances and must never be
    // mutated, so they keep fully-strong traversal.
    uint8_t weakbits = 0;
    if (ctx->defer_weak && !isfixed(obj2gco(h)))
    {
        if (const char* modev = luaC_gettablemode(ctx->global, h))
        {
            if (strchr(modev, 'k'))
                weakbits |= kWeakKey;
            if (strchr(modev, 'v'))
                weakbits |= kWeakValue;
            if (weakbits)
                ctx->weak_tables.push_back({h, weakbits});
        }
    }

    // Traverse array elements
    for (int i = 0; i < h->sizearray; ++i)
    {
        if (iscollectable(&h->array[i]) && (!(weakbits & kWeakValue) || hasweakvaluesemantics(gcvalue(&h->array[i]))))
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
                if (iscollectable(&n.key) && (!(weakbits & kWeakKey) || hasweakvaluesemantics(gcvalue(&n.key))))
                    enqueueobj(ctx, gcvalue(&n.key));
                // Traverse value
                if (iscollectable(&n.val) && (!(weakbits & kWeakValue) || hasweakvaluesemantics(gcvalue(&n.val))))
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

    // Traverse any internal references udatas might have, if applicable.
    switch(u->tag)
    {
        case UTAG_LLEVENTS:
            enqueueobj(ctx, obj2gco(((lua_LLEvents*)&u->data)->handlers_tab));
            break;
        case UTAG_LLTIMERS:
            enqueueobj(ctx, obj2gco(((lua_LLTimers*)&u->data)->timers_tab));
            break;
        case UTAG_UUID:
            enqueueobj(ctx, obj2gco(((lua_LSLUUID*)&u->data)->str));
            break;
        default:
            break;
    }
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

static void traverseclass(ReachableContext* ctx, LuauClass* lco)
{
    // Traverse class name
    enqueueobj(ctx, obj2gco(lco->name));

    // Traverse the name -> offset map
    enqueueobj(ctx, obj2gco(lco->memberstooffset));

    // Traverse member names
    for (uint32_t i = 0; i < lco->numberofallmembers; ++i)
        enqueueobj(ctx, obj2gco(lco->offsettomember[i]));

    // Traverse static members. Instance member offsets come first, so the
    // static members are the tail of the offset space.
    for (uint32_t i = 0; i < lco->numberofallmembers - lco->numberofinstancemembers; ++i)
    {
        if (iscollectable(&lco->staticmembers[i]))
            enqueueobj(ctx, gcvalue(&lco->staticmembers[i]));
    }

    // Traverse the class object's own metatable, then the one handed to its instances
    enqueueobj(ctx, obj2gco(lco->metatable));

    if (lco->instancemetatable)
        enqueueobj(ctx, obj2gco(lco->instancemetatable));
}

static void traverseobject(ReachableContext* ctx, LuauObject* inst)
{
    // Traverse the class this is an instance of
    enqueueobj(ctx, obj2gco(inst->lclass));

    // Traverse instance members. The instance owns this array and its count,
    // so use those rather than reaching through the class - same as
    // `traverseobject` in lgc.cpp and `luaR_freeobject`.
    for (uint32_t i = 0; i < inst->numberofmembers; ++i)
    {
        if (iscollectable(&inst->members[i]))
            enqueueobj(ctx, gcvalue(&inst->members[i]));
    }
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

    case LUA_TCLASS:
        traverseclass(ctx, gco2class(o));
        break;

    case LUA_TOBJECT:
        traverseobject(ctx, gco2object(o));
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
    case LUA_TCLASS:
    {
        LuauClass* lco = gco2class(obj);
        return sizeof(LuauClass) +
               ((lco->numberofallmembers - lco->numberofinstancemembers) * sizeof(TValue)) +
               (lco->numberofallmembers * sizeof(TString*));
    }
    case LUA_TOBJECT:
        return sizeof(LuauObject) + (gco2object(obj)->numberofmembers * sizeof(TValue));
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
size_t luaC_calclogicalgcosize(GCObject *obj)
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
    constexpr size_t BASE_CLASS_COST = 40;
    constexpr size_t BASE_OBJECT_COST = 20;

    // Make sure that these values are sensible. They should not be _more_ than the
    // actual size of these structs on i686.
    CHECK_GCO_SIZE(BASE_OBJECT_COST, sizeof(LuauObject));
    CHECK_GCO_SIZE(BASE_CLASS_COST, sizeof(LuauClass));
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
    {
        const Udata *udata = gco2u(obj);
        switch (udata->tag)
        {
        case UTAG_UUID:
            return 4;
        case UTAG_QUATERNION:
            return 4 * 4;
        case UTAG_DETECTED_EVENT:
            return 6;
        case UTAG_LLEVENTS:
            return 8;
        case UTAG_LLTIMERS:
            return 8;
        case UTAG_STRBUF:
        {
            const lua_YieldSafeStrBuf* buf = (const lua_YieldSafeStrBuf*)&udata->data;
            return sizeof(lua_YieldSafeStrBuf) + buf->size;
        }
        default:
            return sizeudata(udata->len);
        }
    }
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
    case LUA_TCLASS:
    {
        LuauClass* lco = gco2class(obj);
        return BASE_CLASS_COST +
               ((lco->numberofallmembers - lco->numberofinstancemembers) * TVALUE_COST) +
               (lco->numberofallmembers * POINTER_COST);
    }
    case LUA_TOBJECT:
        return BASE_OBJECT_COST + (gco2object(obj)->numberofmembers * TVALUE_COST);
    default:
        LUAU_ASSERT(!"Unknown object type");
        return 0;
    }
}

// Mirrors isobjcleared in lgc.cpp, with WALKBIT ("reached during the walk")
// standing in for mark color. Objects the walker never traverses (system
// memcats, fixed baseline objects) are conservatively live.
static bool isweakobjdead(GCObject* o)
{
    if (hasweakvaluesemantics(o))
        return false;
    if (isfixed(o) || o->gch.memcat < LUA_FIRST_USER_MEMCAT)
        return false;
    return !testbit(o->gch.marked, WALKBIT);
}

#define isweakrefdead(o) (iscollectable(o) && isweakobjdead(gcvalue(o)))

// Clears weak table entries whose referents the walk could not reach, the way
// cleartable does at the real GC's atomic phase, minus shrinking. Writing
// nil/DEADKEY never allocates or rehashes and only removes references, so
// this is safe in any GC phase and never invalidates iterators.
static void cleardeadweakrefs(ReachableContext* ctx)
{
    for (const auto& weak_entry : ctx->weak_tables)
    {
        LuaTable* h = weak_entry.first;
        uint8_t weakbits = weak_entry.second;

        if (weakbits & kWeakValue)
        {
            for (int i = 0; i < h->sizearray; ++i)
            {
                if (isweakrefdead(&h->array[i]))
                    setnilvalue(&h->array[i]);
            }
        }

        if (h->node == &luaH_dummynode)
            continue;

        int sizenode = 1 << h->lsizenode;
        for (int i = 0; i < sizenode; ++i)
        {
            LuaNode* n = &h->node[i];
            if (ttisnil(&n->val))
                continue;
            if (((weakbits & kWeakKey) && isweakrefdead(&n->key)) ||
                ((weakbits & kWeakValue) && isweakrefdead(&n->val)))
            {
                setnilvalue(&n->val);
                if (iscollectable(&n->key))
                    setttype(&n->key, LUA_TDEADKEY);
            }
        }
    }
}

void luaC_enumreachableuserallocs(
    lua_State* L,
    void* context,
    void (*node)(void* context, GCObject* ptr, uint8_t tt, uint8_t memcat, size_t size),
    const lua_OpaqueGCObjectSet* free_objects
)
{
    ReachableContext ctx;
    ctx.global = L->global;
    ctx.defer_weak = FFlag::SLuaEagerWeakClear;

    // Make sure we can hold the free objects, as well as the best-guesss max number
    // of GCObjects we're likely to see during our walk.
    ctx.marked.reserve((free_objects ? free_objects->size() : 0) + kWalkReserve);
    ctx.worklist.reserve(kWalkReserve);

    // Seed the baseline as already-seen so the walk prunes at it
    if (free_objects)
        for (const void* p : *free_objects)
            mark_seen(&ctx, (GCObject*)p);

    // We start walking from the thread root
    if (mark_seen(&ctx, obj2gco(L)))
        ctx.worklist.push_back(obj2gco(L));

    while (!ctx.worklist.empty())
    {
        GCObject* current = ctx.worklist.back();
        ctx.worklist.pop_back();

        // Call the user-provided traversal callback
        if (current->gch.memcat >= LUA_FIRST_USER_MEMCAT)
            node(context, current, current->gch.tt, current->gch.memcat, luaC_calclogicalgcosize(current));

        // Take any new references the current node has and add them to the worklist
        traverseobj(&ctx, current);
    }

    if (FFlag::SLuaEagerWeakClear)
        cleardeadweakrefs(&ctx);
}

lua_OpaqueGCObjectSet luaC_collectfreeobjects(lua_State* L)
{
    lua_OpaqueGCObjectSet free_objects;
    ReachableContext ctx;
    ctx.global = L->global;
    ctx.marked.reserve(kWalkReserve);
    ctx.worklist.reserve(kWalkReserve);

    const auto *gt_gco = obj2gco(L->gt);

    mark_seen(&ctx, obj2gco(L));

    traverseobj(&ctx, obj2gco(L));

    while (!ctx.worklist.empty())
    {
        GCObject* current = ctx.worklist.back();
        ctx.worklist.pop_back();

        // Don't try to mark the globals table for the user's root thread as free,
        // and don't even bother traversing it
        if (current == gt_gco)
            continue;

        // Collect user memcat objects into the set
        if (current->gch.memcat >= LUA_FIRST_USER_MEMCAT)
            free_objects.insert(current);

        // Traverse child references
        traverseobj(&ctx, current);
    }

    return free_objects;
}
