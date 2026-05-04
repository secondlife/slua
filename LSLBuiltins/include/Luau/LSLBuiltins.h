#pragma once

// ServerLua: addition for builtin constants
void luauSL_init_global_builtins(const char* builtins_file);

void luauSL_lookup_constant_cb(const char* library, const char* member, void** constant);

void luaSL_set_constant_globals(lua_State *L);
