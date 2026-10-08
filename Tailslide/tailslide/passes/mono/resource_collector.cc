#include "resource_collector.hh"

namespace Tailslide {


bool MonoResourceVisitor::visit(LSLGlobalFunction *glob_func) {
  auto *sym = glob_func->getSymbol();
  auto *func_sym_data = getSymbolData(sym);
  _mCurrentFunc = func_sym_data;
  // pick up local declarations
  visitChildren(glob_func);
  _mCurrentFunc = nullptr;
  return false;
}

bool MonoResourceVisitor::visit(LSLEventHandler *handler) {
  auto *sym = handler->getSymbol();
  auto *handler_sym_data = getSymbolData(sym);
  _mCurrentFunc = handler_sym_data;
  // pick up local declarations
  visitChildren(handler);
  _mCurrentFunc = nullptr;
  return false;
}

bool MonoResourceVisitor::visit(LSLDeclaration *decl_stmt) {
  auto *sym = decl_stmt->getSymbol();
  auto *sym_data = getSymbolData(sym);
  sym_data->index = (uint32_t)_mCurrentFunc->locals.size();
  _mCurrentFunc->locals.push_back(sym->getIType());
  return true;
}

MonoSymbolData *MonoResourceVisitor::getSymbolData(LSLSymbol *sym) {
  auto sym_iter = _mSymData->find(sym);
  if (sym_iter != _mSymData->end())
    return &sym_iter->second;
  (*_mSymData)[sym] = {};
  return &_mSymData->find(sym)->second;
}

}
