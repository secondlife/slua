#define llfluent_builder_c

#include "lua.h"
#include "lcommon.h"
#include "lualib.h"
#include "llsl.h"
#include "llfluent_builder.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

struct FluentBuilderDef
{
    std::string apply_fn_name;
    std::string apply_link_fn_name;
    std::vector<FluentParamDescriptor> descs;   // sorted by tag
    std::vector<std::string>           names;   // storage for descriptor name strings
    std::unordered_map<std::string, int> name_to_index;

    std::vector<FluentFlagDescriptor>    flag_descs;
    std::vector<std::string>             flag_names;
    std::unordered_map<std::string, int> flag_name_to_index;
};

FluentBuilderDef* fluent_builder_def_build(
    const char*                  apply_fn_name,
    const char*                  apply_link_fn_name,
    const FluentParamDescriptor* descs,
    size_t                       count
)
{
    auto* def = new FluentBuilderDef;
    def->apply_fn_name      = apply_fn_name;
    def->apply_link_fn_name = apply_link_fn_name;

    // Copy descriptors and deep-copy name strings.
    def->descs.resize(count);
    def->names.resize(count);
    for (size_t i = 0; i < count; ++i)
    {
        def->names[i] = descs[i].name;
        def->descs[i] = descs[i];
        def->descs[i].name = def->names[i].c_str();
    }

    // Sort by tag for deterministic serialization order.
    // names and descs stay in sync via index sort.
    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return def->descs[a].tag < def->descs[b].tag;
    });

    std::vector<FluentParamDescriptor> sorted_descs(count);
    std::vector<std::string>           sorted_names(count);
    for (size_t i = 0; i < count; ++i)
    {
        sorted_names[i] = std::move(def->names[order[i]]);
        sorted_descs[i] = def->descs[order[i]];
        sorted_descs[i].name = sorted_names[i].c_str();
    }
    def->descs = std::move(sorted_descs);
    def->names = std::move(sorted_names);

    // Build name-to-index lookup.
    for (int i = 0; i < (int)count; ++i)
        def->name_to_index[def->descs[i].name] = i;

    return def;
}

void fluent_builder_def_add_flags(
    FluentBuilderDef*           def,
    const FluentFlagDescriptor* descs,
    size_t                      count
)
{
    def->flag_descs.resize(count);
    def->flag_names.resize(count);
    for (size_t i = 0; i < count; ++i)
    {
        def->flag_names[i]      = descs[i].name;
        def->flag_descs[i]      = descs[i];
        def->flag_descs[i].name = def->flag_names[i].c_str();
        def->flag_name_to_index[def->flag_names[i]] = (int)i;
    }
}

