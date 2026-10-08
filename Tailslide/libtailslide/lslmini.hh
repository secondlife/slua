#ifndef TAILSLIDE_LSLMINI_HH
#define TAILSLIDE_LSLMINI_HH

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype> // isprint()
#include <cstdarg>
#include <functional>
#include <sstream>

#include "loctype.hh"
#include "symtab.hh"
#include "ast.hh"
#include "types.hh"
#include "strings.hh"

namespace Tailslide {

typedef int   S32;
typedef float F32;
typedef double F64;

class LSLScript;

/// Add a getter / setter field pair to an LSLASTNode subclass
#define NODE_FIELD_GS(_typ, _name, _index)       \
    _typ *get##_name() {                         \
      LSLASTNode *child = getChild((_index));    \
      assert(child);                             \
      if (child->getNodeType() == NODE_NULL)     \
        return nullptr;                          \
      return (_typ*)(child);                     \
    };                                           \
    void set##_name(_typ *new_val) {             \
      setChild(_index, (LSLASTNode*)(new_val));  \
    }

struct ScriptContext {
  LSLScript *script = nullptr;
  ScriptAllocator *allocator = nullptr;
  Logger *logger = nullptr;
  LSLSymbolTable *builtins = nullptr;
  LSLSymbolTableManager *table_manager = nullptr;
  bool ast_sane = true;
  // any nodes created while this is false will be considered synthetic by default
  bool parsing = false;
  Tailslide::TailslideLType glloc {0};
  void *scanner = nullptr;
  bool collect_assertions = false;
  std::vector<std::pair<int, ErrorCode>> assertions;
};

struct Vector3 {
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {};
    Vector3(): Vector3(0, 0, 0) {};
    float x, y, z;

    bool operator==(const struct Vector3 &other) const {
      return (x == other.x) && (y == other.y) && (z == other.z);
    };
    bool operator!=(const struct Vector3 &other) const {
      return !(*this == other);
    };
};

struct Quaternion {
    Quaternion(float x, float y, float z, float s): x(x), y(y), z(z), s(s) {};
    Quaternion(): Quaternion(0, 0, 0, 1) {};
    float x, y, z, s;

    bool operator==(const struct Quaternion &other) const {
      return (x == other.x) && (y == other.y) && (z == other.z) && (s == other.s);
    };
    bool operator!=(const struct Quaternion &other) const {
      return !(*this == other);
    };
};


class LSLIdentifier : public LSLASTNode {
  public:
    LSLIdentifier( ScriptContext *ctx, const char *name ) : LSLASTNode(ctx), _mName(name) {};
    LSLIdentifier( ScriptContext *ctx, class LSLType *type, const char *name ) : LSLASTNode(ctx), _mName(name) { _mType = type; };
    LSLIdentifier( ScriptContext *ctx, class LSLType *type, const char *name, YYLTYPE *lloc ) : LSLASTNode(ctx, lloc, 0), _mName(name) { _mType = type; };
    LSLIdentifier( ScriptContext *ctx, LSLIdentifier *other ) : LSLASTNode(ctx), _mName(other->getName()) {};

    const char    *getName() { return _mName; }

    void resolveSymbol(LSLSymbolType symbol_type);
    void setSymbol(LSLSymbol *symbol ) { _mSymbol = symbol; };
    virtual LSLSymbol *getSymbol() { return _mSymbol; };

    virtual std::string getNodeName() {
      char buf[256];
      snprintf(buf, 256, "identifier \"%s\"", _mName);
      return buf;
    }
    virtual LSLNodeType getNodeType() { return NODE_IDENTIFIER; };
    virtual class LSLConstant *getConstantValue();

    LSLIdentifier *clone();

  private:
    LSLSymbol                  *_mSymbol = nullptr;
    const char                      *_mName;
};

class LSLGlobalVariable : public LSLASTNode {
  public:
    LSLGlobalVariable( ScriptContext *ctx, class LSLIdentifier *identifier, class LSLExpression *value )
      : LSLASTNode(ctx, 2, identifier, value) { DEBUG( LOG_DEBUG_SPAM, nullptr, "made a global var\n"); };
    NODE_FIELD_GS(LSLIdentifier, Identifier, 0)
    NODE_FIELD_GS(class LSLExpression, Initializer, 1)

