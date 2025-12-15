#define llsl_c
#include "lua.h"
#include "lapi.h"
#include "lgc.h"
#include "ltable.h"
#include "lualib.h"

#include <cstring>
#include <cassert>
#include <cmath>
#include <unordered_set>

#include "lnumutils.h"
#include "llsl.h"
#include "mono_floats.h"
#include "Luau/Bytecode.h"
#include "lllevents.h"
#include "llltimers.h"

// This module is ONLY to be loaded into LSL scripts running under Luau,
// it is NOT for general use by Luau scripts. If you use it otherwise
// I will do the 300 well thing.

// Don't fuse multiply and add in this file, even if the optimization rules would
// normally allow it. It will result in material differences to results.
// Not supported in GCC, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=20785
// There, we rely on CMake to get this right.
#ifndef __GNUC__
#   pragma STDC FP_CONTRACT OFF
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1200 && _MSC_VER < 1800
#   include <float.h>
#   define isfinite _finite
#elif defined(__sun) && defined(__SVR4) //Solaris
#   include <ieeefp.h>
#   define isfinite finite
#else
#   define isfinite std::isfinite
#endif


constexpr size_t UUID_STR_LENGTH = 37;
constexpr int UUID_BYTES = 16;

static const char NULL_UUID[UUID_BYTES] = {0};

static void uuid_bytes_to_str(const char *uuid, char *str)
{
    snprintf(
        str,
        UUID_STR_LENGTH,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (uint8_t)uuid[0],
        (uint8_t)uuid[1],
        (uint8_t)uuid[2],
        (uint8_t)uuid[3],
        (uint8_t)uuid[4],
        (uint8_t)uuid[5],
        (uint8_t)uuid[6],
        (uint8_t)uuid[7],
        (uint8_t)uuid[8],
        (uint8_t)uuid[9],
        (uint8_t)uuid[10],
        (uint8_t)uuid[11],
        (uint8_t)uuid[12],
        (uint8_t)uuid[13],
        (uint8_t)uuid[14],
        (uint8_t)uuid[15]
    );
}

static int lsl_quaternion_ctor(lua_State *L)
{
    const auto x = luaL_checknumber(L, 1);
    const auto y = luaL_checknumber(L, 2);
    const auto z = luaL_checknumber(L, 3);
    const auto s = luaL_checknumber(L, 4);
    auto *quat = (float *)lua_newuserdatataggedwithmetatable(L, sizeof(float) * 4, UTAG_QUATERNION);
    quat[0] = (float)x;
    quat[1] = (float)y;
    quat[2] = (float)z;
    quat[3] = (float)s;
    return 1;
}

int luaSL_pushquaternion(lua_State *L, double x, double y, double z, double s)
{
    auto *quat = (float *)lua_newuserdatataggedwithmetatable(L, sizeof(float) * 4, UTAG_QUATERNION);
    quat[0] = (float)x;
    quat[1] = (float)y;
    quat[2] = (float)z;
    quat[3] = (float)s;
    return 1;
}

static int lsl_key_ctor(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TUSERDATA)
    {
        // If this is already a UUID just return the same UUID.
        bool compressed;
        luaSL_checkuuid(L, 1, &compressed);
        lua_pushvalue(L, 1);
        return 1;
    }
    size_t len;
    const char *data = luaL_checklstring(L, 1, &len);
    return luaSL_pushuuidlstring(L, data, len);
}

static bool parse_uuid_str(const char *in_string, size_t len, char *out, bool flexible);
static int push_uuid_common(lua_State *L, const char *str, size_t len, bool compressed);

// UUID constructor for Lua/SLua mode (strict + canonicalize)
static int lua_uuid_ctor(lua_State *L)
{
    auto arg_type = lua_type(L, 1);
    if (arg_type == LUA_TUSERDATA)
    {
        // If this is already a UUID just return the same UUID.
        bool compressed;
        luaSL_checkuuid(L, 1, &compressed);
        lua_pushvalue(L, 1);
        return 1;
    }
    else if (arg_type == LUA_TBUFFER)
    {
        size_t buf_len = 0;
        auto *data = lua_tobuffer(L, 1, &buf_len);
        if (data && buf_len >= (size_t)UUID_BYTES)
            return push_uuid_common(L, (const char*)data, UUID_BYTES, true);
        luaL_errorL(L, "Buffer too short to be UUID, only %d bytes", (int)buf_len);
    }

    size_t len;
    const char *data = luaL_checklstring(L, 1, &len);

    // Empty string → NULL_KEY
    if (len == 0)
    {
        return push_uuid_common(L, NULL_UUID, UUID_BYTES, true);
    }

    // Try flexible parsing (case-insensitive + broken format)
    char uuid_bytes[UUID_BYTES];
    if (parse_uuid_str(data, len, uuid_bytes, true))
    {
        // Valid UUID → compressed binary
        return push_uuid_common(L, uuid_bytes, UUID_BYTES, true);
    }
    else
    {
        // Invalid UUID → nil
        lua_pushnil(L);
        return 1;
    }
}

static std::string _float_to_str(float v, bool high_precision, bool neg_zero = true)
{
    return NumberFormatter::NumberToString(high_precision ? "F6" : "F5", v, neg_zero);
}

uint8_t lua_lsl_type(const TValue *val)
{
    switch(val->tt)
    {
    case LUA_TNUMBER:
        return (uint8_t)LSLIType::LST_FLOATINGPOINT;
    case LUA_TLIGHTUSERDATA:
    {
        if (val->extra[0] == LU_TAG_LSL_INTEGER)
            return (uint8_t)LSLIType::LST_INTEGER;
        return (uint8_t)LSLIType::LST_ERROR;
    }
    case LUA_TSTRING:
        return (uint8_t)LSLIType::LST_STRING;
    case LUA_TVECTOR:
        return (uint8_t)LSLIType::LST_VECTOR;
    case LUA_TUSERDATA:
    {
        uint8_t tag = uvalue(val)->tag;
        if (tag == UTAG_QUATERNION)
            return (uint8_t)LSLIType::LST_QUATERNION;
        else if (tag == UTAG_UUID)
            return (uint8_t)LSLIType::LST_KEY;
        else
            return (uint8_t)LSLIType::LST_ERROR;
    }
    case LUA_TTABLE:
        return (uint8_t)LSLIType::LST_LIST;
    case LUA_TNIL:
        return (uint8_t)LSLIType::LST_NULL;
    default:
        // No clue what this is, but it sure doesn't have a type in LSL.
        return (uint8_t)LSLIType::LST_ERROR;
    }
}

uint8_t luaSL_lsl_type(lua_State *L, int idx)
{
    luaL_checkany(L, idx);
    const TValue *val = luaA_toobject(L, idx);
    return lua_lsl_type(val);
}

