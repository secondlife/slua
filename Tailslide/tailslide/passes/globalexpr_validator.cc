#include "globalexpr_validator.hh"

namespace Tailslide {

bool GlobalExprValidatingVisitor::visit(LSLGlobalVariable *glob_var) {
  _mValidRValue = true;
  auto *rvalue = glob_var->getInitializer();
  if (rvalue && !rvalue->isConstant()) {
    // don't bother complaining if the rvalue isn't constant due to a type or symbol
    // resolution error that we've already reported.
    if (rvalue->getConstantPrecluded()) {
      _mValidRValue = false;
      return false;
    }
    _mValidRValue = false;
  }
  // Worth checking the children specifically to see if there's anything other
  // than non-constness that'd make the rvalue invalid.
  if (rvalue && _mValidRValue) {
    rvalue->visit(this);
  }

  if (!_mValidRValue) {
    assert(rvalue);
    NODE_ERROR(rvalue, E_GLOBAL_INITIALIZER_NOT_CONSTANT);
  }

  return false;
}

bool GlobalExprValidatingVisitor::visit(LSLFunctionExpression *func_expr) {
  _mValidRValue = false;
  return false;
}

bool SimpleAssignableValidatingVisitor::visit(LSLExpression *expr) {
  // other expression types that don't really need their own visitors, but
  // may not be valid in SA context
  switch(expr->getNodeSubType()) {
    case NODE_CONSTANT_EXPRESSION:
    case NODE_VECTOR_EXPRESSION:
    case NODE_QUATERNION_EXPRESSION:
    case NODE_LIST_EXPRESSION:
      return true;
    default:
      _mValidRValue = false;
      return false;
  }
}

bool SimpleAssignableValidatingVisitor::visit(LSLUnaryExpression *unary_expr) {
  // only unary minus is allowed
  if (unary_expr->getOperation() != '-') {
    _mValidRValue = false;
    return false;
  }
  // and only for certain builtins, by virtue of them _actually_ being lexer tokens
  auto *rvalue = unary_expr->getChildExpr();
  if (rvalue->getNodeSubType() != NODE_LVALUE_EXPRESSION) {
    _mValidRValue = false;
    return false;
  }
  auto *sym = rvalue->getSymbol();
  // Don't check, but also don't re-error if a type or symbol error occurred earlier.
  if (!sym || sym->getIType() == LST_ERROR) {
    return false;
  }
  if (sym->getSubType() != SYM_BUILTIN) {
    _mValidRValue = false;
    return false;
  }
  auto *sym_type = sym->getType();
  if (sym_type != TYPE(LST_INTEGER) && sym_type != TYPE(LST_FLOATINGPOINT)) {
    _mValidRValue = false;
    return false;
  }
  // For some reason TRUE and FALSE are lexed a special way that doesn't allow unary minus
  // in the global context.
  if (!strcmp(sym->getName(), "FALSE") || !strcmp(sym->getName(), "TRUE")) {
    _mValidRValue = false;
    return false;
  }
  return true;
}

bool SimpleAssignableValidatingVisitor::visit(LSLLValueExpression *lvalue) {
  // no member accessors allowed!
  if (lvalue->getMember()) {
    _mValidRValue = false;
    return false;
  }
  // SALists don't allow SAIdentifiers with no rvalue in their declaration.
  if (!_mMonoSemantics && lvalue->getParent()->getNodeSubType() == NODE_LIST_EXPRESSION) {
    LSLASTNode *rvalue = lvalue;
    while (rvalue && rvalue->getNodeSubType() == NODE_LVALUE_EXPRESSION) {
      auto *sym = rvalue->getSymbol();
      if (!sym)
        break;
      // This lvalue references a builtin symbol, we don't expect to be able to
      // follow that to a variable declaration, so we can break.
      if (sym->getSubType() == SYM_BUILTIN)
        break;
      // get the node where the referenced symbol was declared
      auto *decl_node = sym->getVarDecl();
      // this should always be set except in the builtin case, which we handled.
      assert(decl_node);
      // get the rvalue of the node where this global was declared.
      rvalue = decl_node->getChild(1);
    }
    // no rvalue on the SAIdentifier this value originally came from
    if (!rvalue || rvalue->getNodeType() == NODE_NULL) {
      _mValidRValue = false;
      return false;
    }
  }
  return true;
}

}
