/* Lua CJSON - JSON support for Lua
 *
 * Copyright (c) 2010-2012  Mark Pulford <mark@kyne.com.au>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Caveats:
 * - JSON "null" values are represented as lightuserdata since Lua
 *   tables cannot contain "nil". Compare with cjson.null.
 * - Invalid UTF-8 characters are not detected and will be passed
 *   untouched. If required, UTF-8 error checking should be done
 *   outside this library.
 * - Javascript comments are not part of the JSON spec, and are not
 *   currently supported.
 *
 * Note: Decoding is slower than encoding. Lua spends significant
 *       time (30%) managing tables when parsing JSON since it is
 *       difficult to know object/array sizes ahead of time.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <lua.h>
#include <luaconf.h>
#include <lualib.h>
#include <vector>

#include "../lnumutils.h"
#include "llsl.h"
#include "lljson.h"

// ServerLua: strbuf_t is a local alias for lua_YieldSafeStrBuf.
#include "lstrbuf.h"
typedef lua_YieldSafeStrBuf strbuf_t;
#include "fpconv.h"
#include "Luau/Common.h"
#include "../apr/apr_base64.h"
#include "../lgc.h"
#include "../lapi.h"
#include "../lstate.h"
#include "../ltable.h"
#include "../lstring.h"
// ServerLua: yieldable infrastructure for encode/decode
#include "lyieldablemacros.h"

// ServerLua: Shims restoring original cjson strbuf API - captures `l` from enclosing scope
#define strbuf_init(s, len)              luaYB_init(l, (s), (len))
#define strbuf_free(s)                   luaYB_free(l, (s))
#define strbuf_resize(s, len)            luaYB_resize(l, (s), (len))
#define strbuf_append_string(s, str)     luaYB_appendstr(l, (s), (str))
#define strbuf_tostring_inplace(idx, f)  luaYB_tostring(l, (idx), (f))
#define strbuf_reset(s)                  luaYB_reset(s)
#define strbuf_allocated(s)              luaYB_allocated(s)
#define strbuf_empty_length(s)           luaYB_space(s)
#define strbuf_ensure_empty_length(s, n) luaYB_ensure(l, (s), (n))
#define strbuf_empty_ptr(s)              luaYB_ptr(s)
#define strbuf_set_length(s, n)          luaYB_setlen((s), (n))
#define strbuf_extend_length(s, n)       luaYB_extend((s), (n))
#define strbuf_length(s)                 luaYB_len(s)
#define strbuf_append_char(s, c)         luaYB_appendchar(l, (s), (c))
#define strbuf_append_char_unsafe(s, c)  luaYB_appendchar_unsafe((s), (c))
#define strbuf_append_mem(s, c, n)       luaYB_appendmem(l, (s), (c), (n))
#define strbuf_append_mem_unsafe(s, c, n) luaYB_appendmem_unsafe((s), (c), (n))
#define strbuf_ensure_null(s)            luaYB_ensurenull(s)
#define strbuf_string(s, len)            luaYB_data((s), (len))
#define strbuf_pushresult(s)             luaYB_pushresult(l, (s))
#define strbuf_addvalue(s)               luaYB_addvalue(l, (s))

// ServerLua: internal configuration
#define CJSON_MODNAME "lljson"
#define CJSON_VERSION   "2.1.0.11"

#ifdef _MSC_VER
#define snprintf sprintf_s

#ifndef isnan
#include <float.h>
#define isnan(x) _isnan(x)
#endif

#define strncasecmp(x,y,z) _strnicmp(x,y,z)
#define strcasecmp _stricmp
#endif

#ifdef __clang__
#if __has_warning("-Wchar-subscripts")
#pragma clang diagnostic ignored "-Wchar-subscripts"
#endif
#endif

#define JSON_SPARSE_RATIO 2
#define JSON_SPARSE_SAFE 10
#define JSON_MAX_DEPTH 100
#define JSON_NUMBER_PRECISION 14
#define DEFAULT_MAX_SIZE 60000

// ServerLua: Fixed Lua stack positions for encode/decode.
// SlotManager's opaque state always occupies position 1.
enum class EncodeStack
{
    OPAQUE = 1,
    STRBUF = 2,
    CTX = 3,
    REPLACER = 4,
    VALUE = 5,
};

enum class DecodeStack
{
    OPAQUE = 1,
    STRBUF = 2,
    INPUT = 3,
    REVIVER = 4,
};

typedef enum {
    T_OBJ_BEGIN,
    T_OBJ_END,
    T_ARR_BEGIN,
    T_ARR_END,
    T_STRING,
    T_NUMBER,
    T_INTEGER,
    T_BOOLEAN,
    T_NULL,
    T_COLON,
    T_COMMA,
    T_END,
    T_WHITESPACE,
    T_ERROR,
    T_UNKNOWN
} json_token_type_t;

static const char *json_token_type_name[] = {
    "T_OBJ_BEGIN",
    "T_OBJ_END",
    "T_ARR_BEGIN",
    "T_ARR_END",
    "T_STRING",
    "T_NUMBER",
    "T_INTEGER",
    "T_BOOLEAN",
    "T_NULL",
    "T_COLON",
    "T_COMMA",
    "T_END",
    "T_WHITESPACE",
    "T_ERROR",
    "T_UNKNOWN",
    NULL
};

// Static lookup tables for JSON parsing (initialized once)
static json_token_type_t json_ch2token[256];
static char json_escape2char[256];
static bool json_tables_initialized = false;

static void json_init_lookup_tables()
{
    if (json_tables_initialized)
        return;

    /* Tag all characters as an error */
    for (int i = 0; i < 256; i++)
        json_ch2token[i] = T_ERROR;

    /* Set tokens that require no further processing */
    json_ch2token['{'] = T_OBJ_BEGIN;
    json_ch2token['}'] = T_OBJ_END;
    json_ch2token['['] = T_ARR_BEGIN;
    json_ch2token[']'] = T_ARR_END;
    json_ch2token[','] = T_COMMA;
    json_ch2token[':'] = T_COLON;
    json_ch2token['\0'] = T_END;
    json_ch2token[' '] = T_WHITESPACE;
    json_ch2token['\t'] = T_WHITESPACE;
    json_ch2token['\n'] = T_WHITESPACE;
    json_ch2token['\r'] = T_WHITESPACE;

    /* Update characters that require further processing */
    json_ch2token['f'] = T_UNKNOWN;     /* false? */
    json_ch2token['i'] = T_UNKNOWN;     /* inf, infinity? */
    json_ch2token['I'] = T_UNKNOWN;
    json_ch2token['n'] = T_UNKNOWN;     /* null, nan? */
    json_ch2token['N'] = T_UNKNOWN;
    json_ch2token['t'] = T_UNKNOWN;     /* true? */
    json_ch2token['"'] = T_UNKNOWN;     /* string? */
    json_ch2token['+'] = T_UNKNOWN;     /* number? */
    json_ch2token['-'] = T_UNKNOWN;
    for (int i = 0; i < 10; i++)
        json_ch2token['0' + i] = T_UNKNOWN;

    /* Lookup table for parsing escape characters */
    for (int i = 0; i < 256; i++)
        json_escape2char[i] = 0;          /* String error */
    json_escape2char['"'] = '"';
    json_escape2char['\\'] = '\\';
    json_escape2char['/'] = '/';
    json_escape2char['b'] = '\b';
    json_escape2char['t'] = '\t';
    json_escape2char['n'] = '\n';
    json_escape2char['f'] = '\f';
    json_escape2char['r'] = '\r';
    json_escape2char['u'] = 'u';          /* Unicode parsing required */

    json_tables_initialized = true;
}

struct json_config_t {
    int encode_sparse_convert = 0;
    bool allow_sparse = false;
    bool sl_tagged_types = false;
    bool sl_tight_encoding = false;
    bool has_replacer = false;
    bool skip_tojson = false;
};

typedef struct {
    const char *data;
    const char *ptr;
    // ServerLua: Temporary storage for strings, backed by a GC-rooted strbuf userdata.
    lua_YieldSafeStrBuf *tmp;
    json_config_t *cfg;
    int current_depth;
    bool has_reviver;  // When true, a reviver function is on the decode stack
} json_parse_t;

typedef struct {
    json_token_type_t type;
    size_t index;
    union {
        const char *string;
        double number;
        lua_Integer integer;
        int boolean;
    } value;
    size_t string_len;
} json_token_t;