static int _lsl_cast_internal(lua_State* L, bool in_list, bool neg_zero, bool nil_as_default)
{
    lua_checkstack(L, 4);

    luaL_checkany(L, 1);
    const TValue *val = luaA_toobject(L, 1);
    int to_type = luaL_checkinteger(L, 2);
    // Since we don't allow this to be called with arbitrary args, this must be valid.
    LUAU_ASSERT(to_type < (int)LSLIType::LST_ERROR);
    auto to_lsl_type = (LSLIType)to_type;
    auto existing_type = (LSLIType)luaSL_lsl_type(L, 1);

    if (existing_type == LSLIType::LST_ERROR)
    {
        // Hack to let bool -> int work, even though it's not a real LSL type
        auto lua_existing_type = lua_type(L, 1);
        if (lua_existing_type == LUA_TBOOLEAN && to_type == (int)LSLIType::LST_INTEGER)
        {
            luaSL_pushinteger(L, luaL_checkboolean(L, 1));
            return 1;
        }
        return 0;
    }

    if (to_lsl_type == existing_type)
    {
        // Self-cast, just return the object.
        lua_pushvalue(L, 1);
        return 1;
    }

    if (to_lsl_type == LSLIType::LST_LIST)
    {
        // Create a single-element table, with this value as the only entry
        lua_createtable(L, 1, 0);
        LuaTable *tab = hvalue(luaA_toobject(L, -1));
        setobj(L, &tab->array[0], luaA_toobject(L, 1));
        return 1;
    }

    TValue new_tv;
    switch(existing_type)
    {
        case LSLIType::LST_FLOATINGPOINT:
        {
            switch(to_lsl_type)
            {
                case LSLIType::LST_INTEGER:
                {
                    // If this seems weird, that's because it is. Mono truncates to 32-bit float
                    // before converting to integer. You'll note that we cast to int64_t first before
                    // truncating to int32_t, which also seems odd. We need that to emulate x86-64
                    // integer wraparound on AArch64, or `print(((integer)((float)0x7FffFFfe)))`
                    // will print 2147483647 rather than -2147483648. See
                    // https://stackoverflow.com/questions/66279679/casting-float-to-int-with-wrap-around-on-aarch64-arm64
                    float nval = (float)nvalue(val);
                    if (isfinite(nval))
                    {
                        setintvalue(&new_tv, (int32_t)((int64_t)(nval)));
                    }
                    else
                    {
                        // Mono treats non-finite values as INT32_MIN.
                        setintvalue(&new_tv, INT32_MIN);
                    }
                    break;
                }
                case LSLIType::LST_STRING:
                {
                    // Again, truncates to float first.
                    lua_pushstring(L, _float_to_str((float)nvalue(val), true, neg_zero).c_str());
                    return 1;
                }
                default:
                    return 0;
            }
            break;
        }
        case LSLIType::LST_INTEGER:
        {
            switch(to_lsl_type)
            {
                case LSLIType::LST_FLOATINGPOINT:
                    // Yes, this is double rather than float. This is intentional
                    // and matches Mono behavior.
                    setnvalue(&new_tv, (double)intvalue(val)); break;
                case LSLIType::LST_STRING:
                    // push instead of break, we can use the pushstring helper.
                    lua_pushstring(L, std::to_string(intvalue(val)).c_str());
                    return 1;
                default:
                    return 0;
            }
            break;
        }
        case LSLIType::LST_STRING:
        {
            switch(to_lsl_type)
            {
                case LSLIType::LST_INTEGER:
                {
                    // Default to decimal
                    int32_t base = 10;
                    auto *num_str = svalue(val);

                    // Check to see if this is a hexadecimal number.
                    // This logic is necessary to prevent octal from being used.
                    if( (num_str[0] == '0') &&
                        (num_str[1] == 'x' || num_str[1] == 'X') )
                    {
                        base = 16;
                    }
                    // Convert to string using the specified base, doing unsigned->signed conversion
                    int32_t result = (int32_t)strtoul(num_str, NULL, base);
                    setintvalue(&new_tv, result);
                    break;
                }
                case LSLIType::LST_FLOATINGPOINT:
                    // Intentionally truncate to 32-bit space. That's what Mono does.
                    setnvalue(&new_tv, (float)atof(svalue(val))); break;
                case LSLIType::LST_VECTOR:
                {
                    float vec[3];
                    char ignored;
                    char term;
                    // We have extra spaces Mono / LSO2 don't have to deal with
                    // https://feedback.secondlife.com/scripting-bugs/p/whitespace-unexpectedly-breaks-string-to-vector-string-to-rot-typecasts
                    int num;
                    if (nil_as_default)
                    {
                        // This is for SLua casts
                        // %c at the end so we can detect trailing junk.
                        num = sscanf(svalue(val), " < %f , %f , %f %c %c", &vec[0], &vec[1], &vec[2], &term, &ignored);
                        // Capturing the `>` in %c is the only way we can ensure it's present.
                        if (num == 4 && term == '>')
                            num = 3;
                    }
                    else
                    {
                        num = sscanf(svalue(val), "<%f, %f, %f>", &vec[0], &vec[1], &vec[2]);
                    }

                    if (num == 3)
                    {
                        setvvalue(&new_tv, vec[0], vec[1], vec[2], 0.0f);
                    }
                    else if(nil_as_default)
                    {
                        setnilvalue(&new_tv);
                    }
                    else
                    {
                        setvvalue(&new_tv, 0.0f, 0.0f, 0.0f, 0.0f);
                    }
                    break;
                }
                case LSLIType::LST_QUATERNION:
                {
                    float quat[4];
                    char ignored;
                    char term;
                    int num;
                    if (nil_as_default)
                    {
                        // %c at the end so we can detect trailing junk.
                        num = sscanf(svalue(val), " < %f , %f , %f , %f %c %c", &quat[0], &quat[1], &quat[2], &quat[3], &term, &ignored);
                        // Capturing the `>` in %c is the only way we can ensure it's present.
                        if (num == 5 && term == '>')
                            num = 4;
                    }
                    else
                    {
                        num = sscanf(svalue(val), "<%f, %f, %f, %f>", &quat[0], &quat[1], &quat[2], &quat[3]);
                    }

                    if (num != 4)
                    {
                        if (nil_as_default)
                        {
                            lua_pushnil(L);
                            return 1;
                        }
                        memset(quat, 0, sizeof(float) * 4);
                    }
                    luaSL_pushquaternion(L, quat[0], quat[1], quat[2], quat[3]);
                    return 1;
                }
                case LSLIType::LST_KEY:
                    luaSL_pushuuidlstring(L, (const char *)&val->value.gc->ts.data, val->value.gc->ts.len);
                    return 1;
                default:
                    return 0;
            }
            break;
        }
        case LSLIType::LST_VECTOR:
        {
            const float *v = vvalue(val);
            switch(to_lsl_type)
            {
                case LSLIType::LST_STRING:
                {
                    if (in_list)
                    {
                        lua_pushfstringL(L, "<%s, %s, %s>",
                            _float_to_str((float)v[0], true, neg_zero).c_str(),
                            _float_to_str((float)v[1], true, neg_zero).c_str(),
                            _float_to_str((float)v[2], true, neg_zero).c_str()
                        );
                    }
                    else
                    {
                        lua_pushfstringL(L, "<%s, %s, %s>",
                            _float_to_str((float)v[0], false).c_str(),
                            _float_to_str((float)v[1], false).c_str(),
                            _float_to_str((float)v[2], false).c_str()
                        );
                    }
                    break;
                }
                default:
                    return 0;
            }
            return 1;
        }
        case LSLIType::LST_QUATERNION:
        {
            auto* v = (const float*)(uvalue(val)->data);
            switch (to_lsl_type)
            {
                case LSLIType::LST_STRING:
                {
                    if (in_list)
                    {
                        lua_pushfstringL(L, "<%s, %s, %s, %s>",
                            _float_to_str((float)v[0], true, neg_zero).c_str(),
                            _float_to_str((float)v[1], true, neg_zero).c_str(),
                            _float_to_str((float)v[2], true, neg_zero).c_str(),
                            _float_to_str((float)v[3], true, neg_zero).c_str()
                        );
                    }
                    else
                    {
                        lua_pushfstringL(L, "<%s, %s, %s, %s>",
                            _float_to_str((float)v[0], false).c_str(),
                            _float_to_str((float)v[1], false).c_str(),
                            _float_to_str((float)v[2], false).c_str(),
                            _float_to_str((float)v[3], false).c_str()
                        );
                    }
                    break;
                }
                default:
                    return 0;
            }
            return 1;
        }
        case LSLIType::LST_KEY:
        {
            if (to_lsl_type != LSLIType::LST_STRING)
                return 0;

            auto* key = (lua_LSLUUID*)(uvalue(val)->data);
            if(key->compressed)
            {
                const char *uuid = getstr(key->str);
                char str[UUID_STR_LENGTH] = {};
                uuid_bytes_to_str(uuid, (char *)&str);
                lua_pushlstring(L, (char *)&str, UUID_STR_LENGTH - 1);
                return 1;
            }
            else
            {
                setsvalue(L, &new_tv, key->str);
            }
            break;
        }
        case LSLIType::LST_LIST:
        {
            if (to_lsl_type != LSLIType::LST_STRING)
                return 0;

            LuaTable *table_val = hvalue(val);
            const size_t len = luaH_getn(table_val);
            // Stupid hack, just string cast all elements and concat at the end.
            lua_pushstring(L, "");
            for(size_t idx = 0; idx<len; ++idx)
            {
                if (ttisstring(&table_val->array[idx]))
                {
                    // Don't need to cast strings, just push.
                    luaA_pushobject(L, &table_val->array[idx]);
                }
                else
                {
                    lua_pushcfunction(L, &lsl_cast_list_elem, "cast_list");
                    luaA_pushobject(L, &table_val->array[idx]);
                    luaSL_pushinteger(L, LSLIType::LST_STRING);
                    lua_call(L, 2, 1);
                }
                lua_concat(L, 2);

                // Matches Mono behavior
                if (lua_strlen(L, -1) > UINT16_MAX)
                {
                    luaD_throw(L, LUA_ERRMEM);
                }
            }
            return 1;
        }
        default:
        {
            return 0;
        }
    }
    luaA_pushobject(L, &new_tv);
    return 1;
}

