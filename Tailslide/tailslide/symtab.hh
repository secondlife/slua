#ifndef TAILSLIDE_SYMTAB_HH
#define TAILSLIDE_SYMTAB_HH

#include <cassert>
#include <clocale>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include "allocator.hh"
#include "unordered_cstr_map.hh"

namespace Tailslide {

enum LSLIType : uint8_t {
  LST_NULL          = 0,
  LST_INTEGER       = 1,
  LST_FLOATINGPOINT = 2,
  LST_STRING        = 3,
  LST_KEY           = 4,
  LST_VECTOR        = 5,
  LST_QUATERNION    = 6,
  LST_LIST          = 7,
  LST_ERROR         = 8,   // special value so processing can continue without throwing bogus errors
  LST_MAX           = 9,
};

enum LSLSymbolType       { SYM_ANY = -1, SYM_VARIABLE, SYM_FUNCTION, SYM_STATE, SYM_LABEL, SYM_EVENT };
enum LSLSymbolTableType  { SYMTAB_GLOBAL, SYMTAB_STATE, SYMTAB_FUNCTION, SYMTAB_LEXICAL, SYMTAB_BUILTINS };
enum LSLSymbolSubType    { SYM_LOCAL, SYM_GLOBAL, SYM_BUILTIN, SYM_FUNCTION_PARAMETER, SYM_EVENT_PARAMETER };

class LSLSymbol: public TrackableObject {
  public:
    LSLSymbol( ScriptContext *ctx, const char *name, class LSLType *type, LSLSymbolType symbol_type, LSLSymbolSubType sub_type, YYLTYPE *lloc, class LSLParamList *function_decl = NULL, class LSLASTNode *var_decl = NULL, class LSLLabel *label_decl = NULL  )
      : TrackableObject(ctx), _mName(name), _mType(type), _mSymbolType(symbol_type), _mSubType(sub_type), _mLoc(*lloc), _mFunctionDecl(function_decl), _mVarDecl(var_decl),
        _mLabelDecl(label_decl), _mConstantValue(NULL), _mInitialValue(NULL), _mReferences(0), _mAssignments(0), _mMangledName(NULL) {};

    LSLSymbol( ScriptContext *ctx, const char *name, class LSLType *type, LSLSymbolType symbol_type, LSLSymbolSubType sub_type, class LSLParamList *function_decl = NULL, class LSLASTNode *var_decl = NULL, class LSLLabel *label_decl = NULL )
      : TrackableObject(ctx), _mName(name), _mType(type), _mSymbolType(symbol_type), _mSubType(sub_type), _mLoc({}), _mFunctionDecl(function_decl), _mVarDecl(var_decl),
        _mLabelDecl(label_decl), _mConstantValue(NULL), _mInitialValue(NULL), _mReferences(0), _mAssignments(0), _mMangledName(NULL) {};

    const char          *getName()         { return _mName; }
    class LSLType  *getType()         { return _mType; }
    LSLIType getIType();

    int                  getReferences() const   { return _mReferences; }
    int                  addReference()    { return ++_mReferences; }
    void                 removeReference() { if (_mReferences > 0) --_mReferences; }
    int                  getAssignments() const  { return _mAssignments; }
    int                  addAssignment()   { return ++_mAssignments; }
    void                 removeAssignment() { if (_mAssignments > 0) --_mAssignments; }
    void                 resetTracking()   { _mAssignments = 0; _mReferences = 0; }

    LSLSymbolType         getSymbolType()  { return _mSymbolType; }
    LSLSymbolSubType      getSubType()     { return _mSubType;    }
    static const char         *getTypeName(LSLSymbolType t)    {
      switch (t) {
        case SYM_VARIABLE:  return "variable";
        case SYM_FUNCTION:  return "function";
        case SYM_STATE:     return "state";
        case SYM_LABEL:     return "label";
        case SYM_ANY:       return "any";
        default:            return "invalid";
      }
    }

    YYLTYPE             *getLoc()         { return &_mLoc; }
    class LSLParamList *getFunctionDecl() { return _mFunctionDecl; }
    class LSLASTNode        *getVarDecl() { return _mVarDecl; }
    class LSLLabel        *getLabelDecl() { return _mLabelDecl; }

