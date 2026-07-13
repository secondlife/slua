#define llruleset_builder_c

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>

#include "lua.h"
#include "lcommon.h"
#include "lualib.h"
#include "llsl.h"
#include "llruleset_builder.h"

#include "lapi.h"     // luaA_toobject, luaA_pushobject
#include "lobject.h"  // TValue, hvalue, ttisnil, ttistable, ttisboolean, ttisnumber, bvalue, nvalue
#include "ltable.h"   // LuaTable, luaH_getnum, luaH_getstr, luaH_getn
#include "lstring.h"  // luaS_new

struct RulesetBuilderDef
{
    std::vector<RulesetParamDescriptor> descs;     // sorted by tag; .name points into string literals
    std::vector<RulesetFlagDescriptor>  flag_descs;
};

namespace
{
    // Converts the value at TOS to its string representation.
    // Numbers, booleans, and strings are always converted. Other types
    // (vectors, quaternions, userdata) are converted only if they have a
    // __tostring metamethod; everything else is skipped.
    // On success: replaces TOS with a Lua string and returns true.
    // On failure: pops TOS and returns false.
    bool slua_ruleset_to_string(lua_State* L)
    {
        int vtype = lua_type(L, -1);

        // Native conversions -- always well-defined.
        if (vtype == LUA_TSTRING || vtype == LUA_TNUMBER || vtype == LUA_TBOOLEAN)
        {
            luaL_tolstring(L, -1, nullptr);
            lua_remove(L, -2);
            return true;
        }

        // Other types: only include if __tostring is present; skip the
        // "typename: 0xaddr" fallback.
        if (luaL_getmetafield(L, -1, "__tostring"))
        {
            lua_pop(L, 1); // pop the metamethod itself
            luaL_tolstring(L, -1, nullptr);
            lua_remove(L, -2);
            return true;
        }

        lua_pop(L, 1);
        return false;
    }

