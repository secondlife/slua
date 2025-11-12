// LSL -> Luau bytecode compiler

#include "Luau/LSLCompiler.h"

#include "Luau/Compiler.h"
#include "Luau/ParseResult.h"

#include <tailslide/tailslide.hh>
#include <tailslide/logger.hh>
#include <tailslide/passes/constant_expression_simplifier.hh>
#include <unordered_set>

using namespace Tailslide;

constexpr uint32_t kMaxRegisterCount = 255;
constexpr uint32_t kMaxLocalCount = 200;
constexpr uint32_t kMaxStringImportRef = 1024;
// static const uint32_t kMaxInstructionCount = 1'000'000'000;

static Luau::Location convertLoc(TailslideLType *loc)
{
    if (loc == nullptr)
        return {{0, 0}, {0, 0}};
    return {
        {(unsigned int)loc->first_line, (unsigned int)loc->first_column},
        {(unsigned int)loc->last_line, (unsigned int)loc->last_column}
    };
}

bool LuauResourceVisitor::visit(LSLScript *script)
{
    // Do functions and event handlers first, in the same order as defined
    for (auto* glob : *script->getGlobals())
    {
        if (glob->getNodeType() != NODE_GLOBAL_FUNCTION)
            continue;
        glob->visit(this);
    }
    // likewise, event handlers are basically functions, visit them all.
    script->getStates()->visit(this);
    return false;
}

bool LuauResourceVisitor::visit(LSLGlobalFunction *glob_func) {
    handleFuncLike(glob_func);
    return false;
}

bool LuauResourceVisitor::visit(LSLState *state)
{
    // This better not be too big to fit in LOADN's args
    if (_mTopStateID >= INT16_MAX)
        throw Luau::CompileError(convertLoc(state->getLoc()), "Too many states");
    getSymbolData(state->getSymbol())->index = (int16_t)_mTopStateID++;
    return true;
}

bool LuauResourceVisitor::visit(LSLEventHandler *handler) {
    handleFuncLike(handler);
    return false;
}

void LuauResourceVisitor::handleFuncLike(LSLASTNode* node)
{
    auto *sym = node->getSymbol();
    auto *func_sym_data = getSymbolData(sym);
    _mCurrentFunc = func_sym_data;
    if (_mTopFuncID >= INT16_MAX)
        throw Luau::CompileError(convertLoc(node->getLoc()), "Too many functions");
    func_sym_data->index = (int16_t)_mTopFuncID++;

    // Register the parameters
    for (auto *param_node : *node->getSymbol()->getFunctionDecl())
    {
        pushLocalNode(param_node);
    }

    // pick up local declarations
    visitChildren(node);
    _mCurrentFunc = nullptr;
}


bool LuauResourceVisitor::visit(LSLDeclaration *decl_stmt) {
    pushLocalNode(decl_stmt);
    return true;
}

void LuauResourceVisitor::pushLocalNode(LSLASTNode *node)
{
    auto *param_sym = node->getSymbol();
    auto *sym_data = getSymbolData(param_sym);

    if (_mCurrentFunc->locals.size() > kMaxLocalCount)
        throw Luau::CompileError(convertLoc(node->getLoc()), "Too many local variables");

    sym_data->index = (uint8_t)_mCurrentFunc->locals.size();
    _mCurrentFunc->locals.push_back(param_sym);
}

LuauSymbolData *LuauResourceVisitor::getSymbolData(LSLSymbol *sym) {
    auto sym_iter = _mSymData->find(sym);
    if (sym_iter != _mSymData->end())
        return &sym_iter->second;
    (*_mSymData)[sym] = {};
    return &_mSymData->find(sym)->second;
}

bool LuauResourceVisitor::visit(LSLUnaryExpression *un_expr)
{
    if (!_mCurrentFunc)
        return false;

    auto op = un_expr->getOperation();
    auto *child_expr = un_expr->getChildExpr();
    if (op == OP_PRE_INCR || op == OP_POST_INCR || op == OP_PRE_DECR || op == OP_POST_DECR)
    {
        auto *lvalue = (LSLLValueExpression *)child_expr;
        // We will need the type's `one` value as a low-indexed constant to do these ops
        _mCurrentFunc->needed_one_types.insert(lvalue->getType());
    }
    else if (op == OP_BIT_NOT)
    {
        _mCurrentFunc->needed_import_strings.insert("bit32");
        _mCurrentFunc->needed_import_strings.insert("bnot");
    }

    return true;
}

bool LuauResourceVisitor::visit(LSLBinaryExpression *bin_expr)
{
    if (!_mCurrentFunc)
        return false;

    switch(bin_expr->getOperation())
    {
        case OP_BIT_AND:
            _mCurrentFunc->needed_import_strings.insert("bit32");
            _mCurrentFunc->needed_import_strings.insert("band");
            break;
        case OP_BIT_OR:
            _mCurrentFunc->needed_import_strings.insert("bit32");
            _mCurrentFunc->needed_import_strings.insert("bor");
            break;
        case OP_BIT_XOR:
            _mCurrentFunc->needed_import_strings.insert("bit32");
            _mCurrentFunc->needed_import_strings.insert("bxor");
            break;
        case OP_SHIFT_LEFT:
            _mCurrentFunc->needed_import_strings.insert("bit32");
            _mCurrentFunc->needed_import_strings.insert("lshift");
            break;
        case OP_SHIFT_RIGHT:
            _mCurrentFunc->needed_import_strings.insert("bit32");
            _mCurrentFunc->needed_import_strings.insert("arshift");
            break;
        case OP_MUL_ASSIGN:
            _mCurrentFunc->needed_import_strings.insert("lsl");
            _mCurrentFunc->needed_import_strings.insert("cast");
            break;
        case '+':
            // List concatenation
            if (bin_expr->getLHS()->getIType() == LST_LIST || bin_expr->getRHS()->getIType() == LST_LIST)
            {
                _mCurrentFunc->needed_import_strings.insert("lsl");
                _mCurrentFunc->needed_import_strings.insert("table_concat");
            }
            break;
        case '=':
            // Check for member assignment
            if (bin_expr->getLHS()->getNodeSubType() == NODE_LVALUE_EXPRESSION)
            {
                auto *lval = (LSLLValueExpression *)bin_expr->getLHS();
                if (lval->getMember())
                {
                    _mCurrentFunc->needed_import_strings.insert("lsl");
                    _mCurrentFunc->needed_import_strings.insert("replace_axis");
                }
            }
            break;
        default:
            break;
    }

    return true;
}

bool LuauResourceVisitor::visit(LSLStateStatement *state_stmt)
{
    if (_mCurrentFunc)
    {
        _mCurrentFunc->needed_import_strings.insert("lsl");
        _mCurrentFunc->needed_import_strings.insert("change_state");
    }
    return true;
}

bool LuauResourceVisitor::visit(LSLBoolConversionExpression *bool_expr)
{
    if (!_mCurrentFunc)
        return false;

    if (bool_expr->getChildExpr()->getIType() == LST_KEY)
    {
        _mCurrentFunc->needed_import_strings.insert("lsl");
        _mCurrentFunc->needed_import_strings.insert("is_key_truthy");
    }
    return true;
}

bool LuauResourceVisitor::visit(LSLTypecastExpression *typecast_expr)
{
    if (_mCurrentFunc)
    {
        _mCurrentFunc->needed_import_strings.insert("lsl");
        _mCurrentFunc->needed_import_strings.insert("cast");
    }
    return true;
}

// Special de-sugaring logic not relevant in Mono
class LuauDeSugaringVisitor : public DeSugaringVisitor
{
public:
    explicit LuauDeSugaringVisitor(Tailslide::ScriptAllocator *allocator): DeSugaringVisitor(allocator, true) {};
    bool visit(Tailslide::LSLBinaryExpression *bin_expr) override;
protected:
    using DeSugaringVisitor::visit;
};

bool LuauDeSugaringVisitor::visit(Tailslide::LSLBinaryExpression* bin_expr)
{
    // Luau needs special handling for key == str because there isn't asymmetry
    // like there is in the Mono implementation.
    DeSugaringVisitor::visit(bin_expr);
    switch (bin_expr->getOperation())
    {
        case OP_EQ:
        case OP_NEQ:
            break;
        default:
            return true;
    }

    // Check if one of the operands is a string and one is a key
    auto lhs_type = bin_expr->getLHS()->getIType();
    auto rhs_type = bin_expr->getRHS()->getIType();
    if (lhs_type != LST_KEY && rhs_type != LST_KEY)
        return true;
    if (lhs_type != LST_STRING && rhs_type != LST_STRING)
        return true;

    // Try to cast both operands to a string
    maybeInjectCast(bin_expr->getLHS(), TYPE(LST_STRING));
    maybeInjectCast(bin_expr->getRHS(), TYPE(LST_STRING));

    return true;
}


class LuauLValueMutationVisitor : public ASTVisitor
{
public:
    LuauLValueMutationVisitor(): ASTVisitor() {};
    bool visit(Tailslide::LSLBinaryExpression *bin_expr) override;
    bool visit(Tailslide::LSLUnaryExpression *un_expr) override;

    std::unordered_set<LSLSymbol *> mSeen;
};

bool LuauLValueMutationVisitor::visit(Tailslide::LSLUnaryExpression* un_expr)
{
    switch (un_expr->getOperation())
    {
        case OP_PRE_INCR:
        case OP_POST_INCR:
        case OP_PRE_DECR:
        case OP_POST_DECR:
            break;
        default:
            // not interesting, doesn't mutate
            return true;
    }
    // Okay, we saw this symbol being mutated.
    mSeen.insert(un_expr->getChildExpr()->getSymbol());
    return true;
}

bool LuauLValueMutationVisitor::visit(Tailslide::LSLBinaryExpression* bin_expr)
{
    switch (bin_expr->getOperation())
    {
        // I really hate this operator :)
        case OP_MUL_ASSIGN:
        case '=':
            break;
        default:
            // not interesting, doesn't mutate
            return true;
    }

    // Okay, LHS should be an lvalue.
    mSeen.insert(bin_expr->getLHS()->getSymbol());

    return true;
}


