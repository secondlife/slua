#include "doctest.hh"
#include "passes/mono/script_compiler.hh"
#include "testutils.hh"

namespace Tailslide {

TEST_SUITE_BEGIN("Mono conformance");

#define SIMPLE_CIL_CONFORMANCE(name) TEST_CASE((name)) { checkCILOutput((name)); }

SIMPLE_CIL_CONFORMANCE("event_handler_order.lsl")
SIMPLE_CIL_CONFORMANCE("two_states.lsl")
SIMPLE_CIL_CONFORMANCE("various_globals.lsl")
SIMPLE_CIL_CONFORMANCE("global_negation.lsl")
SIMPLE_CIL_CONFORMANCE("simple_function_defs.lsl")
SIMPLE_CIL_CONFORMANCE("lso_jump_behavior.lsl")
SIMPLE_CIL_CONFORMANCE("constant_expressions.lsl")
SIMPLE_CIL_CONFORMANCE("function_calls.lsl")
SIMPLE_CIL_CONFORMANCE("branching.lsl")
SIMPLE_CIL_CONFORMANCE("simple_declarations.lsl")
SIMPLE_CIL_CONFORMANCE("lvalue.lsl")
SIMPLE_CIL_CONFORMANCE("casts.lsl")
SIMPLE_CIL_CONFORMANCE("assignments.lsl")
SIMPLE_CIL_CONFORMANCE("vector_ambiguity.lsl")
SIMPLE_CIL_CONFORMANCE("simple_binops.lsl")
SIMPLE_CIL_CONFORMANCE("binary_expressions.lsl")
SIMPLE_CIL_CONFORMANCE("xnary_expressions.lsl")
SIMPLE_CIL_CONFORMANCE("print_expression.lsl")
SIMPLE_CIL_CONFORMANCE("state_change.lsl")
SIMPLE_CIL_CONFORMANCE("lsl_conformance.lsl")
SIMPLE_CIL_CONFORMANCE("lsl_conformance2.lsl")
SIMPLE_CIL_CONFORMANCE("deprecated_function.lsl")
SIMPLE_CIL_CONFORMANCE("event_handlers.lsl")
TEST_CASE("sub_replacement.lsl") {
    checkCILOutput("sub_replacement.lsl", {
      .optimize_sutractions = true,
      .omit_unnecessary_pushes = false
    });
}
TEST_CASE("push_omission.lsl") {
  checkCILOutput("push_omission.lsl", {
      .optimize_sutractions = false,
      .omit_unnecessary_pushes = true
  });
}

TEST_SUITE_END();

}
