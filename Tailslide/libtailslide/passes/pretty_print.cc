#include <iomanip>
#include <cmath>

#include "pretty_print.hh"
#include "../lslmini.hh"
#include "../strings.hh"


namespace Tailslide {

class ScopedTabSetter {
public:
  ScopedTabSetter(PrettyPrintVisitor *visitor, int tabs): _mOldTabs(visitor->mTabs), _mVisitor(visitor) {
    _mVisitor->mTabs = tabs;
  };
  ~ScopedTabSetter() {
    _mVisitor->mTabs = _mOldTabs;
  }
private:
  int _mOldTabs;
  PrettyPrintVisitor *_mVisitor;
};

static bool cast_requires_parens(LSLTypecastExpression *node) {
  auto *child_expr = node->getChildExpr();
  switch (child_expr->getNodeType()) {
    case NODE_CONSTANT:
    case NODE_IDENTIFIER:
      return false;
    default:;
  }

  switch (child_expr->getNodeSubType()) {
    case NODE_PRINT_EXPRESSION:
    case NODE_LVALUE_EXPRESSION:
    case NODE_FUNCTION_EXPRESSION:
    case NODE_LIST_EXPRESSION:
    case NODE_QUATERNION_EXPRESSION:
    case NODE_VECTOR_EXPRESSION:
    case NODE_CONSTANT_EXPRESSION:
      return false;
    default:;
  }
  return true;
}

// Check if this if/for/while/etc. body has braces
static int is_braceless(LSLASTNode *branch) {
  return (branch->getNodeSubType() != NODE_COMPOUND_STATEMENT);
}

void PrettyPrintVisitor::prettifyChildrenSep(LSLASTNode *parent, const char *separator) {
  for (auto *child: *parent) {
    child->visit(this);
    if (child->getNext())
      mStream << separator;
  }
}

// special-case for vectors and quaternions so we don't accidentally output
// expressions in the members in a context where the brackets could be interpreted
// as shift operators
void PrettyPrintVisitor::prettifyCoordinateMembers(LSLASTNode *coord_node) {
  std::stringstream orig_stream(std::move(mStream));
  mStream = std::stringstream();
  prettifyChildrenSep(coord_node, ", ");
  std::string innards {mStream.str()};
  mStream = std::move(orig_stream);
  if (innards.front() == '<')
    mStream << ' ';
  mStream << innards;
  if (innards.back() == '>')
    mStream << ' ';
}

bool PrettyPrintVisitor::visit(LSLASTNode *node) {
  std::stringstream descriptor_sstr;
  descriptor_sstr << "UNKNOWN NODE \"" << node->getNodeName() << "\" @ " << (void *)this << "> */";
  std::string node_descriptor = descriptor_sstr.str();

  doTabs();
  mStream << "/* <" << node_descriptor << "\n";
  visitChildren(node);
  doTabs();
  mStream << "/* </" << node_descriptor << "\n";

  return false;
}

bool PrettyPrintVisitor::visit(LSLIdentifier *id) {
  LSLSymbol *sym = id->getSymbol();
  const char *name = id->getName();
  if (sym != nullptr && sym->getMangledName()
      && ((_mOpts.mangle_local_names && sym->getSubType() == SYM_LOCAL)
          || (_mOpts.mangle_global_names && sym->getSubType() != SYM_LOCAL && sym->getSymbolType() != SYM_FUNCTION)
          || (_mOpts.mangle_func_names && sym->getSymbolType() == SYM_FUNCTION))) {
    if (_mOpts.show_unmangled) {
      mStream << "/*" << name << "*/";
    }
    mStream << sym->getMangledName();
  } else {
    mStream << name;
  }
  return false;
}




//
// Global Stuff
//

bool PrettyPrintVisitor::visit(LSLState *state) {
  doTabs();
  auto *state_id = state->getIdentifier();
  if (!strcmp("default", state_id->getName()))
    mStream << "default";
  else {
    mStream << "state ";
    state_id->visit(this);
  }

  mStream << "\n{\n";

  // Print all of the event handlers under this state
  {
    ScopedTabSetter setter(this, mTabs + 1);
    prettifyChildrenSep(state->getEventHandlers(), "\n");
  }
  doTabs();
  mStream << "}\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLEventHandler *handler) {
  doTabs();

  // event name
  handler->getIdentifier()->visit(this);
  // params
  mStream << '(';
  handler->getArguments()->visit(this);
  mStream << ")\n";
  // body
  handler->getStatements()->visit(this);
  return false;
}

bool PrettyPrintVisitor::visit(LSLGlobalVariable *glob_var) {
  doTabs();

  LSLASTNode *id = glob_var->getIdentifier();
  id->getType()->visit(this);
  mStream << ' ';
  id->visit(this);

  if (auto *assignable = glob_var->getInitializer()) {
    mStream << " = ";
    assignable->visit(this);
  }
  mStream << ";\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLGlobalFunction *glob_func) {
  doTabs();
  // print the return type if we have one
  auto *id = glob_func->getIdentifier();
  if (id->getIType() != LST_NULL) {
    id->getType()->visit(this);
    mStream << ' ';
  }

  id->visit(this);
  mStream << '(';
  glob_func->getArguments()->visit(this);
  mStream << ")\n";
  glob_func->getStatements()->visit(this);
  mStream << '\n';
  return false;
}

bool PrettyPrintVisitor::visit(LSLParamList *params) {
  for (auto *child_node : *params) {
    child_node->getType()->visit(this);
    mStream << ' ';
    child_node->visit(this);
    if (child_node->getNext())
      mStream << ", ";
  }
  return false;
}





//
// Statements
//

bool PrettyPrintVisitor::visit(LSLNopStatement *nop_stmt) {
  doTabs();
  mStream << ";\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLExpressionStatement *expr_stmt) {
  doTabs();
  expr_stmt->getExpr()->visit(this);
  mStream << ";\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLCompoundStatement *compound_stmt) {
  doTabs();
  mStream << "{\n";

  {
    ScopedTabSetter setter(this, mTabs + 1);
    visitChildren(compound_stmt);
  }

  doTabs();
  mStream << "}\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLDeclaration *decl_stmt) {
  doTabs();

  LSLASTNode *id = decl_stmt->getIdentifier();
  id->getType()->visit(this);
  mStream << ' ';
  id->visit(this);

  if (auto *assignable = decl_stmt->getInitializer()) {
    mStream << " = ";
    assignable->visit(this);
  }
  mStream << ";\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLStateStatement *state_stmt) {
  doTabs();
  mStream << "state ";
  state_stmt->getIdentifier()->visit(this);
  mStream << ";\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLLabel *label_stmt) {
  doTabs();
  mStream << '@';
  label_stmt->getIdentifier()->visit(this);
  mStream << ";\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLJumpStatement *jump_stmt) {
  doTabs();
  mStream << "jump ";
  jump_stmt->getIdentifier()->visit(this);
  mStream << ";\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLReturnStatement *ret_stmt) {
  doTabs();
  mStream << "return";
  if (auto *ret_expr = ret_stmt->getExpr()) {
    mStream << ' ';
    ret_expr->visit(this);
  }
  mStream << ";\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLIfStatement *if_stmt) {
  // Don't indent the "if" in "else if", but _do_ invent if we're in the true branch.
  // Basically, perverse things like:
  // if (foo)
  //     if (bar)
  //         baz();
  LSLASTNode * parent = if_stmt->getParent();
  if (parent->getNodeSubType() != NODE_IF_STATEMENT || parent->getChild(1) == if_stmt)
    doTabs();

  mStream << "if (";
  if_stmt->getCheckExpr()->visit(this);
  mStream << ")\n";
  // Only increase indentation of the child if this is a braceless `if` branch
  {
    LSLASTNode *true_branch = if_stmt->getTrueBranch();
    ScopedTabSetter setter(this, mTabs + is_braceless(true_branch));
    true_branch->visit(this);
  }

  if (auto *false_branch= if_stmt->getFalseBranch()) {
    doTabs();
    mStream << "else";
    bool chained_if = false_branch->getNodeSubType() == NODE_IF_STATEMENT;
    if (chained_if)
      mStream << ' ';
    else
      mStream << '\n';

    {
      ScopedTabSetter setter(this, mTabs + (is_braceless(false_branch) && !chained_if));
      false_branch->visit(this);
    }
  }
  return false;
}

bool PrettyPrintVisitor::visit(LSLWhileStatement *while_stmt) {
  doTabs();
  mStream << "while (";
  while_stmt->getCheckExpr()->visit(this);
  mStream << ")\n";
  {
    auto *body = while_stmt->getBody();
    ScopedTabSetter setter(this, mTabs + is_braceless(body));
    body->visit(this);
  }
  return false;
}

bool PrettyPrintVisitor::visit(LSLDoStatement *do_stmt) {
  doTabs();
  mStream << "do\n";
  {
    auto *body = do_stmt->getBody();
    ScopedTabSetter setter(this, mTabs + is_braceless(body));
    body->visit(this);
  }
  doTabs();
  mStream << "while(";
  do_stmt->getCheckExpr()->visit(this);
  mStream << ");\n";
  return false;
}

bool PrettyPrintVisitor::visit(LSLForStatement *for_stmt) {
  doTabs();
  mStream << "for (";
  prettifyChildrenSep(for_stmt->getInitExprs(), ", ");
  mStream << "; ";
  for_stmt->getCheckExpr()->visit(this);
  mStream << "; ";
  prettifyChildrenSep(for_stmt->getIncrExprs(), ", ");
  mStream << ")\n";

  {
    auto *body = for_stmt->getBody();
    ScopedTabSetter setter(this, mTabs + is_braceless(body));
    body->visit(this);
  }
  return false;
}



//
// Expressions
//

bool PrettyPrintVisitor::visit(LSLParenthesisExpression *parens_expr) {
  mStream << '(';
  parens_expr->getChildExpr()->visit(this);
  mStream << ')';
  return false;
}

bool PrettyPrintVisitor::visit(LSLUnaryExpression *unary_expr) {
  auto *expr = unary_expr->getChildExpr();
  LSLOperator operation = unary_expr->getOperation();
  if (operation == OP_POST_INCR || operation == OP_POST_DECR) {
    expr->visit(this);
    mStream << operation_str(operation);
  } else if (operation == '-') {
    // Make sure we don't end up with `--2147483648` on underflow...
    bool need_parens = false;
    if (expr->getChild(0)->getNodeSubType() == NODE_INTEGER_CONSTANT) {
      need_parens = ((LSLIntegerConstant *) expr->getChild(0))->getValue() < 0;
    }
    mStream << '-';
    if (need_parens)
      mStream << '(';
    expr->visit(this);
    if (need_parens)
      mStream << ')';
  } else {
    mStream << operation_str(operation);
    expr->visit(this);
  }
  return false;
}

bool PrettyPrintVisitor::visit(LSLBinaryExpression *bin_expr) {
  bin_expr->getLHS()->visit(this);
  mStream << ' ' << operation_str(bin_expr->getOperation()) << ' ';
  bin_expr->getRHS()->visit(this);
  return false;
}

bool PrettyPrintVisitor::visit(LSLLValueExpression *lvalue) {
  lvalue->getIdentifier()->visit(this);
  if (auto *member = lvalue->getMember()) {
    mStream << '.';
    member->visit(this);
  }
  return false;
}

bool PrettyPrintVisitor::visit(LSLConstantExpression *constant_expr) {
  constant_expr->getConstantValue()->visit(this);
  return false;
}

bool PrettyPrintVisitor::visit(LSLFunctionExpression *func_expr) {
  func_expr->getIdentifier()->visit(this);
  mStream << '(';
  prettifyChildrenSep(func_expr->getArguments(), ", ");
  mStream << ')';
  return false;
}

bool PrettyPrintVisitor::visit(LSLTypecastExpression *cast_expr) {
  mStream << '(';
  cast_expr->getType()->visit(this);
  mStream << ')';
  bool need_parens = cast_requires_parens(cast_expr);
  if (need_parens)
    mStream << '(';
  cast_expr->getChildExpr()->visit(this);
  if (need_parens)
    mStream << ')';
  return false;
}

bool PrettyPrintVisitor::visit(LSLVectorExpression *vec_expr) {
  mStream << '<';
  prettifyCoordinateMembers(vec_expr);
  mStream << '>';
  return false;
}

bool PrettyPrintVisitor::visit(LSLQuaternionExpression *quat_expr) {
  mStream << '<';
  prettifyCoordinateMembers(quat_expr);
  mStream << '>';
  return false;
}

static std::string format_float(double v) {
  if (!std::isfinite(v)) {
    // this really should never happen, as LSL doesn't have a NaN literal and we should
    // refuse to fold constants that result in NaN, but as a last resort try typecasting
    // from string (which will not be legal in a global context.)
    if (std::isnan(v)) {
      return "((float)\"nan\")";
    } else if (v > 0.0) {
      // about as close as we can get to an inf literal in LSL, exponent places it outside
      // the range of both float and double width.
      // oddly enough these _are_ allowed as initializers for globals under LSO, because the fault
      // register gets cleared at the end of compilation. The value just can't ever be used.
      return "2.0e+999";
    } else {
      return "-2.0e+999";
    }
  }
  char pretty_buf[256];
  snprintf(pretty_buf, 256, "%#.6g", v);
  return pretty_buf;
}

bool PrettyPrintVisitor::visit(LSLVectorConstant *vec_const) {
  const Vector3 *v = vec_const->getValue();
  assert (v != nullptr);
  mStream << '<';
  mStream << format_float(v->x);
  mStream << ", ";
  mStream << format_float(v->y);
  mStream << ", ";
  mStream << format_float(v->z);
  mStream << ">";
  return false;
}

bool PrettyPrintVisitor::visit(LSLQuaternionConstant *quat_const) {
  const Quaternion *v = quat_const->getValue();
  assert (v != nullptr);
  mStream << '<';
  mStream << format_float(v->x);
  mStream << ", ";
  mStream << format_float(v->y);
  mStream << ", ";
  mStream << format_float(v->z);
  mStream << ", ";
  mStream << format_float(v->s);
  mStream << ">";
  return false;
}

bool PrettyPrintVisitor::visit(LSLListExpression *list_expr) {
  mStream << '[';
  prettifyChildrenSep(list_expr, ", ");
  mStream << ']';
  return false;
}

bool PrettyPrintVisitor::visit(LSLListConstant *list_const) {
  mStream << '[';
  prettifyChildrenSep(list_const, ", ");
  mStream << ']';
  return false;
}

bool PrettyPrintVisitor::visit(LSLIntegerConstant *int_const) {
  mStream << int_const->getValue();
  return false;
}

bool PrettyPrintVisitor::visit(LSLFloatConstant *float_const) {
  mStream << format_float(float_const->getValue());
  return false;
}

bool PrettyPrintVisitor::visit(LSLStringConstant *str_const) {
  mStream << '"' << escape_string(str_const->getValue()) << '"';
  return false;
}

bool PrettyPrintVisitor::visit(LSLKeyConstant *key_const) {
  return visit((LSLStringConstant *) key_const);
}

bool PrettyPrintVisitor::visit(LSLPrintExpression *print_expr) {
  mStream << "print(";
  print_expr->getChildExpr()->visit(this);
  mStream << ')';
  return false;
}

bool PrettyPrintVisitor::visit(LSLType *type) {
  if (type->getIType() != LST_NULL) {
    mStream << type->getNodeName();
  }
  return false;
}

bool PrettyPrintVisitor::visit(LSLScript *script) {
  return true;
}

void PrettyPrintVisitor::doTabs() {
  for(int i=0; i < mTabs; ++i) {
    mStream << "    ";
  }
}

}