// __newindex: validate type by semantic char, then rawset.
static int fluent_builder_newindex(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    const char* key = luaL_checkstring(L, 2);
    // value is at index 3

    const auto* def = (const FluentBuilderDef*)lua_tolightuserdata(L, lua_upvalueindex(1));

    // Flag boolean properties: virtual aliases over the backing integer field.
    auto flag_it = def->flag_name_to_index.find(key);
    if (flag_it != def->flag_name_to_index.end())
    {
        const FluentFlagDescriptor& fdesc = def->flag_descs[flag_it->second];

        // Find the backing field name by tag (done once per call; small linear scan).
        const char* field_name = nullptr;
        for (const auto& d : def->descs)
            if (d.tag == fdesc.field_tag) { field_name = d.name; break; }

        lua_rawgetfield(L, 1, field_name);
        int cur = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : 0;
        lua_pop(L, 1);

        int next;
        if (lua_isnoneornil(L, 3))
        {
            // nil clears the bit
            next = cur & ~fdesc.mask;
        }
        else
        {
            if (!lua_isboolean(L, 3) && lua_type(L, 3) != LUA_TNUMBER)
                luaL_typeerrorL(L, 3, "boolean or integer");
            bool set = lua_isboolean(L, 3) ? (bool)lua_toboolean(L, 3) : (lua_tointeger(L, 3) != 0);
            next = set ? (cur | fdesc.mask) : (cur & ~fdesc.mask);
        }
        lua_pushinteger(L, next);
        lua_rawsetfield(L, 1, field_name);
        return 0;
    }

    auto it = def->name_to_index.find(key);
    if (it == def->name_to_index.end())
        luaL_errorL(L, "unknown property '%s'", key);

    // nil clears the property
    if (lua_isnoneornil(L, 3))
    {
        lua_pushnil(L);
        lua_rawsetfield(L, 1, key);
        return 0;
    }

    char sem = def->descs[it->second].semantic;
    switch (sem)
    {
    case 'i':
        luaL_checkinteger(L, 3);
        break;
    case 'f':
        luaL_checknumber(L, 3);
        break;
    case 's':
        if (lua_type(L, 3) != LUA_TSTRING)
            luaL_typeerrorL(L, 3, "string");
        break;
    case 'v':
        luaL_checkvector(L, 3);
        break;
    case 'r':
        luaSL_checkquaternion(L, 3);
        break;
    case 'b':
        if (!lua_isboolean(L, 3) && lua_type(L, 3) != LUA_TNUMBER)
            luaL_typeerrorL(L, 3, "boolean or integer");
        break;
    case 'a':
    case 'k':
    {
        int t = lua_type(L, 3);
        if (t != LUA_TSTRING && !(t == LUA_TUSERDATA && lua_userdatatag(L, 3) == UTAG_UUID))
            luaL_typeerrorL(L, 3, "string or uuid");
        break;
    }
    default:
        luaL_errorL(L, "internal: bad builder semantic char '%c'", sem);
    }

    lua_rawsetfield(L, 1, key);
    return 0;
}

// apply(self [, linkNumber])
static int fluent_builder_apply(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    const auto* def = (const FluentBuilderDef*)lua_tolightuserdata(L, lua_upvalueindex(1));
    bool has_link = !lua_isnoneornil(L, 2);
    int link_num = 0;
    if (has_link)
        link_num = luaL_checkinteger(L, 2);

    // Build flattened rules list in tag order.
    lua_newtable(L);
    int list = lua_gettop(L);
    int idx = 0;

    for (const auto& desc : def->descs)
    {
        lua_rawgetfield(L, 1, desc.name);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }
        // append tag then value
        lua_pushinteger(L, desc.tag);
        lua_rawseti(L, list, ++idx);

        // For 'b' semantic, booleans must be coerced to integer for the wire format.
        if (desc.semantic == 'b' && lua_isboolean(L, -1))
            lua_pushinteger(L, lua_toboolean(L, -1));
        else
            lua_pushvalue(L, -1);
        lua_rawseti(L, list, ++idx);

        lua_pop(L, 1); // pop the rawgetfield result
    }

    // Dispatch via ll.* global table.
    lua_rawgetfield(L, LUA_BASEGLOBALSINDEX, "ll");
    if (has_link)
    {
        lua_rawgetfield(L, -1, def->apply_link_fn_name.c_str());
        lua_pushinteger(L, link_num);
        lua_pushvalue(L, list);
        lua_call(L, 2, 0);
    }
    else
    {
        lua_rawgetfield(L, -1, def->apply_fn_name.c_str());
        lua_pushvalue(L, list);
        lua_call(L, 1, 0);
    }
    return 0;
}

