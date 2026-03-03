// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
// This code is based on Lua 5.x implementation licensed under MIT License; see lua_LICENSE.txt for details
#pragma once

#include <limits.h>
#include <stdint.h>

#include "luaconf.h"

#include "Luau/Common.h"

// internal assertions for in-house debugging
#define check_exp(c, e) (LUAU_ASSERT(c), (e))
#define api_check(l, e) LUAU_ASSERT(e)

#ifndef cast_to
#define cast_to(t, exp) ((t)(exp))
#endif

#define cast_byte(i) cast_to(uint8_t, (i))
#define cast_num(i) cast_to(double, (i))
#define cast_int(i) cast_to(int, (i))

/*
** type for virtual-machine instructions
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
typedef uint32_t Instruction;

/*
** macro to control inclusion of some hard tests on stack reallocation
*/
#if defined(HARDSTACKTESTS) && HARDSTACKTESTS
// ServerLua: added a level parameter
#define condhardstacktests(x, l) (HARDSTACKTESTS >= l ? (x) : (void)0)
#else
#define condhardstacktests(x, l) ((void)0)
#endif

// ServerLua: shorthand for simulating the stack reallocation a metamethod
// invocation would cause. Catches callers holding stale StkId across calls
// that might invoke metamethods but take a fast path instead.
// Motivated by a use-after-free in `table.find()` that held StkID across `lua_equal()`
// invocations, but it wasn't caught by hardstacktests since none of the testcases
// actually used a table with an abusive `__eq` that forced reallocs.
#define hardstacktests_tm_realloc(L) condhardstacktests(luaD_reallocstack((L), (L)->stacksize - EXTRA_STACK, 0), 2)

/*
** ServerLua: Consume all LUA_MINSTACK headroom so missing lua_checkstack() calls
** are exposed as stack overflows.
**
** n = number of top-of-stack elements to preserve above the padding.
** Use n > 0 when the function uses top-relative operations
*/
#if HARDSTACKTESTS
#define lua_hardenstack(L, n) \
    do { int _hst_top = lua_gettop(L); \
      LUAU_ASSERT((n) <= LUA_MINSTACK && (n) <= _hst_top); \
      lua_checkstack(L, LUA_MINSTACK); \
      for (int _hst_i = 0; _hst_i < LUA_MINSTACK - (n); _hst_i++) lua_pushnil(L); \
      for (int _hst_i = 0; _hst_i < (n); _hst_i++) \
          lua_pushvalue(L, _hst_top - (n) + 1 + _hst_i); \
    } while(0)
#else
#define lua_hardenstack(L, n) ((void)0)
#endif

/*
** macro to control inclusion of some hard tests on garbage collection
*/
#if defined(HARDMEMTESTS) && HARDMEMTESTS
#define condhardmemtests(x, l) (HARDMEMTESTS >= l ? (x) : (void)0)
#else
#define condhardmemtests(x, l) ((void)0)
#endif