    virtual std::string getNodeName() { return "global var"; }
    virtual LSLNodeType getNodeType() { return NODE_GLOBAL_VARIABLE; };

    virtual LSLConstant *getConstantValue();
    virtual LSLSymbol *getSymbol() {return ((LSLIdentifier *) getChild(0))->getSymbol(); }
};


class LSLConstant : public LSLASTNode {
  public:
    explicit LSLConstant(ScriptContext *ctx) : LSLASTNode(ctx) { _mConstantValue = this; }
    virtual std::string getNodeName() { return "unknown constant"; }
    virtual LSLNodeType getNodeType() { return NODE_CONSTANT; };
    // make a shallow copy of the constant
    virtual LSLConstant *copy(ScriptAllocator *allocator) = 0;
    virtual bool containsNaN() { return false; };
    /// was this constant negated by the parser
    virtual bool wasNegated() { return _mWasNegated; };
    virtual void setWasNegated(bool negated) { _mWasNegated = negated; };
  protected:
    bool _mWasNegated = false;
};

/////////////////////////////////////////////////////
// Integer Constant

class LSLIntegerConstant : public LSLConstant {
  public:
    LSLIntegerConstant( ScriptContext *ctx, int v ) : LSLConstant(ctx), _mValue(v) { _mType = TYPE(LST_INTEGER); }

    virtual std::string getNodeName() {
      char buf[256];
      snprintf(buf, 256, "integer constant: %d", _mValue);
      return buf;
    }

    virtual LSLNodeSubType getNodeSubType() { return NODE_INTEGER_CONSTANT; }

    int getValue() const { return _mValue; }
    virtual LSLConstant *copy(ScriptAllocator *allocator) {
      return allocator->newTracked<LSLIntegerConstant>(_mValue);
    };

  private:
    int _mValue;
};


/////////////////////////////////////////////////////
// Float Constant

class LSLFloatConstant : public LSLConstant {
  public:
    LSLFloatConstant( ScriptContext *ctx, double v ) : LSLConstant(ctx), _mValue(v) { _mType = TYPE(LST_FLOATINGPOINT); }

    virtual std::string getNodeName() {
      char buf[256];
      snprintf(buf, 256, "float constant: %f", _mValue);
      return buf;
    }

    virtual LSLNodeSubType getNodeSubType() { return NODE_FLOAT_CONSTANT; }

    double getValue() const { return _mValue; }
    virtual bool containsNaN();
    virtual LSLConstant *copy(ScriptAllocator *allocator) {
      return allocator->newTracked<LSLFloatConstant>(_mValue);
    };

  private:
    double _mValue;
};


/////////////////////////////////////////////////////
// String Constant

class LSLStringConstant : public LSLConstant {
  public:
    LSLStringConstant( ScriptContext *ctx, const char *v ) : LSLConstant(ctx), _mValue(v) { _mType = TYPE(LST_STRING); }

    virtual std::string getNodeName() {
      char buf[256];
      snprintf(buf, 256, "string constant: \"%s\"", escape_string(_mValue).c_str());
      return buf;
    }

    virtual LSLNodeSubType getNodeSubType() { return NODE_STRING_CONSTANT; }

    const char *getValue() { return _mValue; }
    virtual LSLConstant *copy(ScriptAllocator *allocator) {
      return allocator->newTracked<LSLStringConstant>(_mValue);
    };

  protected:
    const char *_mValue;
};


/////////////////////////////////////////////////////
// String Constant


class LSLKeyConstant : public LSLStringConstant {
  public:
    LSLKeyConstant( ScriptContext *ctx, const char *v ) : LSLStringConstant(ctx, v) { _mType = TYPE(LST_KEY); }
    virtual LSLConstant *copy(ScriptAllocator *allocator) {
      return allocator->newTracked<LSLKeyConstant>(_mValue);
    };

    virtual std::string getNodeName() {
      char buf[256];
      snprintf(buf, 256, "key constant: \"%s\"", escape_string(_mValue).c_str());
      return buf;
    }