// Copied from Compiler.cpp, used for keeping track of the virtual stack in registers
struct RegScope
{
    [[maybe_unused]] explicit RegScope(LuauVisitor* self)
        : self(self)
        , oldTop(self->mRegTop)
    {
    }

    // This ctor is useful to forcefully adjust the stack frame in case we know that registers after a certain point are scratch and can be
    // discarded
    [[maybe_unused]] RegScope(LuauVisitor* self, unsigned int top)
        : self(self)
        , oldTop(self->mRegTop)
    {
        LUAU_ASSERT(top <= self->mRegTop);
        self->mRegTop = top;
    }

    ~RegScope()
    {
        self->mRegTop = oldTop;
    }

    LuauVisitor* self;
    unsigned int oldTop;
};

// Set the target register for the next time an expression's result needs to be placed somewhere
struct TargetRegScope
{
    TargetRegScope(LuauVisitor* self, uint8_t target_reg)
        : self(self)
        , targetReg(target_reg)
    {
        oldTargetReg = self->mTargetReg;
        self->mTargetReg = target_reg;
    }

    ~TargetRegScope()
    {
        LUAU_ASSERT(self->mTargetReg == -1 || self->mTargetReg == targetReg);
        self->mTargetReg = oldTargetReg;
    }

    LuauVisitor* self;
    int16_t targetReg;
    int16_t oldTargetReg;
};

static Luau::BytecodeBuilder::StringRef sref(const std::string &str)
{
    return {str.c_str(), str.length()};
}

static Luau::BytecodeBuilder::StringRef sref(const char *val)
{
    return {val, strlen(val)};
}

static uint8_t srefHash(const Luau::BytecodeBuilder::StringRef string_ref)
{
    return (uint8_t)Luau::BytecodeBuilder::getStringHash(string_ref);
}

LuauVisitor::LuauVisitor(Luau::BytecodeBuilder* builder, LuauSymbolMap &symbol_map)
    : _mSymbolMap(symbol_map)
    , mBuilder(builder)
    , mRegTop(0)
    , mStackSize(0)
    , mTargetReg(-1)
{
    mBuilder->setDumpFlags(Luau::BytecodeBuilder::Dump_Code);
}

bool LuauVisitor::visit(LSLScript* script)
{
    // Do functions and event handlers first, we'll reference them in the main function.
    for (auto* glob : *script->getGlobals())
    {
        if (glob->getNodeType() != NODE_GLOBAL_FUNCTION)
            continue;
        glob->visit(this);
    }
    // likewise, event handlers are basically functions, visit them all.
    script->getStates()->visit(this);

    // Now we can build the implicit main function
    auto main_id = mBuilder->beginFunction(0);
    // Luau doesn't emit a name for the main function
    // mBuilder->setDebugFunctionName(sref("main"));
    // Walk over the globals
    for (auto* glob : *script->getGlobals())
    {
        if (glob->getNodeType() != NODE_GLOBAL_VARIABLE)
            continue;
        glob->visit(this);
    }

    for (auto const &iter : _mFunctionFuncIds)
    {
        // Compiling the function just creates the proto, this creates an actual function
        // object from the proto and assigns it to the global
        [[maybe_unused]] RegScope scope(this);
        auto reg_id = allocReg(script);
        mBuilder->emitAD(LOP_NEWCLOSURE, reg_id, (int16_t)iter.second);
        mBuilder->emitABC(LOP_SETGLOBAL, reg_id, 0, srefHash(sref(iter.first)));
        mBuilder->emitAux(addConstantString(sref(iter.first), INT16_MAX));

        // This needs to be registered as a child function of the main proto
        mBuilder->addChildFunction(iter.second);
    }

    mBuilder->emitABC(LOP_RETURN, 0, 0 + 1, 0);

    mBuilder->endFunction(mStackSize, 0, 0);
    mBuilder->setMainFunction(main_id);
    mBuilder->finalize();
    return false;
}

/// Handle declaration of global variables
bool LuauVisitor::visit(LSLGlobalVariable *glob_var)
{
    [[maybe_unused]] RegScope scope(this);
    nodeSymIdToConstant(glob_var);
    unsigned int initializer_target = mRegTop;

    if (auto *initializer = glob_var->getInitializer())
    {
        if (initializer->getIType() != LST_LIST && (initializer->getConstantValue() != nullptr))
        {
            // Just push the constant if we can, we don't need to do implicit casts or whatever at runtime.
            pushConstant(initializer->getConstantValue());
        }
        else
        {
            // No float truncation needed, constants should already be in float space.
            initializer->visit(this);
        }
    }
    else
    {
        pushConstant(glob_var->getSymbol()->getType()->getDefaultValue());
    }

    setGlobal(initializer_target, glob_var);
    return false;
}

bool LuauVisitor::visit(LSLState *state)
{
    for (auto *handler : *state->getEventHandlers())
    {
        handler->visit(this);
    }
    return false;
}

bool LuauVisitor::visit(LSLEventHandler *handler)
{
    buildFunction(handler);
    return false;
}

bool LuauVisitor::visit(LSLGlobalFunction *func)
{
    buildFunction(func);
    return false;
}

void LuauVisitor::buildFunction(LSLASTNode *func_like)
{
    auto *func_sym = func_like->getSymbol();
    // Temporary registers start above the locals, allocate space for locals
    [[maybe_unused]] RegScope scope(this);
    auto &sym_data = _mSymbolMap[func_like->getSymbol()];
    allocReg(func_like, (uint8_t)sym_data.locals.size());

    auto *param_list = func_sym->getFunctionDecl();
    auto func_id = mBuilder->beginFunction(param_list->getNumChildren());
    // make sure the function id actually matches what we expect
    LUAU_ASSERT(func_id == sym_data.index);

    // Pre-allocate constants that need small indices (detected by LuauResourceVisitor)
    // Do this first to ensure they get low constant indices
    for (auto one_type : sym_data.needed_one_types)
    {
        addConstantUnder(one_type->getOneValue(), UINT8_MAX);
    }

    // Pre-allocate import strings to ensure they're under index 1024
    for (const auto& import_str : sym_data.needed_import_strings)
    {
        addConstantString(sref(import_str), kMaxStringImportRef);
    }

    // If the function has irreducible control flow, we need to make sure that
    // all locals are initialized. If they jump over declaration something
    // insane may happen.
    if (func_sym->getHasUnstructuredJumps())
    {
        int local_idx = 0;
        for (auto local_sym : sym_data.locals)
        {
            // Don't zero out arguments!
            if (local_sym->getSubType() == SYM_LOCAL)
            {
                TargetRegScope target_local_scope(this, local_idx);
                pushConstant(local_sym->getType()->getDefaultValue());
            }
            ++local_idx;
        }
    }

    // Handle any statements
    visitChildren(func_like);

    // This is a bit hacky, but the statements list exists here for both
    // event handlers and global functions.
    auto *child = func_like->getChild(2);
    LUAU_ASSERT(child->getNodeSubType() == NODE_COMPOUND_STATEMENT);
    auto* compount_stmt = (LSLCompoundStatement *)child;

    bool is_last_stmt_ret = false;
    int num_children = compount_stmt->getNumChildren();
    if (num_children)
    {
        auto *last_stmt = compount_stmt->getChild(num_children - 1);
        is_last_stmt_ret = last_stmt->getNodeSubType() == NODE_RETURN_STATEMENT;
    }

    if (!func_sym->getAllPathsReturn() || !is_last_stmt_ret)
    {
        // We need to do this as well if the last statement in a function
        //  isn't a return, the bytecode verifier is very ornery.
        // Synthesize a void return if there wasn't an explicit one
        mBuilder->emitABC(LOP_RETURN, 0, 0 + 1, 0);
    }

    // Make sure the function name is registered in the string table
    nodeSymIdToConstant(func_like, true);
    const std::string &name = _mSymbolNames[func_like->getSymbol()];
    mBuilder->setDebugFunctionName(sref(name));
    // Keep track of these so that we can assign the globals later
    _mFunctionFuncIds.emplace_back(name, func_id);

    for (auto [jump_loc, label_sym] : _mJumpTargets)
    {
        patchJumpOrThrow(jump_loc, _mLabelTargets[label_sym]);
    }
    _mLabelTargets.clear();
    _mJumpTargets.clear();

    // no upvals in LSL, we only have singly-nested coroutines + globals!
    mBuilder->endFunction(mStackSize, 0);
    // need to reset stack size after we leave the function
    mStackSize = 0;
}

/// Handles _local_ declarations, globals are LSLGlobalVariable
bool LuauVisitor::visit(LSLDeclaration *decl)
{
    [[maybe_unused]] RegScope scope(this);
    auto local_reg = _mSymbolMap[decl->getSymbol()].index;
    TargetRegScope target_scope(this, local_reg);
    if (auto *expr = decl->getInitializer())
    {
        expr->visit(this);
        if (needTruncateToFloat(expr))
        {
            // locals must be truncated to float space under Mono rules.
            mBuilder->emitABC(LOP_LSL_DOUBLE2FLOAT, local_reg, local_reg, 0);
        }
    }
    else
    {
        pushConstant(decl->getSymbol()->getType()->getDefaultValue());
    }

    return false;
}

bool LuauVisitor::visit(LSLIfStatement *if_stmt)
{
    uint8_t expr_reg;
    {
        [[maybe_unused]] RegScope expr_scope(this);
        expr_reg = evalExprToSourceReg(if_stmt->getCheckExpr());
    }

    auto *false_node = if_stmt->getFalseBranch();
    auto jump_past_true = mBuilder->emitLabel();
    mBuilder->emitAD(LOP_JUMPIFNOT, expr_reg, 0);

    if_stmt->getTrueBranch()->visit(this);
    if (false_node)
    {
        auto jump_past_false = mBuilder->emitLabel();
        // Jump over the false case
        mBuilder->emitAD(LOP_JUMP, 0, 0);

        auto jump_past_true_target = mBuilder->emitLabel();
        false_node->visit(this);
        auto jump_past_false_target = mBuilder->emitLabel();
        patchJumpOrThrow(jump_past_false, jump_past_false_target);
        patchJumpOrThrow(jump_past_true, jump_past_true_target);
    }
    else
    {
        auto jump_past_true_target = mBuilder->emitLabel();
        patchJumpOrThrow(jump_past_true, jump_past_true_target);
    }
    return false;
}

