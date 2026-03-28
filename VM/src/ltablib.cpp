// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
// This code is based on Lua 5.x implementation licensed under MIT License; see lua_LICENSE.txt for details
#define ltablib_c

#include "lualib.h"

#include "lapi.h"
#include "lnumutils.h"
#include "lstate.h"
#include "ltable.h"
#include "lstring.h"
#include "lgc.h"
#include "ldebug.h"
#include "lvm.h"

// ServerLua: yieldable table.sort
#include "lyieldablemacros.h"

static int foreachi(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    int i;
    int n = lua_objlen(L, 1);
    for (i = 1; i <= n; i++)
    {
        lua_pushvalue(L, 2);   // function
        lua_pushinteger(L, i); // 1st argument
        lua_rawgeti(L, 1, i);  // 2nd argument
        // ServerLua: Check for interrupt to allow pre-emptive abort before calling user iterator function
        luau_callinterrupthandler(L, LUA_INTERRUPT_STDLIB);
        lua_call(L, 2, 1);
        if (!lua_isnil(L, -1))
            return 1;
        lua_pop(L, 1); // remove nil result
    }
    return 0;
}

static int foreach (lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushnil(L); // first key
    while (lua_next(L, 1))
    {
        lua_pushvalue(L, 2);  // function
        lua_pushvalue(L, -3); // key
        lua_pushvalue(L, -3); // value
        // ServerLua: Check for interrupt to allow pre-emptive abort before calling user iterator function
        luau_callinterrupthandler(L, LUA_INTERRUPT_STDLIB);
        lua_call(L, 2, 1);
        if (!lua_isnil(L, -1))
            return 1;
        lua_pop(L, 2); // remove value and result
    }
    return 0;
}

static int maxn(lua_State* L)
{
    double max = 0;
    luaL_checktype(L, 1, LUA_TTABLE);

    LuaTable* t = hvalue(L->base);

    for (int i = 0; i < t->sizearray; i++)
    {
        if (!ttisnil(&t->array[i]))
            max = i + 1;
    }

    for (int i = 0; i < sizenode(t); i++)
    {
        LuaNode* n = gnode(t, i);

        if (!ttisnil(gval(n)) && ttisnumber(gkey(n)))
        {
            double v = nvalue(gkey(n));

            if (v > max)
                max = v;
        }
    }

    lua_pushnumber(L, max);
    return 1;
}

static int getn(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushinteger(L, lua_objlen(L, 1));
    return 1;
}

static void moveelements(lua_State* L, int srct, int dstt, int f, int e, int t)
{
    LuaTable* src = hvalue(L->base + (srct - 1));
    LuaTable* dst = hvalue(L->base + (dstt - 1));

    if (dst->readonly)
        luaG_readonlyerror(L);

    int n = e - f + 1; // number of elements to move

    if (unsigned(f) - 1 < unsigned(src->sizearray) && unsigned(t) - 1 < unsigned(dst->sizearray) &&
        unsigned(f) - 1 + unsigned(n) <= unsigned(src->sizearray) && unsigned(t) - 1 + unsigned(n) <= unsigned(dst->sizearray))
    {
        TValue* srcarray = src->array;
        TValue* dstarray = dst->array;

        if (t > e || t <= f || (dstt != srct && dst != src))
        {
            for (int i = 0; i < n; ++i)
            {
                TValue* s = &srcarray[f + i - 1];
                TValue* d = &dstarray[t + i - 1];
                setobj2t(L, dst, d, s);
            }
        }
        else
        {
            for (int i = n - 1; i >= 0; i--)
            {
                TValue* s = &srcarray[(f + i) - 1];
                TValue* d = &dstarray[(t + i) - 1];
                setobj2t(L, dst, d, s);
            }
        }

        luaC_barrierfast(L, dst);
    }
    else
    {
        if (t > e || t <= f || dst != src)
        {
            for (int i = 0; i < n; ++i)
            {
                lua_rawgeti(L, srct, f + i);
                lua_rawseti(L, dstt, t + i);
            }
        }
        else
        {
            for (int i = n - 1; i >= 0; i--)
            {
                lua_rawgeti(L, srct, f + i);
                lua_rawseti(L, dstt, t + i);
            }
        }
    }
}

