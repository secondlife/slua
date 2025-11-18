#include <cmath>
#include <cstring>
#include <vector>

#include "lgc.h"
#include "lualib.h"
#include "ltable.h"
#include "lapi.h"
#include "llsl.h"
#include "mono_strings.h"
#include "lgcgraph.h"

// Don't fuse multiply and add in this file, even if the optimization rules would
// normally allow it. It will result in material differences to results.
// Not supported in GCC, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=20785
// There, we rely on CMake to get this right.
#ifndef __GNUC__
#   pragma STDC FP_CONTRACT OFF
#endif

// Normalizes an index from Lua stack to 0-based array index
// Handles 1-based vs 0-based mode and negative indices
static int _checkobjectindex(lua_State *L, int len, int stack_idx, bool compat_mode)
{
    int idx = luaL_checkinteger(L, stack_idx);

    if (!compat_mode)
    {
        if (idx == 0)
        {
            luaL_error(L, "passed 0 when a 1-based index was expected");
        }
        else if (idx > 0)
        {
            // positive indices need to be shifted down to be 0-based.
            // negative indices are handled later.
            idx -= 1;
        }
    }

    if(idx < 0)
    {
        return len + idx;
    }
    return idx;
}

// Helper struct for range extraction (normal and wraparound cases)
struct RangeExtractionInfo {
    bool whole_object;      // Return entire object unchanged
    bool empty;             // Return empty object
    int start1, len1;       // First segment (always used if not whole/empty)
    int start2, len2;       // Second segment (used only for wraparound)
};

// Calculates range extraction info for list/string operations
// Handles both normal (start <= end) and wraparound (start > end) cases
// Out-of-bounds indices are clamped to valid range (LSL behavior)
static RangeExtractionInfo _calc_extraction_range(
    lua_State *L,
    int obj_len,           // Length of list/string
    int start_stack_idx,   // Stack position of start index
    int end_stack_idx,     // Stack position of end index
    bool compat_mode       // LSL vs SLua mode
)
{
    RangeExtractionInfo result = {false, false, 0, 0, 0, 0};

    // Empty input always returns empty
    if (obj_len == 0)
    {
        result.empty = true;
        return result;
    }

    // Normalize indices (handles 1-based/0-based conversion and negative indices)
    int start_idx = _checkobjectindex(L, obj_len, start_stack_idx, compat_mode);
    int end_idx = _checkobjectindex(L, obj_len, end_stack_idx, compat_mode);

    if (start_idx <= end_idx)
    {
        // Normal case: start <= end
        // Both out of bounds check
        if (start_idx >= obj_len || end_idx < 0)
        {
            result.empty = true;
            return result;
        }

        // Clamp to valid range (LSL behavior)
        if (end_idx >= obj_len) end_idx = obj_len - 1;
        start_idx = std::max(0, start_idx);
        end_idx = std::max(0, end_idx);

        result.start1 = start_idx;
        result.len1 = end_idx - start_idx + 1;
        result.len1 = std::max(0, result.len1);
        result.start2 = 0;
        result.len2 = 0;
    }
    else
    {
        // Wraparound case: start > end
        // Select items where (i <= end || i >= start)
        // This gives us: [0, end] + [start, obj_len-1]

        // Check if we want the whole object
        if (end_idx >= obj_len
            || start_idx <= 0
            || start_idx - end_idx <= 1)
        {
            result.whole_object = true;
            return result;
        }

        // Two segments
        // First segment: [0, end_idx]
        if (end_idx >= 0)
        {
            result.start1 = 0;
            result.len1 = end_idx + 1;
        }
        else
        {
            result.start1 = 0;
            result.len1 = 0;
        }

        // Second segment: [start_idx, obj_len-1]
        if (start_idx < obj_len)
        {
            result.start2 = start_idx;
            result.len2 = obj_len - start_idx;
        }
        else
        {
            result.start2 = 0;
            result.len2 = 0;
        }

        // If both segments are empty, return empty
        if (result.len1 == 0 && result.len2 == 0)
        {
            result.empty = true;
        }
    }

    return result;
}