bool LuauVisitor::visit(LSLForStatement *for_stmt)
{
    // execute instructions to initialize vars, there can be multiple
    for(auto *init_expr : *for_stmt->getInitExprs())
    {
        init_expr->visit(this);
    }
    auto jump_to_start_label = mBuilder->emitLabel();
    // run the check expression, exiting the loop if it fails
    uint8_t expr_reg;
    {
        [[maybe_unused]] RegScope expr_scope(this);
        expr_reg = evalExprToSourceReg(for_stmt->getCheckExpr());
    }
    auto jump_to_end = mBuilder->emitLabel();
    mBuilder->emitAD(LOP_JUMPIFNOT, expr_reg, 0);
    // run the body of the loop
    for_stmt->getBody()->visit(this);
    // run the increment expressions, there could be multiple.
    for(auto *incr_expr : *for_stmt->getIncrExprs())
    {
        [[maybe_unused]] RegScope expr_scope(this);
        incr_expr->visit(this);
    }
    // jump back up to the check expression at the top
    auto jump_to_start = mBuilder->emitLabel();
    mBuilder->emitAD(LOP_JUMPBACK, 0, 0);
    auto jump_to_end_label = mBuilder->emitLabel();
    patchJumpOrThrow(jump_to_end, jump_to_end_label);
    patchJumpOrThrow(jump_to_start, jump_to_start_label);

    return false;
}

bool LuauVisitor::visit(LSLWhileStatement * while_stmt)
{
    auto jump_to_start_label = mBuilder->emitLabel();
    // run the check expression, exiting the loop if it fails
    uint8_t expr_reg;
    {
        [[maybe_unused]] RegScope expr_scope(this);
        expr_reg = evalExprToSourceReg(while_stmt->getCheckExpr());
    }
    auto jump_to_end = mBuilder->emitLabel();
    mBuilder->emitAD(LOP_JUMPIFNOT, expr_reg, 0);
    // run the body of the loop
    while_stmt->getBody()->visit(this);
    // jump back up to the check expression at the top
    auto jump_to_start = mBuilder->emitLabel();
    mBuilder->emitAD(LOP_JUMPBACK, 0, 0);
    auto jump_to_end_label = mBuilder->emitLabel();
    patchJumpOrThrow(jump_to_end, jump_to_end_label);
    patchJumpOrThrow(jump_to_start, jump_to_start_label);

    return false;
}

bool LuauVisitor::visit(LSLDoStatement * do_stmt)
{
    auto jump_to_start_label = mBuilder->emitLabel();
    // run the body of the loop
    do_stmt->getBody()->visit(this);
    uint8_t expr_reg;
    {
        [[maybe_unused]] RegScope expr_scope(this);
        expr_reg = evalExprToSourceReg(do_stmt->getCheckExpr());
    }
    // run the check expression, exiting the loop if it fails
    auto jump_to_start = mBuilder->emitLabel();
    mBuilder->emitAD(LOP_JUMPIF, expr_reg, 0);
    patchJumpOrThrow(jump_to_start, jump_to_start_label);

    return false;
}

bool LuauVisitor::visit(LSLLabel *label_stmt)
{
    _mLabelTargets[label_stmt->getSymbol()] = (uint32_t)mBuilder->emitLabel();
    return false;
}

bool LuauVisitor::visit(LSLJumpStatement *jump_stmt)
{
    auto label_sym = jump_stmt->getSymbol();
    // Whether we want to interrupt or not depends on whether this would result
    // in jumping backwards. If the label comes after the jump in the script,
    // it's a forward jump.
    bool forward_jump = label_sym->getLabelDecl()->getLoc() > jump_stmt->getLoc();
    _mJumpTargets[(uint32_t)mBuilder->emitLabel()] = label_sym;
    mBuilder->emitAD(forward_jump ? LOP_JUMP : LOP_JUMPBACK, 0, 0);
    return false;
}

bool LuauVisitor::visit(LSLReturnStatement *ret_stmt)
{
    [[maybe_unused]] RegScope scope(this);
    uint8_t source_reg = 0;

    auto *expr = ret_stmt->getExpr();
    if (expr)
    {
        source_reg = evalExprToSourceReg(expr);
        if (needTruncateToFloat(expr))
        {
            // It's not a big deal if we clobber `source_reg`, we're not going to use it again.
            mBuilder->emitABC(LOP_LSL_DOUBLE2FLOAT, source_reg, source_reg, 0);
        }
    }
    // only return a value if the expression actually has a return type
    mBuilder->emitABC(LOP_RETURN, source_reg, (expr && expr->getIType() != LST_NULL) ? 2 : 1, 0);
    return false;
}

bool LuauVisitor::visit(LSLStateStatement *state_stmt)
{
    [[maybe_unused]] RegScope scope(this);
    auto func_reg = allocReg(state_stmt);
    pushImport(func_reg, "lsl", "change_state");
    // This is fine because we know the index will be small enough to fit in `LOADN`.
    mBuilder->emitAD(LOP_LOADN, allocReg(state_stmt), _mSymbolMap[state_stmt->getSymbol()].index);
    // 1 arg, 0 return.
    mBuilder->emitABC(LOP_CALL, func_reg, 1 + 1, 0 + 1);
    return false;
}

bool LuauVisitor::visit(LSLExpressionStatement *expr_stmt)
{
    // Expression statements may create temporaries, so they need their own scope
    [[maybe_unused]] RegScope scope(this);
    expr_stmt->getExpr()->visit(this);
    return false;
}


// Expressions

bool LuauVisitor::visit(LSLPrintExpression *print_expr)
{
    auto print_reg = allocReg(print_expr);
    pushGlobal(print_reg, "print");
    pushArgument(print_expr->getChildExpr(), false);
    // one arg, void return
    mBuilder->emitABC(LOP_CALL, print_reg, 2, 1);
    return false;
}

bool LuauVisitor::visit(LSLConstantExpression* const_expr)
{
    pushConstant(const_expr->getConstantValue());
    return false;
}

bool LuauVisitor::visit(LSLLValueExpression * lvalue_expr)
{
    pushLValue(lvalue_expr, true);
    return false;
}

void LuauVisitor::pushLValue(LSLLValueExpression* lvalue_expr, bool push_member)
{
    auto *sym = lvalue_expr->getSymbol();
    const auto target_reg = takeTargetReg(lvalue_expr);
    auto *member = lvalue_expr->getMember();

    LUAU_ASSERT(sym != nullptr);
    switch(sym->getSubType())
    {
        case SYM_GLOBAL:
        {
            pushGlobal(target_reg, lvalue_expr);
            break;
        }
        case SYM_EVENT_PARAMETER:
        case SYM_FUNCTION_PARAMETER:
        case SYM_LOCAL:
            // Copy the value from the local to the temporary register
            mBuilder->emitABC(LOP_MOVE, target_reg, _mSymbolMap[sym].index, 0);
            break;
        default:
            LUAU_ASSERT(!"Tried to push LValue with unhandled symbol type");
    }

    // Even if we're using a symbol with a member reference, we might want the full object
    // in some cases, hence `push_member`.
    if (push_member && member)
    {
        // clobber the vector or quaternion we just placed with the value of its member
        auto *memberName = member->getName();
        mBuilder->emitABC(LOP_GETTABLEKS, target_reg, target_reg, srefHash(sref(memberName)));
        mBuilder->emitAux(addConstantString(sref(memberName), INT16_MAX));
    }
}

bool LuauVisitor::visit(LSLBoolConversionExpression *bool_expr)
{
    [[maybe_unused]] RegScope scope(this);
    auto *child_expr = bool_expr->getChildExpr();
    if (child_expr->getIType() == LST_INTEGER)
    {
        // We can't do this for float as well because in Luau
        // 0.0 is a truthy value. Only `nil` and `false` are falsy.
        child_expr->visit(this);
        return false;
    }

    LUAU_ASSERT(mTargetReg == -1);
    auto target_reg = allocReg(bool_expr);
    switch(child_expr->getIType())
    {
        case LST_INTEGER:
            // Should be handled above
            LUAU_ASSERT(!"Tried to do integer->bool conversion");
            break;
        case LST_KEY:
        {
            pushImport(target_reg, "lsl", "is_key_truthy");
            child_expr->visit(this);
            mBuilder->emitABC(LOP_CALL, target_reg, 1 + 1, 1 + 1);
            return false;
        }
        case LST_LIST:
        {
            // Any non-zero result is fine.
            auto res_idx = evalExprToSourceReg(child_expr);
            mBuilder->emitABC(LOP_LENGTH, target_reg, res_idx, 0);
            return false;
        }
        default:
            break;
    }

    auto res_idx = evalExprToSourceReg(child_expr);
    auto const_reg = allocReg(bool_expr);
    {
        TargetRegScope target(this, const_reg);
        pushConstant(child_expr->getType()->getDefaultValue());
    }

    // Initialize target to true
    mBuilder->emitAD(LOP_LOADK, target_reg, addConstantInteger(1, INT16_MAX));
    // Jump over setting it to false if true
    mBuilder->emitAD(LOP_JUMPIFNOTEQ, res_idx, 2);
    mBuilder->emitAux(const_reg);
    // Set the target to false if they didn't jump
    mBuilder->emitAD(LOP_LOADK, target_reg, addConstantInteger(0, INT16_MAX));
    return false;
}