    virtual LSLNodeSubType getNodeSubType() { return NODE_KEY_CONSTANT; }
};

/////////////////////////////////////////////////////
// List Constant

class LSLListConstant : public LSLConstant {
  public:
    LSLListConstant( ScriptContext *ctx, class LSLConstant *v ) : LSLConstant(ctx) {
      _mType = TYPE(LST_LIST);
      // so we can do symbol resolution inside the list constant
      if (v != nullptr)
        pushChild(v);
    }

    virtual std::string getNodeName() {
      char buf[256];
      snprintf(buf, 256, "list constant: %d entries", getLength());
      return buf;
    }

    virtual LSLNodeSubType getNodeSubType() { return NODE_LIST_CONSTANT; }

    class LSLConstant *getValue() { return (LSLConstant*) _mChildren; }

    int getLength() { return (int) getNumChildren(); }

    virtual LSLConstant *copy(ScriptAllocator *allocator) {
      auto *new_const = allocator->newTracked<LSLListConstant>(nullptr);
      // need to copy the children since they're going to be re-parented to the copy
      for (auto *old_child : *this) {
        new_const->pushChild(((LSLConstant *)old_child)->copy(allocator));
      }
      return new_const;
    };
};

/////////////////////////////////////////////////////
// Vector Constant

class LSLVectorConstant : public LSLConstant {
  public:
    LSLVectorConstant( ScriptContext *ctx, float x, float y, float z ): LSLConstant(ctx), _mValue({x, y, z}) {
      _mType = TYPE(LST_VECTOR);
    };

    virtual std::string getNodeName() {
      char buf[256];
      snprintf(buf, 256, "vector constant: <%g, %g, %g>", _mValue.x, _mValue.y, _mValue.z);
      return buf;
    }

    virtual LSLNodeSubType getNodeSubType() { return NODE_VECTOR_CONSTANT; }

    const Vector3 *getValue() { return &_mValue; }
    virtual bool containsNaN();

    virtual LSLConstant *copy(ScriptAllocator *allocator) {
      return allocator->newTracked<LSLVectorConstant>(_mValue.x, _mValue.y, _mValue.z);
    };

  private:
    Vector3 _mValue;
};


/////////////////////////////////////////////////////
// Quaternion Constant

class LSLQuaternionConstant : public LSLConstant {
  public:
    LSLQuaternionConstant( ScriptContext *ctx, float x, float y, float z, float s ): LSLConstant(ctx), _mValue({x, y, z, s}) {
      _mType = TYPE(LST_QUATERNION);
    };

    virtual std::string getNodeName() {
      char buf[256];
      snprintf(buf, 256, "quaternion constant: <%g, %g, %g, %g>", _mValue.x, _mValue.y, _mValue.z, _mValue.s);
      return buf;
    }

    virtual LSLNodeSubType getNodeSubType() { return NODE_QUATERNION_CONSTANT; }

    const Quaternion *getValue() { return &_mValue; }
    virtual bool containsNaN();

    virtual LSLConstant *copy(ScriptAllocator *allocator) {
      return allocator->newTracked<LSLQuaternionConstant>(_mValue.x, _mValue.y, _mValue.z, _mValue.s);
    };

  private:
    Quaternion _mValue;
};


class LSLGlobalFunction : public LSLASTNode {
  public:
    LSLGlobalFunction( ScriptContext *ctx, class LSLIdentifier *identifier, class LSLFunctionDec *decl, class LSLStatement *statement )
      : LSLASTNode( ctx, 3, identifier, decl, statement ) {};
    NODE_FIELD_GS(LSLIdentifier, Identifier, 0)
    NODE_FIELD_GS(LSLFunctionDec, Arguments, 1)
    NODE_FIELD_GS(LSLStatement, Statements, 2)

    virtual std::string getNodeName() { return "global func"; }
    virtual LSLNodeType getNodeType() { return NODE_GLOBAL_FUNCTION; };
    virtual LSLSymbol *getSymbol() {return ((LSLIdentifier *) getChild(0))->getSymbol(); }
};

