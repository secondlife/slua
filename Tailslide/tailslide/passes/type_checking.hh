#pragma once

#include "../lslmini.hh"
#include "../visitor.hh"

namespace Tailslide {
class TypeCheckVisitor: public DepthFirstASTVisitor {
  protected:
    virtual bool visit(LSLASTNode *node);
    virtual bool visit(LSLGlobalVariable *glob_var);
    virtual bool visit(LSLDeclaration *decl_stmt);
    virtual bool visit(LSLStateStatement *state_stmt);
    virtual bool visit(LSLReturnStatement *ret_stmt);
    virtual bool visit(LSLIfStatement *if_stmt);
    virtual bool visit(LSLForStatement *for_stmt);
    virtual bool visit(LSLDoStatement *do_stmt);
    virtual bool visit(LSLWhileStatement *while_stmt);
    virtual bool visit(LSLExpression *expr);
    virtual bool visit(LSLEventHandler *handler);
    virtual bool visit(LSLFunctionExpression *func_expr);
    virtual bool visit(LSLLValueExpression *lvalue);
    virtual bool visit(LSLTypecastExpression *cast_expr);
    virtual bool visit(LSLVectorExpression *vec_expr);
    virtual bool visit(LSLQuaternionExpression *quat_expr);
    virtual bool visit(LSLListConstant *list_const);
    virtual bool visit(LSLListExpression *list_expr);
    virtual bool visit(LSLPrintExpression *print_expr);
    
    void handleDeclaration(LSLASTNode *decl_node);
    void validateCheckExpr(LSLExpression *check_expr);
};
}