    class LSLConstant *getConstantValue()                       { return _mConstantValue;    };
    void               setConstantValue(class LSLConstant *v)   { _mConstantValue = v;       };
    class LSLConstant *getInitialValue()                       { return _mInitialValue;    };
    void               setInitialValue(class LSLConstant *v)   { _mInitialValue = v;       };
    bool getConstantPrecluded() const { return _mConstantPrecluded; };
    void setConstantPrecluded(bool precluded) { _mConstantPrecluded = precluded; };

    char                   *getMangledName()             { return _mMangledName; };
    void                    setMangledName(char *m_name) { _mMangledName = m_name; };

    bool getAllPathsReturn() const { return _mAllPathsReturn; }
    void setAllPathsReturn(bool all_ret) { _mAllPathsReturn = all_ret; }

    bool getHasJumps() const { return _mHasJumps; }
    void setHasJumps(bool has_jumps) { _mHasJumps = has_jumps; }
    bool getHasUnstructuredJumps() const { return _mHasUnstructuredJumps; }
    void setHasUnstructuredJumps(bool unstructured_jumps) { _mHasUnstructuredJumps = unstructured_jumps; }

    // For events: 1-based position in builtins.txt, which is the event's
    // number in handled-event bitfields. 0 for anything else.
    int getEventIndex() const { return _mEventIndex; }
    void setEventIndex(int event_index) { _mEventIndex = event_index; }

  private:
    const char          *_mName;
    class LSLType  *_mType;
    LSLSymbolType         _mSymbolType;
    LSLSymbolSubType      _mSubType;
    YYLTYPE              _mLoc;
    class LSLParamList *_mFunctionDecl;
    class LSLASTNode     *_mVarDecl;
    class LSLLabel *_mLabelDecl;
    class LSLConstant *_mConstantValue;
    class LSLConstant *_mInitialValue;
    bool _mConstantPrecluded = false;
    int                  _mReferences;            // how many times this symbol is referred to
    int                  _mAssignments;           // how many times it is assigned to
    char                *_mMangledName;
    bool _mAllPathsReturn = false;
    bool _mHasJumps = false;
    // if the function contains jumps that are not break-like or continue-like
    bool _mHasUnstructuredJumps = false;
    int _mEventIndex = 0;
};

class LSLSymbolTable: public TrackableObject {
  public:
    explicit LSLSymbolTable(ScriptContext *ctx, LSLSymbolTableType symtab_type)
      : TrackableObject(ctx), _mSymbolTableType(symtab_type) {};
    LSLSymbol *lookup( const char *name, LSLSymbolType type = SYM_ANY );
    void            define( LSLSymbol *symbol );
    bool            remove( LSLSymbol *symbol );
    void            checkSymbols();
    void resetTracking();

  private:
    UnorderedCStrMap<LSLSymbol *> _mSymbols;
    std::vector<class LSLLabel *> _mLabels;
    LSLSymbolTableType _mSymbolTableType;

  public:
    UnorderedCStrMap<LSLSymbol*> &getMap() {return _mSymbols;}
    LSLSymbolTableType getTableType() { return _mSymbolTableType; }

    // Used for tracking all labels in a function. Labels in LSL are
    // lexically scoped by specification, function scoped by implementation :(
    void setLabels(const std::vector<class LSLLabel *> &labels) {
      assert(_mSymbolTableType == SYMTAB_FUNCTION);
      _mLabels = labels;
    }
    const std::vector<class LSLLabel *> &getLabels() {
      assert(_mSymbolTableType == SYMTAB_FUNCTION);
      return _mLabels;
    }
};

class LSLSymbolTableManager {
  public:
    explicit LSLSymbolTableManager(ScriptAllocator *allocator) {_mAllocator = allocator;};
    void registerTable(LSLSymbolTable *table) {_mTables.push_back(table);};
    void setMangledNames();
    void resetTracking();
  protected:
    std::vector<LSLSymbolTable *> _mTables {};
    ScriptAllocator *_mAllocator;
};

}

#endif
