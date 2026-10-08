#include "passes/desugaring.hh"
#include "passes/constant_expression_simplifier.hh"
#include "testutils.hh"

using namespace Tailslide;

TEST_SUITE_BEGIN("AST Rewriting");

class AddSubbingVisitor: public ASTVisitor {
public:
  bool visit(LSLBinaryExpression *expr) override {
    if(expr->getOperation() != '+')
      return true;

    // can only do this for types that allow `-` expressions
    switch (expr->getIType()) {
      case LST_INTEGER:
      case LST_FLOATINGPOINT:
      case LST_VECTOR:
      case LST_QUATERNION:
        break;
      default:
        return true;
    }

    // disconnect the left and right expr nodes from the original parent
    // and attach them to the new expression
    auto *new_expr = expr->mContext->allocator->newTracked<LSLBinaryExpression>(
        (LSLExpression *) expr->takeChild(0),
        OP_MINUS,
        (LSLExpression *) expr->takeChild(1)
    );
    // swap the old expression out with the new one
    LSLASTNode::replaceNode(expr, new_expr);
    new_expr->visit(this);
    return false;
  };
};

TEST_CASE("simple_expr_replacement.lsl") {
  OptimizationOptions ctx{.fold_constants=true};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("simple_expr_replacement.lsl", ctx, pretty_ctx, [](LSLScript *script) {
    AddSubbingVisitor visitor;
    script->visit(&visitor);
    // all existing constant values are now potentially dirty, recalculate.
    script->propagateValues();
  });
}

class FancyAddReplacementVisitor: public ASTVisitor {
public:
  bool visit(LSLBinaryExpression *expr) override {
    if(expr->getOperation() != '+')
      return true;

    // our injected `whatever()` returns an integer, so we can only replace `+` in
    // integer and floating point contexts
    switch (expr->getIType()) {
      case LST_INTEGER:
      case LST_FLOATINGPOINT:
        break;
      default:
        return true;
    }
    PrettyPrintVisitor pretty_visitor(PrettyPrintOpts {});
    expr->visit(&pretty_visitor);

    auto *allocator = expr->mContext->allocator;
    char *new_str = allocator->copyStr(pretty_visitor.mStream.str().c_str());

    // replace all addition with a stringified version of the addition expression
    auto new_expr = allocator->newTracked<LSLFunctionExpression>(
        allocator->newTracked<LSLIdentifier>(LSLType::get(LST_INTEGER), "whatever"),
        allocator->newTracked<LSLASTNodeList<LSLExpression>>(
          allocator->newTracked<LSLConstantExpression>(
            allocator->newTracked<LSLStringConstant>(new_str)
          )
        )
    );
    // swap the old expression out with the new one
    LSLASTNode::replaceNode(expr, new_expr);
    // make sure all the newly added nodes have correct type information
    new_expr->determineTypes();
    new_expr->visit(this);
    return false;
  };
};

TEST_CASE("fancy_expr_replacement.lsl") {
  OptimizationOptions ctx{false};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("fancy_expr_replacement.lsl", ctx, pretty_ctx, [](LSLScript *script) {
    // pretend we have a builtin called "whatever()" that takes a string and returns an int
    auto *symbol_table = script->getSymbolTable();
    auto *allocator = script->mContext->allocator;
    auto func_dec = allocator->newTracked<LSLFunctionDec>(
        allocator->newTracked<LSLIdentifier>(LSLType::get(LST_STRING), "foobar")
    );
    symbol_table->define(allocator->newTracked<LSLSymbol>(
        "whatever", LSLType::get(LST_INTEGER), SYM_FUNCTION, SYM_BUILTIN, func_dec
    ));
    FancyAddReplacementVisitor visitor;
    script->visit(&visitor);
    // all existing constant values are now potentially dirty, recalculate.
    script->propagateValues();
  });
}

TEST_CASE("fancy_expr_replacement.lsl") {
  OptimizationOptions ctx{false};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("fancy_expr_replacement.lsl", ctx, pretty_ctx, [](LSLScript *script) {
    // pretend we have a builtin called "whatever()" that takes a string and returns an int
    auto *symbol_table = script->getSymbolTable();
    auto *allocator = script->mContext->allocator;
    auto func_dec = allocator->newTracked<LSLFunctionDec>(
        allocator->newTracked<LSLIdentifier>(LSLType::get(LST_STRING), "foobar")
    );
    symbol_table->define(allocator->newTracked<LSLSymbol>(
        "whatever", LSLType::get(LST_INTEGER), SYM_FUNCTION, SYM_BUILTIN, func_dec
        ));
    FancyAddReplacementVisitor visitor;
    script->visit(&visitor);
    // all existing constant values are now potentially dirty, recalculate.
    script->propagateValues();
  });
}


TEST_CASE("desugaring.lsl") {
  OptimizationOptions ctx{false};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("desugaring.lsl", ctx, pretty_ctx, [](LSLScript *script) {
    LLConformantDeSugaringVisitor visitor(script->mContext->allocator, false);
    script->visit(&visitor);
  });
}

TEST_CASE("constant_simplifier_test.lsl") {
  OptimizationOptions ctx{false};
  PrettyPrintOpts pretty_ctx {};
  checkPrettyPrintOutput("constant_simplifier_test.lsl", ctx, pretty_ctx, [](LSLScript *script) {
    // Run desugaring first to inject typecasts
    LLConformantDeSugaringVisitor desugaring_visitor(script->mContext->allocator, false);
    script->visit(&desugaring_visitor);
    // Recalculate constant values for newly-injected nodes
    script->propagateValues();
    // Run constant expression simplifier
    ConstantExpressionSimplifier simplifier(script->mContext->allocator);
    script->visit(&simplifier);
  });
}

TEST_SUITE_END();
