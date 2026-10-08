#pragma once

#include <map>
#include <vector>

#include "../../visitor.hh"

namespace Tailslide {
struct MonoSymbolData {
  // Index of the given variable within the locals (not for parameters or globals)
  uint32_t index = 0;
  // all locals (if this symbol is a function or event handler)
  std::vector<LSLIType> locals{};
};

typedef std::map<Tailslide::LSLSymbol *, MonoSymbolData> MonoSymbolDataMap;

// Walks the script, figuring out how much space to reserve for data slots
// and what order to place them in.
class MonoResourceVisitor : public ASTVisitor {
  public:
    explicit MonoResourceVisitor(MonoSymbolDataMap *sym_data) : _mSymData(sym_data) {}

  protected:
    bool visit(Tailslide::LSLGlobalFunction *glob_func) override;
    bool visit(Tailslide::LSLEventHandler *handler) override;
    bool visit(Tailslide::LSLDeclaration *decl_stmt) override;
    // not relevant
    bool visit(Tailslide::LSLExpression *expr) override { return false; };

    MonoSymbolData *getSymbolData(Tailslide::LSLSymbol *sym);

    MonoSymbolData *_mCurrentFunc = nullptr;
    MonoSymbolDataMap *_mSymData = nullptr;
};

}
