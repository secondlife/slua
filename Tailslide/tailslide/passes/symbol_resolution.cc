#include "symbol_resolution.hh"

namespace Tailslide {


bool SymbolResolutionVisitor::visit(LSLScript *script) {
  replaceSymbolTable(script, SYMTAB_GLOBAL);
  auto *globals = script->getGlobals();
  // all global var definitions are implicitly hoisted above function definitions
  // all functions and states have their declarations implicitly hoisted as well.
  // walk over just the global vars and prototypes of functions.
  for (auto *global : *globals) {
    if (global->getNodeType() == NODE_GLOBAL_VARIABLE) {
      _mInGlobals = true;
      global->visit(this);
      _mInGlobals = false;
    } else if (global->getNodeType() == NODE_GLOBAL_FUNCTION) {
      // just record the prototype of the function and don't descend for now.
      auto *global_func = (LSLGlobalFunction *)global;
      replaceSymbolTable(global_func, SYMTAB_FUNCTION);
      auto *identifier = global_func->getIdentifier();

      // define function in script scope since functions have their own scope
      identifier->setSymbol(_mAllocator->newTracked<LSLSymbol>(
          identifier->getName(),
          identifier->getType(),
          SYM_FUNCTION,
          SYM_GLOBAL,
          global_func->getLoc(),
          global_func->getArguments()
      ));
      script->defineSymbol(identifier->getSymbol());
    }
  }

  // now walk the states to register their prototypes
  auto *states = script->getStates();
  for (auto *state : *states) {
    replaceSymbolTable(state, SYMTAB_STATE);
    auto *identifier = ((LSLState *)state)->getIdentifier();
    identifier->setSymbol(_mAllocator->newTracked<LSLSymbol>(
        identifier->getName(), identifier->getType(), SYM_STATE, SYM_GLOBAL, identifier->getLoc()
    ));
    state->getParent()->defineSymbol(identifier->getSymbol());
  }

  // visit function bodies
  for (auto *global : *globals) {
    // we've already descended into global vars, don't re-visit.
    if (global->getNodeType() == NODE_GLOBAL_VARIABLE)
      continue;
    global->visit(this);
  }
  // then state bodies
  states->visit(this);
  return false;
}

bool SymbolResolutionVisitor::visit(LSLGlobalVariable *glob_var) {
  // descend first so we can resolve any symbol references present in the rvalue
  // before we've defined the identifier from the lvalue.
  // Necessary so things like `string foo = foo;` will error correctly.
  if (auto *initializer = glob_var->getInitializer())
    initializer->visit(this);

  auto *identifier = glob_var->getIdentifier();
  identifier->setSymbol(_mAllocator->newTracked<LSLSymbol>(
      identifier->getName(), identifier->getType(), SYM_VARIABLE, SYM_GLOBAL, glob_var->getLoc(), nullptr, glob_var));
  glob_var->defineSymbol(identifier->getSymbol());
  return false;
}

bool SymbolResolutionVisitor::visit(LSLDeclaration *decl_stmt) {
  // visit the rvalue first so we correctly handle things like
  // `string foo = foo;`
  if (auto *initializer = decl_stmt->getInitializer())
    initializer->visit(this);

  auto *identifier = decl_stmt->getIdentifier();
  identifier->setSymbol(_mAllocator->newTracked<LSLSymbol>(
      identifier->getName(), identifier->getType(), SYM_VARIABLE, SYM_LOCAL, decl_stmt->getLoc(), nullptr, decl_stmt));
  decl_stmt->defineSymbol(identifier->getSymbol());

  // if (1) string foo; isn't valid!
  if (!decl_stmt->getDeclarationAllowed()) {
    NODE_ERROR(decl_stmt, E_DECLARATION_INVALID_HERE, identifier->getSymbol()->getName());
  }
  return false;
}

/// replace the node's old symbol table, registering the new one.
void SymbolResolutionVisitor::replaceSymbolTable(LSLASTNode *node, LSLSymbolTableType symtab_type) {
  // TODO: unregister old table? need to figure out node copy semantics.
  auto *symtab = _mAllocator->newTracked<LSLSymbolTable>(symtab_type);
  node->setSymbolTable(symtab);
  node->mContext->table_manager->registerTable(symtab);
}

bool SymbolResolutionVisitor::visit(LSLLValueExpression *lvalue) {
  lvalue->setInGlobalContext(_mInGlobals);
  lvalue->getIdentifier()->resolveSymbol(SYM_VARIABLE);
  return false;
}

bool SymbolResolutionVisitor::visit(LSLFunctionExpression *func_expr) {
  func_expr->getIdentifier()->resolveSymbol(SYM_FUNCTION);
  return true;
}

bool SymbolResolutionVisitor::visit(LSLGlobalFunction *glob_func) {
  assert(_mPendingJumps.empty());
  visitChildren(glob_func);
  glob_func->getSymbolTable()->setLabels(_mCollectedLabels);
  resolvePendingJumps(glob_func);
  return false;
}

bool SymbolResolutionVisitor::visit(LSLEventHandler *handler) {
  replaceSymbolTable(handler, SYMTAB_FUNCTION);

  auto *id = handler->getIdentifier();
  // look for a prototype for this event in the builtin namespace
  auto *sym = handler->getRoot()->lookupSymbol(id->getName(), SYM_EVENT);
  if (sym) {
    id->setSymbol(_mAllocator->newTracked<LSLSymbol>(
        id->getName(), id->getType(), SYM_EVENT, SYM_BUILTIN, handler->getLoc(), handler->getArguments()
    ));
    // The handler's own symbol carries the event's number from the builtins
    id->getSymbol()->setEventIndex(sym->getEventIndex());
    handler->getParent()->defineSymbol(id->getSymbol());
  } else {
    NODE_ERROR(handler, E_INVALID_EVENT, id->getName());
  }

  assert(_mPendingJumps.empty());
  visitChildren(handler);
  handler->getSymbolTable()->setLabels(_mCollectedLabels);
  resolvePendingJumps(handler);
  return false;
}

static void register_func_param_symbols(LSLASTNode *proto, bool is_event) {
  for (auto *child : *proto) {
    auto *identifier = (LSLIdentifier *) child;
    identifier->setSymbol(proto->mContext->allocator->newTracked<LSLSymbol>(
        identifier->getName(),
        identifier->getType(),
        SYM_VARIABLE,
        is_event ? SYM_EVENT_PARAMETER : SYM_FUNCTION_PARAMETER,
        child->getLoc()
    ));
    proto->defineSymbol(identifier->getSymbol());
  }
}

bool SymbolResolutionVisitor::visit(LSLFunctionDec *func_dec) {
  register_func_param_symbols(func_dec, false);
  return true;
}

bool SymbolResolutionVisitor::visit(LSLEventDec *event_dec) {
  register_func_param_symbols(event_dec, true);
  return true;
}

bool SymbolResolutionVisitor::visit(LSLLabel *label_stmt) {
  auto *identifier = label_stmt->getIdentifier();
  identifier->setSymbol(_mAllocator->newTracked<LSLSymbol>(
      identifier->getName(), identifier->getType(), SYM_LABEL, SYM_LOCAL, label_stmt->getLoc(), nullptr, nullptr, label_stmt
  ));
  label_stmt->defineSymbol(identifier->getSymbol());
  _mCollectedLabels.emplace_back(label_stmt);
  _mEnclosingLoops[label_stmt] = _mCurrentLoop;
  return true;
}

bool SymbolResolutionVisitor::visit(LSLJumpStatement *jump_stmt) {
  // Jumps are special. Since they can jump forwards in the control flow, we
  // can only resolve the labels they refer to after we leave the enclosing
  // function or event handler, having passed the last place the label it
  // refers to could have been defined.
  _mPendingJumps.emplace_back(jump_stmt);
  _mEnclosingLoops[jump_stmt] = _mCurrentLoop;
  return true;
}

bool SymbolResolutionVisitor::visit(LSLStateStatement *state_stmt) {
  state_stmt->getIdentifier()->resolveSymbol(SYM_STATE);
  return true;
}

bool SymbolResolutionVisitor::visit(LSLCompoundStatement *compound_stmt) {
  replaceSymbolTable(compound_stmt, SYMTAB_LEXICAL);
  return true;
}

bool SymbolResolutionVisitor::visit(LSLDoStatement *do_stmt) {
  visitLoop(do_stmt);
  return false;
}
bool SymbolResolutionVisitor::visit(LSLWhileStatement *while_stmt) {
  visitLoop(while_stmt);
  return false;
}
bool SymbolResolutionVisitor::visit(LSLForStatement *for_stmt) {
  visitLoop(for_stmt);
  return false;
}

void SymbolResolutionVisitor::visitLoop(LSLASTNode *loop_stmt) {
  _mCurrentLoop = loop_stmt;
  visitChildren(loop_stmt);
  _mCurrentLoop = nullptr;
}

void SymbolResolutionVisitor::resolvePendingJumps(LSLASTNode *func_like) {
  for (auto *jump : _mPendingJumps) {
    auto *id = jump->getIdentifier();
    // First do the lookup by lexical scope, triggering an error if it fails.
    id->resolveSymbol(SYM_LABEL);

    // That's all we have to do unless we want to match SL exactly.
    if (!_mLindenJumpSemantics)
      continue;

    // Labels in SL are weird in that they pretend they're lexically scoped but they
    // really aren't in either LSO or Mono. The label you're jumping to _must_
    // be in the lexical scope of your `jump`, but it will actually jump to the
    // last occurrence of a label with a given name within the function body,
    // crossing lexical scope boundaries. This was likely a mistake, but we have
    // to deal with it if we want the same `jump` semantics as LSO. Gnarly.

    // Note that Mono will actually fail to compile correctly on duplicated labels.
    // because they're included in the CIL verbatim.

    if (auto *orig_sym = id->getSymbol()) {
      LSLSymbol *new_sym = nullptr;
      // Now get the label this will jump to in SL, iterate in reverse so the last
      // instance of a label in a function will come first
      for (auto i = _mCollectedLabels.rbegin(); i != _mCollectedLabels.rend(); ++i) {
        auto *cand_sym = (*i)->getSymbol();
        if (!cand_sym || cand_sym->getSymbolType() != SYM_LABEL)
          continue;
        // name matches
        if (!strcmp(cand_sym->getName(), orig_sym->getName())) {
          new_sym = cand_sym;
          break;
        }
      }
      assert(new_sym);
      // This jump specifically will jump to a label other than the one you might expect,
      // so warn on that along with the general warning for duplicate label names.
      if (new_sym != orig_sym) {
        NODE_ERROR(id, W_JUMP_TO_WRONG_LABEL, orig_sym->getName());
      }
      id->setSymbol(new_sym);
    }
  }

  bool has_duplicate_labels = false;
  if (_mLindenJumpSemantics) {
    // Walk the list of collected labels and warn on any duplicated names
    std::set<std::string> label_names;
    for (auto label: _mCollectedLabels) {
      auto *label_id = label->getIdentifier();
      if (label_names.find(label_id->getName()) != label_names.end()) {
        NODE_ERROR(label_id, W_DUPLICATE_LABEL_NAME, label_id->getName());
        has_duplicate_labels = true;
      } else {
        label_names.insert(label_id->getName());
      }
    }
  }

  // Figure out if this jump is break-like or continue-like, and can be represented in languages
  // that only have break / continue and not unstructured jumps. Otherwise the block has to be
  // transformed into a state machine to be representable. Nightmare!
  // Basically:
  //  * if the target label immediately follows the innermost loop statement enclosing
  //    the jump statement then the jump is considered "break-like"
  //  * if the target label occurs at the back edge within the innermost loop statement
  //    enclosing the jump statement then the jump is considered "continue-like"
  //  Enclosing statements themselves aren't considered for back edge detection,
  //  so something like `while(1){if(something){jump foo; 1; @foo;}}` is still considered to be continue-like.
  //  while something like `while(1){jump foo; 1; @foo; {}}`
  //  or `while(1){if(something){jump foo; 1; @foo;}else{}}` is not.
  // If there are duplicate labels (only really possible with linden semantics),
  // we can't reliably determine structured jumps, so treat as unstructured.
  bool has_unstructured_jumps = has_duplicate_labels;
  if (!has_duplicate_labels) {
    for (auto jump : _mPendingJumps) {
      auto *sym = jump->getSymbol();
      if (!sym)
        continue;
      auto *label = sym->getLabelDecl();
      if (!label) {
        has_unstructured_jumps = true;
        continue;
      }

      LSLASTNode *jump_loop = _mEnclosingLoops[jump];

      // if the jump didn't happen in a loop then there's no chance of it being a structured jump.
      if (!jump_loop) {
        has_unstructured_jumps = true;
        continue;
      }

      // If enclosing loop for the jump has an immediate follower, and it's the target label,
      // then this jump is break-like. This plays nicely with multiple following labels.
      LSLASTNode *jump_loop_follower = jump_loop->getNext();
      bool label_follows_loop = false;
      while (jump_loop_follower && jump_loop_follower->getNodeSubType() == NODE_LABEL) {
        if (jump_loop_follower == label) {
          jump->setIsBreakLike(true);
          label_follows_loop = true;
          break;
        }
        jump_loop_follower = jump_loop_follower->getNext();
      }
      if (label_follows_loop)
        continue;

      LSLASTNode *label_loop = _mEnclosingLoops[label];
      // no chance of this being the last statement in jump's enclosing loop, so can't be break-like.
      if (!label_loop || label_loop != jump_loop) {
        has_unstructured_jumps = true;
        continue;
      }

      bool label_is_last = true;
      LSLASTNode *cur_node = label;

      // walk up until we hit the enclosing loop
      while (cur_node != label_loop) {
        // specifically doesn't check the enclosing statement!
        auto *cur_child = cur_node;
        while((cur_child = cur_child->getNext())) {
          // If a non-label statement follows then the label can't be the last statement
          // within the loop. Allows for things like empty `else` branches with null nodes.
          if (cur_child->getNodeType() == NODE_STATEMENT && cur_child->getNodeSubType() != NODE_LABEL) {
            label_is_last = false;
            break;
          }
        }
        if (!label_is_last)
          break;
        cur_node = cur_node->getParent();
      }
      jump->setIsContinueLike(label_is_last);
      if (!label_is_last)
        has_unstructured_jumps = true;
    }
  }

  if (auto *func_sym = func_like->getSymbol()) {
    func_sym->setHasJumps(!_mPendingJumps.empty());
    func_sym->setHasUnstructuredJumps(has_unstructured_jumps);
  }

  _mPendingJumps.clear();
  _mCollectedLabels.clear();
}

}
