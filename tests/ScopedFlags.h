// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Common.h"

#include <vector>

template<typename T>
struct [[nodiscard]] ScopedFValue
{
private:
    Luau::FValue<T>* value = nullptr;
    T oldValue = T();

public:
    ScopedFValue(Luau::FValue<T>& fvalue, T newValue)
    {
        value = &fvalue;
        oldValue = fvalue.value;
        fvalue.value = newValue;
    }

    ScopedFValue(const ScopedFValue&) = delete;
    ScopedFValue& operator=(const ScopedFValue&) = delete;

    ScopedFValue(ScopedFValue&& rhs)
    {
        value = rhs.value;
        oldValue = rhs.oldValue;

        rhs.value = nullptr;
    }

    ScopedFValue& operator=(ScopedFValue&& rhs)
    {
        value = rhs.value;
        oldValue = rhs.oldValue;

        rhs.value = nullptr;

        return *this;
    }

    ~ScopedFValue()
    {
        if (value)
            value->value = oldValue;
    }
};

using ScopedFastFlag = ScopedFValue<bool>;
using ScopedFastInt = ScopedFValue<int>;

// Applies SLUA_REQUIRED_FFLAGS for the enclosing scope, so SL cases don't leak
// the set into unrelated tests in the same binary.
struct [[nodiscard]] ScopedSLuaFlags
{
    std::vector<ScopedFastFlag> flags;

    ScopedSLuaFlags()
    {
#define SLUA_SCOPE_FFLAG(name) \
    if (Luau::FValue<bool>* flag = Luau::findFastFlag(name)) \
        flags.emplace_back(*flag, true);

        SLUA_REQUIRED_FFLAGS(SLUA_SCOPE_FFLAG)
#undef SLUA_SCOPE_FFLAG
    }
};
