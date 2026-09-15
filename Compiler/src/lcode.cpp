// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "luacode.h"

#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h" // ServerLua
#include "Luau/LSLBuiltins.h" // ServerLua
#include "Luau/ParseResult.h" // ServerLua

#include <string.h>

char* luau_compile(const char* source, size_t size, lua_CompileOptions* options, size_t* outsize)
{
    LUAU_ASSERT(outsize);

    Luau::CompileOptions opts;

    if (options)
    {
        static_assert(sizeof(lua_CompileOptions) == sizeof(Luau::CompileOptions), "C and C++ interface must match");
        memcpy(static_cast<void*>(&opts), options, sizeof(opts));
    }

    std::string result = compile(std::string(source, size), opts);

    char* copy = static_cast<char*>(malloc(result.size()));
    if (!copy)
        return nullptr;

    memcpy(copy, result.data(), result.size());
    *outsize = result.size();
    return copy;
}

// ServerLua: the same error encoding as luau_compile(), see Luau::compile()
char* luau_compile_asset(const char* source, size_t size, lua_CompileOptions* options, uint32_t api_version, size_t* outsize)
{
    LUAU_ASSERT(outsize);

    Luau::CompileOptions opts;

    if (options)
    {
        static_assert(sizeof(lua_CompileOptions) == sizeof(Luau::CompileOptions), "C and C++ interface must match");
        memcpy(static_cast<void*>(&opts), options, sizeof(opts));
    }

    std::string result;
    try
    {
        result = Luau::compileAssetOrThrow(std::string(source, size), api_version, opts);
    }
    catch (Luau::ParseErrors& e)
    {
        const Luau::ParseError& parseError = e.getErrors().front();
        result = Luau::BytecodeBuilder::getError(Luau::format(":%d: %s", parseError.getLocation().begin.line + 1, parseError.what()));
    }
    catch (Luau::CompileError& e)
    {
        result = Luau::BytecodeBuilder::getError(Luau::format(":%d: %s", e.getLocation().begin.line + 1, e.what()));
    }

    char* copy = static_cast<char*>(malloc(result.size()));
    if (!copy)
        return nullptr;

    memcpy(copy, result.data(), result.size());
    *outsize = result.size();
    return copy;
}

void luau_set_compile_constant_nil(lua_CompileConstant* constant)
{
    Luau::setCompileConstantNil(constant);
}

void luau_set_compile_constant_boolean(lua_CompileConstant* constant, int b)
{
    Luau::setCompileConstantBoolean(constant, b != 0);
}

void luau_set_compile_constant_number(lua_CompileConstant* constant, double n)
{
    Luau::setCompileConstantNumber(constant, n);
}

void luau_set_compile_constant_integer64(lua_CompileConstant* constant, int64_t l)
{
    Luau::setCompileConstantInteger64(constant, l);
}

void luau_set_compile_constant_vector(lua_CompileConstant* constant, float x, float y, float z, float w)
{
    Luau::setCompileConstantVector(constant, x, y, z, w);
}

void luau_set_compile_constant_vectord(lua_CompileConstant* constant, double x, double y, double z, double w)
{
    Luau::setCompileConstantVectord(constant, x, y, z, w);
}

void luau_set_compile_constant_string(lua_CompileConstant* constant, const char* s, size_t l)
{
    Luau::setCompileConstantString(constant, s, l);
}

// ServerLua: constant folder hook for the LSL builtins
void luauSL_lookup_constant_cb(const char* library, const char* member, lua_CompileConstant* constant)
{
    // We only touch _globals_
    if (library != nullptr)
        return;

    const Luau::SLConstant* sl_constant = Luau::luauSL_find_constant(member);
    if (sl_constant == nullptr)
        return;

    switch (sl_constant->type)
    {
    case Luau::SLConstantType::String:
        luau_set_compile_constant_string(constant, sl_constant->valueString, sl_constant->stringLength);
        break;
    case Luau::SLConstantType::Integer:
        luau_set_compile_constant_number(constant, (double)sl_constant->valueInteger);
        break;
    case Luau::SLConstantType::Float:
        luau_set_compile_constant_number(constant, sl_constant->valueNumber);
        break;
    case Luau::SLConstantType::Vector:
    {
        const auto& vec = sl_constant->valueVector;
        luau_set_compile_constant_vector(constant, vec[0], vec[1], vec[2], 0.0f);
        break;
    }
    default:
        // Can't set these as compile-time constants.
        break;
    }
}