static int tinsert(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = lua_objlen(L, 1);
    int pos; // where to insert new element
    switch (lua_gettop(L))
    {
    case 2:
    {                // called with only 2 arguments
        pos = n + 1; // insert new element at the end
        break;
    }
    case 3:
    {
        pos = luaL_checkinteger(L, 2); // 2nd argument is the position

        // move up elements if necessary
        if (1 <= pos && pos <= n)
            moveelements(L, 1, 1, pos, n, pos + 1);
        break;
    }
    default:
    {
        luaL_error(L, "wrong number of arguments to 'insert'");
    }
    }
    lua_rawseti(L, 1, pos); // t[pos] = v
    return 0;
}

static int tremove(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = lua_objlen(L, 1);
    int pos = luaL_optinteger(L, 2, n);

    if (!(1 <= pos && pos <= n)) // position is outside bounds?
        return 0;                // nothing to remove
    lua_rawgeti(L, 1, pos);      // result = t[pos]

    moveelements(L, 1, 1, pos + 1, n, pos);

    lua_pushnil(L);
    lua_rawseti(L, 1, n); // t[n] = nil
    return 1;
}

/*
** Copy elements (1[f], ..., 1[e]) into (tt[t], tt[t+1], ...). Whenever
** possible, copy in increasing order, which is better for rehashing.
** "possible" means destination after original range, or smaller
** than origin, or copying to another table.
*/
static int tmove(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int f = luaL_checkinteger(L, 2);
    int e = luaL_checkinteger(L, 3);
    int t = luaL_checkinteger(L, 4);
    int tt = !lua_isnoneornil(L, 5) ? 5 : 1; // destination table
    luaL_checktype(L, tt, LUA_TTABLE);

    if (e >= f)
    { // otherwise, nothing to move
        luaL_argcheck(L, f > 0 || e < INT_MAX + f, 3, "too many elements to move");
        int n = e - f + 1; // number of elements to move
        luaL_argcheck(L, t <= INT_MAX - n + 1, 4, "destination wrap around");

        LuaTable* dst = hvalue(L->base + (tt - 1));

        if (dst->readonly) // also checked in moveelements, but this blocks resizes of r/o tables
            luaG_readonlyerror(L);

        if (t > 0 && (t - 1) <= dst->sizearray && (t - 1 + n) > dst->sizearray)
        { // grow the destination table array
            luaH_resizearray(L, dst, t - 1 + n);
        }

        moveelements(L, 1, tt, f, e, t);
    }
    lua_pushvalue(L, tt); // return destination table
    return 1;
}

// ServerLua: table.append(t, ...) - appends varargs to end of table
// Largely a replacement for consecutive `table.insert()` calls.
static int tappend(lua_State* L)
{
    // Need some extra slots for actually appending
    lua_checkstack(L, 2);

    luaL_checktype(L, 1, LUA_TTABLE);
    int orig_len = lua_objlen(L, 1);
    int top = lua_gettop(L);

    for (int i = 2; i <= top; i++)
    {
        lua_pushvalue(L, i);
        lua_rawseti(L, 1, orig_len + (i - 1));
    }

    return 0;
}

// ServerLua: table.extend(dst, src) - appends array portion of src onto dst
// Again, this is sugar for `table.move(src, 1, #src, #dst+1, dst)`
// which is kind of an arcane and obnoxious incantation. Bespoke array-like
// list construction is unfortunately a pretty common requirement for `ll.*()` functions,
// so it makes sense to provide as a first-class member of the stdlib.
static int textend(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    // ignore further arguments
    lua_settop(L, 2);

    lua_checkstack(L, 6);

    int dn = lua_objlen(L, 1);
    int sn = lua_objlen(L, 2);

    // Rearrange stack: (dst, src) -> (src, 1, sn, dn+1, dst)
    lua_insert(L, 1);
    lua_pushinteger(L, 1);
    lua_insert(L, 2);
    lua_pushinteger(L, sn);
    lua_insert(L, 3);
    lua_pushinteger(L, dn + 1);
    lua_insert(L, 4);

    return tmove(L);
}