bool LuauVisitor::visit(LSLTypecastExpression *typecast_expr)
{
    auto *child_expr = typecast_expr->getChildExpr();
    auto from_type = child_expr->getIType();
    auto to_type = typecast_expr->getIType();

    if (from_type == to_type)
    {
        // self-cast is just an identity function, pass through
        child_expr->visit(this);
        return false;
    }

    // This may seem weird, but these kinds of casts are all over LSL math code,
    // particularly int->float promotion, so avoiding function call overhead is very useful.
    if ((from_type == LST_INTEGER && to_type == LST_FLOATINGPOINT) ||
        (from_type == LST_FLOATINGPOINT && to_type == LST_INTEGER))
    {
        const auto dest_reg = takeTargetReg(typecast_expr);
        const auto source_reg = evalExprToSourceReg(child_expr);
        int direction = (from_type == LST_INTEGER) ? 0 : 1;  // 0 = int->float, 1 = float->int
        mBuilder->emitABC(LOP_LSL_CASTINTFLOAT, dest_reg, source_reg, direction);
        return false;
    }

    // Slow path: general casts via function call
    const auto expected_target = mTargetReg;
    const auto func_reg = allocReg(typecast_expr);
    [[maybe_unused]] RegScope reg_scope(this);
    // place the cast closure on the stack
    pushImport(func_reg, "lsl", "cast");
    // We don't need to cast doubles to floats, cast() will handle that.
    pushArgument(child_expr, false);

    mBuilder->emitAD(LOP_LOADN, allocReg(typecast_expr), to_type);
    mBuilder->emitABC(LOP_CALL, func_reg, 2 + 1, 1 + 1);
    maybeMove(expected_target, func_reg);
    return false;
}

bool LuauVisitor::visit(LSLListExpression *list_expr)
{
    auto target_id = takeTargetReg(list_expr);
    mBuilder->emitABC(LOP_NEWTABLE, target_id, 0, 0);
    mBuilder->emitAux(list_expr->getNumChildren());

    int idx = 0;
    for (auto *expr : *list_expr)
    {
        // TODO: do setlist in batches so we don't have a bunch of
        //  unnecessary SetTables?
        // RegScope per iteration because constructing the value may create
        // temporaries that we can get rid of after the element is assigned!
        [[maybe_unused]] RegScope scope(this);
        auto source_id = mRegTop;

        expr->visit(this);

        if (needTruncateToFloat(expr))
        {
            // Doubles are always floats inside lists in LSL-on-Mono
            mBuilder->emitABC(LOP_LSL_DOUBLE2FLOAT, source_id, source_id, 0);
        }

        if (idx <= UINT8_MAX)
        {
            mBuilder->emitABC(LOP_SETTABLEN, source_id, target_id, idx);
        }
        else if (idx <= INT16_MAX)
        {
            // index is large, so we must load a number into a register first to set the element
            auto nreg_id = allocReg(list_expr);
            mBuilder->emitAD(LOP_LOADN, nreg_id, (int16_t)idx);
            mBuilder->emitABC(LOP_SETTABLE, source_id, target_id, nreg_id);
        }
        else
        {
            throw Luau::CompileError(convertLoc(list_expr->getLoc()), "List expression too large!");
        }
        ++idx;
    }
    return false;
}

bool LuauVisitor::visit(LSLVectorExpression *vec_expr)
{
    auto expected_target = mTargetReg;
    // We have to use allocReg because we're going to emit a call
    auto vec_func_reg = allocReg(vec_expr);
    pushImport(vec_func_reg, "vector");
    for (auto *expr : *vec_expr)
    {
        // Don't need to cast floats, they'll be cast when they're put in the vector
        pushArgument((LSLExpression *)expr, false);
    }
    // 3 args, vector return
    mBuilder->emitABC(LOP_CALL, vec_func_reg, 3 + 1, 1 + 1);
    maybeMove(expected_target, vec_func_reg);
    return false;
}

bool LuauVisitor::visit(LSLQuaternionExpression *quat_expr)
{
    auto expected_target = mTargetReg;
    // We have to use allocReg because we're going to emit a call
    auto quat_func_reg = allocReg(quat_expr);
    pushImport(quat_func_reg, "quaternion");
    for (auto *expr : *quat_expr)
    {
        // Don't need to cast floats, they'll be cast when they're put in the quat
        pushArgument((LSLExpression *)expr, false);
    }
    // 4 args, quaternion return
    mBuilder->emitABC(LOP_CALL, quat_func_reg, 4 + 1, 1 + 1);
    maybeMove(expected_target, quat_func_reg);
    return false;
}

bool LuauVisitor::visit(LSLUnaryExpression *un_expr)
{
    const auto expected_target = mTargetReg;
    // This deserves some explanation. Basically, for expressions where we want
    // the result to end up in a specific register, we can put things directly
    // into that target register. `takeTargetReg()` may internally alloc if there
    // was no target register.
    //
    // For that reason, we keep track of both the register we actually used (target_reg)
    // as well as the register we need the result to end up in (expected_target).
    // maybeMove() is used as a helper to maybe copy the value from one register to the
    // expected target, if we even have one. Otherwise, it does nothing.
    const auto target_reg = takeTargetReg(un_expr);
    auto op = un_expr->getOperation();
    auto *expr = un_expr->getChildExpr();

    // We might create temporaries in here
    [[maybe_unused]] RegScope scope(this);

    // These cases are easy, handle them first.
    LuauOpcode luau_op = LOP_NOP;
    switch(op)
    {
        case '-': luau_op = LOP_MINUS; break;
        case '!': luau_op = LOP_NOT; break;
        default:
            break;
    }

    if (luau_op != LOP_NOP)
    {
        mBuilder->emitABC(luau_op, target_reg, evalExprToSourceReg(expr), 0);
        // maybeMove(expected_target, target_reg);
        return false;
    }

    // Bitwise NOT is annoying on account of it being a bit32 call
    if (op == OP_BIT_NOT)
    {
        // TODO: FASTCALL for these?
        auto func_reg = target_reg;
        if (expected_target != -1)
            func_reg = allocReg(un_expr);
        // place the bit32 closure on the stack
        pushImport(func_reg, "bit32", "bnot");
        pushArgument(expr);

        mBuilder->emitABC(LOP_CALL, func_reg, 1 + 1, 1 + 1);
        maybeMove(expected_target, func_reg);
        return false;
    }

    // Everything below here should be a (post|pre)(incr|decr) operator operating on an lvalue.
    auto *parent = un_expr->getParent();

    LUAU_ASSERT(un_expr->getChildExpr()->getNodeSubType() == NODE_LVALUE_EXPRESSION);
    auto *lvalue = (LSLLValueExpression *)un_expr->getChildExpr();
    auto *lvalue_sym = lvalue->getSymbol();

    const bool pre = (op == OP_PRE_INCR || op == OP_PRE_DECR);

    auto *member = lvalue->getMember();

    // This is a post-op that assigns to itself directly. What? That's a no-op!
    // For ex. `foo = foo++`
    // Note: Only applies to locals, since _mSymbolMap[].index is meaningless for globals
    if (!pre && lvalue_sym->getSubType() != SYM_GLOBAL && target_reg == _mSymbolMap[lvalue_sym].index && !member)
    {
        // okay, do nothing. bye!
        return false;
    }

    luau_op = LOP_SUBK;
    if (op == OP_PRE_INCR || op == OP_POST_INCR)
        luau_op = LOP_ADDK;

    // Basically, do we have to move this result for whoever asked for it.
    // Especially important of mutating unary operators like `++bar` where
    // they may be their own statement, and we don't necessarily care about
    // pushing the result.
    const bool want_result = (parent && parent->getNodeType() != NODE_STATEMENT);

    // Pre-allocated by buildFunction() to ensure index < 256 for LOP_ADDK/SUBK
    const auto one_const_idx = addConstantUnder(lvalue->getType()->getOneValue(), UINT8_MAX);

    auto source_reg = evalExprToSourceReg(expr);

    // Because we have ruled out the self-assign post-op case, we don't need a temporary
    // here. We can just assign directly to the target register and the lvalue register.
    if (want_result && !pre)
    {
        // No cast necessary here, should already be a float.
        maybeMove(target_reg, source_reg);
        // Seems like this should be a no-op, shouldn't `expected_target == target_reg`
        // if `expected_target != -1`?
        // maybeMove(expected_target, target_reg);
    }

    // Add (or subtract) one from the lvalue's current value
    mBuilder->emitABC(luau_op, source_reg, source_reg, (uint8_t)one_const_idx);

    bool result_pushed = false;
    if (want_result && pre && lvalue_sym->getSubType() != SYM_GLOBAL && !member)
    {
        // In the simple local case we have to push the result _before_ casting
        // to match Mono behavior. In all other cases we push _after_ casting.
        result_pushed = true;
        maybeMove(target_reg, source_reg);
        // maybeMove(expected_target, target_reg);
    }

    // We don't need to do this for direct assignment to a vector or quaternion member
    // if nothing's going to take the result of the expression, they'll be stored
    // cast to float anyway.
    if (lvalue->getIType() == LST_FLOATINGPOINT && (want_result || !member))
        mBuilder->emitABC(LOP_LSL_DOUBLE2FLOAT, source_reg, source_reg, 0);

    // We wanted a post-cast push.
    if (want_result && pre && !result_pushed)
    {
        maybeMove(target_reg, source_reg);
        // maybeMove(expected_target, target_reg);
    }

    if (member == nullptr)
    {
        if (lvalue_sym->getSubType() == SYM_GLOBAL)
        {
            // Uh, this is a global, so storing to the source register isn't enough.
            // The source register is just a temporary, so we have to go and save it
            // to the actual global now.
            setGlobal(source_reg, lvalue);
        }
    }
    else
    {
        emitReplaceAxisAndStore(un_expr, lvalue, source_reg);
    }

    return false;
}