static int ll_sin(lua_State *L)
{
    luaSL_pushfloat(L, sin(luaL_checknumber(L, 1)));
    return 1;
}

static int ll_getlistlength(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    auto *h = hvalue(luaA_toobject(L, 1));
    luaSL_pushnativeinteger(L, luaH_getn(h));
    return 1;
}

static int _list_accessor_helper(lua_State *L, LSLIType type)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    auto *h = hvalue(luaA_toobject(L, 1));
    int len = luaH_getn(h);
    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));
    int idx = _checkobjectindex(L, len, 2, compat_mode);
    if (idx < len && idx >= 0)
    {
        lua_pushcfunction(L, lsl_cast_list_elem, "lsl_cast_list_elem");
        luaA_pushobject(L, &h->array[idx]);
        luaSL_pushinteger(L, type);
        lua_call(L, 2, 1);
        // If cast failed, get rid of the value and return the default.
        if (lua_type(L, -1) != LUA_TNIL)
            return 1;
        lua_pop(L, 1);
    }
    return 0;
}

static int ll_list2string(lua_State *L)
{
    if (!_list_accessor_helper(L, LSLIType::LST_STRING))
        lua_pushstring(L, "");
    return 1;
}

static int ll_list2integer(lua_State *L)
{
    if (!_list_accessor_helper(L, LSLIType::LST_INTEGER))
    {
        luaSL_pushnativeinteger(L, 0);
    }

    if (!LUAU_IS_LSL_VM(L))
    {
        // LSL integer -> number
        luaSL_pushnativeinteger(L, lua_tointeger(L, -1));
        lua_replace(L, -2);
    }
    return 1;
}

static int ll_list2float(lua_State *L)
{
    if (!_list_accessor_helper(L, LSLIType::LST_FLOATINGPOINT))
        lua_pushnumber(L, 0.0);
    return 1;
}

static int ll_list2key(lua_State *L)
{
    if (!_list_accessor_helper(L, LSLIType::LST_KEY))
        luaSL_pushuuidstring(L, "");
    return 1;
}

static int ll_list2vector(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    auto *h = hvalue(luaA_toobject(L, 1));
    int len = luaH_getn(h);

    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));

    int idx = _checkobjectindex(L, len, 2, compat_mode);
    if (idx < len && idx >= 0)
    {
        // This accessor does NOT auto-cast!
        auto *tv = &h->array[idx];
        if (lua_lsl_type(tv) == (uint8_t)LSLIType::LST_VECTOR)
        {
            luaA_pushobject(L, tv);
            return 1;
        }
    }
    lua_pushvector(L, 0.0, 0.0, 0.0);
    return 1;
}

static int ll_list2rot(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    auto *h = hvalue(luaA_toobject(L, 1));
    int len = luaH_getn(h);

    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));

    int idx = _checkobjectindex(L, len, 2, compat_mode);
    if (idx < len && idx >= 0)
    {
        // This accessor does NOT auto-cast!
        auto *tv = &h->array[idx];
        if (lua_lsl_type(tv) == (uint8_t)LSLIType::LST_QUATERNION)
        {
            luaA_pushobject(L, tv);
            return 1;
        }
    }
    luaSL_pushquaternion(L, 0.0, 0.0, 0.0, 1.0);
    return 1;
}

static int ll_getlistentrytype(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    auto *h = hvalue(luaA_toobject(L, 1));
    int len = luaH_getn(h);

    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));

    int idx = _checkobjectindex(L, len, 2, compat_mode);
    if (idx < len && idx >= 0)
    {
        luaSL_pushinteger(L, lua_lsl_type(&h->array[idx]));
        return 1;
    }
    luaSL_pushinteger(L, 0);
    return 1;
}

