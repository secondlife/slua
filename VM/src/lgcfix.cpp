// ServerLua: GC-fixing logic. Implements luaC_fixall - a depth-first
// traversal from explicit roots that decides per-object fixability
// in post-order. Called once at VM setup, after stdlib + module setup
// completes and before any user script runs.
//
// Basically, lets the GC know that things that are _always_ going to be
// there and will _never_ be changed aren't worth touching during marking.
//
// Uses a single DFS from explicit roots (globals, per-type
// metatables, per-tag userdata metatables, registry)
// decides per-object fixability in post-order. A small fixing pass
// resolves cycles after DFS finishes, then a final commit pass calls
// luaC_fix on every object that came out Fixable.
#include "lgc.h"

#include "lapi.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "llsl.h"

#include <string.h>
#include <unordered_map>
#include <vector>

enum class FixState : uint8_t
{
    InProgress,
    Pending,
    Fixable,
    Unfixable,
};

class FixingPass
{
public:
    explicit FixingPass(lua_State *L) : L(L) {}

    void run();

private:
    FixState classify(GCObject *obj);
    void track_dep(GCObject *child, bool *has_unfixable, std::vector<GCObject*> *deps);
    void track_dep(const TValue *tv, bool *has_unfixable, std::vector<GCObject*> *deps);
    void resolve_pending();
    void commit();

    lua_State *L;
    std::unordered_map<GCObject*, FixState> states {};
    std::unordered_map<GCObject*, std::vector<GCObject*>> pending_deps {};
};

// Every string is fixable, regardless of reachability from the root, so do them all in one heap walk.
static bool gcfixstringsvisitor([[maybe_unused]] void *context, [[maybe_unused]] lua_Page *page, GCObject *obj)
{
    if (obj->gch.tt == LUA_TSTRING)
        luaC_fix(obj);
    return false;
}

void FixingPass::track_dep(GCObject *child, bool *has_unfixable, std::vector<GCObject*> *deps)
{
    if (!child)
        return;
    FixState s = classify(child);
    if (s == FixState::Unfixable)
        *has_unfixable = true;
    else if (s == FixState::InProgress || s == FixState::Pending)
        deps->push_back(child);
}

void FixingPass::track_dep(const TValue *tv, bool *has_unfixable, std::vector<GCObject*> *deps)
{
    if (iscollectable(tv))
        track_dep(gcvalue(tv), has_unfixable, deps);
}

// You may notice that this is quite similar to the BFS traversal in `lgctraverse.cpp`.
// Well, we need depth-first here so we don't have to do a bajillion passes. Such is life.
FixState FixingPass::classify(GCObject *obj)
{
    // Non-obvious, but even though `null` itself isn't necessarily fixable through luaC_fix,
    // it's fixed by nature because it's... nothing. It shouldn't block fixability of referrers.
    if (!obj)
        return FixState::Fixable;

    // If it's already fixed then it's certainly fixable.
    if (isfixed(obj))
        return FixState::Fixable;

    auto it = states.find(obj);
    if (it != states.end())
    {
        // We already visited this, just return its state
        return it->second;
    }

    // Haven't visited this before, mark it in-progress
    states[obj] = FixState::InProgress;

    bool has_unfixable = false;
    bool self_unfixable = false;
    std::vector<GCObject*> deps;

    switch (obj->gch.tt)
    {
    case LUA_TSTRING:
    {
        // Leaf prepass should already have fixed this, explode if it didn't
        LUAU_ASSERT(!"Found unfixed string after traversal");
        // Just mark it fixable anyway in NDEBUG mode...
        states[obj] = FixState::Fixable;
        return FixState::Fixable;
    }
    case LUA_TPROTO:
    {
        Proto *p = gco2p(obj);
        if (p->source)
            track_dep(obj2gco(p->source), &has_unfixable, &deps);
        if (p->debugname)
            track_dep(obj2gco(p->debugname), &has_unfixable, &deps);
        for (int i = 0; i < p->sizek; ++i)
            track_dep(&p->k[i], &has_unfixable, &deps);
        for (int i = 0; i < p->sizeupvalues; ++i)
        {
            if (p->upvalues[i])
                track_dep(obj2gco(p->upvalues[i]), &has_unfixable, &deps);
        }
        for (int i = 0; i < p->sizep; ++i)
        {
            if (p->p[i])
                track_dep(obj2gco(p->p[i]), &has_unfixable, &deps);
        }
        for (int i = 0; i < p->sizelocvars; ++i)
        {
            if (p->locvars[i].varname)
                track_dep(obj2gco(p->locvars[i].varname), &has_unfixable, &deps);
        }
        break;
    }

    case LUA_TFUNCTION:
    {
        Closure *cl = gco2cl(obj);
        // Self-fixability gates. Even when the closure itself is
        // disqualified, recurse into upvalues/proto so they have their
        // own chance to be fixed independently.
        if (!cl->env || !cl->env->safeenv)
            self_unfixable = true;

        if (cl->isC)
        {
            // We make the assumption (and hope it doesn't bite us) that
            // upvalues on C closures at the time of `fixall()` will not be
            // swapped out. If this is not the case, you've probably done something
            // very, very bad.
            for (int i = 0; i < cl->nupvalues; ++i)
                track_dep(&cl->c.upvals[i], &has_unfixable, &deps);
        }
        else
        {
            // Lua closures with upvalues are excluded, upvalues may be replaced later.
            if (cl->nupvalues != 0 || !cl->l.p)
                self_unfixable = true;
            if (cl->l.p)
                track_dep(obj2gco(cl->l.p), &has_unfixable, &deps);
        }
        break;
    }

    case LUA_TTABLE:
    {
        LuaTable *h = gco2h(obj);
        if (!h->readonly)
        {
            // A mutable table cannot be fixed itself, but it's worth scanning children.
            self_unfixable = true;
        }
        if (h->metatable)
            track_dep(obj2gco(h->metatable), &has_unfixable, &deps);
        for (int i = 0; i < h->sizearray; ++i)
            track_dep(&h->array[i], &has_unfixable, &deps);
        if (h->node != &luaH_dummynode)
        {
            int n = sizenode(h);
            for (int i = 0; i < n; ++i)
            {
                LuaNode *node = &h->node[i];
                if (ttisnil(gval(node)))
                    continue;

                const TKey *k = gkey(node);
                if (iscollectable(k))
                    track_dep(gcvalue(k), &has_unfixable, &deps);
                track_dep(gval(node), &has_unfixable, &deps);
            }
        }
        break;
    }

    case LUA_TUSERDATA:
    {
        Udata *u = gco2u(obj);
        if (u->tag >= LUA_UTAG_LIMIT)
        {
            self_unfixable = true;
            break;
        }
        LuaTable *udmt = L->global->udatamt[u->tag];
        if (!udmt)
        {
            // Should never happen, but who knows :shrug:
            self_unfixable = true;
            break;
        }
        // TODO: Hmmm, we should probably restrict this to specific
        //  userdata tags like quats and UUIDs...
        track_dep(obj2gco(udmt), &has_unfixable, &deps);
        break;
    }

    default:
        // Anything else we treat as unfixable.
        self_unfixable = true;
        break;
    }

    // Now that we've reasoned about the properties of this object,
    // what should we do with it?
    FixState result;
    if (self_unfixable || has_unfixable)
    {
        result = FixState::Unfixable;
    }
    else if (!deps.empty())
    {
        // Still not sure what to do with this since we have some
        // pending dependencies. Nothing's obviously blocking yet,
        // so defer until the end.
        pending_deps[obj] = std::move(deps);
        result = FixState::Pending;
    }
    else
    {
        result = FixState::Fixable;
    }
    states[obj] = result;
    return result;
}