bool LuauVisitor::visit(LSLBinaryExpression* bin_expr)
{
    const auto expected_target = mTargetReg;
    const auto target_reg = takeTargetReg(bin_expr);
    const auto op = bin_expr->getOperation();

    // We might create temporaries in here
    [[maybe_unused]] RegScope scope(this);

    auto* lhs = bin_expr->getLHS();
    auto* rhs = bin_expr->getRHS();

    // assignment is very special
    if (op == '=')
    {
        auto *parent = bin_expr->getParent();
        // Basically, do we have to move this result for whoever asked for it.
        // Often `=` is used as if it were a statement, but it's also an expression
        // in LSL, which means that it can be used like `foo = bar = baz = 1`;
        // Only MOVE the result to the target register if we'll actually end up using it.
        const bool want_result = (parent && parent->getNodeType() != NODE_STATEMENT);

        // The left-hand side of `=` _must_ be an lvalue
        LUAU_ASSERT(lhs->getNodeSubType() == NODE_LVALUE_EXPRESSION);
        auto* lval = (LSLLValueExpression*)lhs;
        auto* lval_sym = lval->getSymbol();

        uint8_t source_reg;
        bool have_truncated_float = false;
        if (auto *member = lval->getMember())
        {
            // Evaluate this first, it may be re-used if we need to use the result of the
            // assignment expression.
            // This should be cast to float in _all_ cases since it results in an `ldfld`
            // under LSL-on-Mono.
            source_reg = evalExprToSourceReg(rhs);
            if (needTruncateToFloat(rhs))
            {
                have_truncated_float = true;
                mBuilder->emitABC(LOP_LSL_DOUBLE2FLOAT, source_reg, source_reg, 0);
            }

            emitReplaceAxisAndStore(bin_expr, lval, source_reg);
        }
        else
        {
            if (lval_sym->getSubType() == SYM_GLOBAL)
            {
                source_reg = evalExprToSourceReg(rhs);
                if (needTruncateToFloat(rhs))
                {
                    // Crap, we need to cast to 32-bit in order to match Mono behavior.
                    // If we're eliding a move then the cast is unnecessary, it should
                    // already be in 32-bit float space. Casting the temporary directly
                    // here is okay since under Mono the temporary would _also_ be float32
                    // under Mono since it reloads directly from the global rather than
                    // using `dup`.
                    mBuilder->emitABC(LOP_LSL_DOUBLE2FLOAT, source_reg, source_reg, 0);
                }
                setGlobal(source_reg, lval);
            }
            else
            {
                source_reg = _mSymbolMap[lval_sym].index;
                TargetRegScope target_reg_scope(this, source_reg);
                rhs->visit(this);
            }
        }

        // Something actually wants to capture the result of the `=`,
        // pass it along.
        if (want_result && source_reg != target_reg)
        {
            mBuilder->emitABC(LOP_MOVE, target_reg, source_reg, 0);
            // Seems like this is a no-op?
            // maybeMove(expected_target, target_reg);
        }

        // If the rhs didn't come from an lvalue that would already be cast to float32 space,
        // we now need to cast the local to 32-bit space. Note that we do this _after_ setting
        // the result for the temporaries because in the local case the temporary resulting from
        // the assignment should still be a double even though we stored a float32 in the local.
        if (lval_sym->getSubType() != SYM_GLOBAL && !have_truncated_float && needTruncateToFloat(rhs))
        {
            mBuilder->emitABC(LOP_LSL_DOUBLE2FLOAT, source_reg, source_reg, 0);
        }
        return false;
    }

    // An evil, demonic operator, only exists for int *= float
    if (op == OP_MUL_ASSIGN)
    {
        auto *parent = bin_expr->getParent();
        const bool want_result = (parent && parent->getNodeType() != NODE_STATEMENT);

        // assignment is special
        LUAU_ASSERT(lhs->getNodeSubType() == NODE_LVALUE_EXPRESSION);
        auto* lval = (LSLLValueExpression*)lhs;
        auto* lval_sym = lval->getSymbol();

        uint8_t rhs_reg = evalExprToSourceReg(rhs);
        uint8_t source_reg;
        if (lval_sym->getSubType() == SYM_GLOBAL)
        {
            source_reg = allocReg(bin_expr);
            pushGlobal(source_reg, lval_sym->getName());
        }
        else
        {
            source_reg = _mSymbolMap[lval_sym].index;
        }

        auto floaty_reg = allocReg(bin_expr);
        mBuilder->emitABC(LOP_MUL, floaty_reg, source_reg, rhs_reg);

        // ugh, need to do some weird casting here...
        auto func_reg = allocReg(bin_expr);
        pushImport(func_reg, "lsl", "cast");
        mBuilder->emitABC(LOP_MOVE, allocReg(bin_expr), floaty_reg, 0);
        mBuilder->emitAD(LOP_LOADN, allocReg(bin_expr), LST_INTEGER);
        mBuilder->emitABC(LOP_CALL, func_reg, 2 + 1, 1 + 1);

        if (lval_sym->getSubType() == SYM_GLOBAL)
        {
            setGlobal(func_reg, lval);
        }

        mBuilder->emitABC(LOP_MOVE, source_reg, func_reg, 0);

        // Something actually wants to capture the result of the `*=`,
        // pass it along.
        if (want_result)
        {
            mBuilder->emitABC(LOP_LSL_DOUBLE2FLOAT, floaty_reg, floaty_reg, 0);
            maybeMove(expected_target, floaty_reg);
        }
        return false;
    }

    bool need_rhs_copy = false;

    if (rhs->getNodeSubType() == NODE_LVALUE_EXPRESSION)
    {
        // Oh boy. So in LSL you can do bananas things like `((f = 2.0) + f)`.
        // Due to LSL's operation order, you have to check if anything on the right-hand
        // side is mutated in the left-hand side, and make a copy if necessary.
        // If we elide a `LOP_MOVE` for the rhs lvalue, but it ends up being
        // mutated in the lhs, we could end up with the wrong result. Keep track of
        // cases where we _need_ an rhs copy.

        // NOTE: We don't currently need to do this for `f++` because we don't currently
        // do LOP_MOVE elision in that case, but there should be tests for this.
        auto *lval = (LSLLValueExpression *)rhs;
        auto *rhs_sym = rhs->getSymbol();
        if (rhs_sym->getSubType() == SYM_LOCAL && !lval->getMember())
        {
            // We only need to do this if this is a local and we don't use an accessor.
            LuauLValueMutationVisitor mutation_visitor {};
            lhs->visit(&mutation_visitor);

            // If we saw the lvalue on the rhs being mutated on the lhs, we need a copy.
            need_rhs_copy = mutation_visitor.mSeen.count(rhs_sym);
        }
    }


    if (op == '+')
    {
        // Addition operator is special for certain types
        if (lhs->getIType() == LST_LIST || rhs->getIType() == LST_LIST)
        {
            // Oh, we have an expected target, just alloc a new register for the function call
            // then MOVE the result into it after.
            auto func_reg = target_reg;
            if (expected_target != -1)
                func_reg = allocReg(bin_expr);
            // place the concat closure on the stack
            pushImport(func_reg, "lsl", "table_concat");
            // Push RHS first because LSL is whack and evaluates RHS first.
            pushArgument(rhs);
            pushArgument(lhs);

            mBuilder->emitABC(LOP_CALL, func_reg, 2 + 1, 1 + 1);
            maybeMove(expected_target, func_reg);
            return false;
        }
        else if (lhs->getIType() == LST_STRING && rhs->getIType() == LST_STRING)
        {
            // Concatenation is... special. The arguments need to be contiguous,
            // but LSL evaluates right-to-left, so we need to be careful to place
            // things in the correct order even though they're evaluated
            // out-of-order.
            [[maybe_unused]] RegScope reg_scope(this);
            const auto concat_reg = allocReg(bin_expr, 2);
            {
                TargetRegScope target_reg_scope(this, concat_reg + 1);
                rhs->visit(this);
            }
            {
                TargetRegScope target_reg_scope(this, concat_reg);
                lhs->visit(this);
            }
            mBuilder->emitABC(LOP_CONCAT, target_reg, concat_reg, concat_reg + 1);
            // Seems like this is a no-op?
            // maybeMove(expected_target, target_reg);
            return false;
        }
    }

    // Lists are a little special, with their own weird comparison behavior.
    if (lhs->getIType() == LST_LIST && rhs->getIType() == LST_LIST)
    {
        LUAU_ASSERT(op == OP_EQ || op == OP_NEQ);

        bool rhs_is_const_expr = rhs->getNodeSubType() == NODE_CONSTANT_EXPRESSION;
        if (op == OP_NEQ && rhs_is_const_expr && ((LSLListConstant *)rhs)->getLength() == 0)
        {
            // This case is basically just the same as the object length operator.
            // `lhs_val != []` is a common idiom in LSL for getting the length of a list,
            // so let's special-case it rather than wasting a list temporary.
            const auto lhs_reg = evalExprToSourceReg(lhs);
            mBuilder->emitABC(LOP_LENGTH, target_reg, lhs_reg, 0);
            return false;
        }

        // Visit RHS first because LSL is awful
        auto rhs_reg = evalExprToSourceReg(rhs);
        if (need_rhs_copy)
        {
            uint8_t new_rhs_reg = allocReg(rhs);
            mBuilder->emitABC(LOP_MOVE, new_rhs_reg, rhs_reg, 0);
            rhs_reg = new_rhs_reg;
        }

        const auto lhs_reg = evalExprToSourceReg(lhs);

        // It can basically only be comparison operators in here.
        // Remember, not eq basically subtracts the length of rhs from lhs.
        const auto lhs_len_reg = allocReg(bin_expr);
        const auto rhs_len_reg = allocReg(bin_expr);
        // NOTE: this relies on our changes to make LOP_LENGTH return an int
        //  in the LSL VM case!
        mBuilder->emitABC(LOP_LENGTH, lhs_len_reg, lhs_reg, 0);
        mBuilder->emitABC(LOP_LENGTH, rhs_len_reg, rhs_reg, 0);
        mBuilder->emitABC(LOP_SUB, target_reg, lhs_len_reg, rhs_len_reg);
        if (op == OP_EQ)
        {
            // EQ just checks that there's no difference between the lengths.
            mBuilder->emitABC(LOP_NOT, target_reg, target_reg, 0);
        }
        // maybeMove(expected_target, target_reg);
        return false;
    }

    // These are all the simple cases
    LuauOpcode luau_op = LOP_NOP;
    switch (op)
    {
        case '+': luau_op = LOP_ADD; break;
        case '-': luau_op = LOP_SUB; break;
        case '*': luau_op = LOP_MUL; break;
        case '%': luau_op = LOP_MOD; break;
        case OP_BOOLEAN_AND: luau_op = LOP_AND; break;
        case OP_BOOLEAN_OR: luau_op = LOP_OR; break;
        case '/':
            // Division is a little special, integer division has its own code
            if (lhs->getIType() == LST_INTEGER)
                luau_op = LOP_IDIV;
            else
                luau_op = LOP_DIV;
            break;
        default:
            break;
    }

    if (luau_op != LOP_NOP)
    {
        // Try to use K-variant opcodes if RHS is a scalar numeric constant with low index
        if (rhs->getNodeSubType() == NODE_CONSTANT_EXPRESSION &&
            (rhs->getIType() == LST_INTEGER || rhs->getIType() == LST_FLOATINGPOINT))
        {
            auto* rhs_const = dynamic_cast<LSLConstantExpression*>(rhs);
            auto rhs_const_idx = addConstant(rhs_const->getConstantValue());
            // Constant index is low enough to fit
            if (rhs_const_idx >= 0 && rhs_const_idx <= 255)
            {
                LuauOpcode k_op = LOP_NOP;
                switch (op)
                {
                case '+':
                    k_op = LOP_ADDK;
                    break;
                case '-':
                    k_op = LOP_SUBK;
                    break;
                case '*':
                    k_op = LOP_MULK;
                    break;
                case '%':
                    k_op = LOP_MODK;
                    break;
                case '/':
                    k_op = (lhs->getIType() == LST_INTEGER) ? LOP_IDIVK : LOP_DIVK;
                    break;
                default:
                    break;
                }

                if (k_op != LOP_NOP)
                {
                    const auto lhs_reg = evalExprToSourceReg(lhs);
                    mBuilder->emitABC(k_op, target_reg, lhs_reg, (uint8_t)rhs_const_idx);
                    return false;
                }
            }
        }

        // Okay, this operation wasn't one we could handle with a K-variant opcode.
        // Evaluate RHS into a register (LSL's right-to-left evaluation order)
        auto rhs_reg = evalExprToSourceReg(rhs);
        if (need_rhs_copy)
        {
            uint8_t new_rhs_reg = allocReg(rhs);
            mBuilder->emitABC(LOP_MOVE, new_rhs_reg, rhs_reg, 0);
            rhs_reg = new_rhs_reg;
        }

        // Check if we can use RK-variant opcodes for SUB/DIV
        // Only check this for operations that actually have reversed K-variants
        if ((op == '-' || op == '/') &&
            lhs->getNodeSubType() == NODE_CONSTANT_EXPRESSION &&
            (lhs->getIType() == LST_INTEGER || lhs->getIType() == LST_FLOATINGPOINT))
        {
            auto* lhs_const = dynamic_cast<LSLConstantExpression*>(lhs);
            auto lhs_const_idx = addConstant(lhs_const->getConstantValue());

            // Constant index is low enough to fit
            if (lhs_const_idx >= 0 && lhs_const_idx <= 255)
            {
                LuauOpcode rk_op = (op == '-') ? LOP_SUBRK : LOP_DIVRK;
                mBuilder->emitABC(rk_op, target_reg, (uint8_t)lhs_const_idx, rhs_reg);
                // maybeMove(expected_target, target_reg);
                return false;
            }
        }

        // Fallback to regular register-register operations
        const auto lhs_reg = evalExprToSourceReg(lhs);
        mBuilder->emitABC(luau_op, target_reg, lhs_reg, rhs_reg);
        // maybeMove(expected_target, target_reg);
        return false;
    }

    // Comparison operators
    bool swap_regs = false;
    switch (op)
    {
        case OP_EQ: luau_op = LOP_JUMPIFEQ; break;
        case OP_NEQ: luau_op = LOP_JUMPIFNOTEQ; break;
        case OP_LESS: luau_op = LOP_JUMPIFLT; break;
        case OP_LEQ: luau_op = LOP_JUMPIFLE; break;
        // This is necessary to deal with NaN comparison correctly :(
        case OP_GREATER: luau_op = LOP_JUMPIFLT; swap_regs = true; break;
        case OP_GEQ: luau_op = LOP_JUMPIFLE; swap_regs = true; break;
        default:
            break;
    }

    if (luau_op != LOP_NOP)
    {
        // Visit RHS first because LSL is awful
        uint8_t rhs_reg = evalExprToSourceReg(rhs);
        uint8_t lhs_reg = evalExprToSourceReg(lhs);

        if (swap_regs)
            std::swap(rhs_reg, lhs_reg);

        // Initialize target to true
        mBuilder->emitAD(LOP_LOADK, target_reg, addConstantInteger(1, INT16_MAX));
        // Jump over setting it to false if true
        mBuilder->emitAD(luau_op, lhs_reg, 2);
        mBuilder->emitAux(rhs_reg);
        // Set the target to false if they didn't jump
        mBuilder->emitAD(LOP_LOADK, target_reg, addConstantInteger(0, INT16_MAX));
        // maybeMove(expected_target, target_reg);
        return false;
    }

    // Now handle bitwise operators
    const char* bitwise_method = nullptr;
    switch(op)
    {
        case OP_BIT_AND: bitwise_method = "band"; break;
        case OP_BIT_OR: bitwise_method = "bor"; break;
        case OP_BIT_XOR: bitwise_method = "bxor"; break;
        case OP_SHIFT_LEFT: bitwise_method = "lshift"; break;
        // LSL only has arithmetic right shift, not logical!
        case OP_SHIFT_RIGHT: bitwise_method = "arshift"; break;
        default:
            break;
    }

    if (bitwise_method != nullptr)
    {
        // TODO: FASTCALL for these?
        auto func_reg = target_reg;
        if (expected_target != -1)
            func_reg = allocReg(bin_expr);
        pushImport(func_reg, "bit32", bitwise_method);

        // We need to evaluate right to left, but arguments need to be left to right.
        // Pre-allocate a place for the arguments
        const uint8_t args_reg = allocReg(bin_expr, 2);
        {
            TargetRegScope target_reg_scope(this, args_reg + 1);
            rhs->visit(this);
        }
        {
            TargetRegScope target_reg_scope(this, args_reg);
            lhs->visit(this);
        }

        mBuilder->emitABC(LOP_CALL, func_reg, 2 + 1, 1 + 1);
        maybeMove(expected_target, func_reg);
        return false;
    }

    // should never happen!
    LUAU_ASSERT(!"Fell to bottom of LSLBinaryExpression visitor");

    return false;
}