static int ll_list2list(lua_State *L)
{
    lua_checkstack(L, 5);

    luaL_checktype(L, 1, LUA_TTABLE);
    auto *h = hvalue(luaA_toobject(L, 1));
    int len = luaH_getn(h);

    if (len == 0)
    {
        // Any slice of an empty list is an empty list.
        lua_newtable(L);
        return 1;
    }

    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));

    // Calculate extraction range (handles normal and wraparound cases)
    auto range = _calc_extraction_range(L, len, 2, 3, compat_mode);

    // Handle special cases
    if (range.empty)
    {
        lua_newtable(L);
        return 1;
    }

    if (range.whole_object)
    {
        // Cloning is important in case lists are ever mutable
        TValue new_tv;
        sethvalue(L, &new_tv, luaH_clone(L, h));
        luaA_pushobject(L, &new_tv);
        return 1;
    }

    // Build wanted_indices from range info
    int wanted_len = 0;
    std::vector<std::pair<int, int>> wanted_indices;
    wanted_indices.reserve(2);

    if (range.len1 > 0)
    {
        wanted_indices.push_back({range.start1, range.len1});
        wanted_len += range.len1;
    }

    if (range.len2 > 0)
    {
        wanted_indices.push_back({range.start2, range.len2});
        wanted_len += range.len2;
    }

    lua_createtable(L, wanted_len, 0);
    auto *new_h = hvalue(luaA_toobject(L, -1));

    int i = 0;
    for (const auto &index_pair : wanted_indices)
    {
        for (int j=0; j<index_pair.second; ++j)
        {
            setobj(L, &new_h->array[i], &h->array[index_pair.first + j]);
            LUAU_ASSERT(i < wanted_len);
            ++i;
        }
    }
    LUAU_ASSERT(luaH_getn(new_h) == wanted_len);

    return 1;
}

static int ll_dumplist2string(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TSTRING);
    auto *h = hvalue(luaA_toobject(L, 1));
    int len = luaH_getn(h);

    if (len <= 0)
    {
        lua_pushstring(L, "");
        return 1;
    }

    lua_checkstack(L, 4);

    // Assume at least 1 character per item + separator length for our initial capacity.
    // This is essentially a minimum but gets us in the right ballpark without counting everything
    // StringBuilder will allocate more as needed.
    // Set the max capacity to 64k (the max script size) + a little slop.
    static const int max_size = 65 * 1024;
    bool first = true;
    lua_pushstring(L, "");
    for(int i=0; i<len; ++i)
    {
        if (first)
            lua_pushstring(L, "");
        else
            lua_pushvalue(L, 2);
        first = false;

        // Unlike (string)list_val, this doesn't keep negative zero.
        lua_pushcfunction(L, lsl_cast_list_elem_poszero, "lsl_cast_list_elem_poszero");
        luaA_pushobject(L, &h->array[i]);
        luaSL_pushinteger(L, LSLIType::LST_STRING);
        lua_call(L, 2, 1);
        // But we might have a type we don't understand in the list
        if (lua_type(L, -1) == LUA_TNIL)
            luaL_errorL(L, "non-LSL value in list");

        lua_concat(L, 3);
        if (lua_strlen(L, 3) > max_size)
        {
            luaD_throw(L, LUA_ERRMEM);
        }
    }

    return 1;
}