// At the end of the DFS we'll be left with some pending, potentially fixable
// nodes which couldn't be handled before due to reference cycles. Resolve them now.
void FixingPass::resolve_pending()
{
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (auto &kv : states)
        {
            if (kv.second != FixState::Pending)
                continue;
            auto deps_it = pending_deps.find(kv.first);
            if (deps_it == pending_deps.end())
                continue;
            bool any_unfixable = false;
            bool all_fixable = true;
            for (GCObject *dep : deps_it->second)
            {
                auto dep_it = states.find(dep);
                LUAU_ASSERT(dep_it != states.end());
                FixState s = dep_it->second;
                if (s == FixState::Unfixable)
                {
                    any_unfixable = true;
                    break;
                }
                if (s != FixState::Fixable)
                    all_fixable = false;
            }

            // Now is the time to make a decision as to whether this can be fixed
            if (any_unfixable)
            {
                kv.second = FixState::Unfixable;
                changed = true;
            }
            else if (all_fixable)
            {
                kv.second = FixState::Fixable;
                changed = true;
            }
        }
    }
    // Any remaining Pending are pure cycles whose only blockers are each other
    for (auto &kv : states)
    {
        if (kv.second == FixState::Pending)
            kv.second = FixState::Fixable;
    }
}

void FixingPass::commit()
{
    for (auto &kv : states)
    {
        if (kv.second == FixState::Fixable && !isfixed(kv.first))
            luaC_fix(kv.first);
    }
}

CLANG_NOOPT void GCC_NOOPT FixingPass::run()
{
    LuaTable *base_globals = hvalue(luaA_toobject(L, LUA_BASEGLOBALSINDEX));

    // Visit all potential roots (base globals, udata mts, registry, etc)
    classify(obj2gco(base_globals));

    for (auto tab : L->global->mt)
    {
        if (tab)
            classify(obj2gco(tab));
    }

    for (auto tab : L->global->udatamt)
    {
        if (tab)
            classify(obj2gco(tab));
    }

    classify(obj2gco(hvalue(luaA_toobject(L, LUA_REGISTRYINDEX))));

    // Resolve cycles, then commit
    resolve_pending();
    commit();
}

void luaC_fixall(lua_State *L)
{
    L = lua_mainthread(L);
    int stack_top = lua_gettop(L);
    // Need to do a full GC first so all surviving objects are white.
    luaC_fullgc(L);
    // Freezing only makes sense if the global env can't be swapped out.
    LUAU_ASSERT(L->gt->safeenv);

    // Strings are unconditionally fixable, do them first.
    luaM_visitgco(L, nullptr, gcfixstringsvisitor);

    FixingPass(L).run();

    LUAU_ASSERT(lua_gettop(L) == stack_top);
    luaC_validate(L);
}
