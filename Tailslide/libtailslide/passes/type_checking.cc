#include "type_checking.hh"

namespace Tailslide {

bool TypeCheckVisitor::visit(LSLASTNode *node) {
  if (!node->getType())
    node->setType(LSLType::get(LST_NULL));
  return true;
}

// both LSLGlobalVariable and LSLDeclaration have the same child layout
// so they can have a common visitor.
void TypeCheckVisitor::handleDeclaration(LSLASTNode *decl_node) {
  auto *id = (LSLIdentifier *) decl_node->getChild(0);
  LSLASTNode *rvalue = decl_node->getChild(1);
  // already have our expected type from the declaration
  if (rvalue == nullptr || rvalue->getNodeType() == NODE_NULL)
    return;
  // we already know there's something messed up going on with the types
  // in the rvalue, don't re-error
  if (rvalue->getIType() == LST_ERROR)
    return;
  if (!rvalue->getType()->canCoerce(id->getType())) {
    NODE_ERROR(decl_node, E_WRONG_TYPE_IN_ASSIGNMENT, id->getType()->getNodeName().c_str(),
               id->getName(), rvalue->getType()->getNodeName().c_str());
  }
}

bool TypeCheckVisitor::visit(LSLGlobalVariable *glob_var) {
  handleDeclaration(glob_var);
  return true;
}

bool TypeCheckVisitor::visit(LSLDeclaration *decl_stmt) {
  handleDeclaration(decl_stmt);
  return true;
}


bool TypeCheckVisitor::visit(LSLStateStatement *state_stmt) {
  // TODO: probably makes more sense in the best practices visitor?
  //  not really related to type-checking.
  // see if we're in a state or function, and if we're inside of an if
  bool is_in_if = false;

  for (LSLASTNode *ancestor = state_stmt->getParent(); ancestor; ancestor = ancestor->getParent()) {
    switch (ancestor->getNodeType()) {
      case NODE_STATEMENT:
        if (ancestor->getNodeSubType() == NODE_IF_STATEMENT)
          is_in_if = true;
        break;
      case NODE_STATE:
        // we're in a state, see if it's the same one we're calling
        if (!strcmp(((LSLIdentifier *) ancestor->getChild(0))->getName(), state_stmt->getIdentifier()->getName())) {
          NODE_ERROR(state_stmt, W_CHANGE_TO_CURRENT_STATE);
        }
        // if we've hit a state there's no sense ascending further
        return true;
      case NODE_GLOBAL_FUNCTION:
        if (is_in_if) {
          // see what kind of function it is
          switch (ancestor->getChild(0)->getIType()) {
            case LST_LIST:
            case LST_STRING:
              NODE_ERROR(state_stmt, W_CHANGE_STATE_HACK_CORRUPT);
              break;
            default:
              NODE_ERROR(state_stmt, W_CHANGE_STATE_HACK);
              break;
          }
        } else {
          NODE_ERROR(state_stmt, E_CHANGE_STATE_IN_FUNCTION);
        }
        // if we've hit a function there's no sense ascending further
        return true;
      default:
        break;
    }
  }
  return true;
}


bool TypeCheckVisitor::visit(LSLReturnStatement *ret_stmt) {
  LSLASTNode *ancestor = ret_stmt->getParent();
  LSLASTNode *effective_parent = ret_stmt->getParent();

  // crawl up until we find an event handler or global func
  while (ancestor && ancestor->getNodeType() != NODE_EVENT_HANDLER &&
      ancestor->getNodeType() != NODE_GLOBAL_FUNCTION)
    ancestor = ancestor->getParent();
  // a return with no enclosing handler or function isn't something the grammar
  // should produce, but don't walk off the root if the tree is malformed.
  if (!ancestor)
    return true;
  // figure out if we're directly under an event handler or global func
  // or nested within a control statement
  while (effective_parent && effective_parent->getNodeSubType() == NODE_COMPOUND_STATEMENT)
    effective_parent = effective_parent->getParent();
  bool func_is_parent = effective_parent &&
                        (effective_parent->getNodeType() == NODE_EVENT_HANDLER ||
                         effective_parent->getNodeType() == NODE_GLOBAL_FUNCTION);

  auto *ancestor_type = ancestor->getChild(0)->getType();
  auto *ret_expr = ret_stmt->getExpr();
  auto *ret_type = ret_expr ? ret_expr->getType() : TYPE(LST_NULL);

  // It's an error if we have no return expression and this isn't a void func
  if (!ret_expr && ancestor_type->getIType() != LST_NULL) {
    NODE_ERROR(ret_stmt, E_BAD_RETURN_TYPE, TYPE(LST_NULL)->getNodeName().c_str(), ancestor_type->getNodeName().c_str());
    return true;
  }

  // if an event handler
  if (ancestor->getNodeType() == NODE_EVENT_HANDLER) {
    // make sure we're not returning a value
    // returning the result of a void expression is only allowed if
    // the return is nested in some other control statement.
    if ((ret_expr && func_is_parent) || ret_type->getIType() != LST_NULL) {
      NODE_ERROR(ret_stmt, E_RETURN_VALUE_IN_EVENT_HANDLER);
    }
  } else {
    // otherwise it's a function
    // types that can be coerced to the return type are allowed, but returning returning the
    // result of a void expression follows the same rules as event handlers.
    if (!ret_type->canCoerce(ancestor_type)
        || (ret_expr && ret_expr->getIType() == LST_NULL && func_is_parent)) {
      NODE_ERROR(ret_stmt, E_BAD_RETURN_TYPE, ret_type->getNodeName().c_str(), ancestor_type->getNodeName().c_str());
    }
  }
  return true;
}

void TypeCheckVisitor::validateCheckExpr(LSLExpression *check_expr) {
  if (!check_expr)
    return;
  if (check_expr->getIType() == LST_NULL) {
    NODE_ERROR(check_expr, E_VOID_IN_CONDITION);
  }
}

static bool is_branch_empty(LSLASTNode *branch) {
  if (branch == nullptr || branch->getNodeType() == NODE_NULL)
    return true;
  if (branch->getNodeType() != NODE_STATEMENT)
    return false;
  if (branch->getNodeSubType() == NODE_NOP_STATEMENT)
    return true;
  if (branch->getNodeSubType() == NODE_COMPOUND_STATEMENT) {
    return !branch->hasChildren();
  }

  return false;
}

bool TypeCheckVisitor::visit(LSLIfStatement *if_stmt) {
  if_stmt->setType(TYPE(LST_NULL));
  validateCheckExpr(if_stmt->getCheckExpr());
  // warn if main branch is an empty statement and secondary branch is null
  // or empty
  if (is_branch_empty(if_stmt->getTrueBranch()) && is_branch_empty(if_stmt->getFalseBranch())) {
    NODE_ERROR(if_stmt->getCheckExpr(), W_EMPTY_IF);
  }
  return true;
}

bool TypeCheckVisitor::visit(LSLForStatement *for_stmt) {
  validateCheckExpr(for_stmt->getCheckExpr());
  if (is_branch_empty(for_stmt->getBody())) {
    NODE_ERROR(for_stmt, W_EMPTY_LOOP);
  }
  return true;
}

bool TypeCheckVisitor::visit(LSLDoStatement *do_stmt) {
  validateCheckExpr(do_stmt->getCheckExpr());
  if (is_branch_empty(do_stmt->getBody())) {
    NODE_ERROR(do_stmt, W_EMPTY_LOOP);
  }
  return true;
}

bool TypeCheckVisitor::visit(LSLWhileStatement *while_stmt) {
  validateCheckExpr(while_stmt->getCheckExpr());
  if (is_branch_empty(while_stmt->getBody())) {
    NODE_ERROR(while_stmt, W_EMPTY_LOOP);
  }
  return true;
}


bool TypeCheckVisitor::visit(LSLExpression *expr) {
  LSLOperator operation = expr->getOperation();
  LSLType *type;
  LSLASTNode *left = expr->getChild(0);
  LSLASTNode *right = expr->getChild(1);
  LSLType *l_type = left->getType();
  LSLType *r_type = right ? right->getType() : nullptr;

  if (operation_mutates(operation)) {
    assert (left->getNodeSubType() == NODE_LVALUE_EXPRESSION);
    auto *sym = left->getSymbol();
    // Attempting to mutate a builtin always results in an error.
    if (sym && sym->getSubType() == SYM_BUILTIN) {
      NODE_ERROR(expr, E_BUILTIN_LVALUE, sym->getName());
      expr->setType(TYPE(LST_ERROR));
      return true;
    }
  }

  if (operation == 0 || operation == '(') {
    type = l_type;
  } else if (l_type == TYPE(LST_ERROR) || r_type == TYPE(LST_ERROR)) {
    // If we have a type error on either side of the expression we can't
    // guess as to what the result of the expression is meant to be.
    type = TYPE(LST_ERROR);
  } else {
    type = l_type->getResultType(operation, r_type);
    if (type == nullptr) {
      NODE_ERROR(
          expr,
          E_INVALID_OPERATOR,
          l_type->getNodeName().c_str(),
          operation_str(operation),
          r_type ? r_type->getNodeName().c_str() : ""
      );
      // We don't know what type this expression is supposed to result in,
      // either because this operation is unsupported.
      type = TYPE(LST_ERROR);
    } else if (operation == OP_MUL_ASSIGN && l_type == TYPE(LST_INTEGER) && r_type == TYPE(LST_FLOATINGPOINT)) {
      // see note in `LSLType::getResultType` for details on this case.
      NODE_ERROR(expr, W_INT_FLOAT_MUL_ASSIGN);
    }
  }
  expr->setType(type);
  return true;
}

bool TypeCheckVisitor::visit(LSLListConstant *list_const) {
  for (auto *val_c : *list_const) {
    if (val_c->getIType() == LST_LIST) {
      NODE_ERROR(list_const, E_LIST_IN_LIST);
      val_c->setType(TYPE(LST_ERROR));
    }
  }
  return true;
}

bool TypeCheckVisitor::visit(LSLListExpression *list_expr) {
  for (auto *val_c : *list_expr) {
    auto child_type = val_c->getIType();
    if (child_type == LST_LIST) {
      NODE_ERROR(list_expr, E_LIST_IN_LIST);
      val_c->setType(TYPE(LST_ERROR));
    } else if (child_type == LST_NULL) {
      // not technically illegal in LL's compiler, but doesn't ever make sense,
      // mono crashes on compile and LSO results in weird type confusion bugs
      // and eventual heap corruption.
      NODE_ERROR(list_expr, E_NULL_IN_LIST);
      val_c->setType(TYPE(LST_ERROR));
    }
  }
  return true;
}

// check argument types and count for function calls
// and event handler definitions
static bool validate_func_arg_spec(
    LSLIdentifier *id,
    LSLASTNode *func_node,
    LSLASTNode *params,
    LSLParamList *function_decl
) {
  bool is_event_handler = func_node->getNodeType() == NODE_EVENT_HANDLER;

  LSLIdentifier *declared_param_id;
  LSLIdentifier *passed_param_id;
  int param_num = 1;

  declared_param_id = (LSLIdentifier *) function_decl->getChild(0);
  passed_param_id = (LSLIdentifier *)params->getChild(0);

  while (declared_param_id != nullptr && passed_param_id != nullptr) {
    bool param_compatible;
    if (is_event_handler)
      param_compatible = passed_param_id->getType() == declared_param_id->getType();
    else
      param_compatible = passed_param_id->getType()->canCoerce(declared_param_id->getType());

    if (!param_compatible) {
      NODE_ERROR(func_node, is_event_handler ? E_ARGUMENT_WRONG_TYPE_EVENT : E_ARGUMENT_WRONG_TYPE,
                 passed_param_id->getType()->getNodeName().c_str(),
                 param_num,
                 id->getName(),
                 declared_param_id->getType()->getNodeName().c_str(),
                 declared_param_id->getName()
      );
      return false;
    }
    passed_param_id = (LSLIdentifier *) passed_param_id->getNext();
    declared_param_id = (LSLIdentifier *) declared_param_id->getNext();
    ++param_num;
  }

  if (passed_param_id != nullptr) {
    NODE_ERROR(func_node, is_event_handler ? E_TOO_MANY_ARGUMENTS_EVENT : E_TOO_MANY_ARGUMENTS, id->getName());
    return false;
  } else if (declared_param_id != nullptr) {
    NODE_ERROR(func_node, is_event_handler ? E_TOO_FEW_ARGUMENTS_EVENT : E_TOO_FEW_ARGUMENTS, id->getName());
    return false;
  }
  return true;
}

bool TypeCheckVisitor::visit(LSLFunctionExpression *func_expr) {
  auto *id = func_expr->getIdentifier();
  auto *sym = func_expr->getSymbol();
  func_expr->setType(id->getType());

  // can't check types if function is undeclared
  if (sym == nullptr) {
    func_expr->setType(TYPE(LST_ERROR));
    return true;
  }

  validate_func_arg_spec(id, func_expr, func_expr->getArguments(), sym->getFunctionDecl());
  return true;
}

bool TypeCheckVisitor::visit(LSLEventHandler *handler) {
  auto *id = handler->getIdentifier();
  // can't check arg spec if event handler isn't valid
  if (id->getSymbol() == nullptr)
    return true;

  // get the expected event handler prototype from the builtins
  auto *function_decl = handler->mContext->builtins->lookup(id->getName(), SYM_EVENT)->getFunctionDecl();
  validate_func_arg_spec(id, handler, handler->getArguments(), function_decl);
  return true;
}

bool TypeCheckVisitor::visit(LSLLValueExpression *lvalue) {
  auto *id = lvalue->getIdentifier();
  auto *member_node = lvalue->getMember();
  lvalue->setType(id->getType());

  auto *symbol = id->getSymbol();
  if (!symbol) {
    lvalue->setType(TYPE(LST_ERROR));
    return false;
  }

  auto symbol_type = symbol->getSymbolType();

  /// If we're requesting a member, like var.x or var.y
  if (member_node) {
    const char *name = id->getName();
    const char *member = ((LSLIdentifier *) member_node)->getName();

    if (member != nullptr) {
      // all members must be single letters
      if (member[1] != 0) {
        NODE_ERROR(lvalue, E_INVALID_MEMBER, name, member);
        lvalue->setType(TYPE(LST_ERROR));
        return false;
      }

      /// Make sure it's a variable
      if (symbol_type != SYM_VARIABLE) {
        NODE_ERROR(lvalue, E_MEMBER_NOT_VARIABLE, name, member, LSLSymbol::getTypeName(symbol_type));
        lvalue->setType(TYPE(LST_ERROR));
        return false;
      }

      // ZERO_VECTOR.x is not valid, because it's really `<0,0,0>.x` which is not allowed!
      if (symbol->getSubType() == SYM_BUILTIN) {
        NODE_ERROR(lvalue, E_INVALID_MEMBER, name, member);
        lvalue->setType(TYPE(LST_ERROR));
        return false;
      }

      // Make sure it's a vector or quaternion.
      switch (symbol->getIType()) {
        case LST_QUATERNION:
          if (member[0] == 's') {
            lvalue->setType(TYPE(LST_FLOATINGPOINT));
            break;
          }
          // FALL THROUGH
        case LST_VECTOR:
          switch (member[0]) {
            case 'x':
            case 'y':
            case 'z':
              lvalue->setType(TYPE(LST_FLOATINGPOINT));
              break;
            default:
              NODE_ERROR(lvalue, E_INVALID_MEMBER, name, member);
              lvalue->setType(TYPE(LST_ERROR));
              break;
          }
          break;
        default:
          NODE_ERROR(lvalue, E_MEMBER_WRONG_TYPE, name, member);
          lvalue->setType(TYPE(LST_ERROR));
          break;
      }
    }
  }

  // This refers to a local, walk back and see there's anything
  // between us and its declaration that'd make it unfoldable
  if (symbol->getSubType() == SYM_LOCAL && symbol->getVarDecl() != nullptr) {
    LSLASTNode *local_decl = symbol->getVarDecl();
    assert(local_decl != nullptr);

    // Walk up and find the symbol table for the function this is in
    LSLASTNode *upper_node = lvalue->getParent();
    LSLSymbolTable *func_symtab = nullptr;
    while (upper_node != nullptr) {
      auto *symtab = upper_node->getSymbolTable();
      if (symtab && symtab->getTableType() == SYMTAB_FUNCTION) {
        func_symtab = symtab;
        break;
      }
      upper_node = upper_node->getParent();
    }

    // LValue ref for a local should always be within a function
    assert(func_symtab != nullptr);

    bool is_foldable = true;
    if (*local_decl->getLoc() > *lvalue->getLoc()) {
      // the declaration really should be above us somewhere
      // unless they did something screwy like `if(r)string r = baz;`
      is_foldable = false;
    } else {
      // check if a label occurs between the declaration and lvalue
      for (auto *label_ptr : func_symtab->getLabels()) {
        // hit a label before the declaration? Not gonna inline it.
        // we could be smarter about this, but it's probably not worth it.
        // we could check if there was also a jump to that label somewhere
        // before us in the same scope (before the declaration obviously.)
        if (*label_ptr->getLoc() > *local_decl->getLoc() && *label_ptr->getLoc() < *lvalue->getLoc()) {
          is_foldable = false;
          break;
        }
      }
    }
    lvalue->setIsFoldable(is_foldable);
    return false;
  }
  lvalue->setIsFoldable(true);
  return false;
}

bool TypeCheckVisitor::visit(LSLTypecastExpression *cast_expr) {
  auto *from_type = cast_expr->getChildExpr()->getType();
  auto *to_type = cast_expr->getType();
  if(!is_cast_legal(from_type->getIType(), to_type->getIType())) {
    // this is just an error bubbling up
    if (from_type->getIType() != LST_ERROR) {
      NODE_ERROR(cast_expr, E_ILLEGAL_CAST, from_type->getNodeName().c_str(), to_type->getNodeName().c_str());
    }
  }
  return true;
}

bool TypeCheckVisitor::visit(LSLVectorExpression *vec_expr) {
  for (auto *child : *vec_expr) {
    if (!child->getType()->canCoerce(TYPE(LST_FLOATINGPOINT))) {
      NODE_ERROR(vec_expr, E_WRONG_TYPE_IN_MEMBER_ASSIGNMENT, "vector",
                 child->getType()->getNodeName().c_str());
      return true;
    }
  }
  return true;
}

bool TypeCheckVisitor::visit(LSLQuaternionExpression *quat_expr) {
  for (auto *child : *quat_expr) {
    if (!child->getType()->canCoerce(TYPE(LST_FLOATINGPOINT))) {
      NODE_ERROR(quat_expr, E_WRONG_TYPE_IN_MEMBER_ASSIGNMENT, "quaternion",
                 child->getType()->getNodeName().c_str());
      return true;
    }
  }
  return true;
}

bool TypeCheckVisitor::visit(LSLPrintExpression *print_expr) {
  // No passing void expressions to `print()`
  if (print_expr->getChildExpr()->getIType() == LST_NULL) {
    NODE_ERROR(print_expr, E_ARGUMENT_WRONG_TYPE,
               "null",
               1,
               "print",
               "any",
               "notnull"
    );
  }
  return true;
}

}
