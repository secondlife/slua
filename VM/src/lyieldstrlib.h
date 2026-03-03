// ServerLua: Yieldable pattern-matching functions for the string library.
//
// These replace the base (non-yieldable) find/match/gmatch/gsub in
// luaopen_string via lua_pushcclosurek.  The base implementations remain
// in lstrlib.cpp and can be registered on a table for fuzz-testing via
// luaopen_string_base().
#pragma once

#include "lua.h"

LUAI_FUNC int yieldable_str_find_v0(lua_State* L);
LUAI_FUNC int yieldable_str_find_v0_k(lua_State* L, int status);

LUAI_FUNC int yieldable_str_match_v0(lua_State* L);
LUAI_FUNC int yieldable_str_match_v0_k(lua_State* L, int status);

LUAI_FUNC int yieldable_gmatch(lua_State* L);

LUAI_FUNC int yieldable_str_gsub_v0(lua_State* L);
LUAI_FUNC int yieldable_str_gsub_v0_k(lua_State* L, int status);