class LSLParamList : public LSLASTNodeList<LSLIdentifier> {
  public:
    explicit LSLParamList( ScriptContext *ctx ) : LSLASTNodeList(ctx, nullptr) {};
    LSLParamList( ScriptContext *ctx, class LSLIdentifier *identifiers ) : LSLASTNodeList(ctx, identifiers) {};
};

class LSLFunctionDec : public LSLParamList {
  public:
    explicit LSLFunctionDec(ScriptContext *ctx) : LSLParamList(ctx) {};
    LSLFunctionDec( ScriptContext *ctx, class LSLIdentifier *identifiers ) : LSLParamList(ctx, identifiers) {};
    virtual std::string getNodeName() { return "function decl"; }
    virtual LSLNodeType getNodeType() { return NODE_FUNCTION_DEC; };
};

class LSLEventDec : public LSLParamList {
  public:
    explicit LSLEventDec(ScriptContext *ctx) : LSLParamList(ctx) {};
    LSLEventDec( ScriptContext *ctx, class LSLIdentifier *identifiers ) : LSLParamList(ctx, identifiers) {};

    virtual std::string getNodeName() { return "event decl"; }
    virtual LSLNodeType getNodeType() { return NODE_EVENT_DEC; };
};


class LSLEventHandler : public LSLASTNode {
  public:
  LSLEventHandler( ScriptContext *ctx, class LSLIdentifier *identifier, class LSLEventDec *decl, class LSLStatement *body )
      : LSLASTNode(ctx, 3, identifier, decl, body) {};
  NODE_FIELD_GS(LSLIdentifier, Identifier, 0)
  NODE_FIELD_GS(LSLFunctionDec, Arguments, 1)
  NODE_FIELD_GS(LSLStatement, Statements, 2)

  virtual std::string getNodeName() { return "event handler"; }
  virtual LSLNodeType getNodeType() { return NODE_EVENT_HANDLER; };
  virtual LSLSymbol *getSymbol() {return ((LSLIdentifier *) getChild(0))->getSymbol(); }
};

class LSLState : public LSLASTNode {
  public:
    LSLState( ScriptContext *ctx, class LSLIdentifier *identifier, LSLASTNodeList<LSLEventHandler> *event_handlers)
        : LSLASTNode( ctx, 2, identifier, event_handlers) {};
    NODE_FIELD_GS(LSLIdentifier, Identifier, 0)
    NODE_FIELD_GS(LSLASTNodeList<class LSLEventHandler>, EventHandlers, 1)

    virtual std::string getNodeName() { return "state"; }
    virtual LSLNodeType getNodeType() { return NODE_STATE; };
    virtual LSLSymbol *getSymbol() {return ((LSLIdentifier *) getChild(0))->getSymbol(); }
};


class LSLExpression : public LSLASTNode {
  public:
  explicit LSLExpression(ScriptContext *ctx) : LSLASTNode(ctx, 0), _mOperation(OP_NONE) {};
  LSLExpression(ScriptContext *ctx, int num, ...): LSLASTNode(ctx), _mOperation(OP_NONE) {
    va_list ap;
    va_start(ap, num);
    addChildren(num, ap);
    va_end(ap);
  };

  virtual std::string getNodeName() {
    return "base expression";
  };
  virtual LSLNodeType getNodeType() { return NODE_EXPRESSION; };

  virtual LSLConstant *getConstantValue();
  virtual bool nodeAllowsFolding() { return true; };
  LSLOperator getOperation() const {return _mOperation;};
  void setOperation(LSLOperator op) {_mOperation = op;};
  virtual void setResultNeeded(bool result_needed) { _mResultNeeded = result_needed; };
  bool getResultNeeded() { return _mResultNeeded; }
  protected:
  LSLOperator _mOperation;
  bool _mResultNeeded = true;
};

class LSLStatement : public LSLASTNode {
  public:
    explicit LSLStatement( ScriptContext *ctx ): LSLASTNode(ctx) {}
    LSLStatement( ScriptContext *ctx, int num, ... ): LSLASTNode(ctx) {
      va_list ap;
      va_start(ap, num);
      addChildren(num, ap);
      va_end(ap);
    };
    virtual std::string getNodeName() { return "statement"; }
    virtual LSLNodeType getNodeType() { return NODE_STATEMENT; };
};

