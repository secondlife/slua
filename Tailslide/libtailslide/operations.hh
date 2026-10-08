#pragma once

#include "allocator.hh"

struct YYLTYPE;

namespace Tailslide {
class LSLType;
class LSLConstant;
class LSLIntegerConstant;
class LSLStringConstant;
class LSLFloatConstant;
class LSLListConstant;
class LSLQuaternionConstant;
class LSLVectorConstant;

class AOperationBehavior {
  public:
    virtual LSLConstant *operation(
        LSLOperator oper, LSLConstant *cv, LSLConstant *other_cv, YYLTYPE *lloc) = 0;
    virtual LSLConstant *cast(
        LSLType *to_type, LSLConstant *cv, YYLTYPE *lloc) = 0;
};

// Arbitrary operation behavior implemented by Tailslide itself. May not match the target platform's
// runtime operation behavior!
class TailslideOperationBehavior : public AOperationBehavior {
  public:
    explicit TailslideOperationBehavior(ScriptAllocator *allocator, bool create_heap_values=false) {
      _mAllocator = allocator;
      _mMayCreateHeapValues = create_heap_values;
    };
    // dispatch method
    LSLConstant *operation(
        LSLOperator operation, LSLConstant *cv, LSLConstant *other_cv, YYLTYPE *lloc) override;

    // type-specific operation methods
    LSLConstant *operation(LSLOperator operation, LSLIntegerConstant *cv, LSLConstant *other_cv);
    LSLConstant *operation(LSLOperator operation, LSLFloatConstant *cv, LSLConstant *other_cv);
    LSLConstant *operation(LSLOperator operation, LSLStringConstant *cv, LSLConstant *other_cv);
    LSLConstant *operation(LSLOperator operation, LSLKeyConstant *cv, LSLConstant *other_cv);
    LSLConstant *operation(LSLOperator operation, LSLListConstant *cv, LSLConstant *other_cv);
    LSLConstant *operation(LSLOperator operation, LSLVectorConstant *cv, LSLConstant *other_cv);
    LSLConstant *operation(LSLOperator operation, LSLQuaternionConstant *cv, LSLConstant *other_cv);

    // dispatch method
    LSLConstant *cast(LSLType *to_type, LSLConstant *cv, YYLTYPE *lloc) override;

    // type-specific cast methods
    LSLConstant *cast(LSLType *to_type, LSLIntegerConstant *cv);
    LSLConstant *cast(LSLType *to_type, LSLFloatConstant *cv);
    LSLConstant *cast(LSLType *to_type, LSLStringConstant *cv);
    LSLConstant *cast(LSLType *to_type, LSLKeyConstant *cv);
    LSLConstant *cast(LSLType *to_type, LSLListConstant *cv) { return nullptr; };
    LSLConstant *cast(LSLType *to_type, LSLVectorConstant *cv) { return nullptr; };
    LSLConstant *cast(LSLType *to_type, LSLQuaternionConstant *cv) { return nullptr; };

  protected:
    inline char *joinString(const char *left, const char *right) {
      char *ns = _mAllocator->alloc(strlen(left) + strlen(right) + 1);
      strcpy(ns, left);
      strcat(ns, right);
      return ns;
    }
    ScriptAllocator *_mAllocator;
    bool _mMayCreateHeapValues;
};

}