int lsl_cast_list_elem(lua_State *L)
{
    return _lsl_cast_internal(L, true, true, false);
}

int lsl_cast_list_elem_poszero(lua_State *L)
{
    return _lsl_cast_internal(L, true, false, false);
}

int lsl_cast(lua_State *L)
{
    return _lsl_cast_internal(L, false, true, false);
}

static int lsl_must_cast(lua_State *L)
{
    if (lsl_cast(L) != 1)
    {
        luaL_errorL(L, "unable to cast!");
    }
    return 1;
}

static int lsl_must_cast_nil_default(lua_State *L)
{
    if (_lsl_cast_internal(L, false, true, true) != 1)
    {
        luaL_errorL(L, "unable to cast!");
    }
    return 1;
}

static LuaTable* _lsl_arg_to_table(lua_State *L, int argn)
{
    if (lua_type(L, argn) == LUA_TTABLE)
    {
        return hvalue(luaA_toobject(L, argn));
    }
    else
    {
        // make a new single-element table and place the argument in it
        lua_createtable(L, 1, 0);
        LuaTable *new_tab = hvalue(luaA_toobject(L, -1));
        setobj(L, &new_tab->array[0], luaA_toobject(L, argn));
        return new_tab;
    }
}

static int lsl_table_concat(lua_State *L)
{
    if (lua_type(L, 1) != LUA_TTABLE && lua_type(L, 2) != LUA_TTABLE)
        luaL_errorL(L, "At least one argument must be a table");

    lua_checkstack(L, 4);

    // Note: reversed because of LSL evaluation order!
    LuaTable *l_h = _lsl_arg_to_table(L, 2);
    LuaTable *r_h = _lsl_arg_to_table(L, 1);

    int l_len = luaH_getn(l_h);
    int new_len = l_len + luaH_getn(r_h);

    // Push immediately so this is correctly rooted
    lua_createtable(L, new_len, 0);
    LuaTable *new_h = hvalue(luaA_toobject(L, -1));

    for (int i = 0; i<new_len; ++i)
    {
        LuaTable* source_h = l_h;
        int effective_i = i;
        // Start reading from rhs if we passed the end of lhs
        if (i >= l_len)
        {
            effective_i -= l_len;
            source_h = r_h;
        }
        setobj(L, &new_h->array[i], &source_h->array[effective_i]);
    }
    // Move the object to the return value position, clobbering any temp lists.
    if (lua_gettop(L) != 3)
        lua_replace(L, 3);
    return 1;
}

static int lsl_change_state(lua_State *L)
{
    // Yield the integer up to the VM host
    luaL_checkinteger(L, 1);
    return lua_yield(L, 1);
}

static int lsl_replace_axis(lua_State *L)
{
    luaL_checkany(L, 1);

    size_t len;
    const char* name = luaL_checklstring(L, 2, &len);
    if (len != 1)
        luaL_error(L, "unknown field %s", name);
    int idx;
    switch(name[0])
    {
        case 'x': idx = 0; break;
        case 'y': idx = 1; break;
        case 'z': idx = 2; break;
        case 's': idx = 3; break;
        default: luaL_error(L, "unknown field %s", name);
    }

    double new_val = luaL_checknumber(L, 3);
    float p[4];
    int num_elems = 3;
    if (lua_type(L, 1) == LUA_TVECTOR)
    {
        // We need to copy this first, because pushing the numbers may result
        // in a stack realloc.
        memcpy(&p, lua_tovector(L, 1), sizeof(float) * 3);
    }
    else
    {
        num_elems = 4;
        auto *udata = (const float *)lua_touserdatatagged(L, 1, UTAG_QUATERNION);
        if (udata == nullptr)
            luaL_typeerror(L, 1, "quaternion/vector");
        memcpy(&p, udata, sizeof(float) * 4);
    }

    // Replace the value we want to replace
    p[idx] = new_val;

    // Push the new coordinate
    if (num_elems == 3)
    {
        lua_pushvector(L, p[0], p[1], p[2]);
    }
    else
    {
        luaSL_pushquaternion(L, p[0], p[1], p[2], p[3]);
    }
    return 1;
}

static void mul_quat(const float *a, const float *b, float *res)
{
    res[0] = b[3] * a[0] + b[0] * a[3] + b[1] * a[2] - b[2] * a[1];
    res[1] = b[3] * a[1] + b[1] * a[3] + b[2] * a[0] - b[0] * a[2];
    res[2] = b[3] * a[2] + b[2] * a[3] + b[0] * a[1] - b[1] * a[0];
    res[3] = b[3] * a[3] - b[0] * a[0] - b[1] * a[1] - b[2] * a[2];
}