class LSLNopStatement : public LSLStatement {
  public:
    explicit LSLNopStatement( ScriptContext *ctx) : LSLStatement(ctx, 0) {}
    virtual std::string getNodeName() { return "nop statement"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_NOP_STATEMENT; };
};

class LSLCompoundStatement : public LSLStatement {
  public:
    LSLCompoundStatement( ScriptContext *ctx, class LSLStatement *statements ) : LSLStatement(ctx) {
      if (statements)
        pushChild(statements);
    }
    virtual std::string getNodeName() { return "compound statement"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_COMPOUND_STATEMENT; };
};

class LSLExpressionStatement : public LSLStatement {
  public:
  LSLExpressionStatement( ScriptContext *ctx, class LSLExpression *expr ) : LSLStatement(ctx, 1, expr) {}
  NODE_FIELD_GS(LSLExpression, Expr, 0)

  virtual std::string getNodeName() { return "expression statement"; };
  virtual LSLNodeSubType getNodeSubType() { return NODE_EXPRESSION_STATEMENT; };
};

class LSLStateStatement : public LSLStatement {
  public:
    LSLStateStatement( ScriptContext *ctx, class LSLIdentifier *identifier ) : LSLStatement(ctx, 1, identifier) {};
    NODE_FIELD_GS(LSLIdentifier, Identifier, 0)

    virtual std::string getNodeName() { return "setstate"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_STATE_STATEMENT; };
    virtual LSLSymbol *getSymbol() {return ((LSLIdentifier *) getChild(0))->getSymbol(); }
};

class LSLJumpStatement : public LSLStatement {
  public:
    LSLJumpStatement( ScriptContext *ctx, class LSLIdentifier *identifier ) : LSLStatement(ctx, 1, identifier) {};
    NODE_FIELD_GS(LSLIdentifier, Identifier, 0)

    virtual std::string getNodeName() {
      char buf[256];
      const char *jump_kind = "";
      if (_mIsBreakLike)
        jump_kind = " {break-like}";
      else if (_mIsContinueLike)
        jump_kind = " {continue-like}";
      snprintf(buf, 256, "jump%s", jump_kind);
      return buf;
    };
    virtual LSLNodeSubType getNodeSubType() { return NODE_JUMP_STATEMENT; };
    virtual LSLSymbol *getSymbol() {return ((LSLIdentifier *) getChild(0))->getSymbol(); }

    bool getIsBreakLike() const { return _mIsBreakLike; }
    void setIsBreakLike(bool break_like) { _mIsBreakLike = break_like; }

    bool getIsContinueLike() const { return _mIsContinueLike; }
    void setIsContinueLike(bool continue_like) { _mIsContinueLike = continue_like; }

  protected:
    // whether this jump is semantically equivalent to "break" in other languages
    bool _mIsBreakLike = false;
    // whether this jump is semantically equivalent to "continue" in other languages
    bool _mIsContinueLike = false;
};

class LSLLabel : public LSLStatement {
  public:
    LSLLabel( ScriptContext *ctx, class LSLIdentifier *identifier ) : LSLStatement(ctx, 1, identifier) {};
    NODE_FIELD_GS(LSLIdentifier, Identifier, 0)

    virtual std::string getNodeName() { return "label"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_LABEL; };
    virtual LSLSymbol *getSymbol() {return ((LSLIdentifier *) getChild(0))->getSymbol(); }
};

class LSLReturnStatement : public LSLStatement {
  public:
    LSLReturnStatement( ScriptContext *ctx, class LSLExpression *expression ) : LSLStatement(ctx, 1, expression) {};
    NODE_FIELD_GS(LSLExpression, Expr, 0)

    virtual std::string getNodeName() { return "return"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_RETURN_STATEMENT; };
};

class LSLIfStatement : public LSLStatement {
  public:
    LSLIfStatement( ScriptContext *ctx, class LSLExpression *expression, class LSLStatement *true_branch, class LSLStatement *false_branch)
      : LSLStatement( ctx, 3, expression, true_branch, false_branch ) {};
    NODE_FIELD_GS(LSLExpression, CheckExpr, 0)
    NODE_FIELD_GS(LSLStatement, TrueBranch, 1)
    NODE_FIELD_GS(LSLStatement, FalseBranch, 2)

