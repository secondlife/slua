#ifndef TESTUTILS_HH
#define TESTUTILS_HH

#include "doctest.hh"
#include "passes/mono/script_compiler.hh"
#include "passes/pretty_print.hh"
#include "passes/tree_print.hh"
#include "passes/tree_simplifier.hh"
#include "tailslide.hh"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>


#define SIMPLE_LINT_TEST_CASE(name) TEST_CASE(name) { \
  auto parser = runConformance(name);                 \
  assertNoLintErrors(parser, name);     \
}

using ParserRef = std::unique_ptr<Tailslide::ScopedScriptParser>;

ParserRef runConformance(const char *name, bool allow_syntax_errors=false);

void assertNoLintErrors(const ParserRef &parser, const std::string& name);

std::vector<Tailslide::LogMessage*> getFilteredMessages(const ParserRef &parser);

void checkPrettyPrintOutput(
        const char *name,
        const Tailslide::OptimizationOptions &ctx,
        const Tailslide::PrettyPrintOpts &pretty_opts,
        void (*massager)(Tailslide::LSLScript *script) = nullptr
);

void checkTreeDumpOutput(
    const char *name,
    const Tailslide::OptimizationOptions &ctx,
    void (*massager)(Tailslide::LSLScript *script) = nullptr
);

void checkLSOOutput(
    const char *name,
    void (*massager)(Tailslide::LSLScript *script) = nullptr
);

void checkCILOutput(
    const char *name,
    Tailslide::MonoCompilationOptions options = {},
    void (*massager)(Tailslide::LSLScript *script) = nullptr
);

#endif