static void addfield(lua_State* L, luaL_Strbuf* b, int i, LuaTable* t)
{
    if (t && unsigned(i - 1) < unsigned(t->sizearray) && ttisstring(&t->array[i - 1]))
    {
        TString* ts = tsvalue(&t->array[i - 1]);
        luaL_addlstring(b, getstr(ts), ts->len);
    }
    else
    {
        int tt = lua_rawgeti(L, 1, i);
        if (tt != LUA_TSTRING && tt != LUA_TNUMBER)
            luaL_error(L, "invalid value (%s) at index %d in table for 'concat'", luaL_typename(L, -1), i);
        luaL_addvalue(b);
    }
}

static int tconcat(lua_State* L)
{
    size_t lsep;
    const char* sep = luaL_optlstring(L, 2, "", &lsep);
    luaL_checktype(L, 1, LUA_TTABLE);
    int i = luaL_optinteger(L, 3, 1);
    int last = luaL_opt(L, luaL_checkinteger, 4, lua_objlen(L, 1));

    LuaTable* t = hvalue(L->base);

    luaL_Strbuf b;
    luaL_buffinit(L, &b);
    for (; i < last; i++)
    {
        addfield(L, &b, i, t);
        if (lsep != 0)
            luaL_addlstring(&b, sep, lsep);
    }
    if (i == last) // add last value (if interval was not empty)
        addfield(L, &b, i, t);
    luaL_pushresult(&b);
    return 1;
}

static int tpack(lua_State* L)
{
    int n = lua_gettop(L);    // number of elements to pack
    lua_createtable(L, n, 1); // create result table

    LuaTable* t = hvalue(L->top - 1);

    for (int i = 0; i < n; ++i)
    {
        TValue* e = &t->array[i];
        setobj2t(L, t, e, L->base + i);
    }

    // t.n = number of elements
    TValue* nv = luaH_setstr(L, t, luaS_newliteral(L, "n"));
    setnvalue(nv, n);

    return 1; // return table
}

static int tunpack(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    LuaTable* t = hvalue(L->base);

    int i = luaL_optinteger(L, 2, 1);
    int e = luaL_opt(L, luaL_checkinteger, 3, lua_objlen(L, 1));
    if (i > e)
        return 0;                 // empty range
    unsigned n = (unsigned)e - i; // number of elements minus 1 (avoid overflows)
    if (n >= (unsigned int)INT_MAX || !lua_checkstack(L, (int)(++n)))
        luaL_error(L, "too many results to unpack");

    // fast-path: direct array-to-stack copy
    if (i == 1 && int(n) <= t->sizearray)
    {
        for (i = 0; i < int(n); i++)
            setobj2s(L, L->top + i, &t->array[i]);
        L->top += n;
    }
    else
    {
        // push arg[i..e - 1] (to avoid overflows)
        for (; i < e; i++)
            lua_rawgeti(L, 1, i);
        lua_rawgeti(L, 1, e); // push last element
    }
    return (int)n;
}

// ServerLua: tsort() and its constituent parts are pretty heavily modified to allow for
// interrupt-forced yields, but the core logic is mostly the same.

// ServerLua: Budget between yield checks for the default (non-predicate) comparator path.
// Same concept as YIELD_BATCH_SIZE in lyieldstrlib.cpp.
// This may be raised or lowered without breaking ABI compatibility.
static constexpr int SORT_YIELD_BUDGET = 512;