    virtual std::string getNodeName() { return "if"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_IF_STATEMENT; };
};

class LSLForStatement : public LSLStatement {
  public:
    LSLForStatement(ScriptContext *ctx, class LSLASTNodeList<LSLExpression> *init, class LSLExpression *condition,
                    class LSLASTNodeList<LSLExpression> *cont, class LSLStatement *body)
      : LSLStatement( ctx, 4, init, condition, cont, body ) {};
    NODE_FIELD_GS(LSLASTNodeList<LSLExpression>, InitExprs, 0)
    NODE_FIELD_GS(LSLExpression, CheckExpr, 1)
    NODE_FIELD_GS(LSLASTNodeList<LSLExpression>, IncrExprs, 2)
    NODE_FIELD_GS(LSLStatement, Body, 3)

    virtual std::string getNodeName() { return "for"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_FOR_STATEMENT; };
};

class LSLDoStatement : public LSLStatement {
  public:
    LSLDoStatement( ScriptContext *ctx, class LSLStatement *body, class LSLExpression *condition )
      : LSLStatement(ctx, 2, body, condition) {};
    NODE_FIELD_GS(LSLStatement, Body, 0)
    NODE_FIELD_GS(LSLExpression, CheckExpr, 1)

    virtual std::string getNodeName() { return "do"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_DO_STATEMENT; };
};

class LSLWhileStatement : public LSLStatement {
  public:
    LSLWhileStatement( ScriptContext *ctx, class LSLExpression *condition, class LSLStatement *body )
      : LSLStatement(ctx, 2, condition, body) {};
    NODE_FIELD_GS(LSLExpression, CheckExpr, 0)
    NODE_FIELD_GS(LSLStatement, Body, 1)

    virtual std::string getNodeName() { return "while"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_WHILE_STATEMENT; };
};


class LSLDeclaration : public LSLStatement {
  public:
    LSLDeclaration(ScriptContext *ctx, class LSLIdentifier *identifier, class LSLExpression *value)
      : LSLStatement(ctx, 2, identifier, value) { };
    NODE_FIELD_GS(LSLIdentifier, Identifier, 0)
    NODE_FIELD_GS(LSLExpression, Initializer, 1)

    virtual std::string getNodeName() { return "declaration"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_DECLARATION; };

    virtual LSLConstant *getConstantValue();
    virtual LSLSymbol *getSymbol() {return ((LSLIdentifier *) getChild(0))->getSymbol(); }
};


class LSLConstantExpression: public LSLExpression {
public:
    LSLConstantExpression( ScriptContext *ctx, LSLConstant *constant )
      : LSLExpression(ctx) {
      assert(constant);
      // if this is a static or something already parented to a tree, we must copy
      if (constant->isStatic() || constant->getParent())
        constant = constant->copy(ctx->allocator);
      pushChild(constant);
      _mConstantValue = constant;
      _mType = constant->getType();
    };

    virtual std::string getNodeName() {
      return "constant expression";
    };
    virtual LSLNodeSubType getNodeSubType() { return NODE_CONSTANT_EXPRESSION; };
};


class LSLParenthesisExpression: public LSLExpression {
public:
    LSLParenthesisExpression( ScriptContext *ctx, LSLExpression *expr )
      : LSLExpression(ctx, 1, expr) { _mOperation = OP_PARENS; };
    NODE_FIELD_GS(LSLExpression, ChildExpr, 0)

    std::string getNodeName() override {
      return "parenthesis expression";
    };
    LSLNodeSubType getNodeSubType() override { return NODE_PARENTHESIS_EXPRESSION; };
    void setResultNeeded(bool result_needed) override {
      LSLExpression::setResultNeeded(result_needed);
      getChildExpr()->setResultNeeded(result_needed);
    };
};


