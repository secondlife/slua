#define llprim_c

#include "lua.h"
#include "lcommon.h"
#include "lualib.h"
#include "llsl.h"
#include "llprim.h"

struct PrimParamsSetterMethod
{
    const char *name;
    const char *sem;
    int tag;
    // -1 means "no variant", this has no meaning for anything other than PRIM_TYPE_* rules
    int variant;
};

// Pull in the generated descriptors based on lsl_definitions.yaml
#include "llprim_set_primitive_params.inl"

// Shared wrapper for every ParamsSetter method.
// Uses upvalues to determine what to push and with what semantics
static int prim_params_rule_wrapper(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int tag = lua_tointeger(L, lua_upvalueindex(1));
    int variant = lua_tointeger(L, lua_upvalueindex(2));
    const char *sem = lua_tostring(L, lua_upvalueindex(3));

    int idx = lua_objlen(L, 1);

    lua_pushinteger(L, tag);
    lua_rawseti(L, 1, ++idx);
    if (variant >= 0)
    {
        lua_pushinteger(L, variant);
        lua_rawseti(L, 1, ++idx);
    }

    bool nullable = false;
    // skip `self`
    int argi = 2;

    // The "semantic" string is a string of characters dictating what to expect in the args
    for (const char *p = sem; *p; ++p)
    {
        char c = *p;
        if (c == '?')
        {
            // Just means that we accept `""` literal as a passthrough.
            nullable = true;
            continue;
        }

        // Nullable slots accept "" as the "clear" sentinel.
        // It's an LSL glTF API thing.
        if (nullable && lua_type(L, argi) == LUA_TSTRING)
        {
            size_t len;
            lua_tolstring(L, argi, &len);
            if (len == 0)
            {
                lua_pushvalue(L, argi);
                lua_rawseti(L, 1, ++idx);
                ++argi;
                continue;
            }
        }

        switch (c)
        {
        case 'i':
            luaL_checkinteger(L, argi);
            lua_pushvalue(L, argi);
            break;
        case 'f':
            luaL_checknumber(L, argi);
            lua_pushvalue(L, argi);
            break;
        case 's':
            if (lua_type(L, argi) != LUA_TSTRING)
                luaL_typeerrorL(L, argi, "string");
            lua_pushvalue(L, argi);
            break;
        case 'v':
            luaL_checkvector(L, argi);
            lua_pushvalue(L, argi);
            break;
        case 'r':
            luaSL_checkquaternion(L, argi);
            lua_pushvalue(L, argi);
            break;
        case 'b':
            if (lua_isboolean(L, argi))
                lua_pushinteger(L, lua_toboolean(L, argi));
            else
            {
                luaL_checkinteger(L, argi);
                lua_pushvalue(L, argi);
            }
            break;
        case 'a':
        {
            int t = lua_type(L, argi);
            if (t == LUA_TSTRING ||
                (t == LUA_TUSERDATA && lua_userdatatag(L, argi) == UTAG_UUID))
                lua_pushvalue(L, argi);
            else
                luaL_typeerrorL(L, argi, "string or uuid");
            break;
        }
        default:
            luaL_errorL(L, "internal: bad builder semantic char '%c'", c);
        }
        lua_rawseti(L, 1, ++idx);
        ++argi;
    }

    lua_pushvalue(L, 1);
    return 1;
}

// Convenience ctor, metatable lives in upvalue 1, so this skips a global lookup per call.
static int prim_params_setter_new(lua_State *L)
{
    lua_createtable(L, 0, 0);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L, -2);
    return 1;
}

// Inlined here since we won't necessarily have C++ definitions for LSL constants
static constexpr int LINK_THIS = -4;


// Apply the built-up rules with ll.SetLinkPrimitiveParamsFast()
static int prim_params_setter_apply(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int link = LINK_THIS;
    if (!lua_isnoneornil(L, 2))
        link = luaL_checkinteger(L, 2);

    lua_rawgetfield(L, LUA_BASEGLOBALSINDEX, "ll");
    lua_rawgetfield(L, -1, "SetLinkPrimitiveParamsFast");
    lua_pushinteger(L, link);
    lua_pushvalue(L, 1);  // should be `self`, the rule list.
    lua_call(L, 2, 0);
    return 0;
}

void luaSL_setup_llprim_module(lua_State *L)
{
    int top = lua_gettop(L);

    lua_newtable(L);
    lua_newtable(L);
    int mt = lua_gettop(L);

    for (const auto & m : SET_PRIMITIVE_PARAMS_METHODS)
    {
        lua_pushinteger(L, m.tag);
        lua_pushinteger(L, m.variant);
        lua_pushstring(L, m.sem);
        lua_pushcclosurek(L, prim_params_rule_wrapper, m.name, 3, nullptr);
        lua_setfield(L, mt, m.name);
    }

    lua_pushvalue(L, mt);
    lua_pushcclosurek(L, prim_params_setter_new, "new", 1, nullptr);
    lua_setfield(L, mt, "new");

    lua_pushcfunction(L, prim_params_setter_apply, "apply");
    lua_setfield(L, mt, "apply");

    lua_pushvalue(L, mt);
    lua_setfield(L, mt, "__index");

    lua_setreadonly(L, mt, true);
    lua_setfield(L, -2, "ParamsSetter");

    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "llprim");

    LUAU_ASSERT(lua_gettop(L) == top);
}
