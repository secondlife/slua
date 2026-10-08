#include "tailslide.hh"
#include "doctest.hh"
#include "passes/pretty_print.hh"
#include "testutils.hh"

using namespace Tailslide;

TEST_SUITE_BEGIN("Lint");

TEST_CASE("bad_globals.lsl") {
  auto parser = runConformance("bad_globals.lsl", true);
  // syntax error due to bad tokens
  CHECK(getFilteredMessages(parser).size() == 2);
}
SIMPLE_LINT_TEST_CASE("camera_demo.lsl")
SIMPLE_LINT_TEST_CASE("check_all_return.lsl")
SIMPLE_LINT_TEST_CASE("color-change.lsl")
SIMPLE_LINT_TEST_CASE("compound_assignment.lsl")
SIMPLE_LINT_TEST_CASE("constants.lsl")
SIMPLE_LINT_TEST_CASE("constprop.lsl")
SIMPLE_LINT_TEST_CASE("declaration_expressions.lsl")
SIMPLE_LINT_TEST_CASE("duplicate_labels.lsl")
SIMPLE_LINT_TEST_CASE("error1.lsl")
SIMPLE_LINT_TEST_CASE("events.lsl")
SIMPLE_LINT_TEST_CASE("forloops.lsl")
SIMPLE_LINT_TEST_CASE("fpinc.lsl")
SIMPLE_LINT_TEST_CASE("fwcheck1.lsl")
TEST_CASE("global_list_refs.lsl") {
  auto parser = runConformance("global_list_refs.lsl");
  // an SAIdentifier for a list is fine under LSO semantics too, the
  // "must have an rvalue" rule only applies within SALists.
  parser->script->validateGlobals(false);
  CHECK(getFilteredMessages(parser).size() == 0);
}
SIMPLE_LINT_TEST_CASE("hex.lsl")
SIMPLE_LINT_TEST_CASE("illegal_cast.lsl")
SIMPLE_LINT_TEST_CASE("irc-4.lsl")
// technically this should not pass because LL's impl has different
// rules for labels shadowing vars from outer scopes than it does
// for vars shadowing functions from outer scopes. It should not
// be able to resolve the `foo` bar for the `llOwnerSay()` call.
SIMPLE_LINT_TEST_CASE("label_shadowing.lsl")
SIMPLE_LINT_TEST_CASE("libhttpdb.lsl")
SIMPLE_LINT_TEST_CASE("lsl_conformance.lsl")
SIMPLE_LINT_TEST_CASE("many_statements.lsl")
SIMPLE_LINT_TEST_CASE("mms_player.lsl")
SIMPLE_LINT_TEST_CASE("nested_lists.lsl")
SIMPLE_LINT_TEST_CASE("parser_abuse.lsl")

#ifndef _WIN32
SIMPLE_LINT_TEST_CASE("parserstackdepth2.lsl")
#endif

TEST_CASE("parserstackdepth3.lsl") {
  auto parser = runConformance("parserstackdepth3.lsl", true);
  CHECK(parser->script == nullptr);
  // parser stack depth error, then syntax error due to unexpected end.
  CHECK(getFilteredMessages(parser).size() == 2);
}
SIMPLE_LINT_TEST_CASE("precluded_globals.lsl")
SIMPLE_LINT_TEST_CASE("print_expression.lsl")
TEST_CASE("print_no_shadowing.lsl") {
  auto parser = runConformance("print_no_shadowing.lsl", true);
  // syntax error due to unexpected keyword
  CHECK(getFilteredMessages(parser).size() == 2);
}
SIMPLE_LINT_TEST_CASE("pathological_expression.lsl")
SIMPLE_LINT_TEST_CASE("print_type_bug.lsl")
SIMPLE_LINT_TEST_CASE("rvalue_assignments.lsl")
SIMPLE_LINT_TEST_CASE("scope1.lsl")
SIMPLE_LINT_TEST_CASE("scope2.lsl")
SIMPLE_LINT_TEST_CASE("scope3.lsl")
SIMPLE_LINT_TEST_CASE("scope4.lsl")
SIMPLE_LINT_TEST_CASE("streamcomment.lsl")
TEST_CASE("strict_salist.lsl") {
  auto parser = runConformance("strict_salist.lsl");
  // assert should be satisfied by validating globals with LSO semantics
  parser->script->validateGlobals(false);
  CHECK(getFilteredMessages(parser).size() == 0);
}
SIMPLE_LINT_TEST_CASE("lenient_salist.lsl")
SIMPLE_LINT_TEST_CASE("test1.lsl")
SIMPLE_LINT_TEST_CASE("test2.lsl")
SIMPLE_LINT_TEST_CASE("unixtime.lsl")
SIMPLE_LINT_TEST_CASE("various_globals.lsl")
SIMPLE_LINT_TEST_CASE("vconst.lsl")
SIMPLE_LINT_TEST_CASE("void_return.lsl")
SIMPLE_LINT_TEST_CASE("warning1.lsl")
SIMPLE_LINT_TEST_CASE("warning2.lsl")
SIMPLE_LINT_TEST_CASE("weirdscope-jump.lsl")
SIMPLE_LINT_TEST_CASE("xytext1.2.lsl")
SIMPLE_LINT_TEST_CASE("ll_global_rules.lsl")