// ServerLua: Comparison macro for yieldable sort. Expects `t` (LuaTable*), `use_pred`
// (bool slot), `saved_sa` (int32_t slot), and `yield_budget` (int local) to
// be in scope. Writes result to cmp_var (bool slot). Each call site passes a
// unique PHASE_NAME.
//
// Yield check fires on every comparison when use_pred is true (short-circuit
// skips the budget decrement). For the default comparator, the budget gates
// yield checks to every SORT_YIELD_BUDGET comparisons.
//
// The LuaTable* is a stable heap pointer — it never moves. Only t->array
// and t->sizearray can change (if the comparator resizes the table), which
// is what saved_sa detects. God do I hate that this is a macro but what can you do.
#define SORT_CMP(cmp_var, i_idx, j_idx, phase_name)                                             \
    if (use_pred || --yield_budget <= 0)                                                         \
    {                                                                                            \
        YIELD_CHECK(L, phase_name##_YINT, LUA_INTERRUPT_STDLIB);                                 \
        yield_budget = SORT_YIELD_BUDGET;                                                        \
    }                                                                                            \
    if (use_pred)                                                                                \
    {                                                                                            \
        {                                                                                        \
            saved_sa = t->sizearray;                                                             \
            if (unsigned(i_idx) >= unsigned(saved_sa) || unsigned(j_idx) >= unsigned(saved_sa))   \
                luaL_error(L, "table modified during sorting");                                  \
            setobj2s(L, L->top, L->base + 2);                                                   \
            setobj2s(L, L->top + 1, &t->array[i_idx]);                                          \
            setobj2s(L, L->top + 2, &t->array[j_idx]);                                          \
            L->top += 3;                                                                         \
        }                                                                                        \
        YIELD_CALL(L, 2, 1, phase_name);                                                         \
        cmp_var = lua_toboolean(L, -1) != 0;                                                     \
        lua_pop(L, 1);                                                                           \
        if (t->sizearray != saved_sa)                                                            \
            luaL_error(L, "table modified during sorting");                                      \
    }                                                                                            \
    else                                                                                         \
    {                                                                                            \
        int _sa = t->sizearray;                                                                  \
        cmp_var = luaV_lessthan(L, &t->array[i_idx], &t->array[j_idx]);                         \
        if (t->sizearray != _sa)                                                                 \
            luaL_error(L, "table modified during sorting");                                      \
    }

inline void sort_swap(lua_State* L, LuaTable* t, int i, int j)
{
    // ServerLua: A comparator yield could allow another coroutine to freeze this table;
    //  other code assumes frozen tables are never mutated, don't violate that invariant.
    if (LUAU_UNLIKELY(t->readonly))
        luaG_readonlyerror(L);

    TValue* arr = t->array;
    int n = t->sizearray;
    LUAU_ASSERT(unsigned(i) < unsigned(n) && unsigned(j) < unsigned(n)); // contract maintained in sort_less after predicate call

    // no barrier required because both elements are in the array before and after the swap
    TValue temp;
    setobj2s(L, &temp, &arr[i]);
    setobj2t(L, t, &arr[i], &arr[j]);
    setobj2t(L, t, &arr[j], &temp);
}

static void sort_siftheap(lua_State* L, SlotManager& parent_slots, int l_init, int u_init, bool use_pred_init, int root_init)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        CMP_LEFT_YINT = 1,
        CMP_LEFT = 2,
        CMP_RIGHT_YINT = 3,
        CMP_RIGHT = 4,
        CMP_LAST_YINT = 5,
        CMP_LAST = 6,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, root, root_init);
    DEFINE_SLOT(int32_t, l, l_init);
    DEFINE_SLOT(int32_t, u, u_init);
    DEFINE_SLOT(int32_t, count, u_init - l_init + 1);
    DEFINE_SLOT(int32_t, left, 0);
    DEFINE_SLOT(int32_t, right, 0);
    DEFINE_SLOT(int32_t, next, 0);
    DEFINE_SLOT(bool, cmp, false);
    DEFINE_SLOT(bool, use_pred, use_pred_init);
    DEFINE_SLOT(int32_t, saved_sa, 0);
    slots.finalize();

    LuaTable* t = hvalue(L->base + 1);
    int yield_budget = SORT_YIELD_BUDGET;

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(CMP_LEFT_YINT);
    YIELD_DISPATCH(CMP_LEFT);
    YIELD_DISPATCH(CMP_RIGHT_YINT);
    YIELD_DISPATCH(CMP_RIGHT);
    YIELD_DISPATCH(CMP_LAST_YINT);
    YIELD_DISPATCH(CMP_LAST);
    YIELD_DISPATCH_END();

    LUAU_ASSERT(l <= u);

    // process all elements with two children
    while (root * 2 + 2 < count)
    {
        left = root * 2 + 1;
        right = root * 2 + 2;
        next = root;

        SORT_CMP(cmp, l + next, l + left, CMP_LEFT);
        if (cmp)
            next = left;

        SORT_CMP(cmp, l + next, l + right, CMP_RIGHT);
        if (cmp)
            next = right;

        if (next == root)
            break;

        sort_swap(L, t, l + root, l + next);
        root = next;
    }

    // process last element if it has just one child
    if (root * 2 + 1 == count - 1)
    {
        SORT_CMP(cmp, l + root, l + root * 2 + 1, CMP_LAST);
        if (cmp)
            sort_swap(L, t, l + root, l + root * 2 + 1);
    }
}