static const char *char2escape[256] = {
    "\\u0000", "\\u0001", "\\u0002", "\\u0003",
    "\\u0004", "\\u0005", "\\u0006", "\\u0007",
    "\\b", "\\t", "\\n", "\\u000b",
    "\\f", "\\r", "\\u000e", "\\u000f",
    "\\u0010", "\\u0011", "\\u0012", "\\u0013",
    "\\u0014", "\\u0015", "\\u0016", "\\u0017",
    "\\u0018", "\\u0019", "\\u001a", "\\u001b",
    "\\u001c", "\\u001d", "\\u001e", "\\u001f",
    NULL, NULL, "\\\"", NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, "\\\\", NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, "\\u007f",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

/* ===== ENCODING ===== */

static void json_encode_exception(lua_State *l, json_config_t *cfg, strbuf_t *json, int lindex,
                                  const char *reason)
{
    luaL_error(l, "Cannot serialise %s: %s",
                  lua_typename(l, lua_type(l, lindex)), reason);
}

/* json_append_string args:
 * - lua_State
 * - JSON strbuf
 * - String (Lua stack index)
 *
 * Returns nothing. Doesn't remove string from Lua stack */
static void json_append_string(lua_State *l, strbuf_t *json, int lindex)
{
    const char *escstr;
    const char *str;
    size_t len;
    size_t i;

    int type = lua_type(l, lindex);
    switch(type) {
        case LUA_TNUMBER:
        case LUA_TSTRING:
            str = lua_tolstring(l, lindex, &len);
            break;
        case LUA_TLIGHTUSERDATA:
        case LUA_TUSERDATA:
            // This is something we're going to have to call into the metatable for.
            // ServerLua: luaL_tolstring needs stack space for metatable lookup and
            //  potential `__tostring` call
            lua_checkstack(l, 5);
            str = luaL_tolstring(l, lindex, &len);
            break;
        default:
            luaL_error(l, "tried to append invalid value as string");
            return;
    }

    /* Worst case is len * 6 (all unicode escapes).
     * This buffer is reused constantly for small strings
     * If there are any excess pages, they won't be hit anyway.
     * This gains ~5% speedup. */
    if (len > SIZE_MAX / 6 - 3)
        abort(); /* Overflow check */
    strbuf_ensure_empty_length(json, len * 6 + 2);

    strbuf_append_char_unsafe(json, '\"');
    for (i = 0; i < len; i++) {
        escstr = char2escape[(unsigned char)str[i]];
        if (escstr)
            strbuf_append_string(json, escstr);
        else
            strbuf_append_char_unsafe(json, str[i]);
    }
    strbuf_append_char_unsafe(json, '\"');
}

static void json_append_tostring(lua_State *l, strbuf_t *json, int lindex)
{
    int top = lua_gettop(l);
    json_append_string(l, json, lindex);
    lua_settop(l, top);
}

/* Find the size of the array on the top of the Lua stack
 * -1   object (not a pure array)
 * >=0  elements in array
 */
static int lua_array_length(lua_State *l, json_config_t *cfg, strbuf_t *json, bool force = false)
{
    double k;
    int max;
    int items;

    max = 0;
    items = 0;

    lua_pushnil(l);
    /* table, startkey */
    while (lua_next(l, -2) != 0) {
        /* table, key, value */
        if (lua_type(l, -2) == LUA_TNUMBER &&
            (k = lua_tonumber(l, -2)) != 0.0) {
            /* Integer >= 1 and in int range? (floor(inf)==inf, so check upper bound) */
            if (floor(k) == k && k >= 1 && k <= INT_MAX) {
                if (k > max)
                    max = (int32_t)k;
                items++;
                lua_pop(l, 1);
                continue;
            }
        }

        /* Must not be an array (non integer key) */
        lua_pop(l, 2);
        return -1;
    }

    /* Encode excessively sparse arrays as objects (if enabled) */
    if (!force && max > items * JSON_SPARSE_RATIO &&
        max > JSON_SPARSE_SAFE) {
        if (!cfg->encode_sparse_convert)
            json_encode_exception(l, cfg, json, -1, "excessively sparse array");

        return -1;
    }

    return max;
}

static void json_check_encode_depth(lua_State *l, json_config_t *cfg,
                                    int current_depth, strbuf_t *json)
{
    /* Ensure there are enough slots free to traverse a table (key,
     * value) and push a string for a potential error message.
     *
     * Unlike "decode", the key and value are still on the stack when
     * lua_checkstack() is called.  Hence an extra slot for luaL_error()
     * below is required just in case the next check to lua_checkstack()
     * fails.
     *
     * While this won't cause a crash due to the EXTRA_STACK reserve
     * slots, it would still be an improper use of the API. */
    if (current_depth <= JSON_MAX_DEPTH && lua_checkstack(l, 7))
        return;

    luaL_error(l, "Cannot serialise, excessive nesting (%d)",
               current_depth);
}

// ServerLua: Forward declarations for yieldable encode helpers
static int json_append_data(lua_State* l, SlotManager& parent_slots,
                                       json_config_t* cfg, int current_depth, strbuf_t* json);
static void json_append_array(lua_State* l, SlotManager& parent_slots,
                                         json_config_t* cfg, int current_depth,
                                         strbuf_t* json, int array_length, int raw);
static void json_append_object(lua_State* l, SlotManager& parent_slots,
                                          json_config_t* cfg, int current_depth, strbuf_t* json);

// ServerLua: helper to re-obtain the strbuf pointer from stack position 2.
// Position 1 is reserved for the SlotManager opaque buffer.
static strbuf_t* json_get_strbuf(lua_State* l)
{
    return (strbuf_t*)lua_touserdatatagged(l, 2, UTAG_STRBUF);
}

static void json_append_array(lua_State* l, SlotManager& parent_slots,
                                         json_config_t* cfg, int current_depth,
                                         strbuf_t* json, int array_length, int raw)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        ELEMENT = 1,
        NEXT_ELEMENT = 2,
        REPLACER_CHECK = 3,
        REPLACER_CALL = 4,
        TOJSON_CHECK = 5,
        TOJSON_CALL = 6,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, i, 1);
    DEFINE_SLOT(int32_t, comma, 0);
    DEFINE_SLOT(int32_t, json_pos, 0);
    DEFINE_SLOT(bool, replacer_removed, false);
    slots.finalize();

    json = json_get_strbuf(l);

    if (slots.isInit())
        strbuf_append_char(json, '[');

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(ELEMENT);
    YIELD_DISPATCH(NEXT_ELEMENT);
    YIELD_DISPATCH(REPLACER_CHECK);
    YIELD_DISPATCH(REPLACER_CALL);
    YIELD_DISPATCH(TOJSON_CHECK);
    YIELD_DISPATCH(TOJSON_CALL);
    YIELD_DISPATCH_END();

    for (; i <= array_length; ++i) {
        replacer_removed = false;

        if (raw) {
            lua_rawgeti(l, -1, i);
        } else {
            lua_pushinteger(l, i);
            lua_gettable(l, -2);
        }
        /* table, value */

        if (cfg->has_replacer) {
            // Resolve __tojson before replacer (JS compat: toJSON -> replacer)
            if (!cfg->skip_tojson && lua_istable(l, -1) && luaL_getmetafield(l, -1, "__tojson")) {
                // Stack: table, value, __tojson_fn
                lua_pushvalue(l, -2);                        // self
                lua_pushvalue(l, (int)EncodeStack::CTX);     // ctx
                YIELD_CHECK(l, TOJSON_CHECK, LUA_INTERRUPT_LLLIB);
                YIELD_CALL(l, 2, 1, TOJSON_CALL);
                // Stack: table, value, resolved
                lua_remove(l, -2);  // remove original value
                // Stack: table, resolved
            }

            lua_pushvalue(l, (int)EncodeStack::REPLACER);
            lua_pushinteger(l, i);     // key (1-based index)
            lua_pushvalue(l, -3);      // value (possibly __tojson-resolved)
            lua_pushvalue(l, -5);      // parent (the array table)
            YIELD_CHECK(l, REPLACER_CHECK, LUA_INTERRUPT_LLLIB);
            YIELD_CALL(l, 3, 1, REPLACER_CALL);
            // Stack: table, value, result
            if (lua_tolightuserdatatagged(l, -1, LU_TAG_JSON_INTERNAL) == JSON_REMOVE) {
                lua_pop(l, 2);  // pop result + value -> table
                replacer_removed = true;
            } else {
                lua_remove(l, -2);  // remove original value -> table, result
            }
        }

        if (!replacer_removed) {
            json_pos = strbuf_length(json);
            if (comma++ > 0)
                strbuf_append_char(json, ',');

            // Not a slot: assigned by YIELD_HELPER before read, no goto crosses this decl.
            bool skip;
            YIELD_HELPER(l, ELEMENT,
                skip = (bool)json_append_data(l, slots, cfg, current_depth, json));
            if (skip) {
                strbuf_set_length(json, json_pos);
                if (comma == 1) {
                    comma = 0;
                }
            }

            lua_pop(l, 1);
        }

        YIELD_CHECK(l, NEXT_ELEMENT, LUA_INTERRUPT_LLLIB);
    }

    strbuf_append_char(json, ']');
}

// ServerLua: forward declaration for NaN handling in json_append_number
static void json_append_tagged_float(lua_State *l, strbuf_t *json, double num, int precision);

static void json_append_number(lua_State *l, json_config_t *cfg,
                               strbuf_t *json, int lindex)
{
    int len;
    double num = lua_tonumber(l, lindex);

    // NaN has no JSON representation.
    // slencode: tagged float for round-trip. encode: null (matches JSON.stringify).
    if (isnan(num)) {
        if (cfg->sl_tagged_types)
            json_append_tagged_float(l, json, num, JSON_NUMBER_PRECISION);
        else
            strbuf_append_mem(json, "null", 4);
        return;
    }

    if (isinf(num)) {
        // Infinity doesn't have a real representation in JSON, but we can fake it
        // with a large enough value that it will be equal to infinity when deserialized.
        // Note that this is particular to the bit-ness of the FP representation, but
        // surely nobody's deserializing 256-bit FP values from JSON!
        if (signbit(num)) {
            strbuf_append_string(json, "-1e9999");
        } else {
            strbuf_append_string(json, "1e9999");
        }
        return;
    }

    strbuf_ensure_empty_length(json, FPCONV_G_FMT_BUFSIZE);
    len = fpconv_g_fmt(strbuf_empty_ptr(json), num, JSON_NUMBER_PRECISION);
    strbuf_extend_length(json, len);
}

static const float DEFAULT_VECTOR[3] = {0.0f, 0.0f, 0.0f};
static const float DEFAULT_QUATERNION[4] = {0.0f, 0.0f, 0.0f, 1.0f};

static void json_append_coordinate_component(lua_State *l, strbuf_t *json, float val, bool tight = false) {
    if (tight && val == 0.0f && !signbit(val))
        return;  // Omit positive zeros in tight mode
    char format_buf[256] = {};
    // Use shared helper to ensure consistent normalization of non-finite values
    size_t str_len = luai_formatfloat(format_buf, sizeof(format_buf), "%.6f", val);
    str_len = luai_trimfloat(format_buf, str_len);
    strbuf_append_mem(json, format_buf, str_len);
}

// Helper to append a tagged vector value: !v<x,y,z> or tight: !v1,,3
// ZERO_VECTOR in tight mode -> "!v"
static void json_append_tagged_vector(lua_State *l, strbuf_t *json, const float *a, bool tight = false) {
    strbuf_append_string(json, tight ? "\"!v" : "\"!v<");
    if (tight && memcmp(a, DEFAULT_VECTOR, sizeof(DEFAULT_VECTOR)) == 0) {
        strbuf_append_char(json, '"');
        return;
    }
    json_append_coordinate_component(l, json, a[0], tight);
    strbuf_append_char(json, ',');
    json_append_coordinate_component(l, json, a[1], tight);
    strbuf_append_char(json, ',');
    json_append_coordinate_component(l, json, a[2], tight);
    strbuf_append_string(json, tight ? "\"" : ">\"");
}

// Helper to append a tagged quaternion value: !q<x,y,z,w> or tight: !q,,,1
// ZERO_ROTATION (0,0,0,1) in tight mode -> "!q"
static void json_append_tagged_quaternion(lua_State *l, strbuf_t *json, const float *a, bool tight = false) {
    strbuf_append_string(json, tight ? "\"!q" : "\"!q<");
    if (tight && memcmp(a, DEFAULT_QUATERNION, sizeof(DEFAULT_QUATERNION)) == 0) {
        strbuf_append_char(json, '"');
        return;
    }
    json_append_coordinate_component(l, json, a[0], tight);
    strbuf_append_char(json, ',');
    json_append_coordinate_component(l, json, a[1], tight);
    strbuf_append_char(json, ',');
    json_append_coordinate_component(l, json, a[2], tight);
    strbuf_append_char(json, ',');
    json_append_coordinate_component(l, json, a[3], tight);
    strbuf_append_string(json, tight ? "\"" : ">\"");
}

static void json_append_buffer(lua_State *l, strbuf_t *json, int lindex)
{
    size_t buf_len = 0;
    void *data = luaL_checkbuffer(l, lindex, &buf_len);

    // Need to use something that will automatically de-alloc in case we hit
    //  a memory limit appending to the buffer.
    std::vector<char> encode_buf((size_t)apr_base64_encode_len(buf_len));
    size_t encoded_len = apr_base64_encode_binary(encode_buf.data(), (const uint8_t *)data, buf_len);
    if (encoded_len > 0)
    {
        // exclude the trailing null
        strbuf_append_mem(json, encode_buf.data(), encoded_len - 1);
    }
}

