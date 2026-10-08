#include "constant_expression_simplifier.hh"
#include "../lslmini.hh"

namespace Tailslide {

bool ConstantExpressionSimplifier::extractConstantCoordComponents(
    LSLASTNode *coord_expr, int expected_count, float *out_values) {
  // Check we have the right number of children
  if (coord_expr->getNumChildren() != expected_count)
    return false;

  int idx = 0;
  for (auto *child : *coord_expr) {
    if (child->getNodeSubType() != NODE_CONSTANT_EXPRESSION)
      return false;

    auto *const_val = child->getConstantValue();
    if (!const_val)
      return false;

    // Must be a float constant (desugaring should have typecast integers to float),
    // and we should have already simplified that cast to a constant earlier.
    if (const_val->getIType() != LST_FLOATINGPOINT)
      return false;

    out_values[idx++] = ((LSLFloatConstant *)const_val)->getValue();
  }

  return true;
}

bool ConstantExpressionSimplifier::visit(LSLVectorExpression *vec_expr) {
  float values[3];
  if (!extractConstantCoordComponents(vec_expr, 3, values))
    return true;

  auto *vec_constant = _mAllocator->newTracked<LSLVectorConstant>(
      values[0], values[1], values[2]
  );
  vec_constant->setLoc(vec_expr->getLoc());

  auto *constant_expr = _mAllocator->newTracked<LSLConstantExpression>(vec_constant);
  constant_expr->setLoc(vec_expr->getLoc());

  // Replace the vector expression with the constant expression
  LSLASTNode::replaceNode(vec_expr, constant_expr);

  return false;
}

bool ConstantExpressionSimplifier::visit(LSLTypecastExpression *cast_expr) {
  auto *child = cast_expr->getChildExpr();
  auto cast_type = cast_expr->getIType();
  auto child_type = child->getIType();

  // Eliminate redundant same-type casts
  if (cast_type == child_type) {
    LSLASTNode::replaceNode(cast_expr, child);
    return false;
  }

  // Only simplify if the child is a constant expression
  if (child->getNodeSubType() != NODE_CONSTANT_EXPRESSION) {
    return true;
  }

  // Only touch integer<->float casts, those are the only ones we're reasonably confident
  // about, and these are the most likely to happen.
  if ((cast_type != LST_INTEGER && cast_type != LST_FLOATINGPOINT) ||
      (child_type != LST_INTEGER && child_type != LST_FLOATINGPOINT)) {
    return true;
  }

  // If the typecast has a constant value, replace the typecast with its result.
  auto *const_val = cast_expr->getConstantValue();
  if (const_val) {
    auto *constant_expr = _mAllocator->newTracked<LSLConstantExpression>(const_val);
    constant_expr->setLoc(cast_expr->getLoc());
    LSLASTNode::replaceNode(cast_expr, constant_expr);
    return false;
  }

  return true;
}

}
