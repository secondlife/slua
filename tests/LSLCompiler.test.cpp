// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/LSLCompiler.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Compiler.h"
#include "Luau/ParseResult.h"

#include "doctest.h"

using namespace Luau;

TEST_SUITE_BEGIN("LSLCompiler");

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