static void sort_heap(lua_State* L, SlotManager& parent_slots, int l_init, int u_init, bool use_pred_init)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        BUILD_SIFT = 1,
        SORT_SIFT = 2,
        SORT_CHECK = 3,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, l, l_init);
    DEFINE_SLOT(int32_t, u, u_init);
    DEFINE_SLOT(int32_t, count, u_init - l_init + 1);
    DEFINE_SLOT(int32_t, i, count / 2 - 1);
    DEFINE_SLOT(bool, use_pred, use_pred_init);
    slots.finalize();

    LuaTable* t = hvalue(L->base + 1);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(BUILD_SIFT);
    YIELD_DISPATCH(SORT_SIFT);
    YIELD_DISPATCH(SORT_CHECK);
    YIELD_DISPATCH_END();

    LUAU_ASSERT(l <= u);

    for (; i >= 0; --i)
        YIELD_HELPER(L, BUILD_SIFT, sort_siftheap(L, slots, l, u, use_pred, i));

    for (i = count - 1; i > 0; --i)
    {
        sort_swap(L, t, l, l + i);
        YIELD_HELPER(L, SORT_SIFT, sort_siftheap(L, slots, l, l + i - 1, use_pred, 0));
        YIELD_CHECK(L, SORT_CHECK, LUA_INTERRUPT_STDLIB);
    }
}