static int ll_deletesublist(lua_State *L)
{
    lua_checkstack(L, 4);

    luaL_checktype(L, 1, LUA_TTABLE);
    auto *h = hvalue(luaA_toobject(L, 1));
    int len = luaH_getn(h);

    if (len == 0)
    {
        // Any slice of an empty list is an empty list.
        lua_newtable(L);
        return 1;
    }

    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));

    // Calculate deletion range (inverse of extraction)
    auto range = _calc_extraction_range(L, len, 2, 3, compat_mode);

    // For delete: empty range means nothing to delete, return whole list
    if (range.empty)
    {
        // Cloning is important in case lists are ever mutable
        TValue new_tv;
        sethvalue(L, &new_tv, luaH_clone(L, h));
        luaA_pushobject(L, &new_tv);
        return 1;
    }

    // For delete: whole_object means delete everything, return empty list
    if (range.whole_object)
    {
        lua_newtable(L);
        return 1;
    }

    // Normal case: delete single range [start1, start1+len1)
    if (range.len2 == 0)
    {
        int count = range.len1;
        int wanted_len = len - count;
        lua_createtable(L, wanted_len, 0);
        auto *new_h = hvalue(luaA_toobject(L, -1));

        int j = 0;
        for (int i=0; i<len; ++i)
        {
            // Skip the range we want to delete
            if (i >= range.start1 && i < range.start1 + count)
            {
                continue;
            }
            setobj(L, &new_h->array[j], &h->array[i]);
            ++j;
        }
        return 1;
    }
    else
    {
        // Wraparound case: delete [0, len1) and [start2, start2+len2)
        // This means KEEP the middle section: [len1, start2)
        // Delegate to list2list to extract the kept section
        // Always use compat_mode=true since we're providing already-normalized 0-based indices
        lua_pushboolean(L, true);
        lua_pushcclosurek(L, ll_list2list, "List2List", 1, nullptr);
        lua_pushvalue(L, 1);
        // The kept section is from (end+1) to (start-1) in 0-based indices
        // range.len1 = end_idx + 1, so start of kept section is range.len1
        // range.start2 is start of second wraparound segment, so end of kept section is range.start2 - 1
        luaSL_pushinteger(L, range.len1);
        luaSL_pushinteger(L, range.start2 - 1);
        lua_call(L, 3, 1);
        return 1;
    }
}

static int ll_listinsertlist(lua_State *L)
{
    lua_checkstack(L, 4);

    luaL_checktype(L, 1, LUA_TTABLE);
    auto * dest_h = hvalue(luaA_toobject(L, 1));
    int dest_len = luaH_getn(dest_h);
    luaL_checktype(L, 2, LUA_TTABLE);
    auto *src_h = hvalue(luaA_toobject(L, 2));
    int src_len = luaH_getn(src_h);

    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));

    int target = _checkobjectindex(L, dest_len, 3, compat_mode);

    LuaTable *cloned_h = nullptr;
    TValue new_tv;

    if (dest_len == 0)
    {
        // dest array is empty, push a copy of src
        cloned_h = luaH_clone(L, src_h);
    }
    else if (src_len == 0)
    {
        // src array is empty, push a copy of dest
        cloned_h = luaH_clone(L, dest_h);
    }

    if (cloned_h != nullptr)
    {
        sethvalue(L, &new_tv, cloned_h);
        luaA_pushobject(L, &new_tv);
        return 1;
    }

    target = std::min(dest_len, std::max(0, target));

    int wanted_len = dest_len + src_len;
    lua_createtable(L, wanted_len, 0);
    auto *new_h = hvalue(luaA_toobject(L, -1));

    if (target == dest_len)
    {
        // Put the new array right after the old one
        memcpy(&new_h->array[0], &dest_h->array[0], sizeof(TValue) * dest_len);
        memcpy(&new_h->array[dest_len], &src_h->array[0], sizeof(TValue) * src_len);
    }
    else
    {
        // Okay we actually have to insert in the middle
        for(int i=0, new_idx=0; new_idx<wanted_len; ++i, ++new_idx)
        {
            if (i == target)
            {
                for (int j=0; j<src_len; ++j, ++new_idx)
                {
                    setobj(L, &new_h->array[new_idx], &src_h->array[j]);
                }
            }
            setobj(L, &new_h->array[new_idx], &dest_h->array[i]);
        }
    }
    return 1;
}

