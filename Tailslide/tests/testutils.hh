#ifndef TESTUTILS_HH
#define TESTUTILS_HH

#include "doctest.h"
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

// Every optimization on, what most of the optimized fixtures expect
inline Tailslide::OptimizationOptions allOptimizations() {
  Tailslide::OptimizationOptions ctx{};
  ctx.fold_constants = true;
  ctx.prune_unused_locals = true;
  ctx.prune_unused_globals = true;
  ctx.prune_unused_functions = true;
  return ctx;
}

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