// Helper to parse a hex character to its value (returns -1 on error)
static inline int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

// Helper to parse UUID string (36 chars) to bytes (16 bytes)
// This is more strict than the typical UUID handling in SLua
// because we don't need to handle "old" format UUIDs.
static bool parse_uuid_to_bytes(const char *str, size_t len, uint8_t *out) {
    if (len != 36) return false;
    int out_idx = 0;
    for (size_t i = 0; i < len && out_idx < 16; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (str[i] != '-') return false;
            continue;
        }
        int hi = hex_char_to_int(str[i]);
        int lo = hex_char_to_int(str[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[out_idx++] = (uint8_t)((hi << 4) | lo);
        i++;
    }
    return out_idx == 16;
}

// Helper to append a tagged UUID value: !uxxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx or tight: !u<base64>
static void json_append_tagged_uuid(lua_State *l, strbuf_t *json, int lindex, bool tight = false) {
    lua_LSLUUID* uuid = (lua_LSLUUID*)lua_touserdatatagged(l, lindex, UTAG_UUID);
    uint8_t uuid_bytes[16];

    if (uuid->compressed) {
        // Already raw bytes
        memcpy(uuid_bytes, getstr(uuid->str), 16);
    } else {
        // Parse string form to bytes - error if invalid
        const char *str = getstr(uuid->str);
        size_t len = uuid->str->len;
        if (!parse_uuid_to_bytes(str, len, uuid_bytes)) {
            luaL_error(l, "invalid UUID format for JSON encoding");
        }
    }

    if (tight) {
        // Check for null UUID (all zeros)
        bool is_null = true;
        for (int i = 0; i < 16; i++) {
            if (uuid_bytes[i] != 0) {
                is_null = false;
                break;
            }
        }
        if (is_null) {
            strbuf_append_string(json, "\"!u\"");
        } else {
            // Base64 encode - 16 bytes -> 24 chars, but we strip the '==' padding
            char encoded[25];
            apr_base64_encode_binary(encoded, uuid_bytes, 16);
            strbuf_append_string(json, "\"!u");
            strbuf_append_mem(json, encoded, 22);  // Strip '==' padding
            strbuf_append_char(json, '"');
        }
    } else {
        // Normal string form - output canonical UUID format
        int top = lua_gettop(l);
        size_t len;
        // luaL_tolstring needs stack space for metatable lookup and potential __tostring call
        lua_checkstack(l, 5);
        const char *str = luaL_tolstring(l, lindex, &len);
        strbuf_append_string(json, "\"!u");
        strbuf_append_mem(json, str, len);
        strbuf_append_char(json, '"');
        lua_settop(l, top);
    }
}

// Helper to append a tagged float: !f3.14 (used for all numeric keys in SL mode)
static void json_append_tagged_float(lua_State *l, strbuf_t *json, double num, int precision) {
    strbuf_append_string(json, "\"!f");

    if (isnan(num)) {
        strbuf_append_mem(json, "NaN", 3);
    } else if (isinf(num)) {
        // Use 1e9999 which overflows to infinity when parsed back
        if (signbit(num))
            strbuf_append_string(json, "-1e9999");
        else
            strbuf_append_string(json, "1e9999");
    } else {
        strbuf_ensure_empty_length(json, FPCONV_G_FMT_BUFSIZE);
        int len = fpconv_g_fmt(strbuf_empty_ptr(json), num, precision);
        strbuf_extend_length(json, len);
    }

    strbuf_append_char(json, '"');
}

// Helper to append a string that may need ! escaping for SL tagged mode
static void json_append_string_sl(lua_State *l, strbuf_t *json, int lindex) {
    size_t len;
    const char *str = lua_tolstring(l, lindex, &len);

    // Check if string starts with '!' - needs escaping
    bool needs_escape = (len > 0 && str[0] == '!');

    strbuf_ensure_empty_length(json, len * 6 + 4); // Extra space for potential !! prefix
    strbuf_append_char_unsafe(json, '"');

    if (needs_escape)
        strbuf_append_char_unsafe(json, '!');

    for (size_t i = 0; i < len; i++) {
        const char *escstr = char2escape[(unsigned char)str[i]];
        if (escstr)
            strbuf_append_string(json, escstr);
        else
            strbuf_append_char_unsafe(json, str[i]);
    }
    strbuf_append_char_unsafe(json, '"');
}

static void json_append_object(lua_State* l, SlotManager& parent_slots,
                                          json_config_t* cfg, int current_depth, strbuf_t* json)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        VALUE = 1,
        NEXT_PAIR = 2,
        REPLACER_CHECK = 3,
        REPLACER_CALL = 4,
        TOJSON_CHECK = 5,
        TOJSON_CALL = 6,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, comma, 0);
    DEFINE_SLOT(int32_t, json_pos, 0);
    DEFINE_SLOT(bool, replacer_removed, false);
    slots.finalize();

    json = json_get_strbuf(l);

    /* Object */
    if (slots.isInit()) {
        strbuf_append_char(json, '{');
        lua_pushnil(l);
    }

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(VALUE);
    YIELD_DISPATCH(NEXT_PAIR);
    YIELD_DISPATCH(REPLACER_CHECK);
    YIELD_DISPATCH(REPLACER_CALL);
    YIELD_DISPATCH(TOJSON_CHECK);
    YIELD_DISPATCH(TOJSON_CALL);
    YIELD_DISPATCH_END();

    /* table, startkey */
    while (lua_next(l, -2) != 0) {
        /* table, key, value */
        replacer_removed = false;

        if (cfg->has_replacer) {
            // Resolve __tojson before replacer (JS compat: toJSON -> replacer)
            if (!cfg->skip_tojson && lua_istable(l, -1) && luaL_getmetafield(l, -1, "__tojson")) {
                // Stack: table, key, value, __tojson_fn
                lua_pushvalue(l, -2);                        // self
                lua_pushvalue(l, (int)EncodeStack::CTX);     // ctx
                YIELD_CHECK(l, TOJSON_CHECK, LUA_INTERRUPT_LLLIB);
                YIELD_CALL(l, 2, 1, TOJSON_CALL);
                // Stack: table, key, value, resolved
                lua_remove(l, -2);  // remove original value
                // Stack: table, key, resolved
            }

            lua_pushvalue(l, (int)EncodeStack::REPLACER);
            lua_pushvalue(l, -3);  // key
            lua_pushvalue(l, -3);  // value (possibly __tojson-resolved)
            lua_pushvalue(l, -6);  // parent (the object table)
            YIELD_CHECK(l, REPLACER_CHECK, LUA_INTERRUPT_LLLIB);
            YIELD_CALL(l, 3, 1, REPLACER_CALL);
            // Stack: table, key, value, result
            if (lua_tolightuserdatatagged(l, -1, LU_TAG_JSON_INTERNAL) == JSON_REMOVE) {
                lua_pop(l, 2);  // pop result + value -> table, key
                replacer_removed = true;
            } else {
                lua_remove(l, -2);  // remove original value -> table, key, result
            }
        }

        if (!replacer_removed) {
            json_pos = strbuf_length(json);
            if (comma++ > 0)
                strbuf_append_char(json, ',');

            int keytype;
            keytype = lua_type(l, -2);

            if (cfg->sl_tagged_types) {
                // SL tagged mode: accept any key type and tag appropriately
                switch (keytype) {
                case LUA_TSTRING:
                    json_append_string_sl(l, json, -2);
                    break;
                case LUA_TNUMBER:
                    json_append_tagged_float(l, json, lua_tonumber(l, -2), JSON_NUMBER_PRECISION);
                    break;
                case LUA_TVECTOR: {
                    const float* a = lua_tovector(l, -2);
                    json_append_tagged_vector(l, json, a, cfg->sl_tight_encoding);
                    break;
                }
                case LUA_TBUFFER: {
                    strbuf_append_string(json, "\"!d");
                    json_append_buffer(l, json, -2);
                    strbuf_append_char(json, '"');
                    break;
                }
                case LUA_TUSERDATA: {
                    int tag = lua_userdatatag(l, -2);
                    if (tag == UTAG_UUID) {
                        json_append_tagged_uuid(l, json, -2, cfg->sl_tight_encoding);
                    } else if (tag == UTAG_QUATERNION) {
                        const float *a = luaSL_checkquaternion(l, -2);
                        json_append_tagged_quaternion(l, json, a, cfg->sl_tight_encoding);
                    } else {
                        json_encode_exception(l, cfg, json, -2,
                                              "unsupported userdata type as table key");
                    }
                    break;
                }
                case LUA_TBOOLEAN:
                    strbuf_append_string(json, lua_toboolean(l, -2) ? "\"!b1\"" : "\"!b0\"");
                    break;
                default:
                    json_encode_exception(l, cfg, json, -2,
                                          "unsupported table key type");
                    /* never returns */
                }
                strbuf_append_char(json, ':');
            } else {
                // Standard JSON mode: only string and number keys
                if (keytype == LUA_TNUMBER) {
                    strbuf_append_char(json, '"');
                    json_append_number(l, cfg, json, -2);
                    strbuf_append_mem(json, "\":", 2);
                } else if (keytype == LUA_TSTRING) {
                    json_append_string(l, json, -2);
                    strbuf_append_char(json, ':');
                } else {
                    json_encode_exception(l, cfg, json, -2,
                                          "table key must be a number or string");
                    /* never returns */
                }
            }

            /* table, key, value */
            // Not a slot: assigned by YIELD_HELPER before read, no goto crosses this decl.
            bool skip;
            YIELD_HELPER(l, VALUE,
                skip = (bool)json_append_data(l, slots, cfg, current_depth, json));
            if (skip) {
                strbuf_set_length(json, json_pos);
                if (comma == 1) {
                    comma = 0;
                }
            }

            lua_pop(l, 1);
        }
        /* table, key */

        YIELD_CHECK(l, NEXT_PAIR, LUA_INTERRUPT_LLLIB);
    }

    strbuf_append_char(json, '}');
}

