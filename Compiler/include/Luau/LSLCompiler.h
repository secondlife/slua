#ifndef LUAU_LSLCOMPILER_H
#define LUAU_LSLCOMPILER_H
#include <map>
#include <unordered_set>
#include <tailslide/passes/desugaring.hh>

#include "Luau/BytecodeBuilder.h"

namespace Tailslide {

struct LuauSymbolData {
    // Index of the given variable within the locals (not for globals!)
    // For functions, this is the function ID.
    uint32_t index = 0;
    // all locals (if this symbol is a function or event handler)
    std::vector<LSLSymbol *> locals{};
    // Constants that need small indices (tracked during resource visitor pass)
    // Each import is stored as module,method pair (e.g., "bit32","bnot" or "lsl","cast")
    std::unordered_set<std::string> needed_import_strings; // Individual strings used in imports
    bool needs_int_one = false;           // For integer ++/--
    bool needs_float_one = false;         // For float ++/--
    bool needs_vector_one = false;        // For vector ++/-- (unlikely but possible)
};

typedef std::unordered_map<LSLSymbol *, LuauSymbolData> LuauSymbolMap;

// Walks the script, figuring out how much space to reserve for data slots
// and what order to place them in.
class LuauResourceVisitor : public ASTVisitor {
public:
    explicit LuauResourceVisitor(LuauSymbolMap *sym_data) : _mSymData(sym_data) {}

protected:
    bool visit(LSLScript* script) override;
    bool visit(LSLGlobalFunction *glob_func) override;
    bool visit(LSLState *state) override;
    bool visit(LSLEventHandler *handler) override;
    bool visit(LSLDeclaration* decl_stmt) override;
    // Expression visitors to detect constant needs
    bool visit(LSLUnaryExpression *un_expr) override;
    bool visit(LSLBinaryExpression *bin_expr) override;
    bool visit(LSLStateStatement *state_stmt) override;
    bool visit(LSLBoolConversionExpression *bool_expr) override;
    bool visit(LSLTypecastExpression *typecast_expr) override;

    /// Assign each local and parameter a register
    void pushLocalNode(LSLASTNode* node);
    /// Track declaration of parameters and loals within a function-like node
    void handleFuncLike(LSLASTNode *node);

    LuauSymbolData *getSymbolData(LSLSymbol *sym);

    LuauSymbolData *_mCurrentFunc = nullptr;
    LuauSymbolMap *_mSymData = nullptr;
    uint32_t _mTopFuncID = 0;
    uint32_t _mTopStateID = 0;
};


class LuauVisitor : public ASTVisitor
{
public:
    explicit LuauVisitor(Luau::BytecodeBuilder *builder, LuauSymbolMap &symbol_map);

protected:
    bool visit(LSLScript* script) override;
    bool visit(LSLGlobalVariable* glob_var) override;
    bool visit(LSLState* state) override;
    bool visit(LSLEventHandler* handler) override;
    bool visit(LSLGlobalFunction* func) override;
    void buildFunction(LSLASTNode* func_like);
    bool visit(LSLDeclaration* decl) override;
    bool visit(LSLIfStatement *if_stmt) override;
    bool visit(LSLForStatement *for_stmt) override;
    bool visit(LSLWhileStatement * while_stmt) override;
    bool visit(LSLDoStatement * do_stmt) override;
    bool visit(LSLLabel *label_stmt) override;
    bool visit(LSLJumpStatement *jump_stmt) override;
    bool visit(LSLReturnStatement* ret_stmt) override;
    bool visit(LSLStateStatement* state_stmt) override;
    bool visit(LSLExpressionStatement* expr_stmt) override;
    bool visit(LSLPrintExpression* print_expr) override;
    bool visit(LSLConstantExpression* const_expr) override;
    void pushLValue(LSLLValueExpression* lvalue_expr, bool push_member);
    bool visit(LSLBoolConversionExpression *bool_expr) override;
    bool visit(LSLListExpression* list_expr) override;
    bool visit(LSLVectorExpression* vec_expr) override;
    bool visit(LSLQuaternionExpression* quat_expr) override;
    bool visit(LSLTypecastExpression* typecast_expr) override;
    bool visit(LSLFunctionExpression* print_expr) override;
    bool visit(LSLLValueExpression* lvalue_expr) override;
    bool visit(LSLUnaryExpression* un_expr) override;
    bool visit(LSLBinaryExpression* bin_expr) override;

    /// Handles an expression, which may do `MOVE` ellision if it's a simple local LValue.
    /// Returns the register the expression result is stored in.
    /// Note that this can ONLY be done in cases where we know a `MOVE` isn't strictly
    /// necessary, it isn't appropriate when we need a particular register order like in `CALL`s.
    /// Basically, if you want the result to end up in a specific register, you can't use this.
    uint8_t handlePositionIndependentExpr(LSLExpression *expr);
    void pushArgument(LSLExpression* expr, bool want_float_cast=true);
    unsigned int pushConstant(LSLConstant* cv);

    uint8_t allocReg(LSLASTNode *node, unsigned int count = 1);
    int16_t nodeSymIdToConstant(LSLASTNode* node, bool string_only = false);
    /// Get the target register for the current expression.
    /// This allows MOVE ellision when storing something directly into a local
    uint8_t takeTargetReg(LSLASTNode* node);
    /// Maybe emit a MOVE instruction if expected_target != and the regs don't match
    void maybeMove(const int16_t expected_target, const uint8_t actual_reg) const;
    void pushGlobal(uint8_t target_reg, const char* global_name);
    void pushGlobal(uint8_t target_reg, LSLASTNode* node);
    void setGlobal(uint8_t source_reg, LSLASTNode* glob_node);
    void pushImport(uint8_t target_reg, const char* import1, const char* import2) const;
    void pushImport(uint8_t target_reg, const char* import1) const;
    void patchJumpOrThrow(size_t jumpLabel, size_t targetLabel);
    bool needTruncateToFloat(Tailslide::LSLExpression* expr) const;
    int16_t addConstantUnder(LSLConstant* cv, size_t limit) const;
    int16_t addConstant(LSLConstant* cv) const;
    int16_t addConstantFloat(float num, size_t limit) const;
    int16_t addConstantInteger(int32_t num, size_t limit) const;
    int16_t addConstantString(Luau::BytecodeBuilder::StringRef str, size_t limit) const;
    int16_t addImport(Luau::BytecodeBuilder::StringRef str1, Luau::BytecodeBuilder::StringRef str2, size_t limit) const;
    int16_t addImport(Luau::BytecodeBuilder::StringRef str1, size_t limit) const;

    // map of symbols to their registers in their local context
    LuauSymbolMap &_mSymbolMap;
    std::unordered_map<LSLSymbol *, std::string> _mSymbolNames;
    // vector of function names to their closure constant IDs
    // for reasons unknown to me, these closures need to be created in order,
    // so we need a vector rather than a map!
    std::vector<std::pair<std::string, uint32_t>> _mFunctionFuncIds;
    std::unordered_map<uint32_t, LSLSymbol *> _mJumpTargets;
    std::unordered_map<LSLSymbol *, uint32_t> _mLabelTargets;
public:
    Luau::BytecodeBuilder *mBuilder;
    // current top of the stack, per-function
    unsigned int mRegTop;
    // current max stack size in the function
    unsigned int mStackSize;
    int16_t mTargetReg;
};

}

void compileLSLOrThrow(Luau::BytecodeBuilder &bcb, const std::string &source);
std::string compileLSL(const std::string &source);

#endif // LUAU_LSLCOMPILER_H