static void sort_rec(lua_State* L, SlotManager& parent_slots, int l_init, int u_init, int limit_init, bool use_pred_init)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        CMP_UL_YINT = 1,
        CMP_UL = 2,
        CMP_ML_YINT = 3,
        CMP_ML = 4,
        CMP_UM_YINT = 5,
        CMP_UM = 6,
        PART_FWD_YINT = 7,
        PART_FWD = 8,
        PART_REV_YINT = 9,
        PART_REV = 10,
        RECURSE_SMALL = 11,
        RECURSE_LARGE = 12,
        HEAP = 13,
        LOOP_CHECK = 14,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, l, l_init);
    DEFINE_SLOT(int32_t, u, u_init);
    DEFINE_SLOT(int32_t, limit, limit_init);
    DEFINE_SLOT(bool, use_pred, use_pred_init);
    DEFINE_SLOT(int32_t, m, 0);
    DEFINE_SLOT(int32_t, p, 0);
    DEFINE_SLOT(int32_t, i, 0);
    DEFINE_SLOT(int32_t, j, 0);
    DEFINE_SLOT(bool, cmp, false);
    DEFINE_SLOT(int32_t, saved_sa, 0);
    slots.finalize();

    LuaTable* t = hvalue(L->base + 1);
    int yield_budget = SORT_YIELD_BUDGET;

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(CMP_UL_YINT);
    YIELD_DISPATCH(CMP_UL);
    YIELD_DISPATCH(CMP_ML_YINT);
    YIELD_DISPATCH(CMP_ML);
    YIELD_DISPATCH(CMP_UM_YINT);
    YIELD_DISPATCH(CMP_UM);
    YIELD_DISPATCH(PART_FWD_YINT);
    YIELD_DISPATCH(PART_FWD);
    YIELD_DISPATCH(PART_REV_YINT);
    YIELD_DISPATCH(PART_REV);
    YIELD_DISPATCH(RECURSE_SMALL);
    YIELD_DISPATCH(RECURSE_LARGE);
    YIELD_DISPATCH(HEAP);
    YIELD_DISPATCH(LOOP_CHECK);
    YIELD_DISPATCH_END();

    // sort range [l..u] (inclusive, 0-based)
    while (l < u)
    {
        // if the limit has been reached, quick sort is going over the permitted nlogn complexity, so we fall back to heap sort
        if (limit == 0)
        {
            YIELD_HELPER(L, HEAP, sort_heap(L, slots, l, u, use_pred));
            return;
        }

        // sort elements a[l], a[(l+u)/2] and a[u]
        // note: this simultaneously acts as a small sort and a median selector
        SORT_CMP(cmp, u, l, CMP_UL);
        if (cmp) // a[u] < a[l]?
            sort_swap(L, t, u, l); // swap a[l] - a[u]
        if (u - l == 1)
            break;                       // only 2 elements
        m = l + ((u - l) >> 1);          // midpoint
        SORT_CMP(cmp, m, l, CMP_ML);
        if (cmp) // a[m]<a[l]?
            sort_swap(L, t, m, l);
        else
        {
            SORT_CMP(cmp, u, m, CMP_UM);
            if (cmp) // a[u]<a[m]?
                sort_swap(L, t, m, u);
        }
        if (u - l == 2)
            break; // only 3 elements

        // here l, m, u are ordered; m will become the new pivot
        p = u - 1;
        sort_swap(L, t, m, u - 1); // pivot is now (and always) at u-1

        // a[l] <= P == a[u-1] <= a[u], only need to sort from l+1 to u-2
        i = l;
        j = u - 1;
        for (;;)
        { // invariant: a[l..i] <= P <= a[j..u]
            // repeat ++i until a[i] >= P
            for (;;)
            {
                ++i;
                SORT_CMP(cmp, i, p, PART_FWD);
                if (!cmp)
                    break;
                if (i >= u)
                    luaL_error(L, "invalid order function for sorting");
            }
            // repeat --j until a[j] <= P
            for (;;)
            {
                --j;
                SORT_CMP(cmp, p, j, PART_REV);
                if (!cmp)
                    break;
                if (j <= l)
                    luaL_error(L, "invalid order function for sorting");
            }
            if (j < i)
                break;
            sort_swap(L, t, i, j);
        }

        // swap pivot a[p] with a[i], which is the new midpoint
        sort_swap(L, t, p, i);

        // adjust limit to allow 1.5 log2N recursive steps
        limit = (limit >> 1) + (limit >> 2);

        // a[l..i-1] <= a[i] == P <= a[i+1..u]
        // sort smaller half recursively; the larger half is sorted in the next loop iteration
        if (i - l < u - i)
        {
            YIELD_HELPER(L, RECURSE_SMALL, sort_rec(L, slots, l, i - 1, limit, use_pred));
            l = i + 1;
        }
        else
        {
            YIELD_HELPER(L, RECURSE_LARGE, sort_rec(L, slots, i + 1, u, limit, use_pred));
            u = i - 1;
        }

        YIELD_CHECK(L, LOOP_CHECK, LUA_INTERRUPT_STDLIB);
    }
}

DEFINE_YIELDABLE(tsort, 0)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        SORT = 1,
    };

    SlotManager slots(L, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, n, 0);
    DEFINE_SLOT(bool, use_pred, false);
    slots.finalize();

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(SORT);
    YIELD_DISPATCH_END();

    if (slots.isInit())
    {
        luaL_checktype(L, 2, LUA_TTABLE);
        LuaTable* t = hvalue(L->base + 1);
        n = luaH_getn(t);
        if (t->readonly)
            luaG_readonlyerror(L);

        if (!lua_isnoneornil(L, 3))
        {
            luaL_checktype(L, 3, LUA_TFUNCTION);
            use_pred = true;
        }
        lua_settop(L, 3);
    }

    if (n > 0)
        YIELD_HELPER(L, SORT, sort_rec(L, slots, 0, n - 1, n, use_pred));

    return 0;
}