/* Serialise Lua data into JSON string. */
// ServerLua: Yieldable version of json_append_data.
static int json_append_data(lua_State* l, SlotManager& parent_slots,
                             json_config_t* cfg, int current_depth, strbuf_t* json)
{
    YIELDABLE_RETURNS(0);
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        APPEND_ARRAY = 1,
        APPEND_OBJECT = 2,
        TOJSON_CALL = 3,
        TOJSON_RECURSE = 4,
        APPEND_ARRAY_AUTO = 5,
        APPEND_ARRAY_LUD = 6,
        TOJSON_CHECK = 7,
        LEN_CHECK = 8,
        LEN_CALL = 9,
        APPEND_OBJECT_MT = 10,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, depth, current_depth);
    DEFINE_SLOT(int32_t, array_length, 0);
    DEFINE_SLOT(bool, raw, true);
    DEFINE_SLOT(uint8_t, type, LUA_TNIL);
    DEFINE_SLOT(bool, as_array, false);
    DEFINE_SLOT(bool, force_object, false);
    DEFINE_SLOT(bool, has_metatable, false);
    DEFINE_SLOT(int32_t, len, 0);
    slots.finalize();

    json = json_get_strbuf(l);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(APPEND_ARRAY);
    YIELD_DISPATCH(APPEND_OBJECT);
    YIELD_DISPATCH(TOJSON_CHECK);
    YIELD_DISPATCH(TOJSON_CALL);
    YIELD_DISPATCH(TOJSON_RECURSE);
    YIELD_DISPATCH(APPEND_ARRAY_AUTO);
    YIELD_DISPATCH(APPEND_ARRAY_LUD);
    YIELD_DISPATCH(LEN_CHECK);
    YIELD_DISPATCH(LEN_CALL);
    YIELD_DISPATCH(APPEND_OBJECT_MT);
    YIELD_DISPATCH_END();

    type = lua_type(l, -1);

    switch (type) {
    case LUA_TSTRING:
        if (cfg->sl_tagged_types)
            json_append_string_sl(l, json, -1);
        else
            json_append_string(l, json, -1);
        break;
    case LUA_TNUMBER:
        json_append_number(l, cfg, json, -1);
        break;
    case LUA_TBOOLEAN:
        if (lua_toboolean(l, -1))
            strbuf_append_mem(json, "true", 4);
        else
            strbuf_append_mem(json, "false", 5);
        break;
    case LUA_TTABLE:
    {
        depth++;
        json_check_encode_depth(l, cfg, depth, json);

        as_array = false;
        has_metatable = lua_getmetatable(l, -1);

        if (has_metatable) {
            if (!cfg->sl_tagged_types) {
                // ServerLua: Check __jsontype metamethod for shape control
                lua_rawgetfield(l, -1, "__jsontype");
                if (!lua_isnil(l, -1)) {
                    if (!lua_isstring(l, -1))
                        luaL_error(l, "invalid __jsontype value (expected string)");
                    const char* jsontype;
                    jsontype = lua_tostring(l, -1);
                    if (strcmp(jsontype, "object") == 0) {
                        force_object = true;
                    } else if (strcmp(jsontype, "array") == 0) {
                        as_array = true;
                        raw = false;
                    } else {
                        luaL_error(l, "invalid __jsontype value: '%s' (expected \"array\" or \"object\")", jsontype);
                    }
                }
                lua_pop(l, 1);  // pop __jsontype (or nil)
            }

            lua_pop(l, 1);  // pop metatable

            // __tojson provides content, __jsontype provides shape
            if (!cfg->skip_tojson && luaL_getmetafield(l, -1, "__tojson")) {
                lua_pushvalue(l, -2);                           // self
                lua_pushvalue(l, (int)EncodeStack::CTX);        // ctx table
                YIELD_CHECK(l, TOJSON_CHECK, LUA_INTERRUPT_LLLIB);
                YIELD_CALL(l, 2, 1, TOJSON_CALL);
                // Stack: ..., original_table, tojson_result
                if (lua_istable(l, -1)) {
                    // Table result: replace original, fall through to shape handling
                    lua_remove(l, -2);
                } else {
                    // Non-table result: encode directly, shape hints don't apply
                    YIELD_HELPER(l, TOJSON_RECURSE,
                        json_append_data(l, slots, cfg, depth, json));
                    lua_pop(l, 1);
                    return 0;
                }
            }

            if (force_object) {
                YIELD_HELPER(l, APPEND_OBJECT_MT,
                    json_append_object(l, slots, cfg, depth, json));
                break;
            }

            if (as_array) {
                // Validate: __jsontype="array" requires all keys to be positive integers
                len = lua_array_length(l, cfg, json, true);
                if (len < 0)
                    luaL_error(l, "cannot encode as array: table has non-integer keys");

                // __len overrides the detected length (validation already passed)
                if (luaL_getmetafield(l, -1, "__len")) {
                    lua_pushvalue(l, -2);
                    YIELD_CHECK(l, LEN_CHECK, LUA_INTERRUPT_LLLIB);
                    YIELD_CALL(l, 1, 1, LEN_CALL);
                    array_length = lua_tonumber(l, -1);
                    lua_pop(l, 1);
                } else {
                    array_length = len;
                }
            }
        }

        if (as_array) {
            YIELD_HELPER(l, APPEND_ARRAY,
                json_append_array(l, slots, cfg, depth, json, array_length, raw));
        } else {
            len = lua_array_length(l, cfg, json, cfg->allow_sparse);

            if (len >= 0) {
                array_length = len;
                YIELD_HELPER(l, APPEND_ARRAY_AUTO,
                    json_append_array(l, slots, cfg, depth, json, array_length, raw));
            } else {
                YIELD_HELPER(l, APPEND_OBJECT,
                    json_append_object(l, slots, cfg, depth, json));
            }
        }
        break;
    }
    case LUA_TNIL:
        if (cfg->sl_tagged_types)
            strbuf_append_mem(json, "\"!n\"", 4);
        else
            strbuf_append_mem(json, "null", 4);
        break;
    case LUA_TLIGHTUSERDATA: {
        void* json_internal_val;
        json_internal_val = lua_tolightuserdatatagged(l, -1, LU_TAG_JSON_INTERNAL);
        if (json_internal_val) {
            if (json_internal_val == JSON_NULL) {
                strbuf_append_mem(json, "null", 4);
                break;
            } else if (json_internal_val == JSON_ARRAY) {
                YIELD_HELPER(l, APPEND_ARRAY_LUD,
                    json_append_array(l, slots, cfg, depth, json, 0, 1));
                break;
            }
        } else if (lua_tolightuserdatatagged(l, -1, LU_TAG_LSL_INTEGER) != nullptr) {
            // let lua_tonumber() handle it
            json_append_number(l, cfg, json, -1);
            break;
        }

        json_encode_exception(l, cfg, json, -1, "type not supported");
        // Should never reach here.
        break;
    }
    case LUA_TVECTOR: {
        const float *a = lua_tovector(l, -1);
        if (cfg->sl_tagged_types) {
            json_append_tagged_vector(l, json, a, cfg->sl_tight_encoding);
        } else {
            // We specifically want a short representation here, don't use %f!
            strbuf_append_string(json, "\"<");
            json_append_coordinate_component(l, json, a[0]);
            strbuf_append_char(json, ',');
            json_append_coordinate_component(l, json, a[1]);
            strbuf_append_char(json, ',');
            json_append_coordinate_component(l, json, a[2]);
            strbuf_append_string(json, ">\"");
        }
        break;
    }
    case LUA_TBUFFER: {
        strbuf_append_char(json, '"');
        if (cfg->sl_tagged_types)
            strbuf_append_string(json, "!d");

        json_append_buffer(l, json, -1);
        strbuf_append_char(json, '"');
        break;
    }
    case LUA_TUSERDATA: {
        int tag = lua_userdatatag(l, -1);
        if (tag == UTAG_UUID) {
            if (cfg->sl_tagged_types)
                json_append_tagged_uuid(l, json, -1, cfg->sl_tight_encoding);
            else
                json_append_tostring(l, json, -1);
        } else if (tag == UTAG_QUATERNION) {
            const float *a = luaSL_checkquaternion(l, -1);
            if (cfg->sl_tagged_types) {
                json_append_tagged_quaternion(l, json, a, cfg->sl_tight_encoding);
            } else {
                strbuf_append_string(json, "\"<");
                json_append_coordinate_component(l, json, a[0]);
                strbuf_append_char(json, ',');
                json_append_coordinate_component(l, json, a[1]);
                strbuf_append_char(json, ',');
                json_append_coordinate_component(l, json, a[2]);
                strbuf_append_char(json, ',');
                json_append_coordinate_component(l, json, a[3]);
                strbuf_append_string(json, ">\"");
            }
        } else {
            json_encode_exception(l, cfg, json, -1, "type not supported");
        }
        break;
    }
    default:
        /* Remaining types (LUA_TFUNCTION, LUA_TTHREAD) cannot be serialised */
        json_encode_exception(l, cfg, json, -1, "type not supported");
        /* never returns */
    }
    return 0;
}

// ServerLua: Shared yieldable body for json_encode / json_encode_sl.
// sl_tagged selects between standard JSON and SL tagged type encoding.
static int json_encode_common(lua_State* l, bool is_init, bool sl_tagged)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        APPEND_DATA = 1,
        ROOT_TOJSON_CHECK = 2,
        ROOT_TOJSON_CALL = 3,
        ROOT_REPLACER_CHECK = 4,
        ROOT_REPLACER_CALL = 5,
    };

    SlotManager slots(l, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(bool, tight_encoding, false);
    DEFINE_SLOT(bool, skip_tojson, false);
    slots.finalize();

    json_config_t cfg;
    cfg.skip_tojson = skip_tojson;
    if (sl_tagged) {
        cfg.sl_tagged_types = true;
        cfg.encode_sparse_convert = 1;
        cfg.sl_tight_encoding = tight_encoding;
    }

    if (is_init) {
        // Args already validated by init wrapper.
        // SlotManager inserted nil at pos 1, original args shifted to pos 2+.
        // Stack: [opaque(1), value(2), opts_table?(3)]

        // Extract all options from the table while it's at position 3.
        // The table is never read again after init.
        if (lua_istable(l, 3)) {
            if (sl_tagged) {
                lua_rawgetfield(l, 3, "tight");
                tight_encoding = lua_toboolean(l, -1);
                cfg.sl_tight_encoding = tight_encoding;
                lua_pop(l, 1);
            }
            lua_rawgetfield(l, 3, "skip_tojson");
            skip_tojson = lua_toboolean(l, -1);
            cfg.skip_tojson = skip_tojson;
            lua_pop(l, 1);
            lua_rawgetfield(l, 3, "allow_sparse");
            cfg.allow_sparse = lua_toboolean(l, -1);
            lua_pop(l, 1);
            lua_rawgetfield(l, 3, "replacer");
            if (!lua_isfunction(l, -1)) {
                lua_pop(l, 1);
                lua_pushnil(l);
            }
            // Stack: [opaque(1), value(2), opts(3), replacer_or_nil(4)]
            lua_remove(l, 3);
        } else {
            lua_pushnil(l);
        }
        // Stack: [opaque(1), value(2), replacer_or_nil(3)]

        luaYB_push(l);
        lua_insert(l, (int)EncodeStack::STRBUF);
        // Stack: [opaque(1), strbuf(2), value(3), replacer_or_nil(4)]

        // Create frozen context table for __tojson(self, ctx)
        lua_newtable(l);
        lua_pushstring(l, sl_tagged ? "sljson" : "json");
        lua_rawsetfield(l, -2, "mode");
        lua_pushboolean(l, tight_encoding);
        lua_rawsetfield(l, -2, "tight");
        lua_setreadonly(l, -1, true);
        lua_insert(l, (int)EncodeStack::CTX);
        // Stack: [opaque(1), strbuf(2), ctx(3), value(4), replacer_or_nil(5)]

        // Insert replacer at its slot, pushing value to the top
        lua_insert(l, (int)EncodeStack::REPLACER);
        /* Stack: [opaque(1), strbuf(2), ctx(3), replacer_or_nil(4), value(5)] */

        lua_hardenstack(l, 1);
    }

    cfg.has_replacer = !lua_isnil(l, (int)EncodeStack::REPLACER);
    strbuf_t* buf = json_get_strbuf(l);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(APPEND_DATA);
    YIELD_DISPATCH(ROOT_TOJSON_CHECK);
    YIELD_DISPATCH(ROOT_TOJSON_CALL);
    YIELD_DISPATCH(ROOT_REPLACER_CHECK);
    YIELD_DISPATCH(ROOT_REPLACER_CALL);
    YIELD_DISPATCH_END();

    // Resolve __tojson on root value before replacer (JS compat: toJSON -> replacer)
    lua_checkstack(l, 4);
    if (cfg.has_replacer && !cfg.skip_tojson && lua_istable(l, (int)EncodeStack::VALUE)
        && luaL_getmetafield(l, (int)EncodeStack::VALUE, "__tojson")) {
        lua_pushvalue(l, (int)EncodeStack::VALUE);     // self
        lua_pushvalue(l, (int)EncodeStack::CTX);       // ctx
        YIELD_CHECK(l, ROOT_TOJSON_CHECK, LUA_INTERRUPT_LLLIB);
        YIELD_CALL(l, 2, 1, ROOT_TOJSON_CALL);
        // Stack: [..., resolved]
        lua_replace(l, (int)EncodeStack::VALUE);
    }

    // Call replacer on root value: replacer(nil, value, nil)
    if (cfg.has_replacer) {
        lua_pushvalue(l, (int)EncodeStack::REPLACER);
        lua_pushnil(l);                                // key = nil (root)
        lua_pushvalue(l, (int)EncodeStack::VALUE);     // value (possibly __tojson-resolved)
        lua_pushnil(l);                                // parent = nil (root)
        YIELD_CHECK(l, ROOT_REPLACER_CHECK, LUA_INTERRUPT_LLLIB);
        YIELD_CALL(l, 3, 1, ROOT_REPLACER_CALL);
        // Stack: [..., result]
        if (lua_tolightuserdatatagged(l, -1, LU_TAG_JSON_INTERNAL) == JSON_REMOVE) {
            lua_pop(l, 1);
            // Replace value with JSON null
            lua_pushlightuserdatatagged(l, JSON_NULL, LU_TAG_JSON_INTERNAL);
        }
        lua_replace(l, (int)EncodeStack::VALUE);
    }

    lua_pushvalue(l, (int)EncodeStack::VALUE);
    lua_hardenstack(l, 1);
    YIELD_HELPER(l, APPEND_DATA,
        json_append_data(l, slots, &cfg, 0, buf));

    lua_settop(l, (int)EncodeStack::STRBUF);
    strbuf_tostring_inplace((int)EncodeStack::STRBUF, true);
    luau_interruptoncalltail(l);
    return 1;
}