bool LuauVisitor::visit(LSLFunctionExpression *func_expr)
{
    auto func_sym = func_expr->getSymbol();
    if (func_sym->getSubType() == SYM_BUILTIN && !strcmp(func_sym->getName(), "llGetListLength"))
    {
        // llGetListLength() is a special case, we can just replace it with the
        // opcode used for Lua's # operator for getting list length. This also
        // has the nice property that we can emit directly into a local if possible.
        const auto target_reg = takeTargetReg(func_expr);
        auto *first_arg = (LSLExpression *)func_expr->getArguments()->getChild(0);
        // Make sure we don't keep around any registers used for generating the list
        [[maybe_unused]] RegScope scope(this);
        const auto res_idx = evalExprToSourceReg(first_arg);
        mBuilder->emitABC(LOP_LENGTH, target_reg, res_idx, 0);
        return false;
    }

    // Load the closure for the function
    const auto expected_target = mTargetReg;
    const auto func_reg = allocReg(func_expr);

    [[maybe_unused]] RegScope scope(this);

    if (func_sym->getSubType() == SYM_BUILTIN)
    {
        // Chop off the leading `ll`, It's superfluous because these
        // functions will all live in the `ll` module.
        nodeSymIdToConstant(func_expr);
        pushImport(func_reg, "ll", _mSymbolNames[func_sym].c_str());
    }
    else
    {
        pushGlobal(func_reg, func_expr);
    }

    // Load the parameters onto the stack, in order
    for (auto *param_node : *func_expr->getArguments())
    {
        pushArgument(param_node);
    }

    auto arg_result = func_expr->getArguments()->getNumChildren() + 1;
    mBuilder->emitABC(
        LOP_CALL,
        func_reg,
        arg_result,
        (func_expr->getIType() == LST_NULL) ? 1 : 2
    );
    maybeMove(expected_target, func_reg);
    return false;
}


// Helpers

void LuauVisitor::pushGlobal(uint8_t target_reg, const char* global_name)
{
    mBuilder->emitABC(LOP_GETGLOBAL, target_reg, 0, srefHash(sref(global_name)));
    mBuilder->emitAux(addConstantString(sref(global_name), INT16_MAX));
}

void LuauVisitor::pushGlobal(uint8_t target_reg, LSLASTNode *node)
{
    nodeSymIdToConstant(node);
    const std::string &sym_name = _mSymbolNames[node->getSymbol()];
    mBuilder->emitABC(LOP_GETGLOBAL, target_reg, 0, srefHash(sref(sym_name)));
    mBuilder->emitAux(addConstantString(sref(sym_name), INT16_MAX));
}

void LuauVisitor::pushImport(const uint8_t target_reg, const char* import1) const
{
    const auto import1_const_idx = addConstantString(sref(import1), kMaxStringImportRef);
    const auto built_import = Luau::BytecodeBuilder::getImportId(import1_const_idx);
    const auto import_idx = addImport(sref(import1), INT16_MAX);
    mBuilder->emitAD(LOP_GETIMPORT, target_reg, import_idx);
    mBuilder->emitAux(built_import);
}

void LuauVisitor::pushImport(const uint8_t target_reg, const char* import1, const char* import2) const
{
    const auto module_const_idx = addConstantString(sref(import1), kMaxStringImportRef);
    const auto method_const_idx = addConstantString(sref(import2), kMaxStringImportRef);
    const auto built_import = Luau::BytecodeBuilder::getImportId(module_const_idx, method_const_idx);
    const auto import_idx = addImport(sref(import1), sref(import2), INT16_MAX);
    mBuilder->emitAD(LOP_GETIMPORT, target_reg, import_idx);
    mBuilder->emitAux(built_import);
}

void LuauVisitor::setGlobal(uint8_t source_reg, LSLASTNode *glob_node)
{
    auto const_idx = nodeSymIdToConstant(glob_node);
    const auto& glob_name = _mSymbolNames[glob_node->getSymbol()];
    auto glob_sref = sref(glob_name);
    mBuilder->emitABC(LOP_SETGLOBAL, source_reg, 0, srefHash(glob_sref));
    mBuilder->emitAux(const_idx);
}