class LSLBinaryExpression : public LSLExpression {
public:
    LSLBinaryExpression( ScriptContext *ctx, LSLExpression *lvalue, LSLOperator oper, LSLExpression *rvalue )
    : LSLExpression(ctx, 2, lvalue, rvalue) { _mOperation = oper; };
    NODE_FIELD_GS(LSLExpression, LHS, 0)
    NODE_FIELD_GS(LSLExpression, RHS, 1)

    virtual std::string getNodeName() {
      char buf[256];
      snprintf( buf, 256, "binary expression: '%s'", operation_repr_str(_mOperation) );
      return buf;
    };
    virtual LSLNodeSubType getNodeSubType() { return NODE_BINARY_EXPRESSION; };
};

class LSLUnaryExpression : public LSLExpression {
public:
    LSLUnaryExpression( ScriptContext *ctx, LSLExpression *lvalue, LSLOperator oper )
            : LSLExpression(ctx, 1, lvalue) { _mOperation = oper; };
    NODE_FIELD_GS(LSLExpression, ChildExpr, 0)

    virtual std::string getNodeName() {
      char buf[256];
      snprintf( buf, 256, "unary expression: '%s'", operation_repr_str(_mOperation) );
      return buf;
    };
    virtual LSLNodeSubType getNodeSubType() { return NODE_UNARY_EXPRESSION; };
};

class LSLTypecastExpression : public LSLExpression {
  public:
    LSLTypecastExpression(ScriptContext *ctx, LSLType *type, LSLExpression *expression )
      : LSLExpression(ctx, 1, expression) { _mType = type;};
    NODE_FIELD_GS(LSLExpression, ChildExpr, 0)

    virtual std::string getNodeName() { return "typecast expression"; }
    virtual LSLNodeSubType getNodeSubType() { return NODE_TYPECAST_EXPRESSION; };
};

/// synthesized node to represent cases where something must be converted to boolean
class LSLBoolConversionExpression : public LSLExpression {
  public:
  LSLBoolConversionExpression(ScriptContext *ctx, LSLExpression *expression )
      : LSLExpression(ctx, 1, expression) { _mType = TYPE(LST_INTEGER);};
  NODE_FIELD_GS(LSLExpression, ChildExpr, 0)

  virtual std::string getNodeName() { return "boolean conversion"; }
  virtual LSLNodeSubType getNodeSubType() { return NODE_BOOL_CONVERSION_EXPRESSION; };
};

class LSLPrintExpression : public LSLExpression {
  public:
    LSLPrintExpression( ScriptContext *ctx, LSLExpression *expression )
      : LSLExpression( ctx, 1, expression ) { _mType = TYPE(LST_NULL); };
    NODE_FIELD_GS(LSLExpression, ChildExpr, 0)

    virtual std::string getNodeName() { return "print() call"; }
    virtual LSLNodeSubType getNodeSubType() { return NODE_PRINT_EXPRESSION; };
};

class LSLFunctionExpression : public LSLExpression {
  public:
    LSLFunctionExpression( ScriptContext *ctx, LSLIdentifier *identifier, LSLASTNodeList<LSLExpression> *arguments )
      : LSLExpression( ctx, 2, identifier, arguments) {};
    NODE_FIELD_GS(LSLIdentifier, Identifier, 0)
    NODE_FIELD_GS(LSLASTNodeList<LSLExpression>, Arguments, 1)

    virtual std::string getNodeName() { return "function call"; }
    virtual LSLNodeSubType getNodeSubType() { return NODE_FUNCTION_EXPRESSION; };

    virtual bool nodeAllowsFolding() { return false; };
    virtual LSLSymbol *getSymbol() {return ((LSLIdentifier *) getChild(0))->getSymbol(); }
};

class LSLVectorExpression : public LSLExpression {
  public:
    LSLVectorExpression(ScriptContext *ctx, LSLExpression *x, LSLExpression *y, LSLExpression *z )
      : LSLExpression(ctx, 3, x, y, z) { _mType = TYPE(LST_VECTOR); }
    NODE_FIELD_GS(LSLExpression, X, 0)
    NODE_FIELD_GS(LSLExpression, Y, 1)
    NODE_FIELD_GS(LSLExpression, Z, 2)

