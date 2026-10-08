#ifndef TAILSLIDE_VISITOR_HH
#define TAILSLIDE_VISITOR_HH

#include "lslmini.hh"

namespace Tailslide {

class ASTVisitor {
  public:
    virtual bool visit(LSLASTNode *node) { return true; }
    virtual bool visit(LSLASTNullNode *node) { return false; }
    virtual bool visit(LSLASTNodeList<LSLASTNode> *node) { return true; };
    virtual bool visit(LSLScript *script) {
      return visit((LSLASTNode *) script);
    };
    virtual bool visit(LSLIdentifier *id) {
      return visit((LSLASTNode *) id);
    };
    virtual bool visit(LSLGlobalVariable *glob_var) {
      return visit((LSLASTNode *) glob_var);
    };
    virtual bool visit(LSLConstant *constant) {
      return visit((LSLASTNode *) constant);
    };
    virtual bool visit(LSLIntegerConstant *int_const) {
      return visit((LSLConstant *) int_const);
    };
    virtual bool visit(LSLFloatConstant *float_const) {
      return visit((LSLConstant *) float_const);
    };
    virtual bool visit(LSLStringConstant *str_const) {
      return visit((LSLConstant *) str_const);
    };
    virtual bool visit(LSLKeyConstant *key_const) {
      return visit((LSLConstant *) key_const);
    };
    virtual bool visit(LSLListConstant *list_const) {
      return visit((LSLConstant *) list_const);
    };
    virtual bool visit(LSLVectorConstant *vec_const) {
      return visit((LSLConstant *) vec_const);
    };
    virtual bool visit(LSLQuaternionConstant *quat_const) {
      return visit((LSLConstant *) quat_const);
    };
    virtual bool visit(LSLGlobalFunction *glob_func) {
      return visit((LSLASTNode *) glob_func);
    };
    virtual bool visit(LSLParamList *params) {
      return visit((LSLASTNodeList<LSLASTNode> *) params);
    };
    virtual bool visit(LSLFunctionDec *func_dec) {
      return visit((LSLParamList *) func_dec);
    };
    virtual bool visit(LSLEventDec *event_dec) {
      return visit((LSLParamList *) event_dec);
    };
    virtual bool visit(LSLState *state) {
      return visit((LSLASTNode *) state);
    };
    virtual bool visit(LSLEventHandler *handler) {
      return visit((LSLASTNode *) handler);
    };
    virtual bool visit(LSLStatement *stmt) {
      return visit((LSLASTNode *) stmt);
    };
    virtual bool visit(LSLCompoundStatement *compound_stmt) {
      return visit((LSLStatement *) compound_stmt);
    };
    virtual bool visit(LSLNopStatement *nop_stmt) {
      return visit((LSLStatement *) nop_stmt);
    };
    virtual bool visit(LSLExpressionStatement *expr_stmt) {
      return visit((LSLStatement *) expr_stmt);
    };
    virtual bool visit(LSLStateStatement *state_stmt) {
      return visit((LSLStatement *) state_stmt);
    };
    virtual bool visit(LSLJumpStatement *jump_stmt) {
      return visit((LSLStatement *) jump_stmt);
    };
    virtual bool visit(LSLLabel *label_stmt) {
      return visit((LSLStatement *) label_stmt);
    };
    virtual bool visit(LSLReturnStatement *ret_stmt) {
      return visit((LSLStatement *) ret_stmt);
    };
    virtual bool visit(LSLIfStatement *if_stmt) {
      return visit((LSLStatement *) if_stmt);
    };
    virtual bool visit(LSLForStatement *for_stmt) {
      return visit((LSLStatement *) for_stmt);
    };
    virtual bool visit(LSLDoStatement *do_stmt) {
      return visit((LSLStatement *) do_stmt);
    };
    virtual bool visit(LSLWhileStatement *while_stmt) {
      return visit((LSLStatement *) while_stmt);
    };
    virtual bool visit(LSLDeclaration *decl_stmt) {
      return visit((LSLStatement *) decl_stmt);
    };
    virtual bool visit(LSLExpression *expr) {
      return visit((LSLASTNode *) expr);
    };
    virtual bool visit(LSLBinaryExpression *bin_expr) {
      return visit((LSLExpression *) bin_expr);
    };
    virtual bool visit(LSLUnaryExpression *unary_expr) {
      return visit((LSLExpression *) unary_expr);
    };
    virtual bool visit(LSLConstantExpression *constant_expr) {
      return visit((LSLExpression *) constant_expr);
    };
    virtual bool visit(LSLParenthesisExpression *parens_expr) {
      return visit((LSLExpression *) parens_expr);
    };
    virtual bool visit(LSLTypecastExpression *cast_expr) {
      return visit((LSLExpression *) cast_expr);
    };
    virtual bool visit(LSLBoolConversionExpression *bool_expr) {
      return visit((LSLExpression *) bool_expr);
    };
    virtual bool visit(LSLPrintExpression *print_expr) {
      return visit((LSLExpression *) print_expr);
    };
    virtual bool visit(LSLFunctionExpression *func_expr) {
      return visit((LSLExpression *) func_expr);
    };
    virtual bool visit(LSLVectorExpression *vec_expr) {
      return visit((LSLExpression *) vec_expr);
    };
    virtual bool visit(LSLQuaternionExpression *quat_expr) {
      return visit((LSLExpression *) quat_expr);
    };
    virtual bool visit(LSLListExpression *list_expr) {
      return visit((LSLExpression *) list_expr);
    };
    virtual bool visit(LSLLValueExpression *lvalue) {
      return visit((LSLExpression *) lvalue);
    };
    virtual bool visit(LSLType *node) {
      return visit((LSLASTNode*)node);
    }

    virtual bool visitSpecific(LSLASTNode *node);
    void visitChildren(LSLASTNode *node);
    // only used for depth-first visitors
    virtual bool beforeDescend(LSLASTNode *node) {return true;}
    virtual bool isDepthFirst() {return false;}
};

class DepthFirstASTVisitor: public ASTVisitor {
    virtual bool isDepthFirst() {return true;}
};

}

#endif //TAILSLIDE_VISITOR_HH
