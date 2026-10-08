#ifndef _TYPES_HH
#define _TYPES_HH 1
#include "ast.hh"           // LSLASTNode
#include <vector>

namespace Tailslide {


enum LSLOperator : uint8_t {
  OP_NONE = '\0',
  OP_PARENS = '(',
  OP_PLUS = '+',
  OP_MINUS = '-',
  OP_MUL = '*',
  OP_DIV = '/',
  OP_MOD = '%',
  OP_ASSIGN = '=',
  OP_BIT_NOT = '~',
  OP_BIT_XOR = '^',
  OP_BIT_AND = '&',
  OP_BIT_OR = '|',
  OP_BOOLEAN_NOT = '!',
  OP_LESS = '<',
  OP_GREATER = '>',
  OP_BOOLEAN_AND = 128,
  OP_BOOLEAN_OR ,
  OP_PRE_INCR,
  OP_PRE_DECR,
  OP_POST_INCR,
  OP_POST_DECR,
  OP_ADD_ASSIGN,
  OP_SUB_ASSIGN,
  OP_MUL_ASSIGN,
  OP_DIV_ASSIGN,
  OP_MOD_ASSIGN,
  OP_SHIFT_LEFT,
  OP_SHIFT_RIGHT,
  OP_LEQ,
  OP_GEQ,
  OP_EQ,
  OP_NEQ,
};

class LSLType : public LSLASTNode {
  public:
    explicit LSLType(LSLIType type, bool static_def=false) : LSLASTNode(nullptr), _mIType(type) {
      // Parenting the global LSLType instances to a specific script's tree is illegal
      if (static_def)
        markStatic();
    };
    static LSLType *get(LSLIType type ) {
      return &_sTypes[type];
    }
    bool canCoerce(LSLType *to );
    LSLType *getResultType(LSLOperator op, LSLType *right);


    LSLIType getIType() { return _mIType; };
    virtual std::string getNodeName() {
      switch (_mIType) {
        case LST_ERROR:         return "error";
        case LST_INTEGER:       return "integer";
        case LST_FLOATINGPOINT: return "float";
        case LST_STRING:        return "string";
        case LST_KEY:           return "key";
        case LST_VECTOR:        return "vector";
        case LST_QUATERNION:    return "quaternion";
        case LST_LIST:          return "list";
        case LST_NULL:          return "none";
        default:                return "!invalid!";
      }
    }
    virtual LSLNodeType getNodeType() { return NODE_TYPE; };

    class LSLConstant *getDefaultValue() { return _mDefaultVal; }
    void setDefaultValue(class LSLConstant *default_val) { _mDefaultVal = default_val; }
    class LSLConstant *getOneValue() { return _mOneVal; }
    void setOneValue(class LSLConstant *one_val) { _mOneVal = one_val; }
  private:
    LSLIType _mIType;
    class LSLConstant *_mDefaultVal = nullptr;
    class LSLConstant *_mOneVal = nullptr;
    static LSLType _sTypes[LST_MAX];
};

// convenience
inline LSLType *TYPE(LSLIType type) {
  return LSLType::get(type);
}

LSLOperator decouple_compound_operation(LSLOperator operation);
bool operation_mutates(LSLOperator operation);
const char *operation_str(LSLOperator operation);
const char *operation_repr_str(LSLOperator operation);
bool is_cast_legal(LSLIType from, LSLIType to);


}

#endif /* not _TYPES_HH */
