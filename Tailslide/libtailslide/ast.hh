#ifndef TAILSLIDE_AST_HH
#define TAILSLIDE_AST_HH
#include <cassert>
#include <cstdlib> // nullptr
#include <cstdarg> // va_arg
#include <set>
#include "symtab.hh" // symbol table
#include "logger.hh"
#include "allocator.hh"

namespace Tailslide {

// Base node types
enum LSLNodeType {

  NODE_NODE,
  NODE_NULL,
  NODE_AST_NODE_LIST,
  NODE_SCRIPT,
  NODE_IDENTIFIER,
  NODE_GLOBAL_VARIABLE,
  NODE_CONSTANT,
  NODE_GLOBAL_FUNCTION,
  NODE_FUNCTION_DEC,
  NODE_EVENT_DEC,
  NODE_STATE,
  NODE_EVENT_HANDLER,
  NODE_STATEMENT,
  NODE_EXPRESSION,
  NODE_TYPE,
};

// Node Sub-types
enum LSLNodeSubType {

  NODE_NO_SUB_TYPE,

  NODE_INTEGER_CONSTANT,
  NODE_FLOAT_CONSTANT,
  NODE_STRING_CONSTANT,
  NODE_KEY_CONSTANT,
  NODE_VECTOR_CONSTANT,
  NODE_QUATERNION_CONSTANT,
  NODE_LIST_CONSTANT,

  NODE_COMPOUND_STATEMENT,
  NODE_NOP_STATEMENT,
  NODE_EXPRESSION_STATEMENT,
  NODE_RETURN_STATEMENT,
  NODE_LABEL,
  NODE_JUMP_STATEMENT,
  NODE_IF_STATEMENT,
  NODE_FOR_STATEMENT,
  NODE_DO_STATEMENT,
  NODE_WHILE_STATEMENT,
  NODE_DECLARATION,
  NODE_STATE_STATEMENT,

  NODE_TYPECAST_EXPRESSION,
  NODE_BOOL_CONVERSION_EXPRESSION,
  NODE_PRINT_EXPRESSION,
  NODE_FUNCTION_EXPRESSION,
  NODE_VECTOR_EXPRESSION,
  NODE_QUATERNION_EXPRESSION,
  NODE_LIST_EXPRESSION,
  NODE_LVALUE_EXPRESSION,
  NODE_BINARY_EXPRESSION,
  NODE_UNARY_EXPRESSION,
  NODE_PARENTHESIS_EXPRESSION,
  NODE_CONSTANT_EXPRESSION,
};

struct OptimizationOptions;
class ASTVisitor;

template<class T>
struct node_child_iterator {
  static_assert(std::is_base_of<class LSLASTNode, T>::value);

  public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type   = std::ptrdiff_t;
  using value_type        = T*;
  // TODO: this might be horrible. The ownership story is still very weird for
  //  the AST, and all consumers expect pointers right now, so leaving it as is.
  using pointer           = value_type;
  using reference         = value_type;

  explicit node_child_iterator(pointer ptr) : _mPtr(ptr) {}
  T *operator*() const { return _mPtr; }
  T *operator->() { return _mPtr; }

  node_child_iterator & operator++() {
    _mPtr = static_cast<T*>(_mPtr->getNext());
    return *this;
  }

  node_child_iterator operator++(int) {
    node_child_iterator tmp = *this;
    ++(*this);
    return tmp;
  }

  friend bool operator== (const node_child_iterator & a, const node_child_iterator & b) { return a._mPtr == b._mPtr; };
  friend bool operator!= (const node_child_iterator & a, const node_child_iterator & b) { return a._mPtr != b._mPtr; };

  private:
  pointer _mPtr;
};

class LSLASTNode : public TrackableObject {
  public:
    explicit LSLASTNode(ScriptContext *ctx);

    LSLASTNode( ScriptContext *ctx, YYLTYPE *loc, int num, ... )
      : LSLASTNode(ctx) {
      _mLoc = *loc;
      va_list ap;
      va_start(ap, num);
      addChildren(num, ap);
      va_end(ap);
    }

    explicit LSLASTNode( ScriptContext *ctx, int num, ... ) : LSLASTNode(ctx) {
      va_list ap;
      va_start(ap, num);
      addChildren(num, ap);
      va_end(ap);
    }

    ~LSLASTNode() override = default;

    void addChildren(int num, va_list ap);

    LSLASTNode *getNext() { return _mNext; }
    LSLASTNode *getPrev() { return _mPrev; }
    LSLASTNode *getParent() { return _mParent; }

    LSLASTNode *getChild(int i) {
      LSLASTNode *c = _mChildren;
      while (i-- && c)
        c = c->getNext();
      return c;
    }

    void setChild(int i, LSLASTNode *new_val) {
      LSLASTNode *c = _mChildren;
      while (i-- && c) {
        c = c->getNext();
      }
      assert(c);
      if (!new_val)
        new_val = newNullNode();
      LSLASTNode::replaceNode(c, new_val);
    }

    int getNumChildren() const {
      LSLASTNode *c = _mChildren;
      int num = 0;
      // empty children (NODE_NULL) are still considered valid.
      while (c) {
        c = c->getNext();
        ++num;
      }
      return num;
    };

    bool hasChildren() const {
      return _mChildren != nullptr;
    }

    // Get the topmost node in the tree
    LSLASTNode *getRoot() {
      LSLASTNode *last_node = this;
      LSLASTNode *test_root;
      while ((test_root = last_node->getParent()) != nullptr)
        last_node = test_root;
      return last_node;
    }