static int ll_listreplacelist(lua_State *L)
{
    lua_checkstack(L, 5);

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);
    int orig_len = lua_objlen(L, 1);
    // Let llDeleteSubList handle the tricky part.
    lua_pushboolean(L, true);
    lua_pushcclosurek(L, ll_deletesublist, "DeleteSubList", 1, nullptr);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 3);
    lua_pushvalue(L, 4);
    lua_call(L, 3, 1);
    // Replace the dest list with our mutated list
    lua_replace(L, 1);

    auto * dest_h = hvalue(luaA_toobject(L, 1));
    int dest_len = luaH_getn(dest_h);

    auto *src_h = hvalue(luaA_toobject(L, 2));
    int src_len = luaH_getn(src_h);

    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));

    int target = _checkobjectindex(L, orig_len, 3, compat_mode);

    LuaTable *cloned_h = nullptr;
    TValue new_tv;
    if (src_len == 0 || target < 0)
    {
        // Inserting an empty list or to an invalid index is a no-op.
        // Just return dest
        // Cloning is important in case we expose this to Lua, which has mutable tables.
        cloned_h = luaH_clone(L, dest_h);
    }
    else if (dest_len == 0)
    {
        // Inserting a list into an empty list should just return src
        cloned_h = luaH_clone(L, src_h);
    }

    if (cloned_h)
    {
        sethvalue(L, &new_tv, cloned_h);
        luaA_pushobject(L, &new_tv);
        return 1;
    }

    target = std::min(target, dest_len);
    // llListInsertList can handle this.
    lua_pushboolean(L, true);
    lua_pushcclosurek(L, ll_listinsertlist, "ListInsertList", 1, nullptr);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    luaSL_pushinteger(L, target);
    lua_call(L, 3, 1);
    return 1;
}

// These are a combination of logic from `LslLibrary.cs`, Mono's `Math.cs` and Mono's `sysmath.c`.
static int ll_abs(lua_State *L)
{
    int val = luaL_checkinteger(L, 1);
    // Deal with weird abs() implementations
    // Overflow will just make this `INT32_MIN` again
    if (val == INT32_MIN)
    {
        luaSL_pushnativeinteger(L, INT32_MIN);
        return 1;
    }
    luaSL_pushnativeinteger(L, val < 0 ? -val : val);
    return 1;
}

static int ll_fabs(lua_State *L)
{
    float val = (float)luaL_checknumber(L, 1);
    // This matches Mono 2.x logic, I suppose this means we could pass through
    // -0.0 without inverting the sign.
    luaSL_pushfloat(L, val < 0.0f ? -val : val);
    return 1;
}

static int ll_floor(lua_State *L)
{
    auto val = floor((float)luaL_checknumber(L, 1));
    luaSL_pushnativeinteger(L, (int32_t)val);
    return 1;
}

static int ll_ceil(lua_State *L)
{
    double val = (float)luaL_checknumber(L, 1);
    double res = floor(val);
    if (val != res)
        ++res;

    luaSL_pushnativeinteger(L, (int32_t)res);
    return 1;
}

static int ll_cos(lua_State *L)
{
    luaSL_pushfloat(L, cos(luaL_checknumber(L, 1)));
    return 1;
}

static int ll_tan(lua_State *L)
{
    luaSL_pushfloat(L, tan(luaL_checknumber(L, 1)));
    return 1;
}

static int ll_atan2(lua_State *L)
{
    double result;
    double y = (float)luaL_checknumber(L, 1);
    double x = (float)luaL_checknumber(L, 2);

    if ((y == HUGE_VAL && x == HUGE_VAL) ||
        (y == HUGE_VAL && x == -HUGE_VAL) ||
        (y == -HUGE_VAL && x == HUGE_VAL) ||
        (y == -HUGE_VAL && x == -HUGE_VAL))
    {
        result = NAN;
    }
    else
    {
        result = atan2(y, x);
        if (result == -0.0)
            result = 0.0;
    }

    luaSL_pushfloat(L, result);
    return 1;
}

