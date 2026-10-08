#pragma once

#include "../lslmini.hh"
#include "../visitor.hh"

namespace Tailslide {

class FinalPassVisitor : public ASTVisitor {
  public:
    virtual bool visit(LSLGlobalFunction *glob_func);
    virtual bool visit(LSLEventHandler *handler);
    virtual bool visit(LSLIfStatement *if_stmt);
    virtual bool visit(LSLBinaryExpression *bin_expr);
    virtual bool visit(LSLExpressionStatement *expr_stmt);
};

}