static int tcreate(lua_State* L)
{
    int size = luaL_checkinteger(L, 1);
    if (size < 0)
        luaL_argerror(L, 1, "size out of range");

    if (!lua_isnoneornil(L, 2))
    {
        lua_createtable(L, size, 0);
        LuaTable* t = hvalue(L->top - 1);

        StkId v = L->base + 1;

        for (int i = 0; i < size; ++i)
        {
            TValue* e = &t->array[i];
            setobj2t(L, t, e, v);
        }
    }
    else
    {
        lua_createtable(L, size, 0);
    }

    return 1;
}

// TODO: SeverLua: tfind uses equalobj -> luaV_equalval which invokes __eq metamethods.
//  The entire loop is non-yieldable, so a large table of userdata with __eq
//  could accumulate enough time to get killed by the overtime handler.
//  Consider making yieldable with a budget-gated YIELD_CHECK (straightforward:
//  only state is the loop index).
static int tfind(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checkany(L, 2);
    int init = luaL_optinteger(L, 3, 1);
    if (init < 1)
        luaL_argerror(L, 3, "index out of range");

    LuaTable* t = hvalue(L->base);

    for (int i = init;; ++i)
    {
        const TValue* e = luaH_getnum(t, i);
        if (ttisnil(e))
            break;

        StkId v = L->base + 1;

        if (equalobj(L, v, e))
        {
            lua_pushinteger(L, i);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

static int tclear(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    LuaTable* tt = hvalue(L->base);
    if (tt->readonly)
        luaG_readonlyerror(L);

    luaH_clear(tt);
    // ServerLua: Release the iterorder array if there was one
    luaH_overrideiterorder(L, tt, 0);
    return 0;
}

static int tfreeze(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_argcheck(L, !lua_getreadonly(L, 1), 1, "table is already frozen");
    luaL_argcheck(L, !luaL_getmetafield(L, 1, "__metatable"), 1, "table has a protected metatable");

    lua_setreadonly(L, 1, true);

    lua_pushvalue(L, 1);
    return 1;
}

static int tisfrozen(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_pushboolean(L, lua_getreadonly(L, 1));
    return 1;
}

static int tclone(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_argcheck(L, !luaL_getmetafield(L, 1, "__metatable"), 1, "table has a protected metatable");

    LuaTable* tt = luaH_clone(L, hvalue(L->base));

    TValue v;
    sethvalue(L, &v, tt);
    luaA_pushobject(L, &v);

    return 1;
}

// ServerLua: shrink table to optimal size
// Optional second arg: if true, allow moving sparse array elements to hash (may change iteration order)
static int tshrink(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    LuaTable* t = hvalue(L->base);

    if (t->readonly)
        luaG_readonlyerror(L);

    if (t->memcat < LUA_FIRST_USER_MEMCAT)
        luaL_argerror(L, 1, "cannot shrink system table");

    bool reorder = lua_toboolean(L, 2) != 0;
    luaH_shrink(L, t, reorder);
    lua_pushvalue(L, 1);
    return 1;
}

static const luaL_Reg tab_funcs[] = {
    {"concat", tconcat},
    {"foreach", foreach},
    {"foreachi", foreachi},
    {"getn", getn},
    {"maxn", maxn},
    {"insert", tinsert},
    {"remove", tremove},
    {"pack", tpack},
    {"unpack", tunpack},
    {"move", tmove},
    {"create", tcreate},
    {"find", tfind},
    {"clear", tclear},
    {"freeze", tfreeze},
    {"isfrozen", tisfrozen},
    {"clone", tclone},
    {"shrink", tshrink},
    // ServerLua: table.append, table.extend
    {"append", tappend},
    {"extend", textend},
    {NULL, NULL},
};

int luaopen_table(lua_State* L)
{
    luaL_register(L, LUA_TABLIBNAME, tab_funcs);

    // ServerLua: override sort registration with yieldable version (needs continuation)
    lua_pushcclosurek(L, tsort_v0, "sort", 0, tsort_v0_k);
    lua_setfield(L, -2, "sort");

    // Lua 5.1 compat
    lua_pushcfunction(L, tunpack, "unpack");
    lua_setglobal(L, "unpack");

    return 1;
}