void rot_vec(const float *vec, const float *rot, float *res)
{
    // This is cribbed from ScriptTypes.cs in indra, rather than `llmath`.
    res[0] =
        rot[3] * rot[3] * vec[0] +
        2.0f * rot[1] * rot[3] * vec[2] -
        2.0f * rot[2] * rot[3] * vec[1] +
        rot[0] * rot[0] * vec[0] +
        2.0f * rot[1] * rot[0] * vec[1] +
        2.0f * rot[2] * rot[0] * vec[2] -
        rot[2] * rot[2] * vec[0] -
        rot[1] * rot[1] * vec[0];

    res[1] =
        2.0f * rot[0] * rot[1] * vec[0] +
        rot[1] * rot[1] * vec[1] +
        2.0f * rot[2] * rot[1] * vec[2] +
        2.0f * rot[3] * rot[2] * vec[0] -
        rot[2] * rot[2] * vec[1] +
        rot[3] * rot[3] * vec[1] -
        2.0f * rot[0] * rot[3] * vec[2] -
        rot[0] * rot[0] * vec[1];

    res[2] =
        2.0f * rot[0] * rot[2] * vec[0] +
        2.0f * rot[1] * rot[2] * vec[1] +
        rot[2] * rot[2] * vec[2] -
        2.0f * rot[3] * rot[1] * vec[0] -
        rot[1] * rot[1] * vec[2] +
        2.0f * rot[3] * rot[0] * vec[1] -
        rot[0] * rot[0] * vec[2] +
        rot[3] * rot[3] * vec[2];
}

void conj_quat(float *quat)
{
    quat[0] *= -1.0f;
    quat[1] *= -1.0f;
    quat[2] *= -1.0f;
}

void copy_quat(float *dest, const float *src)
{
    memcpy(dest, src, sizeof(float) * 4);
}

static bool validate_uuid_str(const char *in_string, size_t len)
{
    // This is cribbed from indra's LLUUID parser
    bool broken_format = false;
    if (len != (UUID_STR_LENGTH - 1))        /* Flawfinder: ignore */
    {
        // Note: this is a comment from the SL server codebase!!! Oops, guess we have to support
        //   this crap forever and ever now.
        // I'm a moron.  First implementation didn't have the right UUID format.
        if (len == (UUID_STR_LENGTH - 2))        /* Flawfinder: ignore */
        {
            broken_format = true;
        }
        else
        {
            return false;
        }
    }

    bool all_zero = true;

    uint8_t cur_pos = 0;
    for (uint32_t i = 0; i < UUID_BYTES; i++)
    {
        if ((i == 4) || (i == 6) || (i == 8) || (i == 10))
        {
            cur_pos++;
            if (broken_format && (i==10))
            {
                // Missing - in the broken format
                cur_pos--;
            }
        }

        if ((in_string[cur_pos] >= '0') && (in_string[cur_pos] <= '9'))
        {
        }
        else if ((in_string[cur_pos] >= 'a') && (in_string[cur_pos] <='f'))
        {
        }
        else if ((in_string[cur_pos] >= 'A') && (in_string[cur_pos] <='F'))
        {
        }
        else
        {
            return false;
        }
        if (all_zero && in_string[cur_pos] != '0')
            all_zero = false;
        cur_pos++;

        if ((in_string[cur_pos] >= '0') && (in_string[cur_pos] <= '9'))
        {
        }
        else if ((in_string[cur_pos] >= 'a') && (in_string[cur_pos] <='f'))
        {
        }
        else if ((in_string[cur_pos] >= 'A') && (in_string[cur_pos] <='F'))
        {
        }
        else
        {
            return false;
        }
        if (all_zero && in_string[cur_pos] != '0')
            all_zero = false;
        cur_pos++;
    }
    return !all_zero;
}

static bool parse_uuid_str(const char *in_string, size_t len, char *out, bool flexible)
{
    // This is cribbed from indra's UUID parser,
    // except it will return false for broken_format UUIDs and empty
    // strings aren't treated as the null UUID, unless flexible mode is enabled.
    // When flexible=true: accepts uppercase (converts to lowercase), accepts broken format (35 chars)

    bool broken_format = false;
    if (len != (UUID_STR_LENGTH - 1))        /* Flawfinder: ignore */
    {
        // When flexible, allow the broken 35-character format (missing one hyphen)
        if (flexible && len == (UUID_STR_LENGTH - 2))        /* Flawfinder: ignore */
        {
            broken_format = true;
        }
        else
        {
            return false;
        }
    }

    uint8_t cur_pos = 0;
    for (int i = 0; i < UUID_BYTES; i++)
    {
        if ((i == 4) || (i == 6) || (i == 8) || (i == 10))
        {
            // skip by dashes
            cur_pos++;
            if (broken_format && (i == 10))
            {
                // Missing dash in the broken format
                cur_pos--;
            }
        }

        out[i] = 0;

        if ((in_string[cur_pos] >= '0') && (in_string[cur_pos] <= '9'))
        {
            out[i] |= (uint8_t)(in_string[cur_pos] - '0');
        }
        else if ((in_string[cur_pos] >= 'a') && (in_string[cur_pos] <='f'))
        {
            out[i] |= (uint8_t)(10 + in_string[cur_pos] - 'a');
        }
        else if ((in_string[cur_pos] >= 'A') && (in_string[cur_pos] <='F'))
        {
            if (flexible)
            {
                // In flexible mode, convert uppercase to lowercase
                out[i] |= (uint8_t)(10 + in_string[cur_pos] - 'A');
            }
            else
            {
                // We consider capitals to be non-canonical, we don't want to silently
                // convert the string to lowercase
                return false;
            }
        }
        else
        {
            return false;
        }

        out[i] <<= 4;
        cur_pos++;

        if ((in_string[cur_pos] >= '0') && (in_string[cur_pos] <= '9'))
        {
            out[i] |= (uint8_t)(in_string[cur_pos] - '0');
        }
        else if ((in_string[cur_pos] >= 'a') && (in_string[cur_pos] <='f'))
        {
            out[i] |= (uint8_t)(10 + in_string[cur_pos] - 'a');
        }
        else if ((in_string[cur_pos] >= 'A') && (in_string[cur_pos] <='F'))
        {
            if (flexible)
            {
                // In flexible mode, convert uppercase to lowercase
                out[i] |= (uint8_t)(10 + in_string[cur_pos] - 'A');
            }
            else
            {
                // see above note about capital letters
                return false;
            }
        }
        else
        {
            return false;
        }
        cur_pos++;
    }

    return true;
}

// Copied from LLUUID::validate()
static int lsl_is_key_truthy(lua_State *L)
{
    auto* a = (lua_LSLUUID *)lua_touserdatatagged(L, 1, UTAG_UUID);
    if (a == nullptr)
        luaL_typeerror(L, 1, "uuid");
    if (a->compressed)
    {
        // Compressed UUIDs with binary data are easy
        lua_pushboolean(L, memcmp(&NULL_UUID, getstr(a->str), UUID_BYTES) != 0);
        return 1;
    }

    size_t len = a->str->len;
    const char *in_string = (const char *)&a->str->data;
    lua_pushboolean(L, validate_uuid_str(in_string, len));
    return 1;
}

