#include "bytecode_format.hh"
#include "library_funcs.hh"
#include "resource_collector.hh"

namespace Tailslide {
bool LSOResourceVisitor::visit(LSLScript *script) {
  // build up symbol data for library functions
  for (auto builtin_val: script->mContext->builtins->getMap()) {
    auto *sym = builtin_val.second;
    if (sym->getSymbolType() != SYM_FUNCTION)
      continue;
    auto sym_data = getSymbolData(sym);
    // get the library num for this function
    auto func_idx_iter = LSO_LIBRARY_FUNCS.find(sym->getName());
    if (func_idx_iter != LSO_LIBRARY_FUNCS.end())
      sym_data->index = func_idx_iter->second;
    else
      // There are lot of functions we don't have the library num for,
      // just put a placeholder number.
      sym_data->index = 0xFFff;

    // figure out the size of the parameters
    handleFuncDecl(sym_data, sym->getFunctionDecl());
  }
  return true;
}

bool LSOResourceVisitor::visit(LSLGlobalFunction *glob_func) {
  auto *sym = glob_func->getSymbol();
  auto *func_sym_data = getSymbolData(sym);
  func_sym_data->index = _mFuncCount++;
  // enrich function prototype and parameters with sizing information
  handleFuncDecl(func_sym_data, sym->getFunctionDecl());

  _mCurrentFunc = func_sym_data;
  // pick up local declarations
  visitChildren(glob_func);
  _mCurrentFunc = nullptr;
  return false;
}

bool LSOResourceVisitor::visit(LSLGlobalVariable *glob_var) {
  auto *sym = glob_var->getSymbol();
  auto sym_data = getSymbolData(sym);
  // Offset to actual data, type, null terminator for name (empty)
  _mGlobalsOffset += 4 + 1 + 1;
  sym_data->offset = _mGlobalsOffset;
  _mGlobalsOffset += sym_data->size = LSO_TYPE_DATA_SIZES[sym->getIType()];
  return true;
}

bool LSOResourceVisitor::visit(LSLState *state) {
  auto *sym_data = getSymbolData(state->getSymbol());
  sym_data->index = _mStateCount++;
  _mCurrentState = sym_data;
  visitChildren(state);
  _mCurrentState = nullptr;
  return false;
}


bool LSOResourceVisitor::visit(LSLDeclaration *decl_stmt) {
  auto *sym = decl_stmt->getSymbol();
  auto *sym_data = getSymbolData(sym);
  sym_data->index = (uint32_t)_mCurrentFunc->locals.size();
  sym_data->offset = _mCurrentFunc->size;
  // local slots are smaller than globals, no overhead for offset to data, type and name.
  _mCurrentFunc->size += sym_data->size = LSO_TYPE_DATA_SIZES[sym->getIType()];
  _mCurrentFunc->locals.push_back(sym->getIType());
  return true;
}


bool LSOResourceVisitor::visit(LSLEventHandler *handler) {
  auto *sym = handler->getSymbol();
  auto *handler_sym_data = getSymbolData(sym);
  // enrich handler prototype and parameters with sizing information
  handleFuncDecl(handler_sym_data, sym->getFunctionDecl());

  _mCurrentFunc = handler_sym_data;
  // pick up local declarations
  visitChildren(handler);
  _mCurrentFunc = nullptr;
  // the LSO event handler index is the event's position in builtins.txt
  handler_sym_data->index = sym->getEventIndex();
  assert(handler_sym_data->index);
  // register the state as having this handler as well
  _mCurrentState->handlers.insert((LSOHandlerType)handler_sym_data->index);
  return false;
}

// shared by functions and event handlers
void LSOResourceVisitor::handleFuncDecl(LSOSymbolData *func_sym_data, LSLASTNode *func_decl) {
  if (!func_decl || !func_decl->hasChildren())
    return;

  for (auto *param : *func_decl) {
    auto param_type = param->getIType();
    auto param_size = LSO_TYPE_DATA_SIZES[param_type];
    // may not have a symbol if this is a builtin function!
    auto *param_sym = param->getSymbol();
    if (param_sym) {
      auto *param_sym_data = getSymbolData(param_sym);
      param_sym_data->offset = func_sym_data->offset;
      param_sym_data->size = param_size;
    }

    func_sym_data->offset += param_size;
    func_sym_data->function_args.push_back(param_type);
  }
  // function size is parameters + locals, add the parameter sizes
  // before we start looking at the locals
  func_sym_data->size = func_sym_data->offset;
}

LSOSymbolData *LSOResourceVisitor::getSymbolData(LSLSymbol *sym) {
  auto sym_iter = _mSymData->find(sym);
  if (sym_iter != _mSymData->end())
    return &sym_iter->second;
  (*_mSymData)[sym] = {};
  return &_mSymData->find(sym)->second;
}

}
