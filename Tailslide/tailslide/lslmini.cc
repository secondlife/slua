#include <cstdio>
#include <cstring>
#include <cassert>
#include <cmath>

#include "lslmini.hh"
#include "logger.hh"
#include "ast.hh"
#include "visitor.hh"
#include "passes/tree_simplifier.hh"
#include "passes/symbol_resolution.hh"
#include "passes/globalexpr_validator.hh"


namespace Tailslide {

const char *DEPRECATED_FUNCTIONS[][2] = {
        {"llSoundPreload",     "llPreloadSound"},
        {"llSound",            "llPlaySound, llLoopSound, or llTriggerSound"},
        {"llMakeExplosion",    "llParticleSystem"},
        {"llMakeFire",         "llParticleSystem"},
        {"llMakeFountain",     "llParticleSystem"},
        {"llMakeSmoke",        "llParticleSystem"},
        {"llRemoteLoadScript", "llRemoteLoadScriptPin and llSetRemoteScriptAccessPin"},
        {"llPointAt",          nullptr},
        {"llStopPointAt",      nullptr},
        {nullptr,              nullptr},
};


// Lookup a symbol, propagating up the tree until it is found.
LSLSymbol *LSLASTNode::lookupSymbol(const char *name, LSLSymbolType type) {
  LSLSymbol *sym = nullptr;

  // If we have a symbol table of our own, look for it there
  if (_mSymbolTable)
    sym = _mSymbolTable->lookup(name, type);

  // If we have no symbol table, or it wasn't in it, but we have a parent, ask them
  if (sym == nullptr && getParent())
    sym = getParent()->lookupSymbol(name, type);

  return sym;
}

LSLSymbol *LSLScript::lookupSymbol(const char *name, LSLSymbolType sym_type) {
  auto *sym = mContext->builtins->lookup(name, sym_type);
  if (sym != nullptr)
    return sym;
  return LSLASTNode::lookupSymbol(name, sym_type);
}

// Define a symbol, propagating up the tree to the nearest scope level.
void LSLASTNode::defineSymbol(LSLSymbol *symbol) {

  // If we have a symbol table, define it there
  if (_mSymbolTable) {
    LSLSymbol *shadow;

    DEBUG(LOG_DEBUG_SPAM, nullptr, "symbol definition caught in %s\n", getNodeName().c_str());

    // Check if already defined, if it exists in the current scope then shadowing is never allowed!
    shadow = _mSymbolTable->lookup(symbol->getName());
    if (shadow) {
      if (shadow->getSymbolType() == SYM_EVENT)
        if (symbol->getSymbolType() == SYM_EVENT)
          NODE_ERROR(symbol, E_MULTIPLE_EVENT_HANDLERS, symbol->getName());
        else
          NODE_ERROR(symbol, E_EVENT_AS_IDENTIFIER, symbol->getName());
      else
        NODE_ERROR(symbol, E_DUPLICATE_DECLARATION, symbol->getName(), shadow->getLoc()->first_line, shadow->getLoc()->first_column);
    } else {
      // Check for shadowed declarations
      if (getParent())
        shadow = getParent()->lookupSymbol(symbol->getName(), symbol->getSymbolType());
      // If we still didn't find anything, look in the root scope for _any_ kind of symbol,
      // shadowing certain kinds of builtins can be problematic.
      if (shadow == nullptr && getRoot())
        shadow = getRoot()->lookupSymbol(symbol->getName(), SYM_ANY);

      // define it for now even if it shadows so that we have something to work with.
      _mSymbolTable->define(symbol);

      if (shadow != nullptr) {
        // events are _expected_ to "shadow" the event prototype declaration from the outer scope.
        if (shadow->getSymbolType() == SYM_EVENT && symbol->getSymbolType() == SYM_EVENT)
          return;
        if (shadow->getSubType() == SYM_BUILTIN) {
          // you're never allowed to shadow event names
          if (shadow->getSymbolType() == SYM_EVENT)
            NODE_ERROR(symbol, E_EVENT_AS_IDENTIFIER, symbol->getName());
          // builtin function names may be shadowed, but only by locals, not globals.
          else if (shadow->getSymbolType() == SYM_FUNCTION && symbol->getSubType() != SYM_GLOBAL)
            return;
          else
            // anything else is an error
            NODE_ERROR(symbol, E_SHADOW_CONSTANT, symbol->getName());
        } else {
          // nothing in a local scope can ever shadow a function, both
          // can be referenced simultaneously. Anything other than a function _will_ be shadowed.
          if (shadow->getSymbolType() != SYM_FUNCTION || symbol->getSymbolType() == SYM_FUNCTION)
            NODE_ERROR(symbol, W_SHADOW_DECLARATION, symbol->getName(), LINECOL(shadow->getLoc()));
        }
      }
    }
  } else if (getParent()) {
    // Otherwise, ask our parent to define it
    getParent()->defineSymbol(symbol);
  } else {
    // .. but if we don't have a parent, we're in trouble.
    throw "nowhere to define symbol!";
  }
}

// Define any symbols we have, and ask our children to
void LSLASTNode::collectSymbols() {
  SymbolResolutionVisitor visitor(true, mContext->allocator);
  this->visit(&visitor);
}


/// Identifiers should have their type/symbol set by their parent node, because they don't know what
/// kind of symbol they represent by themselves. For example, this should work:
//    string test() { return "hi"; }
//    string func() {
//      integer test = 1;
//      llOwnerSay(test());
//    }
//  But if "test" looked itself up, it would think it is an integer. It's parent function
//  expression node can tell it what it needs to be before determining it's own type.
void LSLIdentifier::resolveSymbol(LSLSymbolType symbol_type) {

  // If we already have a symbol, we don't need to look it up.
  if (_mSymbol != nullptr) {
    _mType = _mSymbol->getType();
    return;
  }

  // If it's a builtin, check for deprecation
  if (symbol_type == SYM_FUNCTION) {
    int i;
    for (i = 0; DEPRECATED_FUNCTIONS[i][0]; ++i) {
      if (!strcmp(_mName, DEPRECATED_FUNCTIONS[i][0])) {
        if (DEPRECATED_FUNCTIONS[i][1] == nullptr) {
          NODE_ERROR(this, W_DEPRECATED, _mName);
        } else {
          NODE_ERROR(this, W_DEPRECATED_WITH_REPLACEMENT, _mName, DEPRECATED_FUNCTIONS[i][1]);
        }
        break;
      }
    }
  }

  // Look up the symbol with the requested type
  _mSymbol = lookupSymbol(_mName, symbol_type);

  if (_mSymbol == nullptr) {                       // no symbol of the right type
    _mSymbol = lookupSymbol(_mName, SYM_ANY);    // so try the wrong one, so we can have a more descriptive error message in that case.
    if (_mSymbol != nullptr && _mSymbol->getSymbolType() != symbol_type) {
      NODE_ERROR(this, E_WRONG_TYPE, _mName,
                 LSLSymbol::getTypeName(symbol_type),
                 LSLSymbol::getTypeName(_mSymbol->getSymbolType())
      );
    } else {
      /* Name suggestion was here */
      if (_mType != TYPE(LST_ERROR)) {
        // don't re-warn about undeclared if we already know we're broken.
        NODE_ERROR(this, E_UNDECLARED, _mName);
      }
    }

    // Set our symbol to null and type to error since we don't know what they should be.
    _mSymbol = nullptr;
    _mType = TYPE(LST_ERROR);
    return;
  }

  // Set our type to our symbol's type.
  _mType = _mSymbol->getType();
}

void LSLASTNode::checkSymbols() {
  if (getSymbolTable() != nullptr)
    getSymbolTable()->checkSymbols();

  for (auto *node : *this)
    node->checkSymbols();
}


class NodeReferenceUpdatingVisitor : public ASTVisitor {
  public:
    virtual bool visit(LSLExpression *expr) {
      if (operation_mutates(expr->getOperation())) {
        auto *child = (LSLLValueExpression *)expr->getChild(0);
        assert(child->getNodeSubType() == NODE_LVALUE_EXPRESSION);
        // track the assignment
        auto *sym = child->getSymbol();
        if (!sym || sym->getSubType() == SYM_BUILTIN) {
          // make sure we don't muck with the assignment count on a builtin symbol!
          return true;
        }
        sym->addAssignment();
      }
      return true;
    };