static int lsl_mul_quat(lua_State *L)
{
    auto* a = (const float *)lua_touserdatatagged(L, 1, UTAG_QUATERNION);
    if (a == nullptr)
        luaL_typeerror(L, 1, "quaternion");

    if (auto* b = (const float *)lua_touserdatatagged(L, 2, UTAG_QUATERNION))
    {
        // I checked the SL codebase and I don't believe LLQuaternion normalizes
        // here, so we should be correct to not normalize either.
        float res[4] = {0.0f};
        mul_quat(a, b, res);
        luaSL_pushquaternion(L, res[0], res[1], res[2], res[3]);
    }
    else
        luaL_typeerrorL(L, 2, "quaternion");
    return 1;
}

static int lsl_index_quat(lua_State *L)
{
    const float* p = (const float *)lua_touserdatatagged(L, 1, UTAG_QUATERNION);
    if (!p)
        luaL_typeerror(L, 1, "quaternion");

    size_t len;
    const char* name = luaL_checklstring(L, 2, &len);
    if (len != 1)
        luaL_error(L, "unknown field %s", name);

    size_t idx;
    switch(name[0])
    {
        case 'x': idx = 0; break;
        case 'y': idx = 1; break;
        case 'z': idx = 2; break;
        case 's': idx = 3; break;
        default: luaL_error(L, "unknown field %s", name);
    }
    lua_pushnumber(L, p[idx]);
    return 1;
}

static int lsl_div_quat(lua_State *L)
{
    auto* a = (const float *)lua_touserdatatagged(L, 1, UTAG_QUATERNION);
    if (a == nullptr)
        luaL_typeerror(L, 1, "quaternion");

    if (auto* b = (const float *)lua_touserdatatagged(L, 2, UTAG_QUATERNION))
    {
        float conj_b[4];
        copy_quat(conj_b, b);
        conj_quat(conj_b);

        float res[4] = {0.0f};
        mul_quat(a, conj_b, res);
        luaSL_pushquaternion(L, res[0], res[1], res[2], res[3]);
    }
    else
        luaL_typeerrorL(L, 2, "quaternion");
    return 1;
}

static int lsl_sub_quat(lua_State *L)
{
    auto* a = (const float *)lua_touserdatatagged(L, 1, UTAG_QUATERNION);
    if (a == nullptr)
        luaL_typeerror(L, 1, "quaternion");

    if (auto* b = (const float *)lua_touserdatatagged(L, 2, UTAG_QUATERNION))
    {
        luaSL_pushquaternion(L, a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]);
    }
    else
    {
        luaL_typeerrorL(L, 2, "quaternion");
    }
    return 1;
}

static int lsl_add_quat(lua_State *L)
{
    auto* a = (const float *)lua_touserdatatagged(L, 1, UTAG_QUATERNION);
    if (a == nullptr)
        luaL_typeerror(L, 1, "quaternion");

    if (auto* b = (const float *)lua_touserdatatagged(L, 2, UTAG_QUATERNION))
    {
        luaSL_pushquaternion(L, a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]);
    }
    else
    {
        luaL_typeerrorL(L, 2, "quaternion");
    }
    return 1;
}

static int lsl_eq_quat(lua_State *L)
{
    auto* a = (const float *)lua_touserdatatagged(L, 1, UTAG_QUATERNION);
    if (a == nullptr)
        luaL_typeerror(L, 1, "quaternion");

    if (auto* b = (const float *)lua_touserdatatagged(L, 2, UTAG_QUATERNION))
    {
        // Just a bare memcpy() would not be correct here because it would consider -0.0
        // unequal to 0.0, and NaNs would be equal.
        // At least I think. Need to double-check NaN semantics in Mono when in Vector
        // or Quaternion
        lua_pushboolean(L, a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]);
    }
    else
    {
        luaL_typeerrorL(L, 2, "quaternion");
    }
    return 1;
}

static int lsl_unm_quat(lua_State *L)
{
    auto* a = (const float *)lua_touserdatatagged(L, 1, UTAG_QUATERNION);
    if (a == nullptr)
        luaL_typeerror(L, 1, "quaternion");

    luaSL_pushquaternion(L, -a[0], -a[1], -a[2], -a[3]);
    return 1;
}

static int lsl_tostring_quat(lua_State *L)
{
    auto* a = (const float *)lua_touserdatatagged(L, 1, UTAG_QUATERNION);
    if (a == nullptr)
        luaL_typeerror(L, 1, "quaternion");

    char buf[128] = {};
    char* p = buf;
    *p++ = '<';

    const char* format = LUAU_IS_LSL_VM(L) ? "%5.5f" : "%.6g";
    for (int i = 0; i < 4; i++)
    {
        if (i > 0)
        {
            *p++ = ',';
            *p++ = ' ';
        }
        p += luai_formatfloat(p, buf + sizeof(buf) - p, format, a[i]);
    }

    *p++ = '>';

    lua_pushlstring(L, buf, p - buf);
    return 1;
}

static int lsl_tostring_uuid(lua_State *L)
{
    auto* a = (lua_LSLUUID *)lua_touserdatatagged(L, 1, UTAG_UUID);
    if (a == nullptr)
        luaL_typeerror(L, 1, "uuid");

    if (a->compressed)
    {
        LUAU_ASSERT(a->str->len == UUID_BYTES);
        const char *uuid = a->str->data;
        char str[UUID_STR_LENGTH] = {0};
        uuid_bytes_to_str(uuid, (char*)&str);
        lua_pushlstring(L, (char *)&str, UUID_STR_LENGTH - 1);
    }
    else
    {
        TValue tv;
        setsvalue(L, &tv, a->str);
        luaA_pushobject(L, &tv);
    }
    return 1;
}

static int lsl_index_uuid(lua_State *L)
{
    const lua_LSLUUID *p = (const lua_LSLUUID *)lua_touserdatatagged(L, 1, UTAG_UUID);
    if (!p)
        luaL_typeerror(L, 1, "uuid");

    const char* name = luaL_checkstring(L, 2);

    if (!strcmp(name, "istruthy"))
    {
        return lsl_is_key_truthy(L);
    }
    else if (!strcmp(name, "bytes"))
    {
        lua_TValue tv {};
        if (p->compressed)
        {
            setsvalue(L, &tv, p->str);
        }
        else
        {
            setnilvalue(&tv);
        }
        luaA_pushobject(L, &tv);
        return 1;
    }
    else
    {
        luaL_error(L, "unknown field %s", name);
    }
}

