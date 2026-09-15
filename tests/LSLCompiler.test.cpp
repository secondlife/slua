// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/LSLCompiler.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/LSLBuiltins.h"
#include "Luau/Compiler.h"
#include "Luau/ParseResult.h"
#include "luacode.h"

#include "doctest.h"

#include <cstdlib>
#include <cstring>

using namespace Luau;

TEST_SUITE_BEGIN("LSLCompiler");

TEST_CASE("StateHandlerMasks")
{
    LSLScriptInfo info;
    compileLSL(R"(
default {
    state_entry() {}
    touch_start(integer n) {}
    timer() {}
}
state two {
    state_entry() {}
    listen(integer c, string nm, key id, string m) {}
}
)", &info);

    // Bits follow builtins.txt order, which is the server's enum: state_entry
    // is 1, touch_start 3, timer 12, listen 13
    auto bit = [](int index) { return (uint64_t)1 << (index - 1); };
    REQUIRE(info.stateHandlerMasks.size() == 2);
    CHECK(info.stateHandlerMasks[0] == (bit(1) | bit(3) | bit(12)));
    CHECK(info.stateHandlerMasks[1] == (bit(1) | bit(13)));
}

TEST_CASE("StateEventBits")
{
    // The runtime numbers events off LSLBuiltins.h, but Tailslide's numbering
    // is what actually lands in the masks
    LSLScriptInfo info;
    compileLSL(R"(
default {
    state_exit() {}
}
state two {
    state_entry() {}
    moving_start() {}
}
)", &info);

    REQUIRE(info.stateHandlerMasks.size() == 2);
    CHECK(info.stateHandlerMasks[0] == LSLEventBit::StateExit);
    CHECK(info.stateHandlerMasks[1] == (LSLEventBit::StateEntry | LSLEventBit::MovingStart));
}

TEST_CASE("LSLCompileAssetCAPI")
{
    const char* source = R"(
default {
    state_entry() {}
    timer() {}
}
state two {
    touch_start(integer n) {}
}
)";
    size_t size = 0;
    bool is_error = true;
    char* asset = luau_lsl_compile_asset(source, strlen(source), 7, &size, &is_error);
    REQUIRE(asset != nullptr);
    CHECK_FALSE(is_error);

    BytecodeHeader header;
    size_t bytecode_start = 0;
    REQUIRE(readBytecodeHeader(asset, size, header, bytecode_start));
    CHECK(header.isLSL);
    CHECK(header.apiVersion == 7);
    REQUIRE(header.stateHandlerMasks.size() == 2);
    CHECK(header.stateHandlerMasks[0] == (LSLEventBit::StateEntry | LSLEventBit::Timer));
    CHECK(header.stateHandlerMasks[1] == LSLEventBit::TouchStart);
    CHECK(bytecode_start < size);
    free(asset);

    const char* broken = "default { state_entry() { integer x = undeclared_var; } }";
    char* error = luau_lsl_compile_asset(broken, strlen(broken), 0, &size, &is_error);
    REQUIRE(error != nullptr);
    CHECK(is_error);
    CHECK(std::string(error, size).find("undeclared_var") != std::string::npos);
    free(error);
}

TEST_CASE("SingleError")
{
    BytecodeBuilder bcb;

    try
    {
        // Duplicate declaration error
        compileLSLOrThrow(bcb, R"(
integer x;
integer x;
default {
    state_entry() {
    }
}
)");
        FAIL("Expected ParseErrors to be thrown");
    }
    catch (const ParseErrors& e)
    {
        const auto& errors = e.getErrors();
        REQUIRE(!errors.empty());
        CHECK_NE(errors.front().getMessage().find("previously declared"), std::string::npos);
    }
}

TEST_CASE("ErrorMessageWhat")
{
    BytecodeBuilder bcb;

    try
    {
        // Undeclared variable - test what() retval.
        compileLSLOrThrow(bcb, R"(
default {
    state_entry() {
        integer x = undeclared_var;
    }
}
)");
        FAIL("Expected ParseErrors to be thrown");
    }
    catch (const ParseErrors& e)
    {
        const auto& errors = e.getErrors();
        REQUIRE(!errors.empty());

        const ParseError& err = errors.front();
        // Check the full formatted message from what()
        std::string fullMessage = err.what();
        CHECK_NE(fullMessage.find("`undeclared_var' is undeclared"), std::string::npos);
    }
}

TEST_CASE("MultipleErrors")
{
    BytecodeBuilder bcb;

    try
    {
        // Multiple type errors
        compileLSLOrThrow(bcb, R"(
default {
    state_entry() {
        integer x = "hello";
        float y = <1,2,3>;
    }
}
)");
        FAIL("Expected ParseErrors to be thrown");
    }
    catch (const ParseErrors& e)
    {
        const auto& errors = e.getErrors();
        CHECK(errors.size() >= 2);
    }
}

TEST_CASE("ErrorMessageEscaping")
{
    BytecodeBuilder bcb;

    // This test verifies that newlines and carriage returns in error messages are escaped
    // The exact error message content depends on what Tailslide produces, so we just
    // verify that compilation fails and no literal newlines appear in the error
    try
    {
        compileLSLOrThrow(bcb, R"(
default {
    state_entry() {
        integer x = y;
    }
}
)"); // undeclared variable
        FAIL("Expected ParseErrors to be thrown");
    }
    catch (const ParseErrors& e)
    {
        const auto& errors = e.getErrors();
        REQUIRE(!errors.empty());

        // Verify no literal newlines or carriage returns in the error message
        const std::string& msg = errors.front().getMessage();
        CHECK(msg.find('\n') == std::string::npos);
        CHECK(msg.find('\r') == std::string::npos);
    }
}

TEST_CASE("WarningsIncludedWithErrors")
{
    BytecodeBuilder bcb;

    try
    {
        // Code that produces both a warning and an error
        // Shadow declaration warning + type error
        compileLSLOrThrow(bcb, R"(
integer x = 5;
default {
    state_entry() {
        integer x = 10; // shadow declaration (warning)
        float y = "hello"; // type error
    }
}
)");
        FAIL("Expected ParseErrors to be thrown");
    }
    catch (const ParseErrors& e)
    {
        const auto& errors = e.getErrors();
        REQUIRE(!errors.empty());

        // Check if any message is prefixed with "WARN: "
        bool hasWarning = false;
        for (const auto& err : errors)
        {
            if (err.getMessage().find("WARN: ") == 0)
            {
                hasWarning = true;
                break;
            }
        }

        REQUIRE(hasWarning);
    }
}

TEST_CASE("ErrorLocationPreserved")
{
    BytecodeBuilder bcb;

    try
    {
        compileLSLOrThrow(bcb, R"(
default {
    state_entry() {
        integer x = "hello";
    }
}
)");
        FAIL("Expected ParseErrors to be thrown");
    }
    catch (const ParseErrors& e)
    {
        const auto& errors = e.getErrors();
        REQUIRE(!errors.empty());

        const auto& loc = errors.front().getLocation();
        CHECK_EQ(loc.begin.line, 4);
        CHECK_EQ(loc.begin.column, 9);
    }
}

TEST_CASE("CompileSuccess")
{
    BytecodeBuilder bcb;

    // Valid LSL code should compile without throwing
    compileLSLOrThrow(bcb, R"(
default {
    state_entry() {
        integer x = 5;
    }
}
)");

    // If we get here, compilation succeeded
    CHECK(bcb.getBytecode().size() > 0);
}

TEST_SUITE_END();