static int ll_sqrt(lua_State *L)
{
    double result;
    double x = (float)luaL_checknumber(L, 1);

    if (x < 0.0)
    {
        result = NAN;
    }
    else
    {
        result = sqrt(x);
    }

    luaSL_pushfloat(L, result);
    return 1;
}

// largest integer value that can be stored in a double (53-bit max)
static constexpr double LARGEST_INT_DOUBLE = (1ULL << 53) - 1;


static int ll_pow(lua_State *L)
{
    double result;
    double x = (float)luaL_checknumber(L, 1);
    double y = (float)luaL_checknumber(L, 2);

    if (std::isnan(x) || std::isnan(y))
    {
        luaSL_pushfloat(L, NAN);
        return 1;
    }

    if ((x == 1 || x == -1) && (y == HUGE_VAL || y == -HUGE_VAL))
    {
        luaSL_pushfloat(L, NAN);
        return 1;
    }

    /* Ensure we return the same results as Mono 2.x for certain limit values */
    if (x < -LARGEST_INT_DOUBLE) {
        if (y > LARGEST_INT_DOUBLE)
        {
            luaSL_pushfloat(L, HUGE_VAL);
            return 1;
        }
        if (y < -LARGEST_INT_DOUBLE)
        {
            luaSL_pushfloat(L, 0);
            return 1;
        }
    }

    result = pow (x, y);

    /* Ensure we return the same results as Mono 2.x for certain limit values */
    if (std::isnan(result) &&
        (x == -1.0) &&
        ((y > LARGEST_INT_DOUBLE) || (y < -LARGEST_INT_DOUBLE)))
    {
        luaSL_pushfloat(L, 1);
        return 1;
    }

    luaSL_pushfloat(L, (result == -0.0)? 0.0: result);
    return 1;
}

static int ll_log(lua_State *L)
{
    double result;
    double x = (float)luaL_checknumber(L, 1);

    if (x > 0.0f)
    {
        result = log(x);
    }
    else
    {
        result = 0.0;
    }

    luaSL_pushfloat(L, result);
    return 1;
}

static int ll_log10(lua_State *L)
{
    double result;
    double x = (float)luaL_checknumber(L, 1);

    if (x > 0.0f)
    {
        result = log10(x);
    }
    else
    {
        result = 0.0;
    }

    luaSL_pushfloat(L, result);
    return 1;
}

static int ll_acos(lua_State *L)
{
    double result;
    double x = (float)luaL_checknumber(L, 1);
    if (x < -1 || x > 1)
        result = NAN;
    else
        result = acos(x);
    luaSL_pushfloat(L, result);
    return 1;
}

static int ll_asin(lua_State *L)
{
    double result;
    double x = (float)luaL_checknumber(L, 1);
    if (x < -1 || x > 1)
        result = NAN;
    else
        result = asin(x);
    luaSL_pushfloat(L, result);
    return 1;
}