SIMPLE_LINT_TEST_CASE("bugs/0001.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0002.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0003.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0004.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0005.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0006.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0007.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0008.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0009.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0010.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0011.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0012.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0013.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0014.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0015.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0016.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0017.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0018.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0019.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0020.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0021.lsl")
SIMPLE_LINT_TEST_CASE("bugs/0022.lsl")
SIMPLE_LINT_TEST_CASE("bugs/unary_minus_glob_bad_sym.lsl")
SIMPLE_LINT_TEST_CASE("bugs/typecast_builtin.lsl")
SIMPLE_LINT_TEST_CASE("tltp/browser.lsl")
SIMPLE_LINT_TEST_CASE("tltp/exporter.lsl")
SIMPLE_LINT_TEST_CASE("tltp/server.lsl")
SIMPLE_LINT_TEST_CASE("bugs/mutable_global_rvalue.lsl")
SIMPLE_LINT_TEST_CASE("list_append_void.lsl")


TEST_CASE("type_error_no_assert.lsl") {
  auto parser = runConformance("type_error_no_assert.lsl");
  CHECK(getFilteredMessages(parser).size() == 1);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("Optimization");


TEST_CASE("xytext1.2.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("xytext1.2.lsl", ctx, pretty_ctx);
}

TEST_CASE("irc-4.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("irc-4.lsl", ctx, pretty_ctx);
}

TEST_CASE("parser_abuse.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("parser_abuse.lsl", ctx, pretty_ctx);
}

TEST_CASE("scope3.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("scope3.lsl", ctx, pretty_ctx);
}

TEST_CASE("mms_player.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  pretty_ctx.mangle_func_names = true;
  pretty_ctx.mangle_global_names = true;
  pretty_ctx.mangle_local_names = true;
  pretty_ctx.show_unmangled = true;
  checkPrettyPrintOutput("mms_player.lsl", ctx, pretty_ctx);
}

#ifndef _WIN32
TEST_CASE("parserstackdepth2.lsl") {
  OptimizationOptions ctx{};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("parserstackdepth2.lsl", ctx, pretty_ctx);
}
#endif

TEST_CASE("forloops.lsl") {
  OptimizationOptions ctx{};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("forloops.lsl", ctx, pretty_ctx);
}

TEST_CASE("constprop.lsl") {
  OptimizationOptions ctx {
    .fold_constants = true,
    .prune_unused_locals = true,
    .prune_unused_globals = true,
    .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("constprop.lsl", ctx, pretty_ctx);
}

TEST_CASE("tltp/browser.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("tltp/browser.lsl", ctx, pretty_ctx);
}

TEST_CASE("tltp/exporter.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("tltp/exporter.lsl", ctx, pretty_ctx);
}

TEST_CASE("tltp/server.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("tltp/server.lsl", ctx, pretty_ctx);
}

TEST_CASE("fpinc.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("fpinc.lsl", ctx, pretty_ctx);
}

TEST_CASE("vector_products.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("vector_products.lsl", ctx, pretty_ctx);
}

TEST_CASE("key_inlining.lsl") {
  OptimizationOptions ctx {
      .fold_constants = true,
      .prune_unused_locals = true,
      .prune_unused_globals = true,
      .prune_unused_functions = true,
  };
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("key_inlining.lsl", ctx, pretty_ctx);
}

TEST_CASE("num_literals.lsl") {
  OptimizationOptions ctx {};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("num_literals.lsl", ctx, pretty_ctx);
}

TEST_CASE("vector_ambiguity.lsl") {
  OptimizationOptions ctx {};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("vector_ambiguity.lsl", ctx, pretty_ctx);
}

TEST_CASE("infinity_repr.lsl") {
  OptimizationOptions ctx {};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("infinity_repr.lsl", ctx, pretty_ctx);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("Tree Printer");

TEST_CASE("error1.lsl") {
  checkTreeDumpOutput("error1.lsl", OptimizationOptions{});
}

TEST_CASE("tltp/exporter.lsl") {
  checkTreeDumpOutput("tltp/exporter.lsl", OptimizationOptions{});
}

TEST_CASE("pathological_expression.lsl") {
  checkTreeDumpOutput("pathological_expression.lsl", OptimizationOptions{});
}

TEST_CASE("jump_kinds.lsl") {
  checkTreeDumpOutput("jump_kinds.lsl", OptimizationOptions{});
}

static const char *SIMPLE_SCRIPT_BYTES = "default{state_entry(){}}";

TEST_CASE("Parse script buffer") {
  ParserRef parser(new ScopedScriptParser(nullptr));
  parser->context.collect_assertions = true;
  auto script = parser->parseLSLBytes(SIMPLE_SCRIPT_BYTES, (int)strlen(SIMPLE_SCRIPT_BYTES));
  CHECK_NE(nullptr, script);
  CHECK_EQ(0, getFilteredMessages(parser).size());
}

TEST_CASE("LogMessage toString() and getMessage()") {
  auto parser = runConformance("logmessage_test.lsl");
  auto messages = parser->logger.getMessages();

  // Should have exactly 2 messages: 1 warning, 1 error
  CHECK_EQ(messages.size(), 2);

  LogMessage *warning_msg = nullptr;
  LogMessage *error_msg = nullptr;

  for (auto msg : messages) {
    if (msg->getType() == LOG_WARN) {
      warning_msg = msg;
    } else if (msg->getType() == LOG_ERROR) {
      error_msg = msg;
    }
  }

  REQUIRE(warning_msg != nullptr);
  REQUIRE(error_msg != nullptr);

  // Test warning message
  CHECK_EQ(warning_msg->getMessage(), "Suggest parentheses around assignment used as truth value.");
  CHECK_EQ(warning_msg->toString(), " WARN:: (  4, 13): [E20002] Suggest parentheses around assignment used as truth value.");

  // Test error message
  CHECK_EQ(error_msg->getMessage(), "Duplicate declaration of `a'; previously declared at (3, 9).");
  CHECK_EQ(error_msg->toString(), "ERROR:: (  5,  9): [E10001] Duplicate declaration of `a'; previously declared at (3, 9).");
}

static std::map<std::string, LSLSymbol*> getFunctionSymbols(LSLScript *script) {
  std::map<std::string, LSLSymbol*> functions;
  auto *globals = script->getGlobals();
  for (auto *global : *globals) {
    if (global->getNodeType() == NODE_GLOBAL_FUNCTION) {
      auto *func = (LSLGlobalFunction *)global;
      auto *id = func->getIdentifier();
      if (id) {
        auto *sym = id->getSymbol();
        if (sym) {
          functions[id->getName()] = sym;
        }
      }
    }
  }
  return functions;
}

static void checkJumpAnnotations(
    const std::map<std::string, LSLSymbol*>& functions,
    const char* name,
    bool expectedHasJumps,
    bool expectedHasUnstructured) {
  REQUIRE(functions.count(name));
  auto *sym = functions.at(name);
  CHECK_EQ(sym->getHasJumps(), expectedHasJumps);
  CHECK_EQ(sym->getHasUnstructuredJumps(), expectedHasUnstructured);
}

TEST_CASE("jump_annotations.lsl") {
  auto parser = runConformance("jump_annotations.lsl");
  assertNoLintErrors(parser, "jump_annotations.lsl");

  auto *script = parser->script;
  REQUIRE(script != nullptr);

  auto functions = getFunctionSymbols(script);

  // Functions with no jumps
  checkJumpAnnotations(functions, "no_jumps", false, false);

  // Structured jumps (break-like and continue-like)
  checkJumpAnnotations(functions, "break_like", true, false);
  checkJumpAnnotations(functions, "continue_like", true, false);

  // Unstructured jumps
  checkJumpAnnotations(functions, "unstructured_no_loop", true, true);
  checkJumpAnnotations(functions, "unstructured_forward", true, true);
  checkJumpAnnotations(functions, "unstructured_nested", true, true);
  checkJumpAnnotations(functions, "duplicate_labels", true, true);
}

TEST_SUITE_END();