static int push_uuid_common(lua_State *L, const char *str, size_t len, bool compressed)
{
    lua_SLRuntimeState *runtime_state = LUAU_GET_SL_VM_STATE(L);
    LUAU_ASSERT(runtime_state != nullptr);

    // Make sure we have enough room on the stack for the things we're going to do
    lua_checkstack(L, 7);

    int top = lua_gettop(L);

    int typ;
    if (compressed)
        typ = lua_getref(L, runtime_state->uuidCompressedWeakTab);
    else
        typ = lua_getref(L, runtime_state->uuidWeakTab);
    LUAU_ASSERT(typ != LUA_TNONE);

    /* ... weak_table */

    // push the string and duplicate it in case we need to construct an instance from scratch
    lua_pushlstring(L, str, len);
    lua_pushvalue(L, -1);

    /* ... weak_table uuid_str uuid_str */
    if (lua_rawget(L, -3) != LUA_TNIL)
    {
        /* ... weak_table uuid_str uuid_val */

        // Okay, we're able to use an interned UUID, but make sure we call the
        // alloc hook to make sure the memory manager knows that we have a new
        // reference to an interned object that might not have appeared in
        // the script's reference graph before
        auto *uuid_tval = luaA_toobject(L, -1);
        auto *g = L->global;
        if (LUAU_LIKELY(!!g->cb.beforeallocate) && L->activememcat > 1 && gcvalue(uuid_tval)->gch.memcat > 1)
        {
            // See `lgctraverse.cpp` for reasoning behind this
            const size_t uuid_size = 4;
            if (LUAU_LIKELY(g->GCthreshold != SIZE_MAX) && g->cb.beforeallocate(L, 0, uuid_size))
                luaD_throw(L, LUA_ERRMEM);
        }

        lua_replace(L, -3);
        /* ... uuid_val uuid_str */
        lua_pop(L, 1);
        LUAU_ASSERT(top + 1 == lua_gettop(L));
        return 1;
    }

    /* ... weak_table uuid_str nil */
    lua_pop(L, 1);

    // Allocate a userdata with just enough space for the UUID str
    auto *uuid = (lua_LSLUUID *)lua_newuserdatataggedwithmetatable(L,
        sizeof(lua_LSLUUID), UTAG_UUID);

    /* ... weak_table uuid_str uuid_val */

    // The UUID just holds the TString instance for the underlying data,
    // as well as a marker saying whether it's compressed
    uuid->compressed = compressed;

    uuid->str = &luaA_toobject(L, -2)->value.gc->ts;

    // Push the string value
    lua_pushvalue(L, -2);
    /* ... weak_table uuid_str uuid_val uuid_str */
    // Push the uuid userdata
    lua_pushvalue(L, -2);
    /* ... weak_table uuid_str uuid_val uuid_str uuid_val */

    // Set Str -> UUID in the weak table
    lua_settable(L, -5);
    /* ... weak_table uuid_str uuid_val */
    lua_replace(L, -3);
    /* ... uuid_val uuid_str */

    // Pop the string
    lua_pop(L, 1);
    /* ... uuid_val */
    LUAU_ASSERT(top + 1 == lua_gettop(L));
    return 1;
}

int luaSL_pushuuidbytes(lua_State *L, const uint8_t *bytes)
{
    return push_uuid_common(L, (char *)bytes, UUID_BYTES, true);
}

int luaSL_pushuuidlstring(lua_State *L, const char *str, size_t len)
{
    char uuid_bytes[UUID_BYTES] = {0};
    // Try to push this as a compact UUID if it is one.
    if (parse_uuid_str(str, len, (char *)&uuid_bytes, false))
    {
        return push_uuid_common(L, (char *)&uuid_bytes, UUID_BYTES, true);
    }
    else
    {
        return push_uuid_common(L, str, len, false);
    }
}

int luaSL_pushuuidstring(lua_State *L, const char *str)
{
    return luaSL_pushuuidlstring(L, str, strlen(str));
}

static int lsl_to_integer(lua_State *L)
{
    luaL_checkany(L, 1);
    lua_settop(L, 1);
    lua_pushunsigned(L, (unsigned int)LSLIType::LST_INTEGER);
    return lsl_must_cast(L);
}

static int lsl_to_vector(lua_State *L)
{
    luaL_checkany(L, 1);
    lua_settop(L, 1);
    lua_pushunsigned(L, (unsigned int)LSLIType::LST_VECTOR);
    return lsl_must_cast_nil_default(L);
}

static int lsl_to_quaternion(lua_State *L)
{
    luaL_checkany(L, 1);
    lua_settop(L, 1);
    lua_pushunsigned(L, (unsigned int)LSLIType::LST_QUATERNION);
    return lsl_must_cast_nil_default(L);
}

const char *luaSL_checkuuid(lua_State *L, int num_arg, bool *compressed)
{
    int type = luaSL_lsl_type(L, num_arg);
    switch(type)
    {
        case (int)LSLIType::LST_KEY:
        {
            lua_LSLUUID* key = (lua_LSLUUID*)lua_touserdatatagged(L, num_arg, UTAG_UUID);
            *compressed = key->compressed;
            return getstr(key->str);
        }
        case (int)LSLIType::LST_STRING:
            // Note that the UUID _may_ actually be a valid UUID, we're just not sure
            // since the object is actually a string. It's up to the consumer to check!
            *compressed = false;
            return svalue(luaA_toobject(L, num_arg));
        default:
            luaL_typeerror(L, num_arg, "uuid");
    }
}

const float *luaSL_checkquaternion(lua_State *L, int num_arg)
{
    int type = luaSL_lsl_type(L, num_arg);
    if (type != (int)LSLIType::LST_QUATERNION)
        luaL_typeerror(L, num_arg, "quaternion");
    return (const float *)&uvalue(luaA_toobject(L, num_arg))->data;
}

YieldableStatus luaSL_may_interrupt(lua_State *L)
{
    // Can't yield, inside more than one C call.
    if (L->nCcalls > L->baseCcalls)
        return YieldableStatus::BAD_NCALLS;
    if (L->ci == nullptr)
        return YieldableStatus::BAD_CI;
    // only care if we're currently executing a lua function
    if(!isLua(L->ci))
    {
        // We don't need to check the whole CI stack because we assume that
        // if the `nCcalls` check succeeds we're okay to yield.
        return YieldableStatus::OK;
    }

    auto *cl = clvalue(L->ci->func);

    // PC will have been incremented just before we interrupted, set it back to where
    // it was.
    auto real_pc = L->ci->savedpc;
    const auto *proto = cl->l.p;
    --real_pc;

    // Don't try to inject a yield if we're outside the code!
    auto upper_pc_bound = proto->code + proto->sizecode;
    bool out_of_bounds = (real_pc >= upper_pc_bound || real_pc < proto->code);
    LUAU_ASSERT(!out_of_bounds);
    if (out_of_bounds)
        return YieldableStatus::INVALID_PC;

    // This should always be true, we're in an interrupt check!
    bool is_instr_preemptible = luau_is_preemptible(LUAU_INSN_OP(*real_pc));
    if(!is_instr_preemptible)
    {
        if (L->global->calltailinterruptcheck)
        {
            // We may need to go back _two_ in this case, pc will be past the LOP_CALL.
            // If we're this far down the checks, we already know that we're not
            // inside a C function that would have set this flag, so if we're seeing
            // this flag we must be just at the tail of the LOP_CALL handler.
            // Fix up real_pc so it points at the actual LOP_CALL.
            --real_pc;
            out_of_bounds = (real_pc >= upper_pc_bound || real_pc < proto->code);
            if (!out_of_bounds && LUAU_INSN_OP(*real_pc) == LOP_CALL)
                return YieldableStatus::OK;
        }
        LUAU_ASSERT(!"Can't preempt instruction");
        // Don't want to yield here, bail out.
        return YieldableStatus::UNSUPPORTED_INSTR;
    }
    return YieldableStatus::OK;
}

int luaSL_pushnativeinteger(lua_State *L, int val)
{
    if (LUAU_IS_LSL_VM(L))
    {
        luaSL_pushinteger(L, val);
    }
    else
    {
        lua_pushinteger(L, val);
    }
    return 1;
}

