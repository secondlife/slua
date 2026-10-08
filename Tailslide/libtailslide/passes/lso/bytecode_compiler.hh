#pragma once

#include <map>
#include <string>

#include "bytecode_format.hh"
#include "resource_collector.hh"
#include "../../visitor.hh"

namespace Tailslide {

class LSOBytecodeCompiler : public ASTVisitor {
  public:
    explicit LSOBytecodeCompiler(LSOSymbolDataMap &symbol_data_map) : _mSymData (symbol_data_map) {}

  protected:
    void buildFunction(LSLASTNode *func);
    bool visit(LSLEventHandler *handler) override;
    bool visit(LSLGlobalFunction *glob_func) override;

    bool visit(LSLConstantExpression *constant_expr) override;
    bool visit(LSLVectorExpression *vec_expr) override;
    bool visit(LSLQuaternionExpression *quat_expr) override;
    bool visit(LSLListExpression *list_expr) override;
    bool visit(LSLUnaryExpression *unary_expr) override;
    bool visit(LSLBinaryExpression *bin_expr) override;
    bool visit(LSLLValueExpression *lvalue) override;
    bool visit(LSLTypecastExpression *cast_expr) override;
    bool visit(LSLPrintExpression *print_expr) override;
    bool visit(LSLFunctionExpression *func_expr) override;

    bool visit(LSLExpressionStatement *expr_stmt) override;
    bool visit(LSLJumpStatement *jump_stmt) override;
    bool visit(LSLLabel *label_stmt) override;
    bool visit(LSLIfStatement *if_stmt) override;
    bool visit(LSLForStatement *for_stmt) override;
    bool visit(LSLWhileStatement *while_stmt) override;
    bool visit(LSLDoStatement *do_stmt) override;
    bool visit(LSLDeclaration *decl_stmt) override;
    bool visit(LSLReturnStatement *ret_stmt) override;
    bool visit(LSLStateStatement *state_stmt) override;

    void pushConstant(LSLConstant *constant);
    int32_t calculateLValueOffset(LSLLValueExpression *lvalue);
    void storeStackToLValue(LSLLValueExpression *lvalue);
    void popLocals();
    void writeReturn();

    LSOSymbolDataMap &_mSymData;
    LSOSymbolData *_mFuncSymData = nullptr;
    /// map of label name -> position of the jump instruction. Intentionally
    /// disallows multiple jumps to match LL LSO compiler behavior.
    std::map<std::string, uint32_t> _mJumpMap;
    /// map of label symbol ptr to its location in the bytecode
    std::map<std::string, uint32_t> _mLabelMap;

  public:
    LSOBitStream mCodeBS {ENDIAN_BIG};
};

}
