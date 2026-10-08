#include "tree_simplifier.hh"

namespace Tailslide {

bool TreeSimplifyingVisitor::visit(LSLDeclaration *decl_stmt) {
  if (!mOpts.prune_unused_locals)
    return true;

  auto *sym = decl_stmt->getSymbol();
  if (!sym || sym->getReferences() != 1 || sym->getAssignments() != 0)
    return true;
  LSLASTNode *rvalue = decl_stmt->getInitializer();
  // rvalue can't be reduced to a constant, don't know that we don't need
  // the side-effects of evaluating the expression.
  if(rvalue && !rvalue->getConstantValue())
    return true;

  ++mFoldedLevel;
  LSLASTNode *ancestor = decl_stmt;
  // walk up and remove it from whatever symbol table it's in
  while (ancestor != nullptr) {
    if (ancestor->getSymbolTable() != nullptr) {
      if (ancestor->getSymbolTable()->remove(sym))
        break;
    }
    ancestor = ancestor->getParent();
  }
  assert(decl_stmt->getParent() != nullptr);
  decl_stmt->getParent()->removeChild(decl_stmt);
  // child is totally gone now, can't recurse.
  return false;
}

bool TreeSimplifyingVisitor::visit(LSLGlobalFunction *glob_func) {
  return handleGlobal(glob_func);
}

bool TreeSimplifyingVisitor::visit(LSLGlobalVariable *glob_var) {
  return handleGlobal(glob_var);
}

bool TreeSimplifyingVisitor::visit(LSLExpression *expr) {
  if (!mOpts.fold_constants)
    return true;

  LSLConstant *cv = expr->getConstantValue();
  if(!cv)
    return true;
  auto c_type = cv->getIType();
  // this expression results in a list, don't fold the result in.
  if (c_type == LST_LIST)
    return true;
  // Need to be careful about where we inline these (I.E. in list casts or expressions)
  // just avoid inlining them at all for now.
  if (c_type == LST_KEY)
    return true;
  // LSL doesn't really have NaN literals, can't fold this.
  if (cv->containsNaN())
    return true;
  // this expression may result in a new entry in the string constant pool,
  // and we're not allowed to create new ones, don't fold.
  // TODO: Might be worth checking for cases where we're just splicing together two
  //  strings that aren't referenced anywhere else.
  if (!mOpts.may_create_new_strs && c_type == LST_STRING)
    return true;

  // We're going to change its parent / sibling connections,
  // so we need a copy.
  auto *new_expr = expr->mContext->allocator->newTracked<LSLConstantExpression>(cv);
  new_expr->setLoc(expr->getLoc());
  LSLASTNode::replaceNode(expr, new_expr);
  ++mFoldedLevel;

  return false;
}

bool TreeSimplifyingVisitor::visit(LSLLValueExpression *lvalue) {
  if (!mOpts.fold_constants)
    return false;

  auto *sym = lvalue->getSymbol();
  if (!sym)
    return false;
  // Is this a builtin constant? Don't bother replacing it.
  // These references are usually "free" given that they're
  // lexer tokens in SL proper.
  if (sym->getSubType() == SYM_BUILTIN)
    return false;

  // list lvalues may never be inlined.
  if (sym->getIType() == LST_LIST)
    return false;

  // Keys have special inlining rules so key-ness isn't lost.
  if (sym->getIType() == LST_KEY) {
    auto *node_ancestor = lvalue->getParent();
    LSLASTNode *top_expr = lvalue;

    // Need to be careful about which expressions we inline keys under (I.E. list casts and list expressions)
    while (node_ancestor && node_ancestor->getNodeType() == NODE_EXPRESSION) {
      auto ancestor_type = node_ancestor->getNodeSubType();
      if (ancestor_type == NODE_LIST_EXPRESSION)
        return false;
      if (ancestor_type == NODE_TYPECAST_EXPRESSION && node_ancestor->getIType() == LST_LIST)
        return false;
      // key-ness matters for print()!
      if (ancestor_type == NODE_PRINT_EXPRESSION)
        return false;
      // string and key have different bool conversion rules
      if (ancestor_type == NODE_BOOL_CONVERSION_EXPRESSION)
        return false;
      top_expr = node_ancestor;
      node_ancestor = node_ancestor->getParent();
    }
    // Still have ancestors after going past the top level of the expression, might be statements.
    // where the key-ness matters.
    if (top_expr->getIType() == LST_KEY) {
      node_ancestor = top_expr->getParent();
      // Might be the expression list of a for loop, look one above.
      if (node_ancestor && node_ancestor->getNodeType() == NODE_AST_NODE_LIST)
        node_ancestor = node_ancestor->getParent();
      if (node_ancestor && node_ancestor->getNodeType() == NODE_STATEMENT) {
        switch (node_ancestor->getNodeSubType()) {
          case NODE_WHILE_STATEMENT:
          case NODE_IF_STATEMENT:
          case NODE_DO_STATEMENT:
          case NODE_FOR_STATEMENT:
            // have to be careful about inlining directly within conditional expressions because
            // `key k = "1"; if (k) {...}` and `if ("1") { ... }` branch differently!
            return false;
          default:
            break;
        }
      }
    }
  }
  LSLConstant *cv = lvalue->getConstantValue();
  if (cv && !cv->containsNaN()) {
    auto *new_expr = lvalue->mContext->allocator->newTracked<LSLConstantExpression>(cv);
    new_expr->setLoc(lvalue->getLoc());
    LSLASTNode::replaceNode(
        lvalue,
        new_expr
    );
    ++mFoldedLevel;
    return false;
  }
  return false;
}

bool TreeSimplifyingVisitor::visit(LSLConstantExpression *constant_expr) {
  // Don't touch these at all, they can't be simplified any more!
  return false;
}

bool TreeSimplifyingVisitor::handleGlobal(LSLASTNode *glob) {
  // globals are either a single var or a single function.
  // and they both keep their identifier in the first child!
  auto *id = (LSLIdentifier *) (glob->getChild(0));
  assert(id != nullptr && id->getNodeType() == NODE_IDENTIFIER);

  LSLNodeType node_type = glob->getNodeType();
  auto *sym = id->getSymbol();

  if (((node_type == NODE_GLOBAL_FUNCTION && mOpts.prune_unused_functions) ||
       (node_type == NODE_GLOBAL_VARIABLE && mOpts.prune_unused_globals))
      && sym->getReferences() == 1) {
    ++mFoldedLevel;
    // these reside in the global scope, look for the root symbol table and the entry
    LSLASTNode *script = glob->getRoot();
    script->getSymbolTable()->remove(sym);
    // remove the node itself
    glob->getParent()->removeChild(glob);
    return false;
  }
  return true;
}

}
