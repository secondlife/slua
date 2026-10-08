#pragma once

#include "../lslmini.hh"
#include "../visitor.hh"

namespace Tailslide {


// Only cares that global variable rvalue is some expression that can
// be evaluated at compile-time and doesn't mutate state.
class GlobalExprValidatingVisitor: public ASTVisitor {
  protected:
    bool visit(LSLGlobalVariable *glob_var) override;
    bool visit(LSLFunctionExpression *func_expr) override;
    // these aren't interesting for validation of globals
    bool visit(LSLGlobalFunction *glob_func) override { return false; };
    bool visit(LSLState *state) override { return false; };
    // reset to `true` before checking each rvalue, if it's false after
    // visiting the rvalue then raise an error about non-constant expression.
    bool _mValidRValue = true;
};

// More strict form of above. Enforces the same restrictions on
// expressions for global variable assignments as LL's implementation.
// Useful if you care about linting with SL in mind.
class SimpleAssignableValidatingVisitor: public GlobalExprValidatingVisitor {
  public:
    explicit SimpleAssignableValidatingVisitor(bool mono_semantics)
        : GlobalExprValidatingVisitor(), _mMonoSemantics(mono_semantics) {}
  protected:
    bool visit(LSLExpression *expr) override;
    bool visit(LSLUnaryExpression *unary_expr) override;
    bool visit(LSLLValueExpression *lvalue) override;
    // Make things stop complaining about hidden virtual functions
    using GlobalExprValidatingVisitor::visit;

    bool _mMonoSemantics;
};

}
