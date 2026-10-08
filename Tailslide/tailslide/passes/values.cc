#include "../lslmini.hh"

#include "values.hh"


namespace Tailslide {

bool ConstantDeterminingVisitor::beforeDescend(LSLASTNode *node) {
  // invalidate any old constant value we had, it might not be valid anymore
  if (!node->isStatic() && node->getNodeType() != NODE_CONSTANT) {
    node->setConstantValue(nullptr);
    node->setConstantPrecluded(false);
  }

  if (node->getIType() == LST_ERROR) {
    // absolutely no chance of us figuring out a constant value
    // for this node, don't descend.
    node->setConstantPrecluded(true);
    return false;
  }

  // need special iteration order for script nodes
  // don't automatically descend!
  if (node->getNodeType() == NODE_SCRIPT)
    return false;
  return true;
}

bool ConstantDeterminingVisitor::visit(LSLScript *script) {
  // need to iterate over global vars FIRST since expressions in
  // global functions may make use of them.
  for (auto *child : *script->getGlobals()) {
    if (child->getNodeType() == NODE_GLOBAL_VARIABLE)
      child->visit(this);
  }

  // safe to descend into functions and event handlers now
  for (auto *child : *script->getGlobals()) {
    if (child->getNodeType() != NODE_GLOBAL_VARIABLE)
      child->visit(this);
  }
  for (auto *child : *script->getStates()) {
    child->visit(this);
  }
  return false;
}

bool ConstantDeterminingVisitor::visit(LSLDeclaration *decl_stmt) {
  handleDeclaration(decl_stmt);
  return false;
}

void ConstantDeterminingVisitor::handleDeclaration(LSLASTNode *decl_node) {
  // if it's initialized, set its constant value
  auto *sym = decl_node->getSymbol();
  LSLASTNode *rvalue = decl_node->getChild(1);
  if (rvalue && rvalue->getNodeType() != NODE_NULL) {
    sym->setConstantPrecluded(rvalue->getConstantPrecluded());
    if (rvalue->getConstantPrecluded()) {
      // Can't determine a value due to a type or symbol error somewhere
      sym->setConstantValue(nullptr);
    } else {
      auto *cv = rvalue->getConstantValue();
      // Must be a case where we're expecting automatic type promotion,
      // perform it on the rvalue's constant value before assigning it to the symbol
      if (cv && cv->getType() != sym->getType() && cv->getType()->canCoerce(sym->getType())) {
        cv = _mOperationBehavior->cast(sym->getType(), cv, cv->getLoc());
      }
      sym->setConstantValue(cv);
      sym->setInitialValue(cv);
    }
  } else {
    sym->setConstantValue(sym->getType()->getDefaultValue());
    sym->setInitialValue(sym->getType()->getDefaultValue());
  }
}

bool ConstantDeterminingVisitor::visit(LSLExpression *expr) {
  const LSLOperator operation = expr->getOperation();
  LSLConstant *constant_value = expr->getConstantValue();
  DEBUG(
      LOG_DEBUG_SPAM,
      nullptr,
      "expression.determine_value() op=%d cv=%s st=%d\n",
      operation,
      constant_value ? constant_value->getNodeName().c_str() : nullptr,
      expr->getNodeSubType()
  );

  LSLASTNode *left = expr->getChild(0);
  LSLASTNode *right = expr->getChild(1);

  if (left->getIType() == LST_ERROR || (right && right->getIType() == LST_ERROR)) {
    expr->setConstantPrecluded(true);
    return true;
  }

  // only check normal expressions
  switch (expr->getNodeSubType()) {
    case NODE_NO_SUB_TYPE:
    case NODE_CONSTANT_EXPRESSION:
    case NODE_PARENTHESIS_EXPRESSION:
    case NODE_BINARY_EXPRESSION:
    case NODE_UNARY_EXPRESSION:
      break;
    default:
      return true;
  }

  if (operation == 0 || operation == '(')
    constant_value = left->getConstantValue();
  else if (operation == '=') {
    assert(right);
    constant_value = right->getConstantValue();
  } else {
    LSLConstant *c_left = left->getConstantValue();
    LSLConstant *c_right = right ? right->getConstantValue() : nullptr;

    // we need a constant value from the c_left, and if we have a c_right side, it MUST have a constant value too
    if (c_left && (right == nullptr || c_right != nullptr))
      constant_value = _mOperationBehavior->operation(operation, c_left, c_right, expr->getLoc());
    else
      constant_value = nullptr;
  }
  expr->setConstantValue(constant_value);
  return true;
}

bool ConstantDeterminingVisitor::visit(LSLGlobalVariable *glob_var) {
  handleDeclaration(glob_var);
  return false;
}

bool ConstantDeterminingVisitor::visit(LSLLValueExpression *lvalue) {
  LSLSymbol *symbol = lvalue->getSymbol();

  // can't determine value if we don't have a symbol
  if (symbol == nullptr) {
    lvalue->setConstantValue(nullptr);
    lvalue->setConstantPrecluded(true);
    return true;
  }

  // Whatever this refers to couldn't be given a value due to a type or symbol error
  // that we've already reported, so we have no chance of one either. Say so, so nothing
  // complains a second time about the same root cause.
  if (symbol->getConstantPrecluded()) {
    lvalue->setConstantValue(nullptr);
    lvalue->setConstantPrecluded(true);
    return true;
  }

  const char *member_name = nullptr;
  if (auto *member = lvalue->getMember())
    member_name = member->getName();

  LSLConstant *constant_value = nullptr;
  DEBUG(LOG_DEBUG_SPAM, nullptr, "id %s assigned %d times\n", symbol->getName(), symbol->getAssignments());
  if (symbol->getAssignments() == 0 || lvalue->getInGlobalContext()) {
    constant_value = symbol->getConstantValue();

    if (constant_value != nullptr && member_name != nullptr) { // getting a member_name
      switch (constant_value->getIType()) {
        case LST_VECTOR: {
          auto *c = (LSLVectorConstant *) constant_value;
          auto *v = (Vector3 *) c->getValue();
          assert(v);
          switch (member_name[0]) {
            case 'x':
              constant_value = _mAllocator->newTracked<LSLFloatConstant>(v->x);
              break;
            case 'y':
              constant_value = _mAllocator->newTracked<LSLFloatConstant>(v->y);
              break;
            case 'z':
              constant_value = _mAllocator->newTracked<LSLFloatConstant>(v->z);
              break;
            default:
              constant_value = nullptr;
          }
          break;
        }
        case LST_QUATERNION: {
          auto *c = (LSLQuaternionConstant *) constant_value;
          auto *v = (Quaternion *) c->getValue();
          assert(v);
          switch (member_name[0]) {
            case 'x':
              constant_value = _mAllocator->newTracked<LSLFloatConstant>(v->x);
              break;
            case 'y':
              constant_value = _mAllocator->newTracked<LSLFloatConstant>(v->y);
              break;
            case 'z':
              constant_value = _mAllocator->newTracked<LSLFloatConstant>(v->z);
              break;
            case 's':
              constant_value = _mAllocator->newTracked<LSLFloatConstant>(v->s);
              break;
            default:
              constant_value = nullptr;
          }
          break;
        }
        default:
          constant_value = nullptr;
          break;
      }
    }
  }
  lvalue->setConstantValue(constant_value);
  return true;
}

bool ConstantDeterminingVisitor::visit(LSLListExpression *list_expr) {
  auto *new_list_cv = _mAllocator->newTracked<LSLListConstant>(nullptr);

  // make sure they are all constant
  for (auto *child : *list_expr) {
    if (!child->isConstant()) {
      list_expr->setConstantPrecluded(child->getConstantPrecluded());
      return true;
    }
  }

  // create assignables for them
  for (auto *child : *list_expr) {
    new_list_cv->pushChild(child->getConstantValue()->copy(_mAllocator));
  }

  // create constant value
  list_expr->setConstantValue(new_list_cv);
  return true;
}

bool ConstantDeterminingVisitor::visit(LSLVectorExpression *vec_expr) {
  float v[3];
  int cv = 0;

  for (auto *child : *vec_expr) {
    // if we have too many children, make sure we don't overflow cv
    if (cv >= 3)
      return true;

    // all children must be constant
    if (!child->isConstant()) {
      vec_expr->setConstantPrecluded(child->getConstantPrecluded());
      return true;
    }

    // all children must be float/int constants - get their val or bail if they're wrong
    switch (child->getConstantValue()->getIType()) {
      case LST_FLOATINGPOINT:
        v[cv++] = (float)((LSLFloatConstant *) child->getConstantValue())->getValue();
        break;
      case LST_INTEGER:
        v[cv++] = (F32) ((LSLIntegerConstant *) child->getConstantValue())->getValue();
        break;
      default:
        vec_expr->setConstantPrecluded(true);
        return true;
    }
  }

  // make sure we had enough children
  if (cv < 3)
    return true;

  // create constant value
  vec_expr->setConstantValue(_mAllocator->newTracked<LSLVectorConstant>(v[0], v[1], v[2]));
  return true;
}


bool ConstantDeterminingVisitor::visit(LSLQuaternionExpression *quat_expr) {
  float v[4];
  int cv = 0;

  for (auto *child : *quat_expr) {
    // if we have too many children, make sure we don't overflow cv
    if (cv >= 4)
      return true;

    // all children must be constant
    if (!child->isConstant()) {
      quat_expr->setConstantPrecluded(child->getConstantPrecluded());
      return true;
    }

    // all children must be float/int constants - get their val or bail if they're wrong
    switch (child->getConstantValue()->getIType()) {
      case LST_FLOATINGPOINT:
        v[cv++] = (float)((LSLFloatConstant *) child->getConstantValue())->getValue();
        break;
      case LST_INTEGER:
        v[cv++] = (F32) ((LSLIntegerConstant *) child->getConstantValue())->getValue();
        break;
      default:
        quat_expr->setConstantPrecluded(true);
        return true;
    }
  }

  // make sure we had enough children
  if (cv < 4)
    return true;

  // create constant value
  quat_expr->setConstantValue(_mAllocator->newTracked<LSLQuaternionConstant>(v[0], v[1], v[2], v[3]));
  return true;
}

bool ConstantDeterminingVisitor::visit(LSLTypecastExpression *cast_expr) {
  // what are we casting
  auto *child_expr = cast_expr->getChildExpr();
  auto *val = child_expr->getConstantValue();
  cast_expr->setConstantValue(nullptr);
  if (!val) {
    cast_expr->setConstantPrecluded(child_expr->getConstantPrecluded());
    return true;
  }
  const auto to_type = cast_expr->getType();
  cast_expr->setConstantValue(_mOperationBehavior->cast(to_type, val, val->getLoc()));
  return true;
}


}
