#include "lua.h"
#include "lualib.h"
#include <vector>
#include <cstring>

#include "apr/apr_base64.h"

static int llluabase64_encode(lua_State *L)
{
    luaL_checkany(L, 1);
    int type = lua_type(L, 1);
    size_t base_length = 0;
    const char *data = nullptr;

    if (type == LUA_TBUFFER)
        data = (const char *)luaL_checkbuffer(L, 1, &base_length);
    else if (type == LUA_TSTRING)
        data = luaL_checklstring(L, 1, &base_length);
    else
        luaL_typeerror(L, 1, "string or buffer");

    if (base_length != 0)
    {
        std::vector<char> encode_buf((size_t)apr_base64_encode_len(base_length));
        size_t len = apr_base64_encode_binary(encode_buf.data(), (const uint8_t *)data, base_length);
        // Push the string excluding the trailing null
        if (len > 0)
        {
            lua_pushlstring(L, encode_buf.data(), len - 1);
            return 1;
        }
    }

    lua_pushstring(L, "");
    return 1;
}

static int llluabase64_decode(lua_State *L)
{
    size_t base_length = 0;
    const char *data = luaL_checklstring(L, 1, &base_length);
    bool as_buffer = (bool)luaL_optboolean(L, 2, false);

    if (base_length != 0)
    {
        std::vector<uint8_t> decode_buf(base_length);
        size_t len = apr_base64_decode_binary(decode_buf.data(), data);
        if (as_buffer)
        {
            void *ret_buf = lua_newbuffer(L, len);
            memcpy(ret_buf, (const void *)decode_buf.data(), len);
        }
        else
        {
            lua_pushlstring(L, (const char *) decode_buf.data(), len);
        }
    }
    else
    {
        if (as_buffer)
            lua_newbuffer(L, 0);
        else
            lua_pushstring(L, "");
    }
    return 1;
}

static const luaL_Reg to_register[] = {
    {"encode", llluabase64_encode},
    {"decode", llluabase64_decode},
    {nullptr, nullptr}
};

static const char *BASE64_LIB_NAME = "llbase64";

int luaopen_llbase64(lua_State *L)
{
    luaL_register(L, BASE64_LIB_NAME, to_register);
    return 1;
}
