// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
// This code is based on Lua 5.x implementation licensed under MIT License; see lua_LICENSE.txt for details
#pragma once

#include "lobject.h"

#include "Luau/Bytecode.h"

typedef int (*luau_FastFunction)(lua_State* L, StkId res, TValue* arg0, int nresults, StkId args, int nparams);

extern const luau_FastFunction luauF_table[256];

// ServerLua: builtins too cheap to be worth an interrupt check before them.
// A whitelist, so new builtins are checked until shown to belong here.
inline bool luauF_trivial(int bfid)
{
    switch (bfid)
    {
    case LBF_ASSERT:
    case LBF_MATH_ABS:
    case LBF_MATH_ACOS:
    case LBF_MATH_ASIN:
    case LBF_MATH_ATAN2:
    case LBF_MATH_ATAN:
    case LBF_MATH_CEIL:
    case LBF_MATH_COSH:
    case LBF_MATH_COS:
    case LBF_MATH_DEG:
    case LBF_MATH_EXP:
    case LBF_MATH_FLOOR:
    case LBF_MATH_FMOD:
    case LBF_MATH_FREXP:
    case LBF_MATH_LDEXP:
    case LBF_MATH_LOG10:
    case LBF_MATH_LOG:
    case LBF_MATH_MAX:
    case LBF_MATH_MIN:
    case LBF_MATH_MODF:
    case LBF_MATH_POW:
    case LBF_MATH_RAD:
    case LBF_MATH_SINH:
    case LBF_MATH_SIN:
    case LBF_MATH_SQRT:
    case LBF_MATH_TANH:
    case LBF_MATH_TAN:
    case LBF_BIT32_ARSHIFT:
    case LBF_BIT32_BAND:
    case LBF_BIT32_BNOT:
    case LBF_BIT32_BOR:
    case LBF_BIT32_BXOR:
    case LBF_BIT32_BTEST:
    case LBF_BIT32_EXTRACT:
    case LBF_BIT32_LROTATE:
    case LBF_BIT32_LSHIFT:
    case LBF_BIT32_REPLACE:
    case LBF_BIT32_RROTATE:
    case LBF_BIT32_RSHIFT:
    case LBF_TYPE:
    // Produces only as many values as the call has result registers
    case LBF_STRING_BYTE:
    case LBF_STRING_LEN:
    case LBF_TYPEOF:
    case LBF_MATH_CLAMP:
    case LBF_MATH_SIGN:
    case LBF_MATH_ROUND:
    case LBF_RAWGET:
    case LBF_RAWEQUAL:
    case LBF_VECTOR:
    case LBF_BIT32_COUNTLZ:
    case LBF_BIT32_COUNTRZ:
    // Single-result form only
    case LBF_SELECT_VARARG:
    case LBF_RAWLEN:
    case LBF_BIT32_EXTRACTK:
    case LBF_GETMETATABLE:
    case LBF_SETMETATABLE:
    case LBF_BIT32_BYTESWAP:
    case LBF_BUFFER_READI8:
    case LBF_BUFFER_READU8:
    case LBF_BUFFER_WRITEU8:
    case LBF_BUFFER_READI16:
    case LBF_BUFFER_READU16:
    case LBF_BUFFER_WRITEU16:
    case LBF_BUFFER_READI32:
    case LBF_BUFFER_READU32:
    case LBF_BUFFER_WRITEU32:
    case LBF_BUFFER_READF32:
    case LBF_BUFFER_WRITEF32:
    case LBF_BUFFER_READF64:
    case LBF_BUFFER_WRITEF64:
    case LBF_VECTOR_MAGNITUDE:
    case LBF_VECTOR_NORMALIZE:
    case LBF_VECTOR_CROSS:
    case LBF_VECTOR_DOT:
    case LBF_VECTOR_FLOOR:
    case LBF_VECTOR_CEIL:
    case LBF_VECTOR_ABS:
    case LBF_VECTOR_SIGN:
    case LBF_VECTOR_CLAMP:
    case LBF_VECTOR_MIN:
    case LBF_VECTOR_MAX:
    case LBF_MATH_LERP:
    case LBF_VECTOR_LERP:
    case LBF_MATH_ISNAN:
    case LBF_MATH_ISINF:
    case LBF_MATH_ISFINITE:
        return true;
    default:
        return false;
    }
}

typedef int (*luau_ProtectedFastFunction)(lua_State* L, StkId ra, int nparams);

extern const luau_ProtectedFastFunction luauPF_table[2];
