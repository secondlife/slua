#ifndef TAILSLIDE_SYMBOL_RESOLUTION_HH
#define TAILSLIDE_SYMBOL_RESOLUTION_HH

#include <vector>

#include "../lslmini.hh"
#include "../visitor.hh"

namespace Tailslide {

class SymbolResolutionVisitor : public ASTVisitor {
  public:
    SymbolResolutionVisitor(bool linden_jump_semantics, ScriptAllocator *allocator)
      : _mAllocator(allocator), _mLindenJumpSemantics(linden_jump_semantics), _mInGlobals(false) {}

  protected:
    virtual bool visit(LSLDeclaration *decl_stmt);
    virtual bool visit(LSLGlobalVariable *glob_var);
    virtual bool visit(LSLGlobalFunction *glob_func);
    virtual bool visit(LSLLValueExpression *lvalue);
    virtual bool visit(LSLFunctionExpression *func_expr);
    virtual bool visit(LSLScript *script);
    virtual bool visit(LSLFunctionDec *func_dec);
    virtual bool visit(LSLEventHandler *handler);
    virtual bool visit(LSLEventDec *event_dec);
    virtual bool visit(LSLLabel *label_stmt);
    virtual bool visit(LSLJumpStatement *jump_stmt);
    virtual bool visit(LSLStateStatement *state_stmt);
    virtual bool visit(LSLCompoundStatement *compound_stmt);
    virtual bool visit(LSLDoStatement *do_stmt);
    virtual bool visit(LSLWhileStatement *while_stmt);
    virtual bool visit(LSLForStatement *for_stmt);
    void visitLoop(LSLASTNode *loop_stmt);

    void replaceSymbolTable(LSLASTNode *node, LSLSymbolTableType symtab_type);

    void resolvePendingJumps(LSLASTNode *func_like);
    ScriptAllocator *_mAllocator;
    std::vector<LSLJumpStatement*> _mPendingJumps;
    std::vector<LSLLabel*> _mCollectedLabels;
    std::unordered_map<LSLASTNode *, LSLASTNode *> _mEnclosingLoops;
    LSLASTNode *_mCurrentLoop = nullptr;
    bool _mLindenJumpSemantics;
    bool _mInGlobals;
};

}
#endif //TAILSLIDE_SYMBOL_RESOLUTION_HH