// ServerLua: init / continuation wrappers for json_encode
static int json_encode_v0(lua_State* l)
{
    int nargs = lua_gettop(l);
    luaL_argcheck(l, nargs >= 1 && nargs <= 2, 1, "expected 1-2 arguments");
    if (nargs >= 2)
        luaL_checktype(l, 2, LUA_TTABLE);
    return json_encode_common(l, true, false);
}
static int json_encode_v0_k(lua_State* l, int)
{
    lua_checkstack(l, LUA_MINSTACK);
    return json_encode_common(l, false, false);
}
static int json_encode_sl_v0(lua_State* l)
{
    int nargs = lua_gettop(l);
    luaL_checkany(l, 1);
    if (nargs >= 2)
        luaL_checktype(l, 2, LUA_TTABLE);
    return json_encode_common(l, true, true);
}
static int json_encode_sl_v0_k(lua_State* l, int)
{
    lua_checkstack(l, LUA_MINSTACK);
    return json_encode_common(l, false, true);
}

/* ===== DECODING ===== */

static int hexdigit2int(char hex)
{
    if ('0' <= hex  && hex <= '9')
        return hex - '0';

    /* Force lowercase */
    hex |= 0x20;
    if ('a' <= hex && hex <= 'f')
        return 10 + hex - 'a';

    return -1;
}

static int decode_hex4(const char *hex)
{
    int digit[4];
    int i;

    /* Convert ASCII hex digit to numeric digit
     * Note: this returns an error for invalid hex digits, including
     *       NULL */
    for (i = 0; i < 4; i++) {
        digit[i] = hexdigit2int(hex[i]);
        if (digit[i] < 0) {
            return -1;
        }
    }

    return (digit[0] << 12) +
           (digit[1] << 8) +
           (digit[2] << 4) +
            digit[3];
}

// Helper to parse a tight component (empty string = 0.0f)
static float parse_tight_component(const char **ptr, char delimiter) {
    const char *p = *ptr;
    if (*p == delimiter || *p == '\0') {
        // Empty component = 0.0f
        return 0.0f;
    }
    char *end;
    float val = strtof(p, &end);
    *ptr = end;
    return val;
}

// Parse a tagged string and push the appropriate value onto the Lua stack.
// Returns true if the string was tagged and a value was pushed, false if it's a plain string.
// Throws Lua error on malformed tagged strings.
static bool json_parse_tagged_string(lua_State *l, const char *str, size_t len)
{
    if (len < 2 || str[0] != '!')
        return false;

    char tag = str[1];
    const char *payload = str + 2;
    size_t payload_len = len - 2;

    switch (tag) {
    case 'n':
        // Nil: !n
        if (payload_len != 0)
            luaL_error(l, "malformed tagged nil: %s", str);
        lua_pushnil(l);
        return true;

    case '!':
        // Escaped '!' - push string with leading '!' stripped
        lua_pushlstring(l, str + 1, len - 1);
        return true;

    case 'v': {
        // Vector: !v<x,y,z> (normal) or !v1,2,3 (tight) or !v (ZERO_VECTOR)
        float x, y, z;

        if (payload_len == 0) {
            // ZERO_VECTOR shorthand
            lua_pushvector(l, 0.0f, 0.0f, 0.0f);
            return true;
        }

        if (payload[0] == '<') {
            // Normal format with brackets
            if (payload_len < 5 || payload[payload_len - 1] != '>')
                luaL_error(l, "malformed tagged vector: %s", str);

            char *end;
            x = strtof(payload + 1, &end);
            if (*end != ',')
                luaL_error(l, "malformed tagged vector: %s", str);
            y = strtof(end + 1, &end);
            if (*end != ',')
                luaL_error(l, "malformed tagged vector: %s", str);
            z = strtof(end + 1, &end);
            if (*end != '>')
                luaL_error(l, "malformed tagged vector: %s", str);
        } else {
            // Tight format: !v1,2,3 or !v,,1 (empty = 0)
            const char *p = payload;
            x = parse_tight_component(&p, ',');
            if (*p != ',')
                luaL_error(l, "malformed tagged vector: %s", str);
            p++;
            y = parse_tight_component(&p, ',');
            if (*p != ',')
                luaL_error(l, "malformed tagged vector: %s", str);
            p++;
            z = parse_tight_component(&p, '\0');
        }

        lua_pushvector(l, x, y, z);
        return true;
    }

    case 'q': {
        // Quaternion: !q<x,y,z,w> (normal) or !q,,,1 (tight) or !q (ZERO_ROTATION)
        float x, y, z, w;

        if (payload_len == 0) {
            // ZERO_ROTATION shorthand (0,0,0,1)
            luaSL_pushquaternion(l, 0.0f, 0.0f, 0.0f, 1.0f);
            return true;
        }

        if (payload[0] == '<') {
            // Normal format with brackets
            if (payload_len < 7 || payload[payload_len - 1] != '>')
                luaL_error(l, "malformed tagged quaternion: %s", str);

            char *end;
            x = strtof(payload + 1, &end);
            if (*end != ',')
                luaL_error(l, "malformed tagged quaternion: %s", str);
            y = strtof(end + 1, &end);
            if (*end != ',')
                luaL_error(l, "malformed tagged quaternion: %s", str);
            z = strtof(end + 1, &end);
            if (*end != ',')
                luaL_error(l, "malformed tagged quaternion: %s", str);
            w = strtof(end + 1, &end);
            if (*end != '>')
                luaL_error(l, "malformed tagged quaternion: %s", str);
        } else {
            // Tight format: !q,,,1 (empty = 0)
            const char *p = payload;
            x = parse_tight_component(&p, ',');
            if (*p != ',')
                luaL_error(l, "malformed tagged quaternion: %s", str);
            p++;
            y = parse_tight_component(&p, ',');
            if (*p != ',')
                luaL_error(l, "malformed tagged quaternion: %s", str);
            p++;
            z = parse_tight_component(&p, ',');
            if (*p != ',')
                luaL_error(l, "malformed tagged quaternion: %s", str);
            p++;
            w = parse_tight_component(&p, '\0');
        }

        luaSL_pushquaternion(l, x, y, z, w);
        return true;
    }

    case 'u':
        // UUID: !u (null), !u<base64> (22 chars), or !uxxxxxxxx-xxxx-... (36 chars)
        if (payload_len == 0) {
            // Tight null UUID (all zeros)
            static const uint8_t null_uuid[16] = {0};
            luaSL_pushuuidbytes(l, null_uuid);
        } else if (payload_len == 22) {
            // Tight format: base64 encoded UUID
            // Add padding back for decoding
            char padded[25];
            memcpy(padded, payload, 22);
            padded[22] = '=';
            padded[23] = '=';
            padded[24] = '\0';

            uint8_t uuid_bytes[16];
            int decoded_len = apr_base64_decode_binary(uuid_bytes, padded);
            if (decoded_len != 16)
                luaL_error(l, "malformed base64 UUID: %s", str);

            luaSL_pushuuidbytes(l, uuid_bytes);
        } else if (payload_len == 36) {
            // Normal format: UUID string (e.g. "12345678-1234-1234-1234-123456789abc")
            uint8_t uuid_bytes[16];
            if (!parse_uuid_to_bytes(payload, payload_len, uuid_bytes))
                luaL_error(l, "malformed tagged UUID: %s", str);
            luaSL_pushuuidbytes(l, uuid_bytes);
        } else {
            luaL_error(l, "malformed tagged UUID: %s", str);
        }
        return true;

    case 'f': {
        // Float: !f3.14 or !fNaN or !f1e9999 (infinity)
        char *end;
        double num = fpconv_strtod(payload, &end);
        if (end == payload)
            luaL_error(l, "malformed tagged float: %s", str);
        lua_pushnumber(l, num);
        return true;
    }

    case 'b':
        // Boolean: !b1 or !b0
        if (payload_len == 1 && payload[0] == '1') {
            lua_pushboolean(l, 1);
            return true;
        } else if (payload_len == 1 && payload[0] == '0') {
            lua_pushboolean(l, 0);
            return true;
        }
        luaL_error(l, "malformed tagged boolean: %s", str);
        return false;

    case 'd':
    {
        // Buffer (data): !d<base64 data>

        if (payload_len > 0)
        {
            std::vector<uint8_t> decode_buf(payload_len);
            size_t new_len = apr_base64_decode_binary(decode_buf.data(), payload);
            if (new_len > 0)
            {
                void* buf_data = lua_newbuffer(l, new_len);
                memcpy(buf_data, decode_buf.data(), new_len);
                return true;
            }
        }
        lua_newbuffer(l, 0);
        return true;
    }

    default:
        luaL_error(l, "unknown tag '!%c' in string: %s", tag, str);
        return false; // unreachable
    }
}