int luaSL_ismethodstyle(lua_State *L, int idx)
{
    if (lua_type(L, idx) != LUA_TFUNCTION)
        return 0;

    Closure *cl = clvalue(luaA_toobject(L, idx));
    if (cl->isC)
        return 0;  // C functions don't have proto

    return ((cl->l.p->flags & LPF_METHOD_STYLE)) == LPF_METHOD_STYLE;
}

void luaSL_pushindexlike(lua_State *L, int index)
{
    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));
    if (!compat_mode)
    {
        if (index == -1)
        {
            // "-1" == "not found" == nil in Lua semantics
            lua_pushnil(L);
            return;
        }
        else if (index >= 0)
            ++index;
    }

    luaSL_pushnativeinteger(L, index);
}

int luaSL_checkindexlike(lua_State *L, int index)
{
    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));
    int val = luaL_checkinteger(L, index);
    if (!compat_mode)
    {
        if (val == 0)
        {
            luaL_error(L, "passed 0 when a 1-based index was expected");
        }
        else if (val >= 1)
            --val;
    }

    return val;
}

void luaSL_pushboollike(lua_State *L, int val)
{
    bool compat_mode = lua_toboolean(L, lua_upvalueindex(1));
    if (!compat_mode)
    {
        // We want to push _real_ booleans if we're not in compat mode.
        lua_pushboolean(L, val == 0);
    }

    luaSL_pushnativeinteger(L, val);
}

static const luaL_Reg lsllib[] = {
    {"cast", lsl_must_cast},
    {"table_concat", lsl_table_concat},
    {"is_key_truthy", lsl_is_key_truthy},
    {"replace_axis", lsl_replace_axis},
    {"change_state", lsl_change_state},
    {NULL, NULL},
};


int luaopen_lsl(lua_State* L)
{
    if (!LUAU_IS_LSL_VM(L))
    {
        luaL_errorL(L, "Tried to open lsl module in non-LSL VM");
    }
    luaL_register(L, LUA_LSLLIBNAME, lsllib);
    return 1;
}

static void make_weak_uuid_table(lua_State *L)
{
    // create a metatable that makes the table have weak values
    lua_newtable(L);
    lua_pushstring(L, "v");
    lua_setfield(L, -2, "__mode");
    // make the metatable for the weak UUID table readonly
    lua_setreadonly(L, -1, true);
    lua_setmetatable(L, -2);
}

// ServerLua: callable quaternion module
static int quaternion_call(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_remove(L, 1);
    return lsl_quaternion_ctor(L);
}

static inline float quaternion_dot(const float* a, const float* b) {
    return ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2] + (a)[3] * (b)[3]);
}

static int lua_quaternion_normalize(lua_State *L)
{
    const float* quat = luaSL_checkquaternion(L, 1);
    float invNorm = 1.0f / sqrtf(quaternion_dot(quat, quat));
    luaSL_pushquaternion(L, quat[0] * invNorm, quat[1] * invNorm, quat[2] * invNorm, quat[3] * invNorm);
    return 1;
}

static int lua_quaternion_magnitude(lua_State *L)
{
    const float* quat = luaSL_checkquaternion(L, 1);
    lua_pushnumber(L, sqrtf(quaternion_dot(quat, quat)));
    return 1;
}


static int lua_quaternion_dot(lua_State *L)
{
    const float* a = luaSL_checkquaternion(L, 1);
    const float* b = luaSL_checkquaternion(L, 2);
    lua_pushnumber(L, quaternion_dot(a, b));
    return 1;
}

static int lua_quaternion_slerp(lua_State *L)
{
    const float* a = luaSL_checkquaternion(L, 1);
    const float* b = luaSL_checkquaternion(L, 2);
    const float u = luaL_checknumber(L, 3);

    float b_to[4] = {b[0], b[1], b[2], b[3]};
    float cos_t = quaternion_dot(a, b_to);

    bool bflip = false;
    if (cos_t < 0.0f)
    {
        cos_t = -cos_t;
        bflip = true;
    }

    float alpha;
    float beta;
    if(1.0f - cos_t < 0.00001f)
    {
        beta = 1.0f - u;
        alpha = u;
    }
    else
    {
        float theta = acosf(cos_t);
        float sin_t = sinf(theta);
        beta = sinf(theta - u*theta) / sin_t;
        alpha = sinf(u*theta) / sin_t;
    }

    if (bflip)
    {
        beta = -beta;
    }

    luaSL_pushquaternion(L,
        beta*a[0] + alpha*b[0],
        beta*a[1] + alpha*b[1],
        beta*a[2] + alpha*b[2],
        beta*a[3] + alpha*b[3]);
    return 1;
}

static int lua_quaternion_conjugate(lua_State *L)
{
    const float* quat = luaSL_checkquaternion(L, 1);
    luaSL_pushquaternion(L, -quat[0], -quat[1], -quat[2], quat[3]);
    return 1;
}


static inline void push_rotated_vector(lua_State *L, const float* vec) {
    const float* quat = luaSL_checkquaternion(L, 1);
    float res[3] = {0.0f};
    rot_vec(vec, quat, res);
    float invSqrt = 1.0f / sqrtf(res[0] * res[0] + res[1] * res[1] + res[2] * res[2]);
    lua_pushvector(L, res[0] * invSqrt, res[1] * invSqrt, res[2] * invSqrt);
}

static int lua_quaternion_tofwd(lua_State *L)
{
    const float vec[3] = {1.0f, 0.0f, 0.0f};
    push_rotated_vector(L, vec);
    return 1;
}

static int lua_quaternion_toleft(lua_State *L)
{
    const float vec[3] = {0.0f, 1.0f, 0.0f};
    push_rotated_vector(L, vec);
    return 1;
}

static int lua_quaternion_toup(lua_State *L)
{
    const float vec[3] = {0.0f, 0.0f, 1.0f};
    push_rotated_vector(L, vec);
    return 1;
}

static const luaL_Reg quaternionlib[] = {
    {"create", lsl_quaternion_ctor},
    {"normalize", lua_quaternion_normalize},
    {"magnitude", lua_quaternion_magnitude},
    {"dot", lua_quaternion_dot},
    {"slerp", lua_quaternion_slerp},
    {"conjugate", lua_quaternion_conjugate},
    {"tofwd", lua_quaternion_tofwd},
    {"toleft", lua_quaternion_toleft},
    {"toup", lua_quaternion_toup},
    {NULL, NULL},
};

int luaopen_sl_quaternion(lua_State* L, const char* name)
{
    [[maybe_unused]] int old_top = lua_gettop(L);
    lua_newtable(L);
    luaL_register(L, NULL, quaternionlib);

    luaSL_pushquaternion(L, 0.0, 0.0, 0.0, 1.0);
    lua_setfield(L, -2, "identity");

    // ServerLua: `quaternion()` is an alias to `quaternion.create()`, so we need to add a metatable
    //  to the quaternion module which allows calling it.
    lua_newtable(L);
    lua_pushcfunction(L, quaternion_call, "__call");
    lua_setfield(L, -2, "__call");

    // We need to override __iter so generalized iteration doesn't try to use __call.
    lua_rawgetfield(L, LUA_BASEGLOBALSINDEX, "pairs");
    // This is confusing at first, but we want a unique function identity
    // when this shows up anywhere other than globals, otherwise we can
    // muck up Ares serialization.
    luau_dupcclosure(L, -1, "__iter");
    lua_replace(L, -2);
    lua_rawsetfield(L, -2, "__iter");

    lua_setreadonly(L, -1, true);
    lua_setmetatable(L, -2);

    lua_setglobal(L, name);

    LUAU_ASSERT(lua_gettop(L) == old_top);
    return 1;
}

