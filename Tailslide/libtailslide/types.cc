#include "lslmini.hh"
#include "types.hh"

#define  LST_ANY        -1
#define  LST_NONE       -2
#define  LST_BOOLEAN    LST_INTEGER     // for clarity

namespace Tailslide {
// TODO: use structs or something here

const static int COERCION_TABLE[][2] = {
        // wanted type      acceptable type
        {LST_FLOATINGPOINT, LST_INTEGER},
        {LST_STRING,        LST_KEY},
        {LST_KEY,           LST_STRING},
        {-1,                -1},
};

// from->to allowed lookup table
const static int LEGAL_CAST_TABLE[LST_MAX][LST_MAX] = {
    /* NULL   */ {0},
    /* INT    */ {0, 1, 1, 1, 0, 0, 0, 1, 0},
    /* FLOAT  */ {0, 1, 1, 1, 0, 0, 0, 1, 0},
    /* STRING */ {0, 1, 1, 1, 1, 1, 1, 1, 0},
    /* KEY    */ {0, 0, 0, 1, 1, 0, 0, 1, 0},
    /* VECTOR */ {0, 0, 0, 1, 0, 1, 0, 1, 0},
    /* QUAT   */ {0, 0, 0, 1, 0, 0, 1, 1, 0},
    /* LIST   */ {0, 0, 0, 1, 0, 0, 0, 1, 0},
    /* ERROR  */ {0},
};

const static int OPERATOR_RESULTS[][4] = {

        // operator   left type           right type          result type
        // ++
        {OP_PRE_INCR,  LST_INTEGER,       LST_NONE,          LST_INTEGER},
        {OP_PRE_INCR,  LST_FLOATINGPOINT, LST_NONE,          LST_FLOATINGPOINT},
        {OP_POST_INCR, LST_INTEGER,       LST_NONE,          LST_INTEGER},
        {OP_POST_INCR, LST_FLOATINGPOINT, LST_NONE,          LST_FLOATINGPOINT},

        // --
        {OP_PRE_DECR,  LST_INTEGER,       LST_NONE,          LST_INTEGER},
        {OP_PRE_DECR,  LST_FLOATINGPOINT, LST_NONE,          LST_FLOATINGPOINT},
        {OP_POST_DECR, LST_INTEGER,       LST_NONE,          LST_INTEGER},
        {OP_POST_DECR, LST_FLOATINGPOINT, LST_NONE,          LST_FLOATINGPOINT},

        // =
        {'=',         LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {'=',         LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {'=',         LST_FLOATINGPOINT, LST_INTEGER,       LST_FLOATINGPOINT},
        {'=',         LST_STRING,        LST_STRING,        LST_STRING},
        {'=',         LST_STRING,        LST_KEY,           LST_STRING},
        {'=',         LST_KEY,           LST_STRING,        LST_KEY},
        {'=',         LST_KEY,           LST_KEY,           LST_KEY},
        {'=',         LST_VECTOR,        LST_VECTOR,        LST_VECTOR},
        {'=',         LST_QUATERNION,    LST_QUATERNION,    LST_QUATERNION},
        {'=',         LST_LIST,          LST_LIST,          LST_LIST},

        // -
        {'-',         LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {'-',         LST_INTEGER,       LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {'-',         LST_FLOATINGPOINT, LST_INTEGER,       LST_FLOATINGPOINT},
        {'-',         LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {'-',         LST_VECTOR,        LST_VECTOR,        LST_VECTOR},
        {'-',         LST_QUATERNION,    LST_QUATERNION,    LST_QUATERNION},

        // unary -
        {'-',         LST_INTEGER,       LST_NONE,          LST_INTEGER},
        {'-',         LST_FLOATINGPOINT, LST_NONE,          LST_FLOATINGPOINT},
        {'-',         LST_VECTOR,        LST_NONE,          LST_VECTOR},
        {'-',         LST_QUATERNION,    LST_NONE,          LST_QUATERNION},

        // +
        {'+',         LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {'+',         LST_INTEGER,       LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {'+',         LST_FLOATINGPOINT, LST_INTEGER,       LST_FLOATINGPOINT},
        {'+',         LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {'+',         LST_STRING,        LST_STRING,        LST_STRING},
        {'+',         LST_VECTOR,        LST_VECTOR,        LST_VECTOR},
        {'+',         LST_QUATERNION,    LST_QUATERNION,    LST_QUATERNION},
        {'+',         LST_LIST,          LST_ANY,           LST_LIST},
        {'+',         LST_ANY,           LST_LIST,          LST_LIST},

        // *
        {'*',         LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {'*',         LST_INTEGER,       LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {'*',         LST_INTEGER,       LST_VECTOR,        LST_VECTOR}, // scale vector
        {'*',         LST_FLOATINGPOINT, LST_INTEGER,       LST_FLOATINGPOINT},
        {'*',         LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {'*',         LST_FLOATINGPOINT, LST_VECTOR,        LST_VECTOR}, // scale vector
        {'*',         LST_VECTOR,        LST_INTEGER,       LST_VECTOR}, // scale vector
        {'*',         LST_VECTOR,        LST_FLOATINGPOINT, LST_VECTOR}, // scale vector
        {'*',         LST_VECTOR,        LST_VECTOR,        LST_FLOATINGPOINT}, // dot product
        {'*',         LST_VECTOR,        LST_QUATERNION,    LST_VECTOR}, // rotate
        {'*',         LST_QUATERNION,    LST_QUATERNION,    LST_QUATERNION}, // merge rotations, effectively

        // /
        {'/',         LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {'/',         LST_INTEGER,       LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {'/',         LST_FLOATINGPOINT, LST_INTEGER,       LST_FLOATINGPOINT},
        {'/',         LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {'/',         LST_VECTOR,        LST_INTEGER,       LST_VECTOR}, // scale vector
        {'/',         LST_VECTOR,        LST_FLOATINGPOINT, LST_VECTOR}, // scale vector
        {'/',         LST_VECTOR,        LST_QUATERNION,    LST_VECTOR}, // rotate by inverse of rside
        {'/',         LST_QUATERNION,    LST_QUATERNION,    LST_QUATERNION}, // rotate by inverse of rside

        // %
        {'%',         LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {'%',         LST_VECTOR,        LST_VECTOR,        LST_VECTOR}, // cross product

        // >
        {'>',         LST_INTEGER,       LST_INTEGER,       LST_BOOLEAN},
        {'>',         LST_INTEGER,       LST_FLOATINGPOINT, LST_BOOLEAN},
        {'>',         LST_FLOATINGPOINT, LST_INTEGER,       LST_BOOLEAN},
        {'>',         LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_BOOLEAN},

        // <
        {'<',         LST_INTEGER,       LST_INTEGER,       LST_BOOLEAN},
        {'<',         LST_INTEGER,       LST_FLOATINGPOINT, LST_BOOLEAN},
        {'<',         LST_FLOATINGPOINT, LST_INTEGER,       LST_BOOLEAN},
        {'<',         LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_BOOLEAN},

        // <=
        {OP_LEQ,      LST_INTEGER,       LST_INTEGER,       LST_BOOLEAN},
        {OP_LEQ,      LST_INTEGER,       LST_FLOATINGPOINT, LST_BOOLEAN},
        {OP_LEQ,      LST_FLOATINGPOINT, LST_INTEGER,       LST_BOOLEAN},
        {OP_LEQ,      LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_BOOLEAN},

        // >=
        {OP_GEQ,      LST_INTEGER,       LST_INTEGER,       LST_BOOLEAN},
        {OP_GEQ,      LST_INTEGER,       LST_FLOATINGPOINT, LST_BOOLEAN},
        {OP_GEQ,      LST_FLOATINGPOINT, LST_INTEGER,       LST_BOOLEAN},
        {OP_GEQ,      LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_BOOLEAN},

        // ==
        {OP_EQ,       LST_INTEGER,       LST_INTEGER,       LST_BOOLEAN},
        {OP_EQ,       LST_INTEGER,       LST_FLOATINGPOINT, LST_BOOLEAN},
        {OP_EQ,       LST_FLOATINGPOINT, LST_INTEGER,       LST_BOOLEAN},
        {OP_EQ,       LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_BOOLEAN},
        {OP_EQ,       LST_VECTOR,        LST_VECTOR,        LST_BOOLEAN},
        {OP_EQ,       LST_QUATERNION,    LST_QUATERNION,    LST_BOOLEAN},
        {OP_EQ,       LST_STRING,        LST_STRING,        LST_BOOLEAN},
        {OP_EQ,       LST_STRING,        LST_KEY,           LST_BOOLEAN},
        {OP_EQ,       LST_KEY,           LST_STRING,        LST_BOOLEAN},
        {OP_EQ,       LST_KEY,           LST_KEY,           LST_BOOLEAN},
        {OP_EQ,       LST_LIST,          LST_LIST,          LST_BOOLEAN},

        // !=
        {OP_NEQ,      LST_INTEGER,       LST_INTEGER,       LST_BOOLEAN},
        {OP_NEQ,      LST_INTEGER,       LST_FLOATINGPOINT, LST_BOOLEAN},
        {OP_NEQ,      LST_FLOATINGPOINT, LST_INTEGER,       LST_BOOLEAN},
        {OP_NEQ,      LST_FLOATINGPOINT, LST_FLOATINGPOINT, LST_BOOLEAN},
        {OP_NEQ,      LST_VECTOR,        LST_VECTOR,        LST_BOOLEAN},
        {OP_NEQ,      LST_QUATERNION,    LST_QUATERNION,    LST_BOOLEAN},
        // This seriously returns an int, see lscript_execute:list_list_operation()
        // and lsa_cmp_lists. OP_NEQ returns `(len(a) - len(b))`.
        // `(list1 != list2)` is not the same as `!(list1 == list2)`.
        {OP_NEQ,      LST_STRING,        LST_STRING,        LST_INTEGER},
        {OP_NEQ,      LST_STRING,        LST_KEY,           LST_INTEGER},
        {OP_NEQ,      LST_KEY,           LST_STRING,        LST_INTEGER},
        {OP_NEQ,      LST_KEY,           LST_KEY,           LST_INTEGER},
        // Similar to above, but the same under LSO2 and Mono
        {OP_NEQ,      LST_LIST,          LST_LIST,          LST_INTEGER},

        // bitwise operators
        {'&',         LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {'|',         LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {'^',         LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {'~',         LST_INTEGER,       LST_NONE,          LST_INTEGER},

        // boolean opeartors
        {'!',         LST_INTEGER,       LST_NONE,          LST_BOOLEAN},
        {OP_BOOLEAN_AND, LST_INTEGER,       LST_INTEGER,       LST_BOOLEAN},
        {OP_BOOLEAN_OR,  LST_INTEGER,       LST_INTEGER,       LST_BOOLEAN},

        // shift operators
        {OP_SHIFT_LEFT,  LST_INTEGER,       LST_INTEGER,       LST_INTEGER},
        {OP_SHIFT_RIGHT, LST_INTEGER,       LST_INTEGER,       LST_INTEGER},

        // end
        {-1,          -1,                -1,                -1},
};

LSLType LSLType::_sTypes[LST_MAX] = { // NOLINT(cert-err58-cpp)
        LSLType(LST_NULL, true),
        LSLType(LST_INTEGER, true),
        LSLType(LST_FLOATINGPOINT, true),
        LSLType(LST_STRING, true),
        LSLType(LST_KEY, true),
        LSLType(LST_VECTOR, true),
        LSLType(LST_QUATERNION, true),
        LSLType(LST_LIST, true),
        LSLType(LST_ERROR, true),
};

bool LSLType::canCoerce(LSLType *to) {
  int i;

  // error type matches anything
  if (getIType() == LST_ERROR || to->getIType() == LST_ERROR)
    return true;

  // if we're already of the target type, then of course we can be used for it
  if (getIType() == to->getIType())
    return true;

  for (i = 0; COERCION_TABLE[i][1] != -1; i++) {
    if (COERCION_TABLE[i][1] == getIType() && COERCION_TABLE[i][0] == to->getIType()) {
      return true;
    }
  }
  return false;
}

class LSLType *LSLType::getResultType(LSLOperator op, LSLType *right) {
  int i;

  // error on either side is always error
  if (getIType() == LST_ERROR || (right != nullptr && right->getIType() == LST_ERROR))
    return TYPE(LST_ERROR);

  if (op == '(') {
    return right;
  }

  // *_ASSIGN is usually just syntactic sugar. `foo *= 3` is the same as `foo = foo * 3`
  int orig_operation = op;
  op = decouple_compound_operation(op);
  bool compound_assignment = (op != orig_operation);

  // go through each entry in the operator result table
  for (i = 0; OPERATOR_RESULTS[i][0] != -1; i++) {

    const auto &operator_params = OPERATOR_RESULTS[i];
    // no operator match
    if (operator_params[0] != op)
      continue;

    // the left side must match our left side
    if (operator_params[1] != getIType() && (operator_params[1] != LST_ANY || getIType() == LST_NULL))
      continue;

    bool match;

    if (right == nullptr)
      // right IS empty and matches nothing
      match = (operator_params[2] == LST_NONE);
    else
      // or right isn't empty and matches our side
      match = ((operator_params[2] == LST_ANY && right->getIType() != LST_NULL) ||
               operator_params[2] == (int) right->getIType());

    // no type match on the operation
    if (!match)
      continue;

    // send back the type
    auto *ret_type = TYPE((LSLIType) operator_params[3]);
    // for compound assignment operators the type of the operation's retval
    // must additionally match the type of the lvalue, or it is not a valid
    // compound assignment.
    // For example, `int_val += 1.0` and `vec *= <1,1,1>` are forbidden.
    // but something like `float_val += 1` is fine.
    if (compound_assignment && ret_type != this) {
      // ... is mostly true, but not entirely. There's one case in LL's compiler
      // (that was probably a mistake) where `int_val *= float_val` is allowed.
      // `int_val = int_val * float_val` is not legal since `int_val` must be promoted
      // to a float which can't be assigned to an int lvalue, but the compound form doesn't
      // even behave as you'd expect.
      // In LSO it behaves the same as `(int_val = (integer)(int_val * float_val)) * 0.0`.
      // In Mono it causes a runtime VM error due to invalid IL if you actually try to use
      // the retval in something like `llOwnerSay((string)(int_val *= float_val))`.
      // For now let's just warn and pretend it returns a float, because it sort of does in LSO.
      if (op == OP_MUL && getIType() == LST_INTEGER && right && right->getIType() == LST_FLOATINGPOINT) {
        return TYPE(LST_FLOATINGPOINT);
      }
      return nullptr;
    }
    return ret_type;
  }
  return nullptr;
}

bool operation_mutates(LSLOperator operation) {
  switch (operation) {
    case '=':
    case OP_PRE_INCR:
    case OP_POST_INCR:
    case OP_PRE_DECR:
    case OP_POST_DECR:
    case OP_ADD_ASSIGN:
    case OP_SUB_ASSIGN:
    case OP_MUL_ASSIGN:
    case OP_DIV_ASSIGN:
    case OP_MOD_ASSIGN:
      return true;
    default:
      return false;
  }
}

const char *operation_str(LSLOperator operation) {
  switch (operation) {
    case 0:
      return "<constant>";
    case '=':
      return "=";
    case OP_NEQ:
      return "!=";
    case OP_EQ:
      return "==";
    case OP_GEQ:
      return ">=";
    case OP_LEQ:
      return "<=";
    case OP_PRE_INCR:
      return "++";
    case OP_PRE_DECR:
      return "--";
    case OP_POST_INCR:
      return "++";
    case OP_POST_DECR:
      return "--";
    case OP_BOOLEAN_AND:
      return "&&";
    case OP_BOOLEAN_OR:
      return "||";
    case OP_SHIFT_LEFT:
      return "<<";
    case OP_SHIFT_RIGHT:
      return ">>";
    case OP_ADD_ASSIGN:
      return "+=";
    case OP_SUB_ASSIGN:
      return "-=";
    case OP_MUL_ASSIGN:
      return "*=";
    case OP_DIV_ASSIGN:
      return "/=";
    case OP_MOD_ASSIGN:
      return "%=";
    case '+':
      return "+";
    case '-':
      return "-";
    case '*':
      return "*";
    case '/':
      return "/";
    case '%':
      return "%";
    case '~':
      return "~";
    case '^':
      return "^";
    case '&':
      return "&";
    case '|':
      return "|";
    case '!':
      return "!";
    case '<':
      return "<";
    case '>':
      return ">";
    case '(':
      return "()";
    default:
      return "<BUSTED>";
  }
}

const char *operation_repr_str(LSLOperator operation) {
  switch(operation) {
    case OP_POST_INCR:
      return "++ (post)";
    case OP_PRE_INCR:
      return "++ (pre)";
    case OP_POST_DECR:
      return "-- (post)";
    case OP_PRE_DECR:
      return "-- (pre)";
    default:
      return operation_str(operation);
  }
}

bool is_cast_legal(LSLIType from, LSLIType to) {
  return LEGAL_CAST_TABLE[from][to] == 1;
}

LSLOperator decouple_compound_operation(LSLOperator operation) {
  // *_ASSIGN is usually just syntactic sugar. `foo *= 3` is the same as `foo = foo * 3`
  switch (operation) {
    case OP_ADD_ASSIGN: operation = OP_PLUS; break;
    case OP_SUB_ASSIGN: operation = OP_MINUS; break;
    case OP_MUL_ASSIGN: operation = OP_MUL; break;
    case OP_DIV_ASSIGN: operation = OP_DIV; break;
    case OP_MOD_ASSIGN: operation = OP_MOD; break;
    default:
      break;
  }
  return operation;
}

}
