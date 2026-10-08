#pragma once

#include "../visitor.hh"

namespace Tailslide {

/// Takes compound assignments and de-structures them, takes places where
/// implicit typecasting happens and adds explicit typecast nodes.
/// Doesn't currently wrap in parenthesis, so not appropriate for pretty-printing.
class DeSugaringVisitor : public ASTVisitor {
  public:
    explicit DeSugaringVisitor(ScriptAllocator *allocator, bool mono_semantics)
        : _mAllocator(allocator), _mMonoSemantics(mono_semantics) {}
  protected:
    bool visit(LSLBinaryExpression *bin_expr) override;
    bool visit(LSLUnaryExpression *unary_expr) override;
    bool visit(LSLDeclaration *decl_stmt) override;
    bool visit(LSLGlobalVariable *glob_var) override;
    bool visit(LSLVectorExpression *vec_expr) override;
    bool visit(LSLQuaternionExpression *quat_expr) override;
    bool visit(LSLFunctionExpression *func_expr) override;
    bool visit(LSLLValueExpression *lvalue) override;

    bool visit(LSLReturnStatement *ret_stmt) override;
    bool visit(LSLForStatement *for_stmt) override;
    bool visit(LSLWhileStatement *while_stmt) override;
    bool visit(LSLDoStatement *do_stmt) override;
    bool visit(LSLIfStatement *if_stmt) override;
    bool visit(LSLExpressionStatement *expr_stmt) override;

    virtual LSLASTNode *rewriteBuiltinLValue(LSLLValueExpression *lvalue);
    void handleCoordinateExpression(LSLASTNode *coord_expr);
    void maybeInjectCast(LSLExpression *expr, LSLType *to);
    void maybeInjectBoolConversion(LSLExpression *expr);

    ScriptAllocator *_mAllocator;
    bool _mMonoSemantics;
};

// Desugaring that also rewrites the tree to match how LL's compiler
// would have parsed a given node. Results in less optimal bytecode.
class LLConformantDeSugaringVisitor : public DeSugaringVisitor {
  public:
    explicit LLConformantDeSugaringVisitor(ScriptAllocator *allocator, bool mono_semantics)
        : DeSugaringVisitor(allocator, mono_semantics) {};
  protected:
    bool visit(LSLConstantExpression *constant_expr) override;
    // Make things stop complaining about hidden virtual functions
    using DeSugaringVisitor::visit;

    LSLASTNode *rewriteBuiltinLValue(LSLLValueExpression *lvalue) override;
};

}