// ServerLua: callable uuid module
static int uuid_call(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_remove(L, 1);
    return lua_uuid_ctor(L);
}

static const luaL_Reg uuidlib[] = {
    {"create", lua_uuid_ctor},
    {NULL, NULL},
};

int luaopen_sl_uuid(lua_State* L)
{
    [[maybe_unused]] int old_top = lua_gettop(L);
    lua_newtable(L);
    luaL_register(L, NULL, uuidlib);

    // ServerLua: `uuid()` is an alias to `uuid.create()`, so we need to add a metatable
    //  to the uuid module which allows calling it.
    lua_newtable(L);
    lua_pushcfunction(L, uuid_call, "__call");
    lua_setfield(L, -2, "__call");

    // We need to override __iter so generalized iteration doesn't try to use __call.
    lua_rawgetfield(L, LUA_BASEGLOBALSINDEX, "pairs");
    // This is confusing at first, but we want a unique function identity
    // when this shows up anywhere other than globals, otherwise we can
    // muck up Ares serialization.
    luau_dupcclosure(L, -1, "__iter");
    lua_replace(L, -2);
    lua_rawsetfield(L, -2, "__iter");

    lua_setreadonly(L, -1, true);
    lua_setmetatable(L, -2);

    lua_setglobal(L, "uuid");

    LUAU_ASSERT(lua_gettop(L) == old_top);
    return 1;
}

int luaopen_sl(lua_State* L, int expose_internal_funcs)
{
    if (!LUAU_IS_SL_VM(L))
    {
        luaL_errorL(L, "Tried to open sl module in non-SL VM");
    }

    int top = lua_gettop(L);

    // Load these into the global namespace

    if (LUAU_IS_LSL_VM(L))
    {
        lua_pushcfunction(L, lsl_key_ctor, "uuid");
        luau_dupcclosure(L, -1, "touuid");
        lua_setglobal(L, "touuid");
        lua_setglobal(L, "uuid");
    }

    lua_pushcfunction(L, lsl_to_vector, "tovector");
    lua_setglobal(L, "tovector");

    lua_pushcfunction(L, lsl_to_quaternion, "toquaternion");
    luau_dupcclosure(L, -1, "torotation");
    lua_setglobal(L, "torotation");
    lua_setglobal(L, "toquaternion");

    if (LUAU_IS_LSL_VM(L))
    {
        lua_pushcfunction(L, lsl_to_integer, "integer");
        lua_setglobal(L, "integer");
    }

    lua_SLRuntimeState *lsl_runtime_state = LUAU_GET_SL_VM_STATE(L);

    ///////
    /// Keys
    ///////

    // create metatable with all the metamethods
    lua_newtable(L);
    lua_pushvalue(L, -1);
    // pin it to the refs
    lua_ref(L, -1);
    lua_setuserdatametatable(L, UTAG_UUID);

    lua_pushcfunction(L, lsl_tostring_uuid, "__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, lsl_index_uuid, "__index");
    lua_setfield(L, -2, "__index");

    // Give it a proper name for `typeof()`
    lua_pushstring(L, "uuid");
    lua_setfield(L, -2, "__type");

    // Please don't mess with our metatable!
    lua_setreadonly(L, -1, true);
    // Okay, don't need this on the stack anymore
    lua_pop(L, 1);
    LUAU_ASSERT(lua_gettop(L) == top);

    // Create a table for weak references to key instances, keeps their associated TStrings alive.
    lua_newtable(L);
    lsl_runtime_state->uuidWeakTab = lua_ref(L, -1);
    make_weak_uuid_table(L);
    // Pop the weak UUID metatable
    lua_pop(L, 1);
    LUAU_ASSERT(lua_gettop(L) == top);

    // Create a table for weak references to key instances, keeps their associated
    // TStrings alive, for UUID instances that have one.
    lua_newtable(L);
    lsl_runtime_state->uuidCompressedWeakTab = lua_ref(L, -1);
    make_weak_uuid_table(L);
    lua_pop(L, 1);
    LUAU_ASSERT(lua_gettop(L) == top);

    if (!LUAU_IS_LSL_VM(L))
    {
        // Create uuid module table
        luaopen_sl_uuid(L);
        LUAU_ASSERT(lua_gettop(L) == top);
        lua_pushcfunction(L, lua_uuid_ctor, "touuid");
        lua_setglobal(L, "touuid");
    }

    //////
    /// Quaternions
    //////

    // create metatable with all the metamethods
    lua_newtable(L);
    lua_pushvalue(L, -1);

    // pin it to the refs
    lua_ref(L, -1);
    lua_setuserdatametatable(L, UTAG_QUATERNION);

    lua_pushcfunction(L, lsl_index_quat, "__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, lsl_mul_quat, "__mul");
    lua_setfield(L, -2, "__mul");

    lua_pushcfunction(L, lsl_div_quat, "__div");
    lua_setfield(L, -2, "__div");

    lua_pushcfunction(L, lsl_sub_quat, "__sub");
    lua_setfield(L, -2, "__sub");

    lua_pushcfunction(L, lsl_add_quat, "__add");
    lua_setfield(L, -2, "__add");

    lua_pushcfunction(L, lsl_eq_quat, "__eq");
    lua_setfield(L, -2, "__eq");

    lua_pushcfunction(L, lsl_unm_quat, "__unm");
    lua_setfield(L, -2, "__unm");

    lua_pushcfunction(L, lsl_tostring_quat, "__tostring");
    lua_setfield(L, -2, "__tostring");

    // give it a proper name for `typeof()`
    lua_pushstring(L, "quaternion");
    lua_setfield(L, -2, "__type");

    // Please don't mess with our metatable!
    lua_setreadonly(L, -1, true);
    // Okay, don't need this on the stack anymore
    lua_pop(L, 1);
    LUAU_ASSERT(lua_gettop(L) == top);

    // Create quaternion module table
    luaopen_sl_quaternion(L, "quaternion");
    luaopen_sl_quaternion(L, "rotation");
    LUAU_ASSERT(lua_gettop(L) == top);

    //////
    /// DetectedEvent
    //////

    luaSL_setup_detectedevent_metatable(L);
    LUAU_ASSERT(lua_gettop(L) == top);

    //////
    /// LLEvents
    //////

    luaSL_setup_llevents_metatable(L, expose_internal_funcs);
    LUAU_ASSERT(lua_gettop(L) == top);

    //////
    /// LLTimers
    //////

    luaSL_setup_llltimers_metatable(L, expose_internal_funcs);
    LUAU_ASSERT(lua_gettop(L) == top);

    // return "integer" when we call type() on an int
    lua_setlightuserdataname(L, LU_TAG_LSL_INTEGER, "integer");

    LUAU_ASSERT(lua_gettop(L) == top);
    return 1;
}