    virtual std::string getNodeName() { return "vector expression"; }
    virtual LSLNodeSubType getNodeSubType() { return NODE_VECTOR_EXPRESSION; };
};

class LSLQuaternionExpression : public LSLExpression {
  public:
    LSLQuaternionExpression(ScriptContext *ctx, LSLExpression *x, LSLExpression *y, LSLExpression *z, LSLExpression *s )
      : LSLExpression(ctx, 4, x, y, z, s) { _mType = TYPE(LST_QUATERNION); };
    NODE_FIELD_GS(LSLExpression, X, 0)
    NODE_FIELD_GS(LSLExpression, Y, 1)
    NODE_FIELD_GS(LSLExpression, Z, 2)
    NODE_FIELD_GS(LSLExpression, S, 3)

    virtual std::string getNodeName() { return "quaternion expression"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_QUATERNION_EXPRESSION; };
};

class LSLListExpression : public LSLExpression {
  public:
    LSLListExpression( ScriptContext *ctx, LSLExpression *c ) : LSLExpression(ctx) {
      _mType = TYPE(LST_LIST);
      if (c)
        pushChild(c);
    };

    virtual std::string getNodeName() { return "list expression"; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_LIST_EXPRESSION; }
    node_child_iterator<LSLExpression> begin() {
      return node_child_iterator<LSLExpression>(static_cast<LSLExpression*>(_mChildren));
    }
    node_child_iterator<LSLExpression> end() {
      return node_child_iterator<LSLExpression>(nullptr);
    }
};

class LSLLValueExpression : public LSLExpression {
  public:
    LSLLValueExpression( ScriptContext *ctx, LSLIdentifier *identifier, LSLIdentifier *member )
      : LSLExpression(ctx, 2, identifier, member), _mIsFoldable(false), _mInGlobalContext(false) {};
    NODE_FIELD_GS(LSLIdentifier, Identifier, 0)
    NODE_FIELD_GS(LSLIdentifier, Member, 1)

    virtual std::string getNodeName() {
      char buf[256];
      snprintf(buf, 256, "lvalue expression {%sfoldable}", _mIsFoldable ? "" : "not ");
      return buf;
    };
    virtual LSLNodeSubType getNodeSubType() { return NODE_LVALUE_EXPRESSION; };
    virtual LSLConstant *getConstantValue();
    virtual LSLSymbol *getSymbol() {return ((LSLIdentifier*)getChild(0))->getSymbol(); };

    void setIsFoldable(bool foldable) { _mIsFoldable = foldable;};
    bool getIsFoldable() const {return _mIsFoldable;};
    void setInGlobalContext(bool in_global_context) {_mInGlobalContext = in_global_context;};
    bool getInGlobalContext() const {return _mInGlobalContext;};

    LSLLValueExpression *clone();
  private:
    bool _mIsFoldable;
    bool _mInGlobalContext;
};

class LSLScript : public LSLASTNode {
    public:
    LSLScript( ScriptContext *ctx, LSLASTNodeList<LSLASTNode> *globals, LSLASTNodeList<LSLState> *states )
        : LSLASTNode( ctx, 2, globals, states ) {};
    NODE_FIELD_GS(LSLASTNodeList<LSLASTNode>, Globals, 0)
    NODE_FIELD_GS(LSLASTNodeList<LSLState>, States, 1)

    virtual std::string getNodeName() { return "script"; };
    virtual LSLNodeType getNodeType() { return NODE_SCRIPT; };
    virtual LSLSymbol *lookupSymbol(const char *name, LSLSymbolType sym_type);

    void optimize(const OptimizationOptions &ctx);
    void recalculateReferenceData();
    void validateGlobals(bool mono_semantics);
};

// Called once at startup, not thread-safe. Loads builtins from a file, or from
// the copy of builtins.txt embedded in the library when `builtins_file` is null.
// Definitions that can't be read are skipped with a message on stderr, returns
// false if that happened or the file couldn't be opened.
bool tailslide_init_builtins(const char *builtins_file);
// Same, but from a NUL-terminated buffer in builtins.txt format.
bool tailslide_init_builtins_from_data(const char *data);

}

// make sure our define doesn't leak into the public API
#undef NODE_FIELD_GS

#endif