/* Converts a Unicode codepoint to UTF-8.
 * Returns UTF-8 string length, and up to 4 bytes in *utf8 */
static int codepoint_to_utf8(char *utf8, int codepoint)
{
    /* 0xxxxxxx */
    if (codepoint <= 0x7F) {
        utf8[0] = codepoint;
        return 1;
    }

    /* 110xxxxx 10xxxxxx */
    if (codepoint <= 0x7FF) {
        utf8[0] = (codepoint >> 6) | 0xC0;
        utf8[1] = (codepoint & 0x3F) | 0x80;
        return 2;
    }

    /* 1110xxxx 10xxxxxx 10xxxxxx */
    if (codepoint <= 0xFFFF) {
        utf8[0] = (codepoint >> 12) | 0xE0;
        utf8[1] = ((codepoint >> 6) & 0x3F) | 0x80;
        utf8[2] = (codepoint & 0x3F) | 0x80;
        return 3;
    }

    /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (codepoint <= 0x1FFFFF) {
        utf8[0] = (codepoint >> 18) | 0xF0;
        utf8[1] = ((codepoint >> 12) & 0x3F) | 0x80;
        utf8[2] = ((codepoint >> 6) & 0x3F) | 0x80;
        utf8[3] = (codepoint & 0x3F) | 0x80;
        return 4;
    }

    return 0;
}


/* Called when index pointing to beginning of UTF-16 code escape: \uXXXX
 * \u is guaranteed to exist, but the remaining hex characters may be
 * missing.
 * Translate to UTF-8 and append to temporary token string.
 * Must advance index to the next character to be processed.
 * Returns: 0   success
 *          -1  error
 */
static int json_append_unicode_escape(json_parse_t *json)
{
    char utf8[4];       /* Surrogate pairs require 4 UTF-8 bytes */
    int codepoint;
    int surrogate_low;
    int len;
    int escape_len = 6;

    /* Fetch UTF-16 code unit */
    codepoint = decode_hex4(json->ptr + 2);
    if (codepoint < 0)
        return -1;

    /* UTF-16 surrogate pairs take the following 2 byte form:
     *      11011 x yyyyyyyyyy
     * When x = 0: y is the high 10 bits of the codepoint
     *      x = 1: y is the low 10 bits of the codepoint
     *
     * Check for a surrogate pair (high or low) */
    if ((codepoint & 0xF800) == 0xD800) {
        /* Error if the 1st surrogate is not high */
        if (codepoint & 0x400)
            return -1;

        /* Ensure the next code is a unicode escape.
         * Check for null first to be defensive against truncated input. */
        char next_char = *(json->ptr + escape_len);
        if (next_char == '\0' || next_char != '\\')
            return -1;
        next_char = *(json->ptr + escape_len + 1);
        if (next_char == '\0' || next_char != 'u')
            return -1;

        /* Fetch the next codepoint */
        surrogate_low = decode_hex4(json->ptr + 2 + escape_len);
        if (surrogate_low < 0)
            return -1;

        /* Error if the 2nd code is not a low surrogate */
        if ((surrogate_low & 0xFC00) != 0xDC00)
            return -1;

        /* Calculate Unicode codepoint */
        codepoint = (codepoint & 0x3FF) << 10;
        surrogate_low &= 0x3FF;
        codepoint = (codepoint | surrogate_low) + 0x10000;
        escape_len = 12;
    }

    /* Convert codepoint to UTF-8 */
    len = codepoint_to_utf8(utf8, codepoint);
    if (!len)
        return -1;

    /* Append bytes and advance parse index */
    strbuf_append_mem_unsafe(json->tmp, utf8, len);
    json->ptr += escape_len;

    return 0;
}

static void json_set_token_error(json_token_t *token, json_parse_t *json,
                                 const char *errtype)
{
    token->type = T_ERROR;
    token->index = json->ptr - json->data;
    token->value.string = errtype;
}

static void json_next_string_token(json_parse_t *json, json_token_t *token)
{
    const char *escape2char = json_escape2char;
    char ch;

    /* Caller must ensure a string is next */
    LUAU_ASSERT(*json->ptr == '"');

    /* Skip " */
    json->ptr++;

    /* json->tmp is the temporary strbuf used to accumulate the
     * decoded string value.
     * json->tmp is sized to handle JSON containing only a string value.
     */
    strbuf_reset(json->tmp);

    while ((ch = *json->ptr) != '"') {
        if (!ch) {
            /* Premature end of the string */
            json_set_token_error(token, json, "unexpected end of string");
            return;
        }

        /* Handle escapes */
        if (ch == '\\') {
            /* Fetch escape character - check for truncated input */
            ch = *(json->ptr + 1);
            if (!ch) {
                json_set_token_error(token, json, "unexpected end of string after escape");
                return;
            }

            /* Translate escape code and append to tmp string */
            ch = escape2char[(unsigned char)ch];
            if (ch == 'u') {
                if (json_append_unicode_escape(json) == 0)
                    continue;

                json_set_token_error(token, json,
                                     "invalid unicode escape code");
                return;
            }
            if (!ch) {
                json_set_token_error(token, json, "invalid escape code");
                return;
            }

            /* Skip '\' */
            json->ptr++;
        }
        /* Append normal character or translated single character
         * Unicode escapes are handled above */
        strbuf_append_char_unsafe(json->tmp, ch);
        json->ptr++;
    }
    json->ptr++;    /* Eat final quote (") */

    strbuf_ensure_null(json->tmp);

    token->type = T_STRING;
    token->value.string = strbuf_string(json->tmp, &token->string_len);
}

/* JSON numbers should take the following form:
 *      -?(0|[1-9]|[1-9][0-9]+)(.[0-9]+)?([eE][-+]?[0-9]+)?
 *
 * json_next_number_token() uses strtod() which allows other forms:
 * - numbers starting with '+'
 * - NaN, -NaN, infinity, -infinity
 * - hexadecimal numbers
 * - numbers with leading zeros
 *
 * json_is_invalid_number() detects "numbers" which may pass strtod()'s
 * error checking, but should not be allowed with strict JSON.
 *
 * json_is_invalid_number() may pass numbers which cause strtod()
 * to generate an error.
 */
static int json_is_invalid_number(json_parse_t *json)
{
    const char *p = json->ptr;

    /* Reject numbers starting with + */
    if (*p == '+')
        return 1;

    /* Skip minus sign if it exists */
    if (*p == '-')
        p++;

    /* Reject numbers starting with 0x, or leading zeros */
    if (*p == '0') {
        int ch2 = *(p + 1);

        if ((ch2 | 0x20) == 'x' ||          /* Hex */
            ('0' <= ch2 && ch2 <= '9'))     /* Leading zero */
            return 1;

        return 0;
    } else if (*p <= '9') {
        return 0;                           /* Ordinary number */
    }

    /* Reject inf/nan */
    if (!strncasecmp(p, "inf", 3))
        return 1;
    if (!strncasecmp(p, "nan", 3))
        return 1;

    /* Pass all other numbers which may still be invalid, but
     * strtod() will catch them. */
    return 0;
}

static void json_next_number_token(json_parse_t *json, json_token_t *token)
{
    char *endptr;
    long long tmpval = strtoll(json->ptr, &endptr, 10);
    if (json->ptr == endptr || *endptr == '.' || *endptr == 'e' ||
        *endptr == 'E' || *endptr == 'x') {
        token->type = T_NUMBER;
        token->value.number = fpconv_strtod(json->ptr, &endptr);
        if (json->ptr == endptr) {
            json_set_token_error(token, json, "invalid number");
            return;
        }
    } else if (tmpval > INT32_MAX || tmpval < INT32_MIN) {
        /* Typical Lua builds typedef ptrdiff_t to lua_Integer. If tmpval is
         * outside the range of that type, we need to use T_NUMBER to avoid
         * truncation.
         */
        // ServerLua: In our case, it's actually `typedef`'d to `int`,
        // but similar logic applies.
        token->type = T_NUMBER;
        token->value.number = (double)tmpval;
    } else {
        token->type = T_INTEGER;
        token->value.integer = (int)tmpval;
    }
    json->ptr = endptr;     /* Skip the processed number */

    return;
}

/* Fills in the token struct.
 * T_STRING will return a pointer to the json_parse_t temporary string
 * T_ERROR will leave the json->ptr pointer at the error.
 */
static void json_next_token(json_parse_t *json, json_token_t *token)
{
    const json_token_type_t *ch2token = json_ch2token;
    int ch;

    /* Eat whitespace. */
    while (1) {
        ch = (unsigned char)*(json->ptr);
        token->type = ch2token[ch];
        if (token->type != T_WHITESPACE)
            break;
        json->ptr++;
    }

    /* Store location of new token. Required when throwing errors
     * for unexpected tokens (syntax errors). */
    token->index = json->ptr - json->data;

    /* Don't advance the pointer for an error or the end */
    if (token->type == T_ERROR) {
        json_set_token_error(token, json, "invalid token");
        return;
    }

    if (token->type == T_END) {
        return;
    }

    /* Found a known single character token, advance index and return */
    if (token->type != T_UNKNOWN) {
        json->ptr++;
        return;
    }

    /* Process characters which triggered T_UNKNOWN
     *
     * Must use strncmp() to match the front of the JSON string.
     * JSON identifier must be lowercase.
     * When strict_numbers if disabled, either case is allowed for
     * Infinity/NaN (since we are no longer following the spec..) */
    if (ch == '"') {
        json_next_string_token(json, token);
        return;
    } else if (ch == '-' || ('0' <= ch && ch <= '9')) {
        json_next_number_token(json, token);
        return;
    } else if (!strncmp(json->ptr, "true", 4)) {
        token->type = T_BOOLEAN;
        token->value.boolean = 1;
        json->ptr += 4;
        return;
    } else if (!strncmp(json->ptr, "false", 5)) {
        token->type = T_BOOLEAN;
        token->value.boolean = 0;
        json->ptr += 5;
        return;
    } else if (!strncmp(json->ptr, "null", 4)) {
        token->type = T_NULL;
        json->ptr += 4;
        return;
    } else if (json_is_invalid_number(json)) {
        /* Only attempt to process numbers we know are invalid JSON (Inf, NaN, hex).
         * This is required to generate an appropriate token error,
         * otherwise all bad tokens will register as "invalid number" */
        json_next_number_token(json, token);
        return;
    }

    /* Token starts with t/f/n but isn't recognised above. */
    json_set_token_error(token, json, "invalid token");
}

