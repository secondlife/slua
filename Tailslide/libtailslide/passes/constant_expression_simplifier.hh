#pragma once

#include "../visitor.hh"

namespace Tailslide {

class ConstantExpressionSimplifier : public DepthFirstASTVisitor {
  public:
    explicit ConstantExpressionSimplifier(ScriptAllocator *allocator)
        : _mAllocator(allocator) {};

    virtual bool visit(LSLVectorExpression *vec_expr);
    virtual bool visit(LSLTypecastExpression *cast_expr);

  private:
    ScriptAllocator *_mAllocator;

    // Helper for handling the coordinate classes in case we give Quaternions a constant form.
    static bool extractConstantCoordComponents(LSLASTNode *coord_expr, int expected_count, float *out_values);
};

}