void LuauVisitor::emitReplaceAxisAndStore(LSLASTNode* node, LSLLValueExpression* lvalue_expr, uint8_t value_reg)
{
    auto* lvalue_sym = lvalue_expr->getSymbol();
    auto* member = lvalue_expr->getMember();

    // replace_axis(coord, axis_name, new_val)
    uint8_t replace_axis_reg = allocReg(node);
    pushImport(replace_axis_reg, "lsl", "replace_axis");
    pushLValue(lvalue_expr, false);
    const auto member_const_idx = addConstantString(sref(member->getName()), INT16_MAX);
    mBuilder->emitAD(LOP_LOADK, allocReg(node), member_const_idx);

    // Copy the value into place in the arguments
    maybeMove(allocReg(node), value_reg);

    mBuilder->emitABC(LOP_CALL, replace_axis_reg, 3 + 1, 1 + 1);

    // Set the lvalue to the new constructed coord
    if (lvalue_sym->getSubType() == SYM_GLOBAL)
    {
        setGlobal(replace_axis_reg, lvalue_expr);
    }
    else
    {
        mBuilder->emitABC(LOP_MOVE, _mSymbolMap[lvalue_sym].index, replace_axis_reg, 0);
    }
}

/// Push the result of an expression that's part of a function call's arguments.
void LuauVisitor::pushArgument(LSLExpression* expr, bool want_float_cast)
{
    {
        [[maybe_unused]] RegScope scope(this);
        // target reg can't be used if it was set, arguments need to be in a specific place.
        mTargetReg = -1;
        auto expected_reg = mRegTop;
        expr->visit(this);
        // Doubles need to be cast to float32 before we call the function per Mono behavior.
        if (want_float_cast && needTruncateToFloat(expr))
        {
            // Doubles are always floats inside lists in LSL-on-Mono
            mBuilder->emitABC(LOP_LSL_DOUBLE2FLOAT, expected_reg, expected_reg, 0);
        }
    }
    // Mark this register as having been used by the parameter's result
    allocReg(expr);
}

bool LuauVisitor::needTruncateToFloat(Tailslide::LSLExpression* expr)
{
    // The LSL-on-Mono situation is very complicated. FP temporaries are generally
    // doubles, but doing some things with them can truncate them to float space.
    // Check if we need to do so.
    // If you change this behavior at all _please_ add a test in float_precision.lsl.

    if (expr->getIType() != LST_FLOATINGPOINT)
        return false;
    if (expr->getNodeSubType() == NODE_LVALUE_EXPRESSION)
        return false;
    if (expr->getNodeSubType() == NODE_CONSTANT_EXPRESSION)
        return false;
    if (expr->getNodeSubType() == NODE_UNARY_EXPRESSION)
    {
        auto * un_expr = ((LSLUnaryExpression *)expr);
        // We'll be using the lvalue's existing value, it should already be a float
        if (un_expr->getOperation() == OP_POST_DECR || un_expr->getOperation() == OP_POST_INCR)
        {
            return false;
        }
        // but not for pre-incr/decr!
        if (un_expr->getOperation() == OP_PRE_DECR || un_expr->getOperation() == OP_PRE_INCR)
        {
            auto *lval = (LSLLValueExpression *)un_expr->getChildExpr();
            auto *lval_sym = lval->getSymbol();
            // These expressions will always result in a float already truncated to float.
            if (lval->getMember() || lval_sym->getSubType() == SYM_GLOBAL)
                return false;
        }
    }
    return true;
}

/// Add a constant to the constants list and return its ID
int16_t LuauVisitor::addConstant(LSLConstant* cv) const
{
    int32_t constant_id = -1;
    switch(cv->getIType())
    {
        case LST_INTEGER:
            constant_id = mBuilder->addConstantInteger(dynamic_cast<LSLIntegerConstant*>(cv)->getValue());
            break;
        case LST_FLOATINGPOINT:
            constant_id = mBuilder->addConstantNumber((float)dynamic_cast<LSLFloatConstant*>(cv)->getValue());
            break;
        case LST_KEY:
        case LST_STRING:
        {
            const char *sv = dynamic_cast<LSLStringConstant*>(cv)->getValue();
            // LSL strings cannot have embedded null due to how the interop layer works,
            // so this is fine. The only way to generate strings with such odd bytes would
            // be through library functions, but the interop layer implicitly strips them out.
            // The lexer itself also implicitly truncates string literals at the first null.
            constant_id = mBuilder->addConstantString({sv, strlen(sv)});
            break;
        }
        case LST_VECTOR:
        {
            auto *vv = dynamic_cast<LSLVectorConstant *>(cv)->getValue();
            constant_id = mBuilder->addConstantVector(vv->x, vv->y, vv->z, 0.0);
            break;
        }
        default:
            LUAU_ASSERT(!"Unhandled constant type");
    }

    if (constant_id < 0 || constant_id > INT16_MAX)
    {
        throw Luau::CompileError(convertLoc(cv->getLoc()), "Too many constants!");
    }

    return (int16_t)constant_id;
}

int16_t LuauVisitor::addConstantUnder(LSLConstant *cv, size_t limit) const
{
    // Luau handles constants above this very badly. Using a limit above this makes no sense.
    LUAU_ASSERT(limit <= INT16_MAX);

    auto const_id = addConstant(cv);
    // We've already ruled out the negative case
    if ((size_t)const_id >= limit)
    {
        throw Luau::CompileError(convertLoc(cv->getLoc()), "Constant ID over limit!");
    }
    return (int16_t)const_id;
}

int16_t LuauVisitor::addConstantFloat(float num, size_t limit) const
{
    LUAU_ASSERT(limit <= INT16_MAX);

    auto const_id = mBuilder->addConstantNumber(num);
    if (const_id < 0)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Too many constants!");
    }
    if ((size_t)const_id >= limit)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Constant ID over limit!");
    }
    return (int16_t)const_id;
}

int16_t LuauVisitor::addConstantInteger(int32_t num, size_t limit) const
{
    LUAU_ASSERT(limit <= INT16_MAX);

    auto const_id = mBuilder->addConstantInteger(num);
    if (const_id < 0)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Too many constants!");
    }
    if ((size_t)const_id >= limit)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Constant ID over limit!");
    }
    return (int16_t)const_id;
}

int16_t LuauVisitor::addConstantString(Luau::BytecodeBuilder::StringRef str, size_t limit) const
{
    LUAU_ASSERT(limit <= INT16_MAX);
    auto const_id = mBuilder->addConstantString(str);
    if (const_id < 0)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Too many constants!");
    }
    if ((size_t)const_id >= limit)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Constant ID over limit!");
    }
    return (int16_t)const_id;
}

int16_t LuauVisitor::addImport(Luau::BytecodeBuilder::StringRef str1, size_t limit) const
{
    LUAU_ASSERT(limit <= INT16_MAX);
    auto const_id1 = addConstantString(str1, kMaxStringImportRef);
    auto const_id_import = mBuilder->addImport(Luau::BytecodeBuilder::getImportId(const_id1));
    if (const_id_import < 0)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Too many constants!");
    }
    if ((size_t)const_id_import >= limit)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Constant ID over limit!");
    }
    return const_id_import;
}

int16_t LuauVisitor::addImport(Luau::BytecodeBuilder::StringRef str1, Luau::BytecodeBuilder::StringRef str2, size_t limit) const
{
    LUAU_ASSERT(limit <= INT16_MAX);
    auto const_id1 = addConstantString(str1, kMaxStringImportRef);
    auto const_id2 = addConstantString(str2, kMaxStringImportRef);
    auto const_id_import = mBuilder->addImport(Luau::BytecodeBuilder::getImportId(const_id1, const_id2));
    if (const_id_import < 0)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Too many constants!");
    }
    if ((size_t)const_id_import >= limit)
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Constant ID over limit!");
    }
    return (int16_t)const_id_import;
}

unsigned int LuauVisitor::pushConstant(LSLConstant *cv)
{
    auto reg_id = takeTargetReg(cv);
    switch(cv->getIType())
    {
        case LST_LIST:
        {
            // We only support the empty list as a fake constant, to improve API symmetry.
            auto *lv = dynamic_cast<LSLListConstant *>(cv);
            LUAU_ASSERT(!lv->getLength());
            mBuilder->emitABC(LOP_NEWTABLE, reg_id, 0, 0);
            mBuilder->emitAux(0);
            return reg_id;
        }
        case LST_QUATERNION:
        {
            // Quaternions are special, they don't have a constant, really.
            [[maybe_unused]] RegScope quat_scope(this);
            auto quat_func_reg = allocReg(cv);
            auto *qv = dynamic_cast<LSLQuaternionConstant *>(cv)->getValue();
            pushImport(quat_func_reg, "quaternion");
            // We can use LOADN if we know these are whole numbers, but that's unlikely for
            // a quaternion, they're 0.0-1.0 normalized.
            mBuilder->emitAD(LOP_LOADK, allocReg(cv), addConstantFloat(qv->x, INT16_MAX));
            mBuilder->emitAD(LOP_LOADK, allocReg(cv), addConstantFloat(qv->y, INT16_MAX));
            mBuilder->emitAD(LOP_LOADK, allocReg(cv), addConstantFloat(qv->z, INT16_MAX));
            mBuilder->emitAD(LOP_LOADK, allocReg(cv), addConstantFloat(qv->s, INT16_MAX));
            // 4 args, quaternion return
            mBuilder->emitABC(LOP_CALL, quat_func_reg, 4 + 1, 1 + 1);
            // Move the result to the target destination
            mBuilder->emitABC(LOP_MOVE, reg_id, quat_func_reg, 0);
            return reg_id;
        }
        case LST_KEY:
        {
            // Keys are special, they don't have a constant, really.
            [[maybe_unused]] RegScope key_scope(this);
            auto key_func_reg = allocReg(cv);
            pushImport(key_func_reg, "uuid");
            // Add a string constant for the key and load it into the args
            mBuilder->emitAD(LOP_LOADK, allocReg(cv), addConstantUnder(cv, INT16_MAX));
            // 1 arg, key return
            mBuilder->emitABC(LOP_CALL, key_func_reg, 1 + 1, 1 + 1);
            // Move the result to the target destination
            mBuilder->emitABC(LOP_MOVE, reg_id, key_func_reg, 0);
            return reg_id;
        }
        default:
            break;
    }
    mBuilder->emitAD(LOP_LOADK, reg_id, addConstantUnder(cv, INT16_MAX));
    return reg_id;
}