// new([table]): create empty table, attach metatable, and optionally bulk-initialize
// from an initializer table by routing each key/value through __newindex.
// Upvalue 1: metatable
static int fluent_builder_new(lua_State* L)
{
    bool has_init = lua_istable(L, 1);

    // Create the new instance and attach metatable.
    lua_createtable(L, 0, 0);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L, -2);
    // instance is now on top

    if (!has_init)
        return 1;

    int instance_idx = lua_gettop(L);  // absolute index of instance
    const auto* def = (const FluentBuilderDef*)lua_tolightuserdata(L, lua_upvalueindex(2));

    // Fetch __newindex from the metatable.
    lua_pushvalue(L, lua_upvalueindex(1));   // push mt
    lua_getfield(L, -1, "__newindex");       // push __newindex closure
    lua_remove(L, -2);                       // remove mt

    int fn_idx = lua_gettop(L);             // absolute index of __newindex closure

    // Iterate the initializer table (arg 1).
    lua_pushnil(L);
    while (lua_next(L, 1) != 0)
    {
        // Stack: ..., instance(instance_idx), fn(fn_idx), key(top-1), value(top)
        int key_idx = lua_gettop(L) - 1;
        int val_idx = lua_gettop(L);

        // Skip unknown keys silently (type errors on known keys still propagate).
        if (lua_type(L, key_idx) == LUA_TSTRING)
        {
            const char* k = lua_tostring(L, key_idx);
            bool known = def->flag_name_to_index.count(k) || def->name_to_index.count(k);
            if (known)
            {
                lua_pushvalue(L, fn_idx);
                lua_pushvalue(L, instance_idx);
                lua_pushvalue(L, key_idx);
                lua_pushvalue(L, val_idx);
                lua_call(L, 3, 0);
            }
        }

        // Pop value; leave key on top for lua_next.
        lua_pop(L, 1);
    }

    // Pop the __newindex closure; leave instance on stack.
    lua_pop(L, 1);
    return 1;
}

// __index: check flag properties first, then fall through to the metatable for methods.
// Upvalue 1: metatable (for new/apply fallthrough)
// Upvalue 2: def* (for flag lookup)
static int fluent_builder_index(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    const char* key = luaL_checkstring(L, 2);

    const auto* def = (const FluentBuilderDef*)lua_tolightuserdata(L, lua_upvalueindex(2));

    auto flag_it = def->flag_name_to_index.find(key);
    if (flag_it != def->flag_name_to_index.end())
    {
        const FluentFlagDescriptor& fdesc = def->flag_descs[flag_it->second];

        const char* field_name = nullptr;
        for (const auto& d : def->descs)
            if (d.tag == fdesc.field_tag) { field_name = d.name; break; }

        int cur = 0;
        if (field_name)
        {
            lua_rawgetfield(L, 1, field_name);
            if (lua_isnumber(L, -1))
                cur = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);
        }
        lua_pushboolean(L, (cur & fdesc.mask) != 0);
        return 1;
    }

    // Fall through to metatable for new(), apply(), etc.
    lua_rawgetfield(L, lua_upvalueindex(1), key);
    return 1;
}

void slua_open_fluent_builder(
    lua_State*              L,
    const char*             module_name,
    const char*             type_name,
    const FluentBuilderDef* def
)
{
    int top = lua_gettop(L);

    // module table
    lua_newtable(L);
    int module_idx = lua_gettop(L);

    // type metatable
    lua_newtable(L);
    int mt = lua_gettop(L);

    // __newindex — def* as light userdata upvalue
    lua_pushlightuserdata(L, (void*)def);
    lua_pushcclosurek(L, fluent_builder_newindex, "__newindex", 1, nullptr);
    lua_setfield(L, mt, "__newindex");

    // __index closure: flag reads first, then method fallthrough via metatable.
    // new() and apply() are set on mt after this, but the closure holds a reference
    // to the metatable table object itself (not a snapshot), so it sees them.
    lua_pushvalue(L, mt);                          // upvalue 1: metatable
    lua_pushlightuserdata(L, (void*)def);          // upvalue 2: def*
    lua_pushcclosurek(L, fluent_builder_index, "__index", 2, nullptr);
    lua_setfield(L, mt, "__index");

    // new() — upvalue 1: metatable, upvalue 2: def* (for unknown-key skipping)
    lua_pushvalue(L, mt);
    lua_pushlightuserdata(L, (void*)def);
    lua_pushcclosurek(L, fluent_builder_new, "new", 2, nullptr);
    lua_setfield(L, mt, "new");

    // apply() — def* as light userdata upvalue
    lua_pushlightuserdata(L, (void*)def);
    lua_pushcclosurek(L, fluent_builder_apply, "apply", 1, nullptr);
    lua_setfield(L, mt, "apply");

    lua_setreadonly(L, mt, true);

    // module_table[type_name] = metatable
    lua_setfield(L, module_idx, type_name);

    lua_setreadonly(L, module_idx, true);
    lua_setglobal(L, module_name);

    LUAU_ASSERT(lua_gettop(L) == top);
}