    virtual bool visit(LSLIdentifier *id) {
      LSLASTNode *upper_node = id->getParent();
      while (upper_node != nullptr) {
        // HACK: Make recursive calls not count as a reference, won't handle mutual recursion!
        if (upper_node->getNodeType() == NODE_GLOBAL_FUNCTION) {
          auto *ident = (LSLIdentifier *) upper_node->getChild(0);
          if (ident != id && ident->getSymbol() == id->getSymbol())
            return false;
        }
        upper_node = upper_node->getParent();
      }
      if (auto *symbol = id->getSymbol()) {
        // Don't touch builtins, refcounts don't matter there and they may be used
        // cross-thread.
        if (symbol->getSubType() != SYM_BUILTIN) {
          symbol->addReference();
        }
      }
      return false;
    };
};

void LSLScript::recalculateReferenceData() {
  // get updated mutation / reference counts
  mContext->table_manager->resetTracking();
  auto visitor = NodeReferenceUpdatingVisitor();
  visit(&visitor);
}

void LSLScript::optimize(const OptimizationOptions &ctx) {
  int optimized;
  do {
    TreeSimplifyingVisitor folding_visitor(ctx);
    visit(&folding_visitor);
    optimized = folding_visitor.mFoldedLevel;
  } while (optimized);
}


void LSLScript::validateGlobals(bool mono_semantics) {
  SimpleAssignableValidatingVisitor visitor(mono_semantics);
  visit(&visitor);
}


LSLConstant *LSLIdentifier::getConstantValue() {
  if (_mSymbol && _mSymbol->getAssignments() == 0)
    return _mSymbol->getConstantValue();
  return nullptr;
}


LSLConstant *LSLGlobalVariable::getConstantValue() {
  // It's not really constant if it gets mutated more than once, is it?
  // note that initialization during declaration doesn't count.
  if (getSymbol()->getAssignments() == 0) {
    return _mConstantValue;
  }
  return nullptr;
}

LSLConstant *LSLDeclaration::getConstantValue() {
  if (getSymbol()->getAssignments() == 0) {
    return _mConstantValue;
  }
  return nullptr;
}

LSLConstant *LSLExpression::getConstantValue() {
  // replacing `foo = "bar"` with `"bar"` == no
  if (!operation_mutates(_mOperation)) {
    return _mConstantValue;
  }
  return nullptr;
}


LSLConstant *LSLLValueExpression::getConstantValue() {
  if (_mIsFoldable) {
    if (_mInGlobalContext && getSymbol()) {
      // We don't have to be careful about lists in global context,
      //  any constant value would just be a pointer to a previous list's data,
      //  so we don't have to worry about the quadratic explosion nonsense of
      //  the non-global case.
      if (_mConstantValue)
        return _mConstantValue;
      return getSymbol()->getInitialValue();
    }

    // We have to be careful about folding lists
    if (getIType() == LST_LIST) {
      LSLASTNode *top_foldable = this;
      LSLASTNode *current_node = this;

      // Don't fold this in if it's a list expression at the foldable level
      while (current_node != nullptr && current_node->nodeAllowsFolding()) {
        top_foldable = current_node;
        current_node = current_node->getParent();
      }
      if (top_foldable->getIType() == LST_LIST)
        return nullptr;
    }

    return _mConstantValue;
  }
  return nullptr;
}

bool LSLFloatConstant::containsNaN() {
  return std::isnan(getValue());
}

bool LSLVectorConstant::containsNaN() {
  return std::isnan(_mValue.x) || std::isnan(_mValue.y) || std::isnan(_mValue.z);
}

bool LSLQuaternionConstant::containsNaN() {
  return std::isnan(_mValue.x) || std::isnan(_mValue.y) || std::isnan(_mValue.z) || std::isnan(_mValue.s);
}

LSLIdentifier *LSLIdentifier::clone() {
  auto *id = mContext->allocator->newTracked<LSLIdentifier>(
      getType(),
      getName(),
      getLoc()
  );
  id->setSymbol(getSymbol());
  id->setConstantPrecluded(getConstantPrecluded());
  id->setConstantValue(getConstantValue());
  return id;
}

LSLLValueExpression *LSLLValueExpression::clone() {
  // TODO: this isn't very nice, need to think about copy constructors.
  auto *id = getIdentifier();
  auto *accessor = getMember();
  id = id->clone();
  if (accessor)
    accessor = accessor->clone();
  auto *new_expr = mContext->allocator->newTracked<LSLLValueExpression>(id, accessor);
  new_expr->setLoc(getLoc());
  new_expr->setType(getType());
  new_expr->setIsFoldable(getIsFoldable());
  new_expr->setConstantValue(getConstantValue());
  new_expr->setConstantPrecluded(getConstantPrecluded());
  new_expr->setInGlobalContext(getInGlobalContext());
  return new_expr;
}

}