    /// Get our position within our parents' children
    int getParentSlot();

    LSLASTNode *newNullNode();

    /* Set our parent, and make sure all our siblings do too. */
    void setParent(LSLASTNode *newparent );
    // Add child to end of list.
    void pushChild(LSLASTNode *child);
    /* Set our next sibling, and ensure it links back to us. */
    void setNext(LSLASTNode *newnext);
    /* Set our previous sibling, and ensure it links back to us. */
    void setPrev(LSLASTNode *newprev);
    /* remove a child from the list of nodes, shifting other children up */
    void removeChild(LSLASTNode *child);
    /* replace a node from the list of children with null, returning it */
    LSLASTNode *takeChild(int child_num);

    // replace an arbitrary node with another, setting
    // prev, next and parent as appropriate
    static void replaceNode(LSLASTNode *old_node, LSLASTNode *replacement);

    // Reference tracking for symbols
    void incrementSymbolReferences();
    void decrementSymbolReferences();

    void                setType(LSLType *type) { _mType = type;   }
    class LSLType *getType()                    { return _mType;    }
    LSLIType getIType();


    void markStatic() { _mStaticNode = true;}
    bool isStatic() const {return _mStaticNode;}

    /// passes                  ///
    // generic visitor functions
    void visit(ASTVisitor *visitor);

    // Convenience methods for common visitor uses
    void collectSymbols();
    void determineTypes();
    void propagateValues(bool create_heap_values=true);
    void finalPass();
    void checkSymbols(); // look for unused symbols, etc

    /// symbol functions        ///
    virtual LSLSymbol *lookupSymbol(const char *name, LSLSymbolType type );
    void            defineSymbol(LSLSymbol *symbol );
    LSLSymbolTable *getSymbolTable() { return _mSymbolTable; }
    void setSymbolTable(LSLSymbolTable *table) {_mSymbolTable = table;}


    YYLTYPE     *getLoc()     { return &_mLoc; };
    void setLoc(YYLTYPE *yylloc) { _mLoc = *yylloc; };


    // Set whether this node is allowed to be a declaration,
    // usually due to occurring in a conditional statement without a new scope.
    void setDeclarationAllowed(bool allowed) { _mDeclarationAllowed = allowed; };
    bool getDeclarationAllowed() const { return _mDeclarationAllowed; };


    /// identification          ///
    virtual std::string getNodeName() { return "node";    };
    virtual LSLNodeType  getNodeType() { return NODE_NODE; };
    virtual LSLNodeSubType getNodeSubType() { return NODE_NO_SUB_TYPE; }

    /// constants ///
    virtual class LSLConstant  *getConstantValue()    { return _mConstantValue; };
    void setConstantValue(class LSLConstant *cv) {
      if (cv)
        _mConstantPrecluded = false;
      _mConstantValue = cv;
    };
    bool            isConstant()           { return getConstantValue() != nullptr; };
    void setConstantPrecluded(bool precluded) { _mConstantPrecluded = precluded; };
    bool getConstantPrecluded() const { return _mConstantPrecluded; };
    virtual bool nodeAllowsFolding() { return false; };
    virtual LSLSymbol *getSymbol() { return nullptr; }

    bool getSynthesized() const { return _mSynthesized; };
    void setSynthesized(bool synthesized) { _mSynthesized = synthesized; };

  protected:
    bool _mSynthesized = false;
    class LSLType          *_mType;
    LSLSymbolTable         *_mSymbolTable;
    class LSLConstant      *_mConstantValue;
    bool                   _mConstantPrecluded = false;

  protected:
    // head of the linked-list, only set for list-like nodes
    LSLASTNode *_mChildren = nullptr;
    // only set for list-like nodes
    LSLASTNode *_mChildrenTail = nullptr;

  private:
    YYLTYPE                      _mLoc {0};

    LSLASTNode *_mParent;
    LSLASTNode *_mNext;
    LSLASTNode *_mPrev;

    inline void adjustSymbolReferences(bool decrement);

  protected:
    bool                        _mDeclarationAllowed;
    bool                        _mStaticNode = false;

  public:
    node_child_iterator<LSLASTNode> begin() { return node_child_iterator<LSLASTNode>(_mChildren); }
    node_child_iterator<LSLASTNode> end()   { return node_child_iterator<LSLASTNode>(nullptr); }
};

class LSLASTNullNode : public LSLASTNode {
  public:
    explicit LSLASTNullNode(ScriptContext *ctx): LSLASTNode(ctx) {};
    virtual std::string getNodeName() { return "null"; };
    virtual LSLNodeType getNodeType() { return NODE_NULL; };
};

template<class T>
class LSLASTNodeList : public LSLASTNode {
  static_assert(std::is_base_of<LSLASTNode, T>::value, "T Must derive from LSLASTNode!");
  public:
    explicit LSLASTNodeList<T>(ScriptContext *ctx) : LSLASTNode(ctx, 0) {};
    LSLASTNodeList<T>(ScriptContext *ctx, class LSLASTNode *nodes ) : LSLASTNodeList(ctx) {
      if (nodes)
        pushChild(nodes);
    };
    virtual std::string getNodeName() { return "ast node list"; }
    virtual LSLNodeType getNodeType() { return NODE_AST_NODE_LIST; };

    node_child_iterator<T> begin() { return node_child_iterator<T>(static_cast<T*>(_mChildren)); }
    node_child_iterator<T> end()   { return node_child_iterator<T>(nullptr); }
};

}

#endif
