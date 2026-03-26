#pragma once

#include "lua.h"

void luaX_graphheap(lua_State *L, const char *out);
void luaX_graphuserheap(lua_State *L, const char *out, const lua_OpaqueGCObjectSet *free_objects);