/* This function does not return.
 * DO NOT CALL WITH DYNAMIC MEMORY ALLOCATED.
 * The only supported exception is the temporary parser string
 * json->tmp struct.
 * json and token should exist on the stack somewhere.
 * luaL_error() will long_jmp and release the stack */
static void json_throw_parse_error(lua_State *l, json_parse_t *json,
                                   const char *exp, json_token_t *token)
{
    const char *found;

    if (token->type == T_ERROR)
        found = token->value.string;
    else
        found = json_token_type_name[token->type];

    /* Note: token->index is 0 based, display starting from 1 */
    luaL_error(l, "Expected %s but found %s at character %d",
               exp, found, (int)(token->index + 1));
}

static inline void json_decode_ascend(json_parse_t *json)
{
    json->current_depth--;
}

static void json_decode_descend(lua_State *l, json_parse_t *json, int slots)
{
    json->current_depth++;

    if (json->current_depth <= JSON_MAX_DEPTH &&
        lua_checkstack(l, slots)) {
        return;
    }

    luaL_error(l, "Found too many nested data structures (%d) at character %d",
        json->current_depth, (int)(json->ptr - json->data));
}

static inline int32_t json_get_offset(json_parse_t* json)
{
    return (int32_t)(json->ptr - json->data);
}

static inline void json_restore_offset(json_parse_t* json, int32_t ptr_offset)
{
    json->ptr = json->data + ptr_offset;
}

// ServerLua: Parse object key string + colon, push key onto Lua stack.
static void json_parse_object_key(lua_State* l, json_parse_t* json)
{
    json_token_t token;
    json_next_token(json, &token);
    if (token.type != T_STRING)
        json_throw_parse_error(l, json, "object key string", &token);

    if (json->cfg->sl_tagged_types &&
        json_parse_tagged_string(l, token.value.string, token.string_len)) {
    } else {
        lua_pushlstring(l, token.value.string, token.string_len);
    }

    json_next_token(json, &token);
    if (token.type != T_COLON)
        json_throw_parse_error(l, json, "colon", &token);
}

// ServerLua: Forward declarations for yieldable decode helpers
static void json_process_value(lua_State* l, SlotManager& parent_slots,
                                json_parse_t* json, json_token_t* token);
static void json_parse_object_context(lua_State* l, SlotManager& parent_slots, json_parse_t* json);
static void json_parse_array_context(lua_State* l, SlotManager& parent_slots, json_parse_t* json);

static void json_parse_object_context(lua_State* l, SlotManager& parent_slots, json_parse_t* json)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        VALUE = 1,
        NEXT_PAIR = 2,
        REVIVER_CHECK = 3,
        REVIVER_CALL = 4,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, ptr_offset, json_get_offset(json));
    slots.finalize();

    json_restore_offset(json, ptr_offset);
    // ServerLua: 7 slots for table, key, value + reviver call args
    json_decode_descend(l, json, 7);

    if (slots.isInit()) {
        lua_newtable(l);

        json_token_t token;
        json_next_token(json, &token);

        /* Handle empty objects */
        if (token.type == T_OBJ_END) {
            json_decode_ascend(json);
            return;
        }

        /* Rewind - let json_parse_object_key re-parse from the key token */
        json->ptr = json->data + ptr_offset;
        json_parse_object_key(l, json);

        /* Save offset after colon - process_value will parse the value token */
        ptr_offset = json_get_offset(json);
    }

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(VALUE);
    YIELD_DISPATCH(NEXT_PAIR);
    YIELD_DISPATCH(REVIVER_CHECK);
    YIELD_DISPATCH(REVIVER_CALL);
    YIELD_DISPATCH_END();

    while (1) {
        /* Fetch value */
        /* Stack before: [..., table, key] */
        {
            json_token_t token;
            YIELD_HELPER(l, VALUE,
                json_process_value(l, slots, json, &token));
        }
        /* Stack after: [..., table, key, value] */

        // Save offset past the parsed value so reviver yields resume correctly
        ptr_offset = json_get_offset(json);

        if (json->has_reviver) {
            // Call reviver(key, value, parent)
            lua_pushvalue(l, (int)DecodeStack::REVIVER);
            lua_pushvalue(l, -3);  // key
            lua_pushvalue(l, -3);  // value
            lua_pushvalue(l, -6);  // parent (the object table)
            YIELD_CHECK(l, REVIVER_CHECK, LUA_INTERRUPT_LLLIB);
            YIELD_CALL(l, 3, 1, REVIVER_CALL);
            // Stack: [..., table, key, value, result]
            if (lua_tolightuserdatatagged(l, -1, LU_TAG_JSON_INTERNAL) == JSON_REMOVE) {
                // Omit this key/value pair entirely
                lua_pop(l, 3); // pop result, value, key
            } else {
                // Replace value with result, then rawset
                lua_remove(l, -2); // remove original value
                // Stack: [..., table, key, result]
                lua_rawset(l, -3);
            }
        } else {
            /* Set key = value */
            lua_rawset(l, -3);
        }

        json_token_t token;
        json_next_token(json, &token);

        if (token.type == T_OBJ_END) {
            json_decode_ascend(json);
            return;
        }

        if (token.type != T_COMMA)
            json_throw_parse_error(l, json, "comma or object end", &token);

        json_parse_object_key(l, json);

        /* Save offset after colon - process_value will parse the value */
        ptr_offset = json_get_offset(json);

        YIELD_CHECK(l, NEXT_PAIR, LUA_INTERRUPT_LLLIB);
    }
}

static void json_parse_array_context(lua_State* l, SlotManager& parent_slots, json_parse_t* json)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        ELEMENT = 1,
        NEXT_ELEMENT = 2,
        REVIVER_CHECK = 3,
        REVIVER_CALL = 4,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, ptr_offset, json_get_offset(json));
    DEFINE_SLOT(int32_t, i, 1);
    // Track the next index for insertion (may differ from i when reviver removes elements)
    DEFINE_SLOT(int32_t, insert_idx, 1);
    slots.finalize();

    json_restore_offset(json, ptr_offset);
    // ServerLua: 6 slots for table, value + reviver call args
    json_decode_descend(l, json, 6);

    if (slots.isInit()) {
        lua_newtable(l);

        /* Peek at first token - check for empty array */
        const char* before = json->ptr;
        json_token_t token;
        json_next_token(json, &token);

        /* Handle empty arrays */
        if (token.type == T_ARR_END) {
            json_decode_ascend(json);
            return;
        }

        /* Restore ptr - process_value will parse the first element token */
        json->ptr = before;
        ptr_offset = json_get_offset(json);
    }

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(ELEMENT);
    YIELD_DISPATCH(NEXT_ELEMENT);
    YIELD_DISPATCH(REVIVER_CHECK);
    YIELD_DISPATCH(REVIVER_CALL);
    YIELD_DISPATCH_END();

    for (; ; i++) {
        {
            json_token_t token;
            YIELD_HELPER(l, ELEMENT,
                json_process_value(l, slots, json, &token));
        }
        /* Stack: [..., table, value] */

        // Save offset past the parsed value so reviver yields resume correctly
        ptr_offset = json_get_offset(json);

        if (json->has_reviver) {
            lua_pushvalue(l, (int)DecodeStack::REVIVER);
            lua_pushinteger(l, i);     // key (1-based source index)
            lua_pushvalue(l, -3);      // value
            lua_pushvalue(l, -5);      // parent (the array table)
            YIELD_CHECK(l, REVIVER_CHECK, LUA_INTERRUPT_LLLIB);
            YIELD_CALL(l, 3, 1, REVIVER_CALL);
            // Stack: [..., table, value, result]
            if (lua_tolightuserdatatagged(l, -1, LU_TAG_JSON_INTERNAL) == JSON_REMOVE) {
                // Omit this element - don't insert, don't increment insert_idx
                lua_pop(l, 2); // pop result and value
            } else {
                // Replace value with result
                lua_remove(l, -2); // remove original value
                // Stack: [..., table, result]
                lua_rawseti(l, -2, insert_idx);
                insert_idx++;
            }
        } else {
            lua_rawseti(l, -2, i);    /* arr[i] = value */
        }

        json_token_t token;
        json_next_token(json, &token);

        if (token.type == T_ARR_END) {
            // ServerLua: shrink the array to fit the contents, if necessary
            int final_len = json->has_reviver ? (insert_idx - 1) : i;
            LuaTable *t = hvalue(luaA_toobject(l, -1));
            if (t->sizearray != final_len && !t->readonly)
            {
                luaH_resizearray(l, t, final_len);
            }

            json_decode_ascend(json);
            return;
        }

        if (token.type != T_COMMA)
            json_throw_parse_error(l, json, "comma or array end", &token);

        /* Save offset after comma - process_value will parse the next element */
        ptr_offset = json_get_offset(json);

        YIELD_CHECK(l, NEXT_ELEMENT, LUA_INTERRUPT_LLLIB);
    }
}

// ServerLua: Yieldable json_process_value.
static void json_process_value(lua_State* l, SlotManager& parent_slots,
                                json_parse_t* json, json_token_t* token)
{
    YIELDABLE_RETURNS_VOID;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        OBJECT = 1,
        ARRAY = 2,
    };

    SlotManager slots(parent_slots);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, ptr_offset, json_get_offset(json));
    slots.finalize();

    json_restore_offset(json, ptr_offset);

    /* Parse token (re-parsed on resume from saved ptr_offset) */
    json_next_token(json, token);
    if (slots.isInit())
        ptr_offset = json_get_offset(json);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(OBJECT);
    YIELD_DISPATCH(ARRAY);
    YIELD_DISPATCH_END();

    lua_checkstack(l, 1);
    switch (token->type) {
    case T_STRING:
        if (json->cfg->sl_tagged_types &&
            json_parse_tagged_string(l, token->value.string, token->string_len)) {
            // Tagged value was pushed
        } else {
            lua_pushlstring(l, token->value.string, token->string_len);
        }
        break;
    case T_NUMBER:
        lua_pushnumber(l, token->value.number);
        break;
    case T_INTEGER:
        lua_pushinteger(l, token->value.integer);
        break;
    case T_BOOLEAN:
        lua_pushboolean(l, token->value.boolean);
        break;
    case T_OBJ_BEGIN:
        YIELD_HELPER(l, OBJECT,
            json_parse_object_context(l, slots, json));
        break;
    case T_ARR_BEGIN:
        YIELD_HELPER(l, ARRAY,
            json_parse_array_context(l, slots, json));
        break;
    case T_NULL:
        /* In Lua, setting "t[k] = nil" will delete k from the table.
         * Hence a NULL pointer lightuserdata object is used instead */
        lua_pushlightuserdatatagged(l, JSON_NULL, LU_TAG_JSON_INTERNAL);
        break;
    default:
        json_throw_parse_error(l, json, "value", token);
    }
}

