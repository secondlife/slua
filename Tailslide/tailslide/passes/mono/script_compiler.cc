#include "script_compiler.hh"
#include "../desugaring.hh"

namespace Tailslide {

static const char *CIL_USERSCRIPT_CLASS = "class [LslUserScript]LindenLab.SecondLife.LslUserScript";
static const char *CIL_LSL_RUNTIME_CLASS = "class [LslLibrary]LindenLab.SecondLife.LslRunTime";
static const char *CIL_LSL_LIBRARY_CLASS = "class [LslLibrary]LindenLab.SecondLife.Library";
static const std::string CIL_LIST_INITIALIZER = std::string("call ") + CIL_TYPE_NAMES[LST_LIST] + " " + CIL_USERSCRIPT_CLASS + "::CreateList()";

/// LSL name -> CIL name
static std::unordered_map<std::string, std::string> CIL_HANDLER_NAMES {
    {"state_entry", "state_entry"},
    {"state_exit", "state_exit"},
    {"touch_start", "touch_start"},
    {"touch", "touch"},
    {"touch_end", "touch_end"},
    {"collision_start", "collision_start"},
    {"collision", "collision"},
    {"collision_end", "collision_end"},
    {"land_collision_start", "land_collision_start"},
    {"land_collision", "land_collision"},
    {"land_collision_end", "land_collision_end"},
    {"timer", "timer"},
    {"listen", "chat"},
    {"on_rez", "rez"},
    {"sensor", "sensor"},
    {"no_sensor", "no_sensor"},
    {"control", "control"},
    {"money", "money"},
    {"email", "email"},
    {"at_target", "at_target"},
    {"not_at_target", "not_at_target"},
    {"at_rot_target", "at_rot_target"},
    {"not_at_rot_target", "not_at_rot_target"},
    {"run_time_permissions", "run_time_perms"},
    {"changed", "changed"},
    {"attach", "attach"},
    {"dataserver", "dataserver"},
    {"link_message", "link_message"},
    {"moving_start", "moving_start"},
    {"moving_end", "moving_end"},
    {"object_rez", "object_rez"},
    {"remote_data", "remote_event"},
    {"http_response", "http_response"},
    {"http_request", "http_request"},
    {"transaction_result", "transaction_result"},
    {"path_update", "path_update"},
    {"experience_permissions", "experience_permissions"},
    {"experience_permissions_denied", "experience_permissions_denied"},
    {"linkset_data", "linkset_data"}
};


/// compiles to the text representation of CIL assembly, based on analysis
/// of the output of LL's lscript LSL->CIL compiler circa 2009 (when public
/// lscript source had its last functional update.)
bool MonoScriptCompiler::visit(LSLScript *script) {
  LLConformantDeSugaringVisitor de_sugaring_visitor(_mAllocator, true);
  script->visit(&de_sugaring_visitor);

  MonoResourceVisitor resource_visitor(&_mSymData);
  script->visit(&resource_visitor);
  _mScriptClassName = "LSL_00000000_0000_0000_0000_000000000000";

  mCIL << ".assembly extern mscorlib {.ver 1:0:5000:0}\n"
          ".assembly extern LslLibrary {.ver 0:1:0:0}\n"
          ".assembly extern LslUserScript {.ver 0:1:0:0}\n"
          ".assembly extern ScriptTypes {.ver 0:1:0:0}\n"
          ".assembly '" << _mScriptClassName << "' {.ver 0:0:0:0}\n"
          ".class public auto ansi serializable beforefieldinit " << _mScriptClassName << " extends " << CIL_USERSCRIPT_CLASS << "\n"
          "{\n";

  // declare the global variables first
  auto *globals = script->getGlobals();
  for (auto *global : *globals) {
    if (global->getNodeType() != NODE_GLOBAL_VARIABLE)
      continue;
    auto *id = ((LSLGlobalVariable *) global)->getIdentifier();
    mCIL << ".field public " << CIL_TYPE_NAMES[id->getIType()] << " '" << id->getName() << "'\n";
  }

  // now define the globals' values in the ctor for the script
  mCIL << ".method public hidebysig specialname rtspecialname instance default void .ctor () cil managed\n"
          "{\n"
          ".maxstack 500\n";

  _mInGlobalExpr = true;
  for (auto *global: *globals) {
    if (global->getNodeType() != NODE_GLOBAL_VARIABLE)
      continue;
    global->visit(this);
  }
  _mInGlobalExpr = false;

  // call the base constructor for the script class and return
  mCIL << "ldarg.0\n"
          "call instance void " << CIL_USERSCRIPT_CLASS << "::.ctor()\n"
          "ret\n"
          "}\n";

  // now go over the globals _again_ to pick up all the functions
  for (auto *global : *globals) {
    if (global->getNodeType() != NODE_GLOBAL_FUNCTION)
      continue;
    global->visit(this);
  }

  // now look at the event handlers
  script->getStates()->visit(this);

  mCIL << "}\n";

  return false;
}

bool MonoScriptCompiler::visit(LSLGlobalVariable *glob_var) {
  // push a reference to `this` for the later stfld
  mCIL << "ldarg.0\n";
  auto *sym = glob_var->getSymbol();
  if (auto *initializer = glob_var->getInitializer()) {
    initializer->visit(this);
  } else {
    pushConstant(sym->getType()->getDefaultValue());
  }
  mCIL << "stfld " << getGlobalVarSpecifier(sym) << "\n";
  return false;
}

/// Push things that need to be on the stack before we can read or write the ultimate
/// target of the lvalue (reference to `this`, the containing object for accessors, etc.)
void MonoScriptCompiler::pushLValueContainer(LSLLValueExpression *lvalue) {
  auto *sym = lvalue->getSymbol();
  if (sym->getSubType() == SYM_GLOBAL) {
    // push a reference to `this` since this is an attribute of the class
    // and we'll need to `ldfld`
    mCIL << "ldarg.0\n";
  }
  // have an accessor, we need to push the containing object's address!
  if (lvalue->getMember()) {
    if (sym->getSubType() == SYM_GLOBAL) {
      mCIL << "ldflda " << getGlobalVarSpecifier(sym) << "\n";
    } else if (sym->getSubType() == SYM_LOCAL) {
      mCIL << "ldloca.s " << _mSymData[sym].index << "\n";
    } else {
      // event or function param
      mCIL << "ldarga.s '" << sym->getName() << "'\n";
    }
  }
}

/// push the actual value of an lvalue onto the stack
void MonoScriptCompiler::pushLValue(LSLLValueExpression *lvalue) {
  pushLValueContainer(lvalue);
  auto *sym = lvalue->getSymbol();
  if (lvalue->getMember()) {
    // accessor case, containing object is already on the stack and
    // we just have to load the field.
    mCIL << "ldfld " << getLValueAccessorSpecifier(lvalue) << "\n";
  } else {
    if (sym->getSubType() == SYM_GLOBAL) {
      // LslUserScript `this` should already on the stack, load the given field from `this`.
      mCIL << "ldfld " << getGlobalVarSpecifier(sym) << "\n";
    } else if (sym->getSubType() == SYM_LOCAL) {
      // must be a local, reference by index
      // Seems that UThreadInjector may rewrite these to ldloc.0, ldloc.1, ...
      // but we aren't aiming for conformance with its output, only lscript's.
      mCIL << "ldloc.s " << _mSymData[sym].index << "\n";
    } else {
      // event or function param
      mCIL << "ldarg.s '" << sym->getName() << "'\n";
    }
  }
}

void MonoScriptCompiler::pushConstant(LSLConstant *cv) {
  if (!cv)
    return;
  switch(cv->getIType()) {
    case LST_INTEGER: {
      auto int_val = ((LSLIntegerConstant *) cv)->getValue();
      // These values have a single-byte push form
      if (int_val >= 0 && int_val <= 8)
        mCIL << "ldc.i4." << int_val << '\n';
      else if (int_val == -1)
        mCIL << "ldc.i4.m1\n";
      // can use the single-byte operand version of ldc.i4
      else if (int_val >= -128 && int_val <= 127)
        mCIL << "ldc.i4.s " << int_val << '\n';
      else
        mCIL << "ldc.i4 " << int_val << "\n";
      return;
    }
    case LST_FLOATINGPOINT:
      pushFloatLiteral(((LSLFloatConstant *) cv)->getValue());
      return;
    case LST_STRING:
      mCIL << "ldstr \"" << escape_string(((LSLStringConstant *) cv)->getValue()) << "\"\n";
      return;
    case LST_KEY:
      mCIL << "ldstr \"" << escape_string(((LSLKeyConstant *) cv)->getValue()) << "\"\n";
      mCIL << "call " << CIL_TYPE_NAMES[LST_KEY] << " " << CIL_USERSCRIPT_CLASS << "::'CreateKey'(string)\n";
      return;
    case LST_VECTOR: {
      auto *vec_val = ((LSLVectorConstant *) cv)->getValue();
      pushFloatLiteral(vec_val->x);
      pushFloatLiteral(vec_val->y);
      pushFloatLiteral(vec_val->z);
      mCIL << "call " << CIL_TYPE_NAMES[LST_VECTOR] << " " << CIL_USERSCRIPT_CLASS << "::'CreateVector'(float32, float32, float32)\n";
      return;
    }
    case LST_QUATERNION: {
      auto *vec_val = ((LSLQuaternionConstant *) cv)->getValue();
      pushFloatLiteral(vec_val->x);
      pushFloatLiteral(vec_val->y);
      pushFloatLiteral(vec_val->z);
      pushFloatLiteral(vec_val->s);
      mCIL << "call " << CIL_TYPE_NAMES[LST_QUATERNION] << " " << CIL_USERSCRIPT_CLASS << "::'CreateQuaternion'(float32, float32, float32, float32)\n";
      return;
    }
    case LST_LIST: {
      // only know how to write the default empty list as a constant
      assert(!((LSLListConstant *) cv)->getLength());
      mCIL << "call " << CIL_TYPE_NAMES[LST_LIST] << " " << CIL_USERSCRIPT_CLASS << "::CreateList()\n";
      return;
    }
    default:
      assert(0);
  }
}

void MonoScriptCompiler::pushLiteral(LSLConstant *cv) {
  if (cv && cv->getIType() == LST_INTEGER) {
    mCIL << "ldc.i4 " << ((LSLIntegerConstant *) cv)->getValue() << "\n";
    return;
  }
  pushConstant(cv);
}

/// used for a number of cases, including pushing float constants and vec/quat components
void MonoScriptCompiler::pushFloatLiteral(double value) {
  // pushed as a double for some reason
  // use the binary hex form specified in the ECMA standard to preserve precision
  auto d_val = value;
  auto *b_val = reinterpret_cast<uint8_t *>(&d_val);
  // enough room for all of the octets plus null. much less annoying than
  // the stringstream form and no C++20 so we don't have std::format
  const size_t s_val_len = 3 * 8;
  char s_val[s_val_len] = {0};
  snprintf(
      (char *)&s_val,
      s_val_len,
      "%02x %02x %02x %02x %02x %02x %02x %02x",
      b_val[0], b_val[1], b_val[2], b_val[3], b_val[4], b_val[5], b_val[6], b_val[7]
  );
  mCIL << "ldc.r8 (" << (const char*)&s_val << ")\n";
}

void MonoScriptCompiler::storeToLValue(LSLLValueExpression *lvalue, bool push_result) {
  auto *sym = lvalue->getSymbol();
  // coordinate accessor case
  if (lvalue->getMember()) {
    mCIL << "stfld " << getLValueAccessorSpecifier(lvalue) << "\n";
    // Expression assignments need to return their result, load what we just stored onto the stack
    // TODO: This seems really wasteful in many cases, but this is how LL's compiler does it.
    //  I guess `dup` isn't an option because of the `this` reference, but wouldn't creating a
    //  scratch local be better?
    if (push_result)
      pushLValue(lvalue);
  } else if (sym->getSubType() == SYM_GLOBAL) {
    mCIL << "stfld " << getGlobalVarSpecifier(lvalue->getSymbol()) << "\n";
    // same caveat as above
    if (push_result)
      pushLValue(lvalue);
  } else {
    // We can avoid reloading the lvalue from its storage container in these cases by just duplicating
    // the result of the expression on the stack. All we need on the stack for these stores is the value.
    if (push_result)
      mCIL << "dup\n";

    if (sym->getSubType() == SYM_LOCAL) {
      mCIL << "stloc.s " << _mSymData[sym].index << "\n";
    } else {
      // event or function param
      mCIL << "starg.s '" << sym->getName() << "'\n";
    }
  }
}

void MonoScriptCompiler::castTopOfStack(LSLIType from_type, LSLIType to_type) {
  if (from_type == to_type)
    return;

  switch(to_type) {
    case LST_INTEGER:
      switch(from_type) {
        case LST_FLOATINGPOINT:
          mCIL << "call int32 " << CIL_LSL_RUNTIME_CLASS << "::ToInteger(float32)\n";
          return;
        case LST_STRING:
          mCIL << "call int32 " << CIL_LSL_RUNTIME_CLASS << "::StringToInt(string)\n";
          return;
        default:
          assert(0);
      }
    case LST_FLOATINGPOINT:
      switch(from_type) {
        case LST_INTEGER:
          mCIL << "conv.r8\n";
          return;
        case LST_STRING:
          mCIL << "call float32 " << CIL_LSL_RUNTIME_CLASS << "::StringToFloat(string)\n";
          return;
        default:
          assert(0);
      }
    case LST_STRING:
      switch (from_type) {
        case LST_LIST:
          mCIL << "call string " << CIL_LSL_RUNTIME_CLASS << "::ListToString(" << CIL_TYPE_NAMES[LST_LIST] << ")\n";
          return;
        case LST_INTEGER:
          mCIL << "call string class [mscorlib]System.Convert::ToString(" << CIL_TYPE_NAMES[LST_INTEGER] << ")\n";
          return;
        case LST_FLOATINGPOINT:
          mCIL << "call string " << CIL_LSL_RUNTIME_CLASS << "::'ToString'(" << CIL_TYPE_NAMES[from_type] << ")\n";
          return;
        default:
          mCIL << "call string " << CIL_USERSCRIPT_CLASS << "::'ToString'(" << CIL_VALUE_TYPE_NAMES[from_type] << ")\n";
          return;
      }
    case LST_KEY:
      if (from_type == LST_STRING)
        mCIL << "call " << CIL_TYPE_NAMES[LST_KEY] << " " << CIL_USERSCRIPT_CLASS << "::'CreateKey'(string)\n";
      return;
    case LST_VECTOR:
      if (from_type == LST_STRING)
        mCIL << "call " << CIL_TYPE_NAMES[LST_VECTOR] << " " << CIL_USERSCRIPT_CLASS << "::'ParseVector'(string)\n";
      return;
    case LST_QUATERNION:
      if (from_type == LST_STRING)
        mCIL << "call " << CIL_TYPE_NAMES[LST_QUATERNION] << " " << CIL_USERSCRIPT_CLASS << "::'ParseQuaternion'(string)\n";
      return;
    case LST_LIST:
      // All casts to list are the same, box if necessary and then `CreateList(object)`.
      mCIL << CIL_BOXING_INSTRUCTIONS[from_type];
      mCIL << "call " << CIL_TYPE_NAMES[LST_LIST] << " " << CIL_USERSCRIPT_CLASS << "::CreateList(object)\n";
      return;
    default:
      assert(0);
  }
}

std::string MonoScriptCompiler::getGlobalVarSpecifier(LSLSymbol *sym) {
  std::stringstream ss;
  ss << CIL_TYPE_NAMES[sym->getIType()] << " " << _mScriptClassName << "::'" << sym->getName() << "'";
  return ss.str();
}

/// return a specifier that allows referencing the field for the accessor with ldfld or stfld
std::string MonoScriptCompiler::getLValueAccessorSpecifier(LSLLValueExpression *lvalue) {
  auto *accessor_type_name = CIL_TYPE_NAMES[lvalue->getIType()];
  auto *obj_type_name = CIL_TYPE_NAMES[lvalue->getSymbol()->getIType()];
  auto *accessor_name = lvalue->getMember()->getName();
  return std::string(accessor_type_name) + " " + obj_type_name + "::" + accessor_name;
}



bool MonoScriptCompiler::visit(LSLEventHandler *handler) {
  // Need a reference to the state this belongs to for conformant name mangling
  // We're parented to a node list which is parented to the state.
  auto *state_sym = handler->getParent()->getParent()->getSymbol();
  mCIL << ".method public hidebysig instance default void e" << state_sym->getName() << CIL_HANDLER_NAMES[handler->getSymbol()->getName()];
  // parameter list will be handled by `buildFunction()`
  buildFunction(handler);
  return false;
}

bool MonoScriptCompiler::visit(LSLGlobalFunction *glob_func) {
  auto *node_sym = glob_func->getSymbol();
  mCIL << ".method public hidebysig instance default " << CIL_TYPE_NAMES[glob_func->getIdentifier()->getIType()]
       << " 'g" << node_sym->getName() << "'";
  buildFunction(glob_func);
  return false;
}

void MonoScriptCompiler::buildFunction(LSLASTNode *func) {
  // write out the parameter list
  auto *func_sym = func->getSymbol();
  auto *func_decl = func_sym->getFunctionDecl();
  mCIL << "(";
  for (auto *func_param : *func_decl) {
    auto *param_sym = func_param->getSymbol();
    mCIL << CIL_TYPE_NAMES[param_sym->getIType()] << " '" << param_sym->getName() << "'";
    if (func_param->getNext())
      mCIL << ", ";
  }
  mCIL << ") cil managed\n";
  mCIL << "{\n";
  mCIL << ".maxstack 500\n";
  std::string locals_string;
  for (auto param_type : _mSymData[func_sym].locals) {
    if (!locals_string.empty()) {
      locals_string += ", ";
    }
    locals_string += CIL_TYPE_NAMES[param_type];
  }
  if (!locals_string.empty())
    mCIL << ".locals init (" << locals_string << ")\n";
  _mCurrentFuncSym = func->getSymbol();
  visitChildren(func);
  _mCurrentFuncSym = nullptr;
  if (!func_sym->getAllPathsReturn())
    mCIL << "ret\n";
  mCIL << "}\n";
}



bool MonoScriptCompiler::visit(LSLDeclaration *decl_stmt) {
  auto *sym = decl_stmt->getSymbol();
  if (auto *initializer = decl_stmt->getInitializer()) {
    initializer->visit(this);
  } else {
    pushConstant(sym->getType()->getDefaultValue());
  }
  mCIL << "stloc.s " << _mSymData[sym].index << "\n";
  return false;
}

bool MonoScriptCompiler::visit(LSLExpressionStatement *expr_stmt) {
  auto *expr = expr_stmt->getExpr();
  expr->visit(this);
  if (expr->getIType() && !_mPushOmitted)
    mCIL << "pop\n";
  _mPushOmitted = false;
  return false;
}

bool MonoScriptCompiler::visit(LSLReturnStatement *ret_stmt) {
  if (auto *expr = ret_stmt->getExpr())
    expr->visit(this);
  mCIL << "ret\n";
  return false;
}

bool MonoScriptCompiler::visit(LSLLabel *label_stmt) {
  // TODO: right now this roughly matches LL's behavior, but label names
  //  should be mangled to prevent collisions.
  mCIL << "'ul" << label_stmt->getSymbol()->getName() << "':\n";
  return false;
}

bool MonoScriptCompiler::visit(LSLJumpStatement*jump_stmt) {
  // TODO: right now this roughly matches LL's behavior, but label names
  //  should be mangled to prevent collisions.
  mCIL << "br 'ul" << jump_stmt->getSymbol()->getName() << "'\n";
  return false;
}

bool MonoScriptCompiler::visit(LSLIfStatement*if_stmt) {
  auto jump_past_true_num = _mJumpNum++;
  uint32_t jump_past_false_num = 0;
  auto *false_node = if_stmt->getFalseBranch();
  if (false_node)
    jump_past_false_num = _mJumpNum++;

  if_stmt->getCheckExpr()->visit(this);
  mCIL << "brfalse LabelTempJump" << jump_past_true_num << "\n";
  if_stmt->getTrueBranch()->visit(this);
  if (false_node) {
    mCIL << "br LabelTempJump" << jump_past_false_num << "\n";
    mCIL << "LabelTempJump" << jump_past_true_num << ":\n";
    false_node->visit(this);
    mCIL << "LabelTempJump" << jump_past_false_num << ":\n";
  } else {
    mCIL << "LabelTempJump" << jump_past_true_num << ":\n";
  }
  return false;
}

bool MonoScriptCompiler::visit(LSLForStatement*for_stmt) {
  // execute instructions to initialize vars
  for(auto *init_expr : *for_stmt->getInitExprs()) {
    init_expr->visit(this);
    if (init_expr->getIType() && !_mPushOmitted)
      mCIL << "pop\n";
    _mPushOmitted = false;
  }
  auto jump_to_start_num = _mJumpNum++;
  auto jump_to_end_num = _mJumpNum++;
  mCIL << "LabelTempJump" << jump_to_start_num << ":\n";
  // run the check expression, exiting the loop if it fails
  for_stmt->getCheckExpr()->visit(this);
  mCIL << "brfalse LabelTempJump" << jump_to_end_num << "\n";
  // run the body of the loop
  for_stmt->getBody()->visit(this);
  // run the increment expressions
  for(auto *incr_expr : *for_stmt->getIncrExprs()) {
    incr_expr->visit(this);
    if (incr_expr->getIType() && !_mPushOmitted)
      mCIL << "pop\n";
    _mPushOmitted = false;
  }
  // jump back up to the check expression at the top
  mCIL << "br LabelTempJump" << jump_to_start_num << "\n";
  mCIL << "LabelTempJump" << jump_to_end_num << ":\n";
  return false;
}

bool MonoScriptCompiler::visit(LSLWhileStatement*while_stmt) {
  auto jump_to_start_num = _mJumpNum++;
  auto jump_to_end_num = _mJumpNum++;
  mCIL << "LabelTempJump" << jump_to_start_num << ":\n";
  // run the check expression, exiting the loop if it fails
  while_stmt->getCheckExpr()->visit(this);
  mCIL << "brfalse LabelTempJump" << jump_to_end_num << "\n";
  // run the body of the loop
  while_stmt->getBody()->visit(this);
  // jump back up to the check expression at the top
  mCIL << "br LabelTempJump" << jump_to_start_num << "\n";
  mCIL << "LabelTempJump" << jump_to_end_num << ":\n";
  return false;
}

bool MonoScriptCompiler::visit(LSLDoStatement*do_stmt) {
  auto jump_to_start_num = _mJumpNum++;
  mCIL << "LabelTempJump" << jump_to_start_num << ":\n";
  // run the body of the loop
  do_stmt->getBody()->visit(this);
  // run the check expression, jumping back up if it succeeds
  do_stmt->getCheckExpr()->visit(this);
  mCIL << "brtrue LabelTempJump" << jump_to_start_num << "\n";
  return false;
}

bool MonoScriptCompiler::visit(LSLStateStatement *state_stmt) {
  mCIL << "ldarg.0\n"
       << "ldstr \"" << escape_string(state_stmt->getSymbol()->getName()) << "\"\n"
       << "call instance void " << CIL_USERSCRIPT_CLASS << "::ChangeState(string)\n";
  pushConstant(_mCurrentFuncSym->getType()->getDefaultValue());
  mCIL << "ret\n";
  return false;
}



bool MonoScriptCompiler::visit(LSLConstantExpression *constant_expr) {
  pushLiteral(constant_expr->getConstantValue());
  return false;
}

bool MonoScriptCompiler::visit(LSLTypecastExpression *cast_expr) {
  auto *child_expr = cast_expr->getChildExpr();
  assert(child_expr);
  child_expr->visit(this);
  castTopOfStack(child_expr->getIType(), cast_expr->getIType());
  return false;
}

bool MonoScriptCompiler::visit(LSLBoolConversionExpression *bool_expr) {
  auto *child_expr = bool_expr->getChildExpr();
  auto type = child_expr->getIType();
  child_expr->visit(this);
  switch(type) {
    case LST_INTEGER:
      return false;
    case LST_FLOATINGPOINT:
      pushConstant(TYPE(LST_FLOATINGPOINT)->getDefaultValue());
      mCIL << "ceq\n"
           // TODO: LL's compiler does this, is it necessary?
           << "ldc.i4.0\n"
           << "ceq\n";
      return false;
    case LST_STRING:
      pushConstant(TYPE(LST_STRING)->getDefaultValue());
      mCIL << "call bool string::op_Equality(string, string)\n"
           // TODO: LL's compiler does this, is it necessary?
           << "ldc.i4.0\n"
           << "ceq\n";
      return false;
    case LST_VECTOR:
    case LST_QUATERNION:
      pushConstant(TYPE(type)->getDefaultValue());
      mCIL << "call bool " << CIL_USERSCRIPT_CLASS << "::'Equals'(" << CIL_TYPE_NAMES[type] << ", " << CIL_TYPE_NAMES[type] << ")\n"
           // TODO: LL's compiler does this, is it necessary?
           << "ldc.i4.0\n"
           << "ceq\n";
      return false;
    case LST_KEY:
      mCIL << "call bool " << CIL_USERSCRIPT_CLASS <<"::'IsNonNullUuid'(" << CIL_TYPE_NAMES[LST_KEY] << ")\n";
      return false;
    case LST_LIST:
      pushConstant(TYPE(LST_LIST)->getDefaultValue());
      mCIL << "call bool " << CIL_USERSCRIPT_CLASS << "::'Equals'(" << CIL_TYPE_NAMES[LST_LIST] << ", " << CIL_TYPE_NAMES[LST_LIST] << ")\n"
           << "ldc.i4.0\n"
           << "ceq\n";
      return false;
    default:
      assert(0);
  }
  return false;
}

bool MonoScriptCompiler::visit(LSLVectorExpression *vec_expr) {
  visitChildren(vec_expr);
  mCIL << "call " << CIL_TYPE_NAMES[LST_VECTOR] << " " << CIL_USERSCRIPT_CLASS << "::'CreateVector'(float32, float32, float32)\n";
  return false;
}

bool MonoScriptCompiler::visit(LSLQuaternionExpression *quat_expr) {
  visitChildren(quat_expr);
  mCIL << "call " << CIL_TYPE_NAMES[LST_QUATERNION] << " " << CIL_USERSCRIPT_CLASS << "::'CreateQuaternion'(float32, float32, float32, float32)\n";
  return false;
}

bool MonoScriptCompiler::visit(LSLLValueExpression *lvalue) {
  pushLValue(lvalue);
  return false;
}

bool MonoScriptCompiler::visit(LSLListExpression *list_expr) {
  // LL's compiler pushes lists in a different order in globexprs for some reason,
  // maybe something about order of evaluation being important there.
  // match their behavior so it's less annoying to compare output.
  if (_mInGlobalExpr) {
    mCIL << CIL_LIST_INITIALIZER << "\n";
    for (auto child : *list_expr) {
      child->visit(this);
      mCIL << CIL_BOXING_INSTRUCTIONS[child->getIType()]
           << "call " << CIL_TYPE_NAMES[LST_LIST] << " " << CIL_USERSCRIPT_CLASS << "::Append(" << CIL_TYPE_NAMES[LST_LIST] << ", object)\n";
    }
  } else {
    // list elements get evaluated and pushed FIRST
    size_t num_children = 0;
    for (auto *child : *list_expr) {
      child->visit(this);
      mCIL << CIL_BOXING_INSTRUCTIONS[child->getIType()];
      ++num_children;
    }
    // then they get added to the list
    mCIL << CIL_LIST_INITIALIZER << "\n";
    for (size_t i=0; i<num_children; ++i) {
      mCIL << "call " << CIL_TYPE_NAMES[LST_LIST] << " " << CIL_USERSCRIPT_CLASS << "::Prepend(object, " << CIL_TYPE_NAMES[LST_LIST] << ")\n";
    }
  }
  return false;
}

bool MonoScriptCompiler::visit(LSLFunctionExpression *func_expr) {
  auto *func_sym = func_expr->getSymbol();
  // this will be a method on the script instance, push `this` onto the stack
  if (func_sym->getSubType() != SYM_BUILTIN)
    mCIL << "ldarg.0\n";

  // push the arguments onto the stack
  for (auto *child_expr : *func_expr->getArguments()) {
    child_expr->visit(this);
  }

  if (func_sym->getSubType() == SYM_BUILTIN) {
    mCIL << "call " << CIL_TYPE_NAMES[func_expr->getIType()] << " "
         << CIL_LSL_LIBRARY_CLASS << "::'" << func_sym->getName() << "'";
  } else {
    mCIL << "call instance " << CIL_TYPE_NAMES[func_expr->getIType()] << " class "
         << _mScriptClassName + "::'g" + func_sym->getName() + "'";
  }

  // write in the functions' expected parameter types
  auto *func_decl = func_sym->getFunctionDecl();
  mCIL << "(";
  for (auto *func_param : *func_decl) {
    mCIL << CIL_TYPE_NAMES[func_param->getIType()];
    if (func_param->getNext())
      mCIL << ", ";
  }
  mCIL << ")\n";
  return false;
}


/// An operation that is performed via a method call on the UserScript class
typedef struct {
  LSLIType left;
  LSLIType right;
} SimpleBinaryOperationInfo;

/// Cases where the operation is performed via a method call on the UserScript class with
/// a well-known operation name for the method, and operator overloading for the various types.
/// Return value is inferred by looking at the return value of the actual expression node.
const std::map<int, std::pair<const char *, std::vector<SimpleBinaryOperationInfo> > > SIMPLE_BINARY_OPS = {
    {'+', {"Add", {
        {LST_STRING, LST_STRING},
        {LST_VECTOR, LST_VECTOR},
        {LST_QUATERNION, LST_QUATERNION},
    }}},
    {'-', {"Subtract", {
        // not done because we have an optional optimization we can do
        // {LST_INTEGER, LST_INTEGER},
        // not simple, float64 args and return!
        // {LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {LST_VECTOR, LST_VECTOR},
        {LST_QUATERNION, LST_QUATERNION},
    }}},
    {'*', {"Multiply", {
        {LST_VECTOR, LST_FLOATINGPOINT},
        {LST_FLOATINGPOINT, LST_VECTOR},
        {LST_VECTOR, LST_VECTOR},
        {LST_VECTOR, LST_QUATERNION},
        {LST_QUATERNION, LST_QUATERNION},
    }}},
    {'/', {"Divide", {
        {LST_INTEGER, LST_INTEGER},
        // not simple, float64 args and return!
        // {LST_FLOATINGPOINT, LST_FLOATINGPOINT},
        {LST_VECTOR, LST_FLOATINGPOINT},
        {LST_VECTOR, LST_QUATERNION},
        {LST_QUATERNION, LST_QUATERNION},
    }}},
    {'%', {"Modulo", {
        {LST_INTEGER, LST_INTEGER},
        {LST_VECTOR, LST_VECTOR},
    }}},
    {OP_EQ, {"Equals", {
        {LST_VECTOR, LST_VECTOR},
        {LST_QUATERNION, LST_QUATERNION},
        {LST_LIST, LST_LIST},
    }}},
    {OP_NEQ, {"NotEquals", {
        // TODO: desugar `a != b` to `!(a == b)` where possible?
        {LST_LIST, LST_LIST},
    }}},
    {OP_SHIFT_LEFT, {"ShiftLeft", {
        {LST_INTEGER, LST_INTEGER},
    }}},
    {OP_SHIFT_RIGHT, {"ShiftRight", {
        {LST_INTEGER, LST_INTEGER},
    }}},
};

bool MonoScriptCompiler::visit(LSLBinaryExpression *bin_expr) {
  LSLOperator op = bin_expr->getOperation();
  auto *left = bin_expr->getLHS();
  auto *right = bin_expr->getRHS();

  if (op == '=') {
    // we're going to store, so we may need a reference to `this` on top of the stack
    auto *lvalue = (LSLLValueExpression *) left;
    pushLValueContainer(lvalue);
    right->visit(this);
    // store to the lvalue and push the lvalue back onto the stack
    // if we're assigning in an expression context. For something in
    // an expressionstatement context like `foo = 1` we omit the push.
    storeToLValue(lvalue, maybeOmitPush(bin_expr));
    return false;
  } else if (op == OP_MUL_ASSIGN) {
    // The only expression that gets left as a MUL_ASSIGN is the busted `int *= float` case,
    // all others get desugared to `lvalue = lvalue * rhs` in an earlier compile pass.
    // That expression is busted and not the same as `int = int * float`, obviously,
    // but we need to support it.
    auto *lvalue = (LSLLValueExpression *) left;
    pushLValueContainer(lvalue);
    right->visit(this);
    lvalue->visit(this);
    // cast the integer lvalue to a float first
    castTopOfStack(LST_INTEGER, LST_FLOATINGPOINT);
    mCIL << "mul\n";
    // cast the result to an integer so we can store it in the lvalue
    castTopOfStack(LST_FLOATINGPOINT, LST_INTEGER);
    // This will return the wrong type because things expect this expression to return a float.
    // Use of the retval will probably cause a crash.
    storeToLValue(lvalue, maybeOmitPush(bin_expr));
    return false;
  }

  compileBinaryExpression(op, left, right, bin_expr->getIType());
  return false;
}

void MonoScriptCompiler::compileBinaryExpression(LSLOperator op, LSLExpression *left, LSLExpression *right, LSLIType ret_type) {
  const auto left_type = left->getIType();
  const auto right_type = right->getIType();

  // this is an operation that uses the simplified method call form
  if (const auto simple_op = SIMPLE_BINARY_OPS.find(op); simple_op != SIMPLE_BINARY_OPS.end()) {
    // walk through the type pairs this operation has a method call form for
    for (auto [op_left, op_right] : simple_op->second.second) {
      if (left_type != op_left || right_type != op_right)
        continue;
      right->visit(this);
      left->visit(this);
      // right is first argument due to reversed order of evaluation in LSL
      mCIL << "call " << CIL_TYPE_NAMES[ret_type] << " " << CIL_USERSCRIPT_CLASS << "::'"
           << simple_op->second.first << "'(" << CIL_TYPE_NAMES[right_type] << ", " << CIL_TYPE_NAMES[left_type] << ")\n";
      return;
    }
  }

  // handle all the operations that couldn't be handled via templating together a method call
  switch(op) {
    case '+': {
      right->visit(this);
      left->visit(this);
      if (right_type == LST_LIST && left_type != LST_LIST) {
        // prepend whatever this is to the right list
        mCIL << CIL_BOXING_INSTRUCTIONS[left_type];
        mCIL << "call " << CIL_TYPE_NAMES[LST_LIST] << " " << CIL_USERSCRIPT_CLASS << "::Prepend(" << CIL_TYPE_NAMES[LST_LIST] << ", object)\n";
        return;
      } else if (left_type == LST_LIST) {
        // append to the left list (will also join lists)
        mCIL << "call " << CIL_TYPE_NAMES[LST_LIST] << " " << CIL_USERSCRIPT_CLASS << "::Append("
             << CIL_VALUE_TYPE_NAMES[right_type] << ", " << CIL_VALUE_TYPE_NAMES[LST_LIST] << ")\n";
        return;
      }

      switch (left_type) {
        case LST_INTEGER:
        case LST_FLOATINGPOINT:
          mCIL << "add\n";
          return;
        default:
          assert(0);
          return;
      }
    }
    case '-': {
      if (_mOptions.optimize_sutractions) {
        // LSL usually evaluates expressions from right to left, so arguments are
        // often in the wrong order on the stack for CIL's primitive binary operations.
        // LL gets around that by calling methods with the argument orders reversed that
        // call the math operations internally with the correct order. This bit of nastiness
        // is hard to avoid without using a scratch local and a decent amount of bytecode to
        // swap the top two elements of the stack in place.
        //
        // Logically, lhs - rhs is equivalent to (-rhs) + lhs, so do it that way
        // rather than calling the Subtract method with the reversed arguments.
        // Skimming through the IEEE-754 spec there shouldn't be any semantic
        // difference. It's faster and serializes to fewer bytes.
        right->visit(this);
        mCIL << "neg\n";
        left->visit(this);
        switch (left_type) {
          case LST_FLOATINGPOINT:
          case LST_INTEGER:
            mCIL << "add\n";
            return;
          default:
            assert(0);
            return;
        }
      } else {
        right->visit(this);
        left->visit(this);
        switch (left_type) {
          case LST_FLOATINGPOINT:
            mCIL << "call float64 " << CIL_USERSCRIPT_CLASS << "::'Subtract'(float64, float64)\n";
            return;
          case LST_INTEGER:
            mCIL << "call int32 " << CIL_USERSCRIPT_CLASS << "::'Subtract'(int32, int32)\n";
            return;
          default:
            assert(0);
            return;
        }
      }
    }
    case '*': {
      right->visit(this);
      left->visit(this);
      switch (left_type) {
        case LST_INTEGER:
        case LST_FLOATINGPOINT:
          mCIL << "mul\n";
          return;
        default:
          assert(0);
          return;
      }
    }
    case '/': {
      right->visit(this);
      left->visit(this);
      switch (left_type) {
        case LST_FLOATINGPOINT:
          mCIL << "call float64 " << CIL_USERSCRIPT_CLASS << "::'Divide'(float64, float64)\n";
          return;
        default:
          assert(0);
          return;
      }
    }
    case OP_EQ: {
      right->visit(this);
      left->visit(this);
      switch (right_type) {
        case LST_INTEGER:
        case LST_FLOATINGPOINT:
          mCIL << "ceq\n";
          return;
        case LST_STRING:
          // note the key == string and string == key asymmetry here...
          // left is top of stack, so convert left to a string if it isn't one already
          castTopOfStack(left_type, right_type);
          mCIL << "call bool valuetype [mscorlib]System.String::op_Equality(string, string)\n";
          return;
        case LST_KEY:
          // these really should have been pre-casted if necessary, but this is what LL's compiler does
          castTopOfStack(left_type, right_type);
          mCIL << "call int32 " << CIL_USERSCRIPT_CLASS << "::'Equals'(" << CIL_TYPE_NAMES[LST_KEY] << ", " << CIL_TYPE_NAMES[LST_KEY] << ")\n";
          return;
        default:
          assert(0);
          return;
      }
    }
    case OP_NEQ:
      // EQ will visit right and left in the correct order for us
      compileBinaryExpression(OP_EQ, left, right, ret_type);
      // check if result == 0
      mCIL << "ldc.i4.0\n"
           << "ceq\n";
      return;
    case OP_GEQ:
      right->visit(this);
      left->visit(this);
      // not very nice, but operands are swapped from how CIL would like them
      mCIL << "cgt\n"
           << "ldc.i4.0\n"
           << "ceq\n";
      return;
    case OP_LEQ:
      right->visit(this);
      left->visit(this);
      mCIL << "clt\n"
           << "ldc.i4.0\n"
           << "ceq\n";
      return;
    case '>':
      right->visit(this);
      left->visit(this);
      mCIL << "clt\n";
      return;
    case '<':
      right->visit(this);
      left->visit(this);
      mCIL << "cgt\n";
      return;
    case OP_BOOLEAN_AND:
      // We need to interleave our codegen with the code of the expressions,
      // so just visit right to start
      right->visit(this);
      // push whether this returned false
      // necessary because everything EXCEPT 0 is truthy!
      mCIL << "ldc.i4.0\n"
           << "ceq\n";

      left->visit(this);
      mCIL << "ldc.i4.0\n"
           << "ceq\n";

      // binary OR the results together and compare against zero
      // will push whether neither had the false bit set
      mCIL << "or\n"
           << "ldc.i4.0\n"
           << "ceq\n";
      return;
    case OP_BOOLEAN_OR:
      right->visit(this);
      left->visit(this);
      // binary OR the sides together and compare against zero
      mCIL << "or\n"
           << "ldc.i4.0\n"
           << "ceq\n"
           // TODO: LL's codegen compares against zero again. Copy & paste error in their code?
           << "ldc.i4.0\n"
           << "ceq\n";
      return;
    case '&':
      right->visit(this);
      left->visit(this);
      mCIL << "and\n";
      return;
    case '|':
      right->visit(this);
      left->visit(this);
      mCIL << "or\n";
      return;
    case '^':
      right->visit(this);
      left->visit(this);
      mCIL << "xor\n";
      return;
    default:
      assert(0);
  }
  assert(0);
}

bool MonoScriptCompiler::visit(LSLUnaryExpression *unary_expr) {
  auto *child_expr = unary_expr->getChildExpr();
  LSLOperator op = unary_expr->getOperation();

  if (op == OP_POST_DECR || op == OP_POST_INCR) {
    auto *lvalue = (LSLLValueExpression *) child_expr;
    // We need to keep the original value of the expression on the stack,
    // but only if the result of the expr will actually be used
    if (maybeOmitPush(unary_expr))
      pushLValue(lvalue);

    // may need the containing object ref for for the subsequent store
    pushLValueContainer(lvalue);
    // then load another copy of the value to do the operation (wasteful!)
    pushLValue(lvalue);
    // push "one" for the given type
    pushConstant(lvalue->getType()->getOneValue());
    if (op == OP_POST_DECR) {
      mCIL << "sub\n";
    } else {
      mCIL << "add\n";
    }

    // This store + push, then subsequent pop is totally unnecessary, but matches what LL's
    //  compiler does. Only do it if we're not allowed to omit pushes.
    if (_mOptions.omit_unnecessary_pushes) {
      storeToLValue(lvalue, false);
    } else {
      storeToLValue(lvalue, true);
      mCIL << "pop\n";
    }

    return false;
  } else if (op == OP_PRE_INCR || op == OP_PRE_DECR) {
    // This appears to generate different code from `lvalue = lvalue + 1`,
    // so it isn't desugared.
    auto *lvalue = (LSLLValueExpression *) child_expr;
    pushLValueContainer(lvalue);
    pushLValue(lvalue);
    pushConstant(lvalue->getType()->getOneValue());
    if (op == OP_PRE_DECR) {
      mCIL << "sub\n";
    } else {
      mCIL << "add\n";
    }

    storeToLValue(lvalue, maybeOmitPush(unary_expr));
    return false;
  }

  // LL's grammar reads a minus before a number in a global's initializer as a single
  // negative constant rather than a negation, so `integer x = -1;` loads -1 directly.
  if (op == '-' && _mInGlobalExpr && child_expr->getNodeSubType() == NODE_CONSTANT_EXPRESSION) {
    auto *cv = child_expr->getConstantValue();
    if (cv && cv->getIType() == LST_INTEGER) {
      // negate in unsigned space so -INT_MIN wraps back to INT_MIN like LL's compiler
      const auto value = (uint32_t)((LSLIntegerConstant *) cv)->getValue();
      pushLiteral(_mAllocator->newTracked<LSLIntegerConstant>((S32)(0u - value)));
      return false;
    }
    if (cv && cv->getIType() == LST_FLOATINGPOINT) {
      pushFloatLiteral(-((LSLFloatConstant *) cv)->getValue());
      return false;
    }
  }

  auto child_type = child_expr->getIType();
  child_expr->visit(this);
  switch (op) {
    case '-':
      switch(child_type) {
        case LST_INTEGER:
        case LST_FLOATINGPOINT:
          mCIL << "neg\n";
          return false;
        case LST_QUATERNION:
        case LST_VECTOR:
          mCIL << "call " << CIL_TYPE_NAMES[child_type] << " " << CIL_USERSCRIPT_CLASS << "::'Negate'(" << CIL_TYPE_NAMES[child_type] << ")\n";
          return false;
        default:
          assert(0);
          return false;
      }
    case '!': {
      mCIL << "ldc.i4.0\n"
           << "ceq\n";
      return false;
    }
    case '~': {
      mCIL << "not\n";
      return false;
    }
    default:
      assert(0);
      return false;
  }
  assert(0);
  return false;
}

bool MonoScriptCompiler::visit(LSLPrintExpression *print_expr) {
  auto *child_expr = print_expr->getChildExpr();
  child_expr->visit(this);
  castTopOfStack(child_expr->getIType(), LST_STRING);
  mCIL << "call void " << CIL_LSL_LIBRARY_CLASS << "::Print(string)\n";
  return false;
}

}
