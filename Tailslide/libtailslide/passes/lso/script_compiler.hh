#pragma once

#include <vector>

#include "../../lslmini.hh"
#include "../../visitor.hh"
#include "../../bitstream.hh"
#include "bytecode_format.hh"
#include "resource_collector.hh"

namespace Tailslide {

class LSOHeapManager {
  public:
    uint32_t writeConstant(LSLConstant *constant);
    uint32_t writeTerminalBlock();
    LSOBitStream mHeapBS {ENDIAN_BIG};
  protected:
    void writeHeader(uint32_t size, LSLIType type, uint16_t ref_count=1);
};

class LSOGlobalVarManager {
  public:
    explicit LSOGlobalVarManager(LSOHeapManager *heap_manager): _mHeapManager(heap_manager) {}
    void writeVar(LSLConstant *constant, const char *name=nullptr);
    void writePlaceholder(LSLIType type);
    LSOBitStream mGlobalsBS {ENDIAN_BIG};
  protected:
    LSOHeapManager *_mHeapManager;
};

class LSOScriptCompiler : public ASTVisitor {
  public:
    explicit LSOScriptCompiler(ScriptAllocator *allocator) : _mAllocator(allocator) {};
    LSOBitStream mScriptBS {ENDIAN_BIG};
  protected:
    virtual bool visit(LSLScript *script);
    virtual bool visit(LSLGlobalVariable *glob_var);
    virtual bool visit(LSLState *state);
    virtual bool visit(LSLEventHandler *handler);
    virtual bool visit(LSLGlobalFunction *glob_func);

    void writeRegister(LSORegisters reg, uint32_t val);
    void writeEventRegister(LSORegisters reg, uint64_t val);
    bool checkStackHeapCollision();

    LSOBitStream _mRegistersBS {ENDIAN_BIG};
    LSOBitStream _mFunctionsBS {ENDIAN_BIG};
    LSOBitStream _mStatesBS {ENDIAN_BIG};
    LSOBitStream _mStateBS {ENDIAN_BIG};
    LSOBitStream _mCodeBS {ENDIAN_BIG};
    LSOHeapManager _mHeapManager;
    LSOGlobalVarManager _mGlobalVarManager {&_mHeapManager};
    ScriptAllocator *_mAllocator;
    LSOSymbolDataMap _mSymData {};
};

}
