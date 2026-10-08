#ifndef TAILSLIDE_PRETTY_PRINT_HH
#define TAILSLIDE_PRETTY_PRINT_HH

#include <sstream>
#include "../lslmini.hh"
#include "../visitor.hh"

namespace Tailslide {

struct PrettyPrintOpts {
    bool mangle_local_names;
    bool mangle_func_names;
    bool mangle_global_names;
    bool show_unmangled;
};

class PrettyPrintVisitor: public ASTVisitor {
public:
    explicit PrettyPrintVisitor(const PrettyPrintOpts& ctx)
    : mTabs(0), _mOpts(ctx)
    {}

    int mTabs;

    virtual bool visit(LSLASTNode *node);
    virtual bool visit(LSLIdentifier *id);
    virtual bool visit(LSLType *type);
    virtual bool visit(LSLGlobalVariable *glob_var);
    virtual bool visit(LSLIntegerConstant *int_const);
    virtual bool visit(LSLFloatConstant *float_const);
    virtual bool visit(LSLStringConstant *str_const);
    virtual bool visit(LSLKeyConstant *key_const);
    virtual bool visit(LSLListConstant *list_const);
    virtual bool visit(LSLVectorConstant *vec_const);
    virtual bool visit(LSLQuaternionConstant *quat_const);
    virtual bool visit(LSLGlobalFunction *glob_func);
    virtual bool visit(LSLParamList *params);
    virtual bool visit(LSLState *state);
    virtual bool visit(LSLEventHandler *handler);
    virtual bool visit(LSLNopStatement *nop_stmt);
    virtual bool visit(LSLExpressionStatement *expr_stmt);
    virtual bool visit(LSLCompoundStatement *compound_stmt);
    virtual bool visit(LSLStateStatement *state_stmt);
    virtual bool visit(LSLJumpStatement *jump_stmt);
    virtual bool visit(LSLLabel *label_stmt);
    virtual bool visit(LSLReturnStatement *ret_stmt);
    virtual bool visit(LSLIfStatement *if_stmt);
    virtual bool visit(LSLForStatement *for_stmt);
    virtual bool visit(LSLDoStatement *do_stmt);
    virtual bool visit(LSLWhileStatement *while_stmt);
    virtual bool visit(LSLDeclaration *decl_stmt);
    virtual bool visit(LSLUnaryExpression *unary_expr);
    virtual bool visit(LSLBinaryExpression *bin_expr);
    virtual bool visit(LSLLValueExpression *lvalue);
    virtual bool visit(LSLConstantExpression *constant_expr);
    virtual bool visit(LSLTypecastExpression *cast_expr);
    virtual bool visit(LSLPrintExpression *print_expr);
    virtual bool visit(LSLFunctionExpression *func_expr);
    virtual bool visit(LSLVectorExpression *vec_expr);
    virtual bool visit(LSLQuaternionExpression *quat_expr);
    virtual bool visit(LSLListExpression *list_expr);
    virtual bool visit(LSLParenthesisExpression *parens_expr);
    virtual bool visit(LSLScript *script);

    void prettifyChildrenSep(LSLASTNode *node, const char *separator);
    void prettifyCoordinateMembers(LSLASTNode *coord_node);
    void doTabs();
    std::stringstream mStream {};
protected:
    const PrettyPrintOpts &_mOpts;
};

}

#endif //TAILSLIDE_PRETTY_PRINT_HH