    // Encodes a lua array as a CSV string; emit one tag/value pair.
    // returns the new index into the list table.
    int slua_ruleset_serialize_string_csv(lua_State* L, int list, int idx,
        const RulesetParamDescriptor& desc)
    {
        LuaTable* t = hvalue(luaA_toobject(L, -1));
        int n = luaH_getn(t);
        if (n > 0)
        {
            std::string csv;
            for (int j = 1; j <= n; ++j)
            {
                const TValue* elem = luaH_getnum(t, j);
                if (ttisnil(elem))
                    continue;
                luaA_pushobject(L, elem);
                if (slua_ruleset_to_string(L))
                {
                    size_t len = 0;
                    const char* s = lua_tolstring(L, -1, &len);
                    if (s)
                    {
                        if (!csv.empty()) csv += ',';
                        for (size_t k = 0; k < len; ++k)
                        {
                            if (s[k] == '\\' || s[k] == ',') csv += '\\';
                            csv += s[k];
                        }
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1); // pop the array
            if (!csv.empty())
            {
                lua_pushinteger(L, desc.tag);
                lua_rawseti(L, list, ++idx);
                lua_pushlstring(L, csv.c_str(), csv.size());
                lua_rawseti(L, list, ++idx);
            }
        }
        else
        {
            lua_pop(L, 1); // pop empty array, emit nothing
        }

        return idx;
    }


    // Convert a lua table into a series of tag/key/value triples, one per entry,
    // sorted by key.
    // returns the new index into the list table.
    int slua_ruleset_serialize_string_map(lua_State* L, int list, int idx,
        const RulesetParamDescriptor& desc)
    {
        // Collect keys for deterministic ordering.
        std::vector<std::string> keys;
        for (int index = 0; (index = lua_rawiter(L, -1, index)) >= 0;)
        {
            if (lua_type(L, -2) == LUA_TSTRING)
                keys.push_back(lua_tostring(L, -2));
            lua_pop(L, 2);
        }
        std::sort(keys.begin(), keys.end());

        LuaTable* t = hvalue(luaA_toobject(L, -1));
        for (const auto& key : keys)
        {
            const TValue* val = luaH_getstr(t, luaS_new(L, key.c_str()));
            if (ttisnil(val))
                continue;
            luaA_pushobject(L, val);
            if (!slua_ruleset_to_string(L))
                continue;
            lua_pushinteger(L, desc.tag);
            lua_rawseti(L, list, ++idx);
            lua_pushlstring(L, key.c_str(), key.size());
            lua_rawseti(L, list, ++idx);
            lua_rawseti(L, list, ++idx); // value
        }
        lua_pop(L, 1); // pop the table
        return idx;
    }

    // Convert a lua array into a series of tag/value pairs, one per element,
    // preserving order.
    // returns the new index into the list table.
    int slua_ruleset_serialize_string_multi(lua_State* L, int list, int idx,
        const RulesetParamDescriptor& desc)
    {
        LuaTable* t = hvalue(luaA_toobject(L, -1));
        int n = luaH_getn(t);
        for (int j = 1; j <= n; ++j)
        {
            const TValue* elem = luaH_getnum(t, j);
            if (ttisnil(elem))
                continue;
            luaA_pushobject(L, elem);
            if (slua_ruleset_to_string(L))
            {
                lua_pushinteger(L, desc.tag);
                lua_rawseti(L, list, ++idx);
                lua_rawseti(L, list, ++idx);
            }
        }
        lua_pop(L, 1); // pop the array
        return idx;
    }
}

RulesetBuilderDef* ruleset_builder_def_build(
    const RulesetParamDescriptor* descs,
    size_t                        count
)
{
    auto* def = new RulesetBuilderDef;

    // Copy descriptors. .name pointers reference string literals with static
    // storage duration -- no deep copy needed.
    def->descs.assign(descs, descs + count);

    // Sort by tag for deterministic serialization order.
    std::sort(def->descs.begin(), def->descs.end(),
        [](const RulesetParamDescriptor& a, const RulesetParamDescriptor& b) {
            return a.tag < b.tag;
        });

    return def;
}

void ruleset_builder_def_add_flags(
    RulesetBuilderDef*           def,
    const RulesetFlagDescriptor* descs,
    size_t                       count
)
{
    // .name pointers reference string literals -- no deep copy needed.
    def->flag_descs.assign(descs, descs + count);
}

void slua_ruleset_serialize(lua_State* L, int params_idx, const RulesetBuilderDef* def)
{
    lua_newtable(L);
    int list = lua_gettop(L);
    int idx = 0;

    if (lua_isnoneornil(L, params_idx))
        return; // empty list

    // Phase 1: accumulate flag bits per backing field.
    LuaTable* params_h = hvalue(luaA_toobject(L, params_idx));
    std::unordered_map<int, int> flag_set_bits;   // field_tag -> bits to OR in
    std::unordered_map<int, int> flag_clear_bits; // field_tag -> bits to AND out
    for (const auto& fdesc : def->flag_descs)
    {
        const TValue* val = luaH_getstr(params_h, luaS_new(L, fdesc.name));
        if (!ttisnil(val))
        {
            bool set = ttisboolean(val) ? (bool)bvalue(val) : (nvalue(val) != 0.0);
            if (set)
                flag_set_bits[fdesc.field_tag]   |= fdesc.mask;
            else
                flag_clear_bits[fdesc.field_tag] |= fdesc.mask;
        }
    }

    // Phase 2: emit tag/value pairs in tag order.
    for (const auto& desc : def->descs)
    {
        int  raw_int = 0;
        bool has_raw = false;

        const TValue* val = luaH_getstr(params_h, luaS_new(L, desc.name));
        has_raw = !ttisnil(val);
        if (has_raw && ttisnumber(val))
            raw_int = (int)nvalue(val);

        // Integer fields that back flags: merge accumulated bits.
        auto set_it   = flag_set_bits.find(desc.tag);
        auto clear_it = flag_clear_bits.find(desc.tag);
        if (desc.semantic == 'i' &&
            (set_it != flag_set_bits.end() || clear_it != flag_clear_bits.end()))
        {
            int set_mask   = (set_it   != flag_set_bits.end())   ? set_it->second   : 0;
            int clear_mask = (clear_it != flag_clear_bits.end()) ? clear_it->second : 0;
            int merged = (raw_int | set_mask) & ~clear_mask;
            lua_pushinteger(L, desc.tag);
            lua_rawseti(L, list, ++idx);
            lua_pushinteger(L, merged);
            lua_rawseti(L, list, ++idx);
            continue;
        }

        // 'C': string-csv -- value is a Lua array of strings.
        // Encode as comma-joined string; emit one tag/value pair.
        if (desc.semantic == 'C')
        {
            if (!has_raw || !ttistable(val))
                continue;
            luaA_pushobject(L, val);
            idx = slua_ruleset_serialize_string_csv(L, list, idx, desc);
            continue;
        }

        // 'M': string-map -- value is a Lua string-keyed table.
        // Emit one tag/key/value triple per entry, in sorted key order.
        if (desc.semantic == 'M')
        {
            if (!has_raw || !ttistable(val))
                continue;
            luaA_pushobject(L, val);
            idx = slua_ruleset_serialize_string_map(L, list, idx, desc);
            continue;
        }

        // 'N': string-multi -- value is a Lua array of strings.
        // Emit one tag/value pair per element, preserving order.
        if (desc.semantic == 'N')
        {
            if (!has_raw || !ttistable(val))
                continue;
            luaA_pushobject(L, val);
            idx = slua_ruleset_serialize_string_multi(L, list, idx, desc);
            continue;
        }

        if (!has_raw)
            continue;

        // Append tag then value.
        lua_pushinteger(L, desc.tag);
        lua_rawseti(L, list, ++idx);

        if (desc.semantic == 'b' && ttisboolean(val))
            lua_pushinteger(L, bvalue(val));
        else
            luaA_pushobject(L, val);
        lua_rawseti(L, list, ++idx);
    }
}

bool slua_ruleset_coerce(lua_State* L, int params_idx, const RulesetBuilderDef* def)
{
    // Convert relative index to absolute
    if (params_idx < 0 && params_idx > LUA_REGISTRYINDEX)
        params_idx = lua_gettop(L) + params_idx + 1;

    // Not a table? Leave unchanged.
    if (!lua_istable(L, params_idx))
        return false;

    // Check if this is a dict (no integer key 1) vs sequential list (has key 1).
    LuaTable* h = hvalue(luaA_toobject(L, params_idx));
    bool has_first = !ttisnil(luaH_getnum(h, 1));

    if (has_first)
        return false;  // Sequential list, leave unchanged

    // Check if table has any keys at all.
    bool has_keys = (lua_rawiter(L, params_idx, 0) >= 0);
    if (!has_keys)
        return false;  // Empty table, already a valid empty list
    lua_pop(L, 2);  // Pop key and value pushed by lua_rawiter

    // Serialize dict to flat list (pushes new table on stack)
    slua_ruleset_serialize(L, params_idx, def);

    // Replace original table with serialized list
    lua_replace(L, params_idx);

    return true;
}

void slua_register_ruleset_fn(
    lua_State*               L,
    const char*              module_name,
    const char*              fn_name,
    lua_CFunction            fn,
    const RulesetBuilderDef* def
)
{
    int top = lua_gettop(L);

    lua_getglobal(L, module_name);
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    else
    {
        lua_setreadonly(L, -1, false);
    }
    int module_idx = lua_gettop(L);

    lua_pushlightuserdata(L, (void*)def);
    lua_pushcclosurek(L, fn, fn_name, 1, nullptr);
    lua_setfield(L, module_idx, fn_name);

    lua_setreadonly(L, module_idx, true);
    lua_setglobal(L, module_name);

    LUAU_ASSERT(lua_gettop(L) == top);
}
