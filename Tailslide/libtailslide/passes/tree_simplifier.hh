#ifndef TAILSLIDE_TREE_SIMPLIFIER_HH
#define TAILSLIDE_TREE_SIMPLIFIER_HH

#include "../visitor.hh"

namespace Tailslide {

struct OptimizationOptions {
    bool fold_constants = false;
    bool prune_unused_locals = false;
    bool prune_unused_globals = false;
    bool prune_unused_functions = false;
    bool may_create_new_strs = false;
    explicit operator bool() const {
      return fold_constants || prune_unused_functions || prune_unused_locals || prune_unused_globals;
    }
};

class TreeSimplifyingVisitor: public DepthFirstASTVisitor {
  public:
    explicit TreeSimplifyingVisitor(const OptimizationOptions &opts): mOpts(opts) {};
    OptimizationOptions mOpts;
    int mFoldedLevel = 0;

    virtual bool visit(LSLDeclaration *decl_stmt);
    virtual bool visit(LSLGlobalVariable *glob_var);
    virtual bool visit(LSLGlobalFunction *glob_func);
    virtual bool visit(LSLExpression *expr);
    virtual bool visit(LSLLValueExpression *lvalue);
    virtual bool visit(LSLConstantExpression *constant_expr);

    bool handleGlobal(LSLASTNode *glob);
};
}


#endif //TAILSLIDE_TREE_SIMPLIFIER_HH