/// Take a symbol and construct a name for it, creating a constant for the name
/// and returning its string index.
int16_t LuauVisitor::nodeSymIdToConstant(LSLASTNode* node, bool string_only)
{
    // Figure out how we should refer to this node's symbol in Lua, and add a constant for it.
    auto *sym = node->getSymbol();
    LUAU_ASSERT(sym != nullptr);
    if (auto name_iter = _mSymbolNames.find(sym); name_iter != _mSymbolNames.end())
    {
        const std::string& cached_name = name_iter->second;
        return addConstantString(sref(cached_name), INT16_MAX);
    }

    std::string name = sym->getName();
    if (node->getNodeType() == NODE_EVENT_HANDLER)
    {
        // Need a reference to the state this belongs to for conformant name mangling
        // We're parented to a node list which is parented to the state.
        auto *state_sym = node->getParent()->getParent()->getSymbol();
        LUAU_ASSERT(state_sym != nullptr);
        // put a slash so there can be no shenanigans with state name colliding with event name.
        // slash isn't valid in an identifier in the source code.
        name = "_e" + std::to_string(_mSymbolMap[state_sym].index) + "/" + name;
    }
    else if (sym->getSymbolType() == SYM_FUNCTION)
    {
        if (sym->getSubType() == SYM_BUILTIN)
        {
            // Chop off the leading `ll`, It's superfluous because these
            // functions will all live in the `ll` module.
            LUAU_ASSERT(name.find("ll") == 0);
            name = name.substr(2);
        }
        else
        {
            // Prevent mutating assignment of builtin functions
            name = "_f" + name;
        }
    }
    else
    {
        switch(sym->getSubType())
        {
            case SYM_GLOBAL:
                // prevent shadowing of builtin globals, since they're addressed by name
                name = "_g" + name;
                break;
            default:
                LUAU_ASSERT(!"Unhandled symbol type in nodeSymIdToConstant()");
                break;
        }
    }
    _mSymbolNames[sym] = name;
    // Get the version that will be kept around in memory
    const std::string &name_ref = _mSymbolNames[sym];
    // Store it as a string table entry (and maybe a constant entry)
    auto name_sref = sref(name_ref);
    if (string_only)
    {
        mBuilder->addStringTableEntry(name_sref);
        return -1;
    }

    return addConstantString(name_sref, INT16_MAX);
}


/// Evaluate an expression where we don't necessarily care which register
/// the result ends up in. Usually because we just need a temporary
/// for another operation. This has the nice property that if we just
/// want a source register for another operation and we already have a local
/// lvalue, we can just take that lvalue's source reg in many cases.
/// Otherwise it's a temporary we just allocated.
uint8_t LuauVisitor::evalExprToSourceReg(LSLExpression* expr)
{
    // If you need the result loaded into a specific register, calling this is a
    // very bad error.
    LUAU_ASSERT(mTargetReg == -1);

    auto *hoisted_expr = expr;

    if (expr->getNodeSubType() == NODE_BOOL_CONVERSION_EXPRESSION)
    {
        // Integer->bool conversions of an lvalue can be hoisted out
        // and used directly, since the VM allows you to use an integer
        // in most places you'd normally use a `boolean`.
        if (expr->getChild(0)->getIType() == LST_INTEGER)
            hoisted_expr = (LSLExpression *)expr->getChild(0);
    }

    // lvalues are special, we may be able to do `MOVE` elision
    // if what we're trying to shove in a register is already in a register!
    if (hoisted_expr->getNodeSubType() == NODE_LVALUE_EXPRESSION)
    {
        auto *lv = dynamic_cast<LSLLValueExpression *>(hoisted_expr);
        auto *sym = lv->getSymbol();
        if (!lv->getMember())
        {
            switch(sym->getSubType())
            {
                case SYM_EVENT_PARAMETER:
                case SYM_FUNCTION_PARAMETER:
                case SYM_LOCAL:
                    // if this is a local and there's no member accessor, we can just return
                    // the register that the local resides in.
                    return _mSymbolMap[sym].index;
                default:
                    break;
            }
        }
    }
    // Okay, we can't use a hoisted expr, just continue on with `expr`.
    // We expect that the expression will internally create a temporary located at `mRegTop`.
    // Evaluate the expression then return the register for its temporary
    uint8_t expected_reg = mRegTop;
    expr->visit(this);
    return expected_reg;
}

uint8_t LuauVisitor::allocReg(LSLASTNode *node, unsigned int count)
{
    // If we're explicitly allocing a reg, we can't use the target reg anymore.
    mTargetReg = -1;
    unsigned int top = mRegTop;
    if (top + count > kMaxRegisterCount)
        Luau::CompileError::raise(convertLoc(node->getLoc()), "Out of registers when trying to allocate %d registers: exceeded limit %d", count, kMaxRegisterCount);

    mRegTop += count;
    mStackSize = std::max(mStackSize, mRegTop);

    return uint8_t(top);
}

/// Either take the current target register and make sure nobody else uses it,
/// or allocate a new register to use as a target register.
uint8_t LuauVisitor::takeTargetReg(LSLASTNode *node)
{
    if (mTargetReg == -1)
        return allocReg(node);
    auto target = (uint8_t)mTargetReg;
    mTargetReg = -1;
    return target;
}

void LuauVisitor::maybeMove(const int16_t expected_target, const uint8_t actual_reg) const
{
    if (expected_target != -1 && actual_reg != expected_target)
    {
        mBuilder->emitABC(LOP_MOVE, (uint8_t)expected_target, actual_reg, 0);
    }
}

void LuauVisitor::patchJumpOrThrow(size_t jumpLabel, size_t targetLabel)
{
    if (!mBuilder->patchJumpD(jumpLabel, targetLabel))
    {
        throw Luau::CompileError({{0, 0}, {0, 0}}, "Function is too large, can't patch jumps");
    }
}


void compileLSLOrThrow(Luau::BytecodeBuilder &bcb, const std::string &source)
{
    tailslide_init_builtins(nullptr);
    ScopedScriptParser parser(nullptr);
    Logger *logger = &parser.logger;

    auto script = parser.parseLSLBytes(source.c_str(), (int)source.length());
    if (script)
    {
        script->collectSymbols();
        script->determineTypes();
        script->recalculateReferenceData();
        script->propagateValues();
        script->finalPass();

        if (!logger->getErrors())
        {
            script->validateGlobals(true);
            script->checkSymbols();
        }
    }
    if (script == nullptr || logger->getErrors())
    {
        std::vector<Luau::ParseError> errors;

        for (const auto msg : logger->getMessages())
        {
            if (msg->getType() == Tailslide::LOG_ERROR || msg->getType() == Tailslide::LOG_INTERNAL_ERROR)
            {
                // Escape embedded newlines
                std::string errorMsg;
                for (char c : msg->getMessage())
                {
                    if (c == '\n')
                        errorMsg += "\\n";
                    else if (c == '\r')
                        errorMsg += "\\r";
                    else
                        errorMsg += c;
                }

                errors.emplace_back(convertLoc(msg->getLoc()), std::move(errorMsg));
            }
        }

        // Also include warnings when there are errors
        for (const auto msg : logger->getMessages())
        {
            if (msg->getType() == Tailslide::LOG_WARN)
            {
                // Escape embedded newlines and prefix with "WARN: "
                std::string warnMsg = "WARN: ";
                for (char c : msg->getMessage())
                {
                    if (c == '\n')
                        warnMsg += "\\n";
                    else if (c == '\r')
                        warnMsg += "\\r";
                    else
                        warnMsg += c;
                }

                errors.emplace_back(convertLoc(msg->getLoc()), std::move(warnMsg));
            }
        }

        if (errors.empty())
            throw Luau::ParseError({{0,0},{0,0}}, "Unknown compilation failure");

        throw Luau::ParseErrors(std::move(errors));
    }
    // Need to make any casts explicit
    LuauDeSugaringVisitor de_sugaring_visitor(script->mContext->allocator);
    script->visit(&de_sugaring_visitor);

    // Need to propagate values for all the injected casts and whatnot
    script->propagateValues();

    ConstantExpressionSimplifier constant_expression_simplifier(&parser.allocator);
    script->visit(&constant_expression_simplifier);

    // Determine register usage for locals
    LuauSymbolMap symbol_map;
    LuauResourceVisitor luauResourceVisitor(&symbol_map);
    script->visit(&luauResourceVisitor);

    LuauVisitor luauVisitor(&bcb, symbol_map);
    script->visit(&luauVisitor);
}

std::string compileLSL(const std::string &source)
{
    Luau::BytecodeBuilder bcb;
    try
    {
        compileLSLOrThrow(bcb, source);
        return bcb.getBytecode();
    }
    catch (Luau::ParseErrors &e)
    {
        std::string msg = ": Parse Errors:";
        for (const Luau::ParseError &error : e.getErrors()) {
            msg += Luau::format("\nLine %d: %s", error.getLocation().begin.line, error.what());
        }
        return Luau::BytecodeBuilder::getError(msg);
    }
    catch (Luau::CompileError &e)
    {
        std::string msg = Luau::format(":%d: %s", e.getLocation().begin.line, e.what());
        return Luau::BytecodeBuilder::getError(msg);
    }
}

char* luau_lsl_compile(const char* source, size_t size, size_t* outsize, bool *is_error)
{
    *outsize = 0;
    Luau::BytecodeBuilder bcb;
    std::string result;
    try
    {
        compileLSLOrThrow(bcb, std::string(source, size));
        result = bcb.getBytecode();
        *is_error = false;
    }
    catch(Luau::ParseErrors &e)
    {
        std::string msg = ": Parse Errors:";
        for (const Luau::ParseError &error : e.getErrors()) {
            msg += Luau::format("\nLine %d: %s", error.getLocation().begin.line, error.what());
        }

        result = msg;
        *is_error = true;
    }
    catch(Luau::CompileError &e)
    {
        // Users of this function expect only a single error message
        std::string error = Luau::format(":%d: %s", e.getLocation().begin.line, e.what());

        result = error;
        *is_error = true;
    }

    char* copy = static_cast<char*>(malloc(result.size()));
    if (!copy)
        return nullptr;

    memcpy(copy, result.data(), result.size());
    *outsize = result.size();
    return copy;
}