static int ll_to_lower(lua_State *L)
{
    const char *str;
    size_t str_len;
    str = luaL_checklstring(L, 1, &str_len);
    std::string out = to_lower_mono(str, str_len);
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

static int ll_to_upper(lua_State *L)
{
    const char *str;
    size_t str_len;
    str = luaL_checklstring(L, 1, &str_len);
    std::string out = to_upper_mono(str, str_len);
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

static int ll_get_time(lua_State *L)
{
    auto *runtime_state = LUAU_GET_SL_VM_STATE(L);
    double time = runtime_state->clockProvider ? runtime_state->clockProvider(L) : lua_clock();
    if (runtime_state->slIdentifier == LUA_LSL_IDENTIFIER)
    {
        // Cast away all the precision and shove it back in a double if LSL :)
        time = (float)time;
    }
    lua_pushnumber(L, time);
    return 1;
}

static const luaL_Reg lllib[] = {
    {"Fabs", ll_fabs},
    {"Sin", ll_sin},
    {"GetListLength", ll_getlistlength},
    {"DumpList2String", ll_dumplist2string},
    {"Log", ll_log},
    {"Log10", ll_log10},
    {"Atan2", ll_atan2},
    {"Acos", ll_acos},
    {"Asin", ll_asin},
    {"Tan", ll_tan},
    {"Cos", ll_cos},
    {"Pow", ll_pow},
    {"Floor", ll_floor},
    {"Ceil", ll_ceil},
    {"Sqrt", ll_sqrt},
    {"Abs", ll_abs},
    {"ToLower", ll_to_lower},
    {"ToUpper", ll_to_upper},
    {"GetTime", ll_get_time},
    {NULL, NULL},
};

// Functions that have index semantics need to be registered differently,
// as they need an upvalue dictating them how to behave.
static const luaL_Reg llcompateligiblelib[] = {
    {"List2String", ll_list2string},
    {"List2Integer", ll_list2integer},
    {"List2Float", ll_list2float},
    {"List2Key", ll_list2key},
    {"List2Vector", ll_list2vector},
    {"List2Rot", ll_list2rot},
    {"GetListEntryType", ll_getlistentrytype},
    {"List2List", ll_list2list},
    {"DeleteSubList", ll_deletesublist},
    {"ListInsertList", ll_listinsertlist},
    {"ListReplaceList", ll_listreplacelist},
    {NULL, NULL},
};

// These are functions that may be used in tests or in the REPL,
// but are not to be used in user-provided scripts
static int ll_stringlength(lua_State *L)
{
    luaSL_pushnativeinteger(L, (int)strlen(luaL_checkstring(L, 1)));
    return 1;
}

static int ll_getsubstring(lua_State* L)
{
    // UTF-8 aware substring extraction with 1-based indexing (SLua mode)
    // or 0-based indexing (LSL compat mode). Supports negative indices and wraparound.
    // For demo/test purposes only, as it doesn't truncate at null like the real one.
    size_t str_len;
    const char *str_val = luaL_checklstring(L, 1, &str_len);
    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));

    // Convert UTF-8 string to codepoints
    CodepointString codepoints = utf8str_to_codepoints(str_val, str_len);
    int num_codes = (int)codepoints.length();

    // Calculate extraction range (handles normal and wraparound cases)
    auto range = _calc_extraction_range(L, num_codes, 2, 3, compat_mode);

    // Handle special cases
    if (range.empty)
    {
        lua_pushstring(L, "");
        return 1;
    }

    if (range.whole_object)
    {
        lua_pushvalue(L, 1);
        return 1;
    }

    // Extract substring(s)
    std::string result;

    if (range.len2 == 0)
    {
        // Normal case: single segment
        CodepointString substr = codepoints.substr(range.start1, range.len1);
        result = codepoints_to_utf8str(substr, substr.length());
    }
    else
    {
        // Wraparound case: concatenate two segments
        CodepointString substr = codepoints.substr(range.start1, range.len1) + codepoints.substr(range.start2, range.len2);

        result = codepoints_to_utf8str(substr, substr.length());
    }

    lua_pushlstring(L, result.c_str(), result.length());
    return 1;
}

static int ll_ownersay(lua_State* L)
{
    // basically a string-only print
    const char *s = luaL_checkstring(L, 1);
    fwrite(s, 1, strlen(s), stdout);
    fwrite("\n", 1, 1, stdout);
    return 0;
}

static int ll_sleep(lua_State *)
{
    // Do nothing!
    return 0;
}

static int ll_getusedmemory(lua_State* L)
{
    luaC_fullgc(L);
    int total_size = lua_totalmemoverhead(L);

    FILE* f = fopen("/tmp/memdump.json", "w");
    LUAU_ASSERT(f);

    luaC_dump(L, f, nullptr);

    fclose(f);
    lua_pushnumber(L, lua_totalbytes(L, 2));
    return 1;
}

