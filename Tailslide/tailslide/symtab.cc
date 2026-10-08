#include <algorithm>
#include <vector>       // vector::iterator
#include <set>
#include <string>

#include "lslmini.hh"
#include "symtab.hh"

namespace Tailslide {

void LSLSymbolTable::define(LSLSymbol *symbol) {
  _mSymbols.insert(UnorderedCStrMap<LSLSymbol*>::value_type(symbol->getName(), symbol));
  DEBUG(
    LOG_DEBUG_SPAM,
    NULL,
    "defined symbol: %d %s %s\n",
    symbol->getSymbolType(),
    symbol->getType() ? symbol->getType()->getNodeName().c_str() : "!!!NULL!!!",
    symbol->getName()
  );
}

LSLSymbol *LSLSymbolTable::lookup(const char *name, LSLSymbolType type) {
  auto sym_range = _mSymbols.equal_range(name);
  for (auto it = sym_range.first; it != sym_range.second; ++it) {
    if (type == SYM_ANY || type == it->second->getSymbolType())
      return it->second;
  }
  return nullptr;
}

void LSLSymbolTable::checkSymbols() {
  for (auto &symbol: _mSymbols) {
    LSLSymbol *sym = symbol.second;
    if (sym->getSubType() != SYM_BUILTIN && sym->getSubType() != SYM_EVENT_PARAMETER &&
        sym->getReferences() <= 1) {
      // We don't really care if the default state never gets explicitly referenced.
      if (sym->getSymbolType() == SYM_STATE && !strcmp("default", sym->getName()))
        continue;
      NODE_ERROR(sym, W_DECLARED_BUT_NOT_USED, LSLSymbol::getTypeName(sym->getSymbolType()), sym->getName());
    }
  }
}

bool LSLSymbolTable::remove(LSLSymbol *symbol) {
  for (auto iter = _mSymbols.begin(); iter != _mSymbols.end(); ++iter) {
    if (iter->second == symbol) {
      _mSymbols.erase(iter);
      return true;
    }
  }
  return false;
}

void LSLSymbolTable::resetTracking() {
  for (auto &symbol: _mSymbols) {
    symbol.second->resetTracking();
  }
}

/* Oddly enough, using shorter names in globals saves bytecode space. */
void LSLSymbolTableManager::setMangledNames() {
  int seq = 0;
  for (auto &desc_table: _mTables) {
    auto &node_symbols = desc_table->getMap();
    // We want mangled symbol name to be consistent across STL implementations,
    // and our symbol map is specifically unsorted. Place the symbol names in an std::set
    // which will de-dupe and has a specification-imposed iteration order.
    std::set<std::string> key_names;
    for (auto &it: node_symbols) {
      key_names.insert(it.first);
    }
    for (auto &key_name: key_names) {
      auto range = node_symbols.equal_range(key_name.c_str());
      for (auto &symbol = range.first; symbol != range.second; ++symbol) {
        LSLSymbol *sym = symbol->second;
        // can't rename events or builtin names, obviously!
        if (sym->getSymbolType() == SYM_EVENT || sym->getSubType() == SYM_BUILTIN)
          continue;
        // default state _must_ be named default, can't mangle the name.
        if (sym->getSymbolType() == SYM_STATE && !strcmp("default", sym->getName()))
          continue;

        char *mangled_id = _mAllocator->alloc(30);
        while (true) {
          snprintf(mangled_id, 30, "_%x", seq++);
          // Make sure this name isn't already in use
          if (!desc_table->lookup(mangled_id, SYM_ANY)) {
            sym->setMangledName(mangled_id);
            break;
          }
        }
      }
    }
  }
}

void LSLSymbolTableManager::resetTracking() {
  for (auto *table : _mTables) {
    table->resetTracking();
  }
}

LSLIType LSLSymbol::getIType() {
  return _mType->getIType();
}

}
