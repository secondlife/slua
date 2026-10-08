#pragma once

#include <vector>

#include "../../lslmini.hh"
#include "../../visitor.hh"
#include "../../bitstream.hh"
#include "resource_collector.hh"

namespace Tailslide {

struct MonoCompilationOptions {
  bool optimize_sutractions = false;
  bool omit_unnecessary_pushes = false;
};

class MonoScriptCompiler : public ASTVisitor {
  public:
    explicit MonoScriptCompiler(ScriptAllocator *allocator, MonoCompilationOptions options={}) :
        _mAllocator(allocator), _mOptions(options) {};

    std::stringstream mCIL {};
  protected:
    virtual bool visit(LSLScript *script);
    virtual bool visit(LSLGlobalVariable *glob_var);

    void pushLValueContainer(LSLLValueExpression *lvalue);
    void pushLValue(LSLLValueExpression *lvalue);
    void pushConstant(LSLConstant *cv);
    // for constants written in the script, LL's compiler always uses the full-form
    // ldc.i4 for those, only using the short forms for values it generates itself.
    void pushLiteral(LSLConstant *cv);
    void pushFloatLiteral(double value);
    void storeToLValue(LSLLValueExpression *lvalue, bool push_result);
    void castTopOfStack(LSLIType from_type, LSLIType to_type);
    std::string getGlobalVarSpecifier(LSLSymbol *sym);
    std::string getLValueAccessorSpecifier(LSLLValueExpression *lvalue);

    virtual bool visit(LSLEventHandler *handler);
    virtual bool visit(LSLGlobalFunction *glob_func);
    void buildFunction(LSLASTNode *func);

    virtual bool visit(LSLExpressionStatement *expr_stmt);
    virtual bool visit(LSLReturnStatement *ret_stmt);
    virtual bool visit(LSLLabel *label_stmt);
    virtual bool visit(LSLJumpStatement *jump_stmt);
    virtual bool visit(LSLDeclaration *decl_stmt);
    virtual bool visit(LSLIfStatement *if_stmt);
    virtual bool visit(LSLForStatement *for_stmt);
    virtual bool visit(LSLWhileStatement *while_stmt);
    virtual bool visit(LSLDoStatement *do_stmt);
    virtual bool visit(LSLStateStatement *state_stmt);

    virtual bool visit(LSLConstantExpression *constant_expr);
    virtual bool visit(LSLTypecastExpression *cast_expr);
    virtual bool visit(LSLBoolConversionExpression *bool_expr);
    virtual bool visit(LSLVectorExpression *vec_expr);
    virtual bool visit(LSLQuaternionExpression *quat_expr);
    virtual bool visit(LSLLValueExpression *lvalue);
    virtual bool visit(LSLListExpression *list_expr);
    virtual bool visit(LSLFunctionExpression *func_expr);
    virtual bool visit(LSLBinaryExpression *bin_expr);
    void compileBinaryExpression(LSLOperator op, LSLExpression *left, LSLExpression *right, LSLIType ret_type);
    virtual bool visit(LSLUnaryExpression *unary_expr);
    virtual bool visit(LSLPrintExpression *print_expr);

    bool maybeOmitPush(LSLExpression *expr) {
        bool need_push = !_mOptions.omit_unnecessary_pushes || expr->getResultNeeded();
        // tell our parent the push was omitted, it must clear this flag!
        if (!need_push)
          _mPushOmitted = true;
        return need_push;
    }

    ScriptAllocator *_mAllocator;
    MonoSymbolDataMap _mSymData {};
    LSLSymbol *_mCurrentFuncSym = nullptr;
    std::string _mScriptClassName;
    bool _mInGlobalExpr = false;
    MonoCompilationOptions _mOptions {};
    bool _mPushOmitted = false;
    uint32_t _mJumpNum = 0;
};

const char * const CIL_TYPE_NAMES[LST_MAX] = {
    "void",
    "int32",
    "float32",
    "string",
    "valuetype [ScriptTypes]LindenLab.SecondLife.Key",
    "class [ScriptTypes]LindenLab.SecondLife.Vector",
    "class [ScriptTypes]LindenLab.SecondLife.Quaternion",
    "class [mscorlib]System.Collections.ArrayList",
    "ERROR"
};

const char * const CIL_VALUE_TYPE_NAMES[LST_MAX] = {
    "void",
    "int32",
    "float32",
    "string",
    "valuetype [ScriptTypes]LindenLab.SecondLife.Key",
    // TODO: are some of these just misnomers that are equivalent to `CIL_TYPE_NAMES` anyway?
    //  Can something be contextually a valuetype? Maybe they're actually valuetypes and not classes.
    "valuetype [ScriptTypes]LindenLab.SecondLife.Vector",
    "valuetype [ScriptTypes]LindenLab.SecondLife.Quaternion",
    "class [mscorlib]System.Collections.ArrayList",
    "ERROR"
};

const char * const CIL_BOXING_INSTRUCTIONS[LST_MAX] = {
    "ERROR!\n", // void
    "box [mscorlib]System.Int32\n",
    "box [mscorlib]System.Single\n",
    "", // string not a value type, it's already boxed.
    "box [ScriptTypes]LindenLab.SecondLife.Key\n",
    "box [ScriptTypes]LindenLab.SecondLife.Vector\n",
    "box [ScriptTypes]LindenLab.SecondLife.Quaternion\n",
    "", // list not a value type, it's already boxed.
    "ERROR!\n" // error
};

}
