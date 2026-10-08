#ifndef TAILSLIDE_VALUES_HH
#define TAILSLIDE_VALUES_HH

#include "../lslmini.hh"
#include "../visitor.hh"
#include "../operations.hh"

namespace Tailslide {
class ConstantDeterminingVisitor : public DepthFirstASTVisitor {
  public:
    explicit ConstantDeterminingVisitor(AOperationBehavior *behavior, ScriptAllocator *allocator)
        : _mOperationBehavior(behavior), _mAllocator(allocator) {}

    virtual bool beforeDescend(LSLASTNode *node);

    virtual bool visit(LSLScript *script);
    virtual bool visit(LSLDeclaration *decl_stmt);
    virtual bool visit(LSLExpression *expr);
    virtual bool visit(LSLGlobalVariable *glob_var);
    virtual bool visit(LSLLValueExpression *lvalue);
    virtual bool visit(LSLListExpression *list_expr);
    virtual bool visit(LSLVectorExpression *vec_expr);
    virtual bool visit(LSLQuaternionExpression *quat_expr);
    virtual bool visit(LSLTypecastExpression *cast_expr);
  protected:
    AOperationBehavior *_mOperationBehavior = nullptr;
    ScriptAllocator *_mAllocator;

    void handleDeclaration(LSLASTNode *decl_node);
};
}

#endif //TAILSLIDE_VALUES_HH