// ServerLua: Shared yieldable body for json_decode / json_decode_sl.
// sl_tagged selects between standard JSON and SL tagged type decoding.
static int json_decode_common(lua_State* l, bool is_init, bool sl_tagged)
{
    YIELDABLE_RETURNS_DEFAULT;
    enum class Phase : uint8_t
    {
        DEFAULT = 0,
        PROCESS_VALUE = 1,
        ROOT_REVIVER_CHECK = 2,
        ROOT_REVIVER_CALL = 3,
    };

    SlotManager slots(l, is_init);
    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, ptr_offset, 0);
    slots.finalize();

    json_config_t cfg;
    if (sl_tagged) {
        cfg.sl_tagged_types = true;
    }

    if (is_init) {
        /* Args already validated by init wrapper.
         * SlotManager inserted nil at pos 1, original args shifted to pos 2+.
         * Stack: [opaque(1), input_string(2), reviver?(3)] */

        // Ensure reviver or nil occupies a stack slot (will become pos 4 after strbuf insert)
        if (lua_gettop(l) < 3)
            lua_pushnil(l);

        // ServerLua: Create decode scratch buffer in memcat 1 - it's an
        // internal intermediary, not user-visible output. Pre-size to
        // json_len so _unsafe appends can't overflow (decoded <= input).
        // This is only safe because JSON escapes can never produce output
        // larger than the escape sequence.
        {
            // Normally we would use memcat 0 for things that aren't to be "charged"
            // against the user's memory usage, but use 1 so we can distinguish it
            // from more mundane VM-internal allocations.
            [[maybe_unused]] MemcatGuard mcg(l, 1);
            luaYB_push(l);
        }
        lua_insert(l, (int)DecodeStack::STRBUF);
        /* Stack: [opaque(1), strbuf(2), input_string(3), reviver_or_nil(4)] */

        size_t json_len;
        const char* json_data = lua_tolstring(l, (int)DecodeStack::INPUT, &json_len);

        /* Detect Unicode other than UTF-8 (see RFC 4627, Sec 3)
         *
         * CJSON can support any simple data type, hence only the first
         * character is guaranteed to be ASCII (at worst: '"'). This is
         * still enough to detect whether the wrong encoding is in use. */
        if (json_len >= 2 && (!json_data[0] || !json_data[1]))
            luaL_error(l, "JSON parser does not support UTF-16 or UTF-32");

        if (json_len > DEFAULT_MAX_SIZE)
            luaL_errorL(l, "JSON too large to decode");

        strbuf_ensure_empty_length(json_get_strbuf(l), json_len);

        lua_hardenstack(l, 1);
    }

    /* Reconstruct json_parse_t from stack and slots */
    size_t json_len;
    const char* json_data = lua_tolstring(l, (int)DecodeStack::INPUT, &json_len);
    strbuf_t* tmp = json_get_strbuf(l);

    json_parse_t json;
    json.cfg = &cfg;
    json.data = json_data;
    json.ptr = json_data + ptr_offset;
    json.current_depth = 0;
    json.tmp = tmp;
    json.has_reviver = !lua_isnil(l, (int)DecodeStack::REVIVER);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(PROCESS_VALUE);
    YIELD_DISPATCH(ROOT_REVIVER_CHECK);
    YIELD_DISPATCH(ROOT_REVIVER_CALL);
    YIELD_DISPATCH_END();

    {
        json_token_t token;
        YIELD_HELPER(l, PROCESS_VALUE,
            json_process_value(l, slots, &json, &token));
    }

    ptr_offset = (int32_t)(json.ptr - json.data);

    /* Ensure there is no more input left */
    {
        json_token_t token;
        json_next_token(&json, &token);
        if (token.type != T_END)
            json_throw_parse_error(l, &json, "the end", &token);
    }

    // Call reviver on root value if present
    if (json.has_reviver) {
        // Stack: [..., decoded_value]
        lua_checkstack(l, 4);
        lua_pushvalue(l, (int)DecodeStack::REVIVER);  // reviver
        lua_pushnil(l);        // key = nil (root)
        lua_pushvalue(l, -3);  // decoded value
        lua_pushnil(l);        // parent = nil (root)
        YIELD_CHECK(l, ROOT_REVIVER_CHECK, LUA_INTERRUPT_LLLIB);
        YIELD_CALL(l, 3, 1, ROOT_REVIVER_CALL);
        // Stack: [..., decoded_value, result]
        if (lua_tolightuserdatatagged(l, -1, LU_TAG_JSON_INTERNAL) == JSON_REMOVE) {
            lua_pop(l, 2);
            // Return lljson.null - decode must return a value
            lua_pushlightuserdatatagged(l, JSON_NULL, LU_TAG_JSON_INTERNAL);
        } else {
            // Replace decoded value with result
            lua_remove(l, -2);
        }
    }

    luau_interruptoncalltail(l);
    return 1;
}

// ServerLua: init / continuation wrappers for json_decode
static int json_decode_v0(lua_State* l)
{
    int nargs = lua_gettop(l);
    luaL_argcheck(l, nargs >= 1 && nargs <= 2, 1, "expected 1-2 arguments");
    luaL_checkstring(l, 1);
    if (nargs >= 2)
        luaL_checktype(l, 2, LUA_TFUNCTION);
    return json_decode_common(l, true, false);
}
static int json_decode_v0_k(lua_State* l, int)
{
    lua_checkstack(l, LUA_MINSTACK);
    return json_decode_common(l, false, false);
}
static int json_decode_sl_v0(lua_State* l)
{
    int nargs = lua_gettop(l);
    luaL_argcheck(l, nargs >= 1 && nargs <= 2, 1, "expected 1-2 arguments");
    luaL_checkstring(l, 1);
    if (nargs >= 2)
        luaL_checktype(l, 2, LUA_TFUNCTION);
    return json_decode_common(l, true, true);
}
static int json_decode_sl_v0_k(lua_State* l, int)
{
    lua_checkstack(l, LUA_MINSTACK);
    return json_decode_common(l, false, true);
}

/* ===== INITIALISATION ===== */

/* Return cjson module table */
static int lua_cjson_new(lua_State *l)
{
    /* Initialise lookup tables and number conversions */
    json_init_lookup_tables();
    fpconv_init();

    lua_setlightuserdataname(l, LU_TAG_JSON_INTERNAL, "lljson_constant");

    // ServerLua: intern strings used per-call during encode/decode init
    luaS_fix(luaS_newliteral(l, "mode"));
    luaS_fix(luaS_newliteral(l, "json"));
    luaS_fix(luaS_newliteral(l, "sljson"));
    luaS_fix(luaS_newliteral(l, "tight"));
    luaS_fix(luaS_newliteral(l, "replacer"));
    luaS_fix(luaS_newliteral(l, "__tojson"));
    luaS_fix(luaS_newliteral(l, "__jsontype"));
    luaS_fix(luaS_newliteral(l, "array"));
    luaS_fix(luaS_newliteral(l, "object"));

    /* Test if array/object metatables are in registry */
    lua_pushlightuserdatatagged(l, JSON_ARRAY, LU_TAG_JSON_INTERNAL);
    lua_rawget(l, LUA_REGISTRYINDEX);
    if (lua_isnil(l, -1)) {
        /* Create shape metatables.
         *
         * If multiple calls to lua_cjson_new() are made,
         * this prevents overriding the tables at the given
         * registry's index with a new one.
         */
        lua_pop(l, 1);

        /* array_mt */
        lua_pushlightuserdatatagged(l, JSON_ARRAY, LU_TAG_JSON_INTERNAL);
        lua_newtable(l);
        lua_pushliteral(l, "array");
        lua_setfield(l, -2, "__jsontype");
        lua_setreadonly(l, -1, true);
        lua_fixvalue(l, -1);
        lua_rawset(l, LUA_REGISTRYINDEX);

        /* object_mt */
        lua_pushlightuserdatatagged(l, JSON_OBJECT, LU_TAG_JSON_INTERNAL);
        lua_newtable(l);
        lua_pushliteral(l, "object");
        lua_setfield(l, -2, "__jsontype");
        lua_setreadonly(l, -1, true);
        lua_fixvalue(l, -1);
        lua_rawset(l, LUA_REGISTRYINDEX);
    } else {
        lua_pop(l, 1);
    }

    /* cjson module table */
    lua_newtable(l);

    // ServerLua: Register with continuations for yieldable encode/decode
    lua_pushcclosurek(l, json_encode_v0, "encode", 0, json_encode_v0_k);
    lua_setfield(l, -2, "encode");
    lua_pushcclosurek(l, json_decode_v0, "decode", 0, json_decode_v0_k);
    lua_setfield(l, -2, "decode");
    lua_pushcclosurek(l, json_encode_sl_v0, "slencode", 0, json_encode_sl_v0_k);
    lua_setfield(l, -2, "slencode");
    lua_pushcclosurek(l, json_decode_sl_v0, "sldecode", 0, json_decode_sl_v0_k);
    lua_setfield(l, -2, "sldecode");

    /* Set cjson.null */
    lua_pushlightuserdatatagged(l, JSON_NULL, LU_TAG_JSON_INTERNAL);
    lua_setfield(l, -2, "null");

    /* Set cjson.remove - sentinel to omit values in reviver/replacer */
    lua_pushlightuserdatatagged(l, JSON_REMOVE, LU_TAG_JSON_INTERNAL);
    lua_setfield(l, -2, "remove");

    /* Set cjson.array_mt */
    lua_pushlightuserdatatagged(l, JSON_ARRAY, LU_TAG_JSON_INTERNAL);
    lua_rawget(l, LUA_REGISTRYINDEX);
    lua_setfield(l, -2, "array_mt");

    /* Set cjson.object_mt */
    lua_pushlightuserdatatagged(l, JSON_OBJECT, LU_TAG_JSON_INTERNAL);
    lua_rawget(l, LUA_REGISTRYINDEX);
    lua_setfield(l, -2, "object_mt");

    /* Set cjson.empty_array / empty_object - frozen tables with cloned shape metatables.
     * Cloned (not shared with array_mt/object_mt) to avoid Ares duplicate-permanent-object errors. */
    lua_newtable(l);
    lua_newtable(l);
    lua_pushliteral(l, "array");
    lua_setfield(l, -2, "__jsontype");
    lua_setreadonly(l, -1, true);
    lua_fixvalue(l, -1);
    lua_setmetatable(l, -2);
    lua_setreadonly(l, -1, true);
    lua_fixvalue(l, -1);
    lua_setfield(l, -2, "empty_array");

    lua_newtable(l);
    lua_newtable(l);
    lua_pushliteral(l, "object");
    lua_setfield(l, -2, "__jsontype");
    lua_setreadonly(l, -1, true);
    lua_fixvalue(l, -1);
    lua_setmetatable(l, -2);
    lua_setreadonly(l, -1, true);
    lua_fixvalue(l, -1);
    lua_setfield(l, -2, "empty_object");

    return 1;
}

int luaopen_cjson(lua_State *l)
{
    lua_cjson_new(l);

    /* Register a global "cjson" table. */
    lua_pushvalue(l, -1);
    lua_setglobal(l, CJSON_MODNAME);

    /* Return cjson table */
    return 1;
}

/* vi:ai et sw=4 ts=4:
 */
