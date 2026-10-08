#include "final_pass.hh"

namespace Tailslide {

class AllPathsReturnVisitor: public ASTVisitor {
  protected:
    virtual bool visit(LSLExpression *expr) { return false; };
    virtual bool visit(LSLCompoundStatement *compound_stmt) {
      mAllReturn = false;
      return true;
    };
    virtual bool visit(LSLStatement *stmt) {
      mAllReturn = false;
      return false;
    };
    virtual bool visit(LSLReturnStatement *ret_stmt) {
      mAllReturn = true;
      return false;
    };
    virtual bool visit(LSLIfStatement *if_stmt) {
      auto *false_branch = if_stmt->getFalseBranch();

      // no false path, so all paths within `if` _couldn't_ return.
      if (!false_branch) {
        mAllReturn = false;
        return false;
      }

      if_stmt->getTrueBranch()->visit(this);
      bool true_ret = mAllReturn;
      false_branch->visit(this);
      bool false_ret = mAllReturn;
      mAllReturn = true_ret && false_ret;
      return false;
    }
  public:
    bool mAllReturn = false;
};

bool FinalPassVisitor::visit(LSLGlobalFunction *glob_func) {
  AllPathsReturnVisitor visitor;
  glob_func->getStatements()->visit(&visitor);

  auto *id = glob_func->getIdentifier();
  // this function has a non-null return type, it requires explicit returns.
  if (id->getIType() != LST_NULL && !visitor.mAllReturn) {
    NODE_ERROR(id, E_NOT_ALL_PATHS_RETURN);
  }
  if (auto *sym = glob_func->getSymbol()) {
    sym->setAllPathsReturn(visitor.mAllReturn);
  }
  return true;
}

bool FinalPassVisitor::visit(LSLEventHandler *handler) {
  AllPathsReturnVisitor visitor;
  handler->getStatements()->visit(&visitor);
  if (auto *sym = handler->getSymbol()) {
    sym->setAllPathsReturn(visitor.mAllReturn);
  }
  return true;
}

bool FinalPassVisitor::visit(LSLIfStatement *if_stmt) {
  // see if expression is constant
  auto *cond = if_stmt->getCheckExpr();
  if (cond->getConstantValue() != nullptr) {
    // TODO: can conditions be something other than integer?
    // ^ Yep, `key`s for one, and probably a bunch of others.
    // `if (key_foo) {}` is the fastest way to validate UUIDs in LSL.
    if (cond->getConstantValue()->getNodeSubType() == NODE_INTEGER_CONSTANT) {
      if (((LSLIntegerConstant *) cond->getConstantValue())->getValue()) {
        NODE_ERROR(cond, W_CONDITION_ALWAYS_TRUE);
      } else {
        NODE_ERROR(cond, W_CONDITION_ALWAYS_FALSE);
      }
    }
  }

  // if top expression is an assignment
  if (cond->getOperation() == '=') {
    NODE_ERROR(cond, W_ASSIGNMENT_IN_COMPARISON);
  }

  return true;
}

bool FinalPassVisitor::visit(LSLBinaryExpression *bin_expr) {
  LSLConstant *left_cv = bin_expr->getLHS()->getConstantValue();
  LSLConstant *right_cv = bin_expr->getRHS()->getConstantValue();

  if (!left_cv || !right_cv)
    return true;

  if (left_cv->getType() != TYPE(LST_LIST) || right_cv->getType() != TYPE(LST_LIST))
    return true;

  auto *left_lcv = (LSLListConstant *)left_cv;
  auto *right_lcv = (LSLListConstant *)right_cv;

  switch (bin_expr->getOperation()) {
    case OP_NEQ:
    case OP_EQ: {
      // warn on list == list unless it's against an empty list constant,
      // integer len = (list_val != []) is a common pattern for getting the length of a list.
      if (left_lcv->getLength() != 0 && right_lcv->getLength() != 0) {
        NODE_ERROR(bin_expr, W_LIST_COMPARE);
      }
    }
    default:
      break;
  }
  return true;
}

bool FinalPassVisitor::visit(LSLExpressionStatement *expr_stmt) {
  if (expr_stmt->getExpr()->getOperation() == OP_EQ) {
    NODE_ERROR(expr_stmt, W_EQ_AS_STATEMENT);
  }
  return true;
}

}