static int ll_getfreememory(lua_State *L)
{
    luaC_fullgc(L);
    luaX_graphheap(L, "/tmp/whatever.json");
    luaSL_pushnativeinteger(L, (int)lua_userthreadsize(L, nullptr));
    return 1;
}

static int ll_detectedtouchface(lua_State *L)
{
    // Just return whatever we passed in if it was an int.
    luaSL_pushnativeinteger(L, luaSL_checkindexlike(L, 1));
    return 1;
}

// Obviously this is not a good CSPRNG, but it's only for testing.
static int ll_generatekey(lua_State *L)
{
    uint32_t pcg32_random(uint64_t* state);
    uint8_t uuid_bytes[16] = {0};
    for(size_t i=0; i<16; i += 4)
    {
        auto rand_val = pcg32_random(&L->global->rngstate);
        memcpy(&uuid_bytes[i], &rand_val, 4);
    }
    luaSL_pushuuidbytes(L, (const uint8_t*)&uuid_bytes);
    return 1;
}

// Test functions that need compat mode handling for index semantics
static const luaL_Reg lltestcompateligiblelib[] = {
    {"GetSubString", ll_getsubstring},
    {NULL, NULL},
};

static const luaL_Reg lltestlib[] = {
    {"OwnerSay", ll_ownersay},
    // We have to use LL's internal implementation because there are unicode
    // semantics we need to consider here.
    {"StringLength", ll_stringlength},
    {"Sleep", ll_sleep},
    {"GetUsedMemory", ll_getusedmemory},
    {"GetFreeMemory", ll_getfreememory},
    // Strictly just for testing that the `ll.Detected*()` wrappers work at all.
    {"DetectedTouchFace", ll_detectedtouchface},
    {"GenerateKey", ll_generatekey},
    {NULL, NULL},
};

int luaopen_ll(lua_State* L, int testing_funcs)
{
    if (LUAU_IS_LSL_VM(L))
    {
        luaL_register(L, LUA_LLLIBNAME, lllib);
        // pepper in the functions that have "compat" modes, with compat mode on
        luaL_register_noclobber_compat(L, LUA_LLLIBNAME, llcompateligiblelib, true);
        lua_pop(L, 1);
        if (testing_funcs)
        {
            // Pepper in some extra functions if we're testing
            luaL_register_noclobber(L, LUA_LLLIBNAME, lltestlib);
            lua_pop(L, 1);
            luaL_register_noclobber_compat(L, LUA_LLLIBNAME, lltestcompateligiblelib, true);
            lua_pop(L, 1);
        }
    }
    else
    {
        luaL_register(L, LUA_LLLIBNAME, lllib);
        // pepper in the functions that have "compat" modes, with compat mode off
        luaL_register_noclobber_compat(L, LUA_LLLIBNAME, llcompateligiblelib, false);
        lua_pop(L, 1);
        // llcompat is basically ll, but with all compat mode functions
        luaL_register(L, LUA_LLCOMPATLIBNAME, lllib);
        lua_pop(L, 1);
        luaL_register_noclobber_compat(L, LUA_LLCOMPATLIBNAME, llcompateligiblelib, true);
        lua_pop(L, 1);
        if (testing_funcs)
        {
            // Pepper in some extra functions if we're testing
            luaL_register_noclobber(L, LUA_LLLIBNAME, lltestlib);
            lua_pop(L, 1);
            luaL_register_noclobber_compat(L, LUA_LLLIBNAME, lltestcompateligiblelib, false);
            lua_pop(L, 1);
            luaL_register_noclobber(L, LUA_LLCOMPATLIBNAME, lltestlib);
            lua_pop(L, 1);
            luaL_register_noclobber_compat(L, LUA_LLCOMPATLIBNAME, lltestcompateligiblelib, true);
            lua_pop(L, 1);
        }
    }
    return 1;
}
