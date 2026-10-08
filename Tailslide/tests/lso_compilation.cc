#include "doctest.hh"
#include "passes/lso/bytecode_format.hh"
#include "passes/lso/script_compiler.hh"
#include "tailslide.hh"
#include "testutils.hh"

namespace Tailslide {

TEST_SUITE_BEGIN("LSO");

TEST_CASE("Simple register initialization") {
  auto script = runConformance("constprop.lsl");
  LSOScriptCompiler visitor(&script->allocator);
  script->script->visit(&visitor);
  LSOBitStream script_bs(std::move(visitor.mScriptBS));

  script_bs.moveTo(LSO_REGISTER_OFFSETS[LREG_VN]);
  uint32_t version_num;
  script_bs >> version_num;
  CHECK_EQ(version_num, LSO_VERSION_NUM);
  CHECK_EQ(script_bs.size(), TOTAL_LSO_MEMORY);
}

TEST_CASE("Simple heap compilation") {
  // so we don't have to set up the context on the allocator
  ScopedScriptParser parser(nullptr);

  // create a list and write it (and its contents) to the heap
  auto *list_const = parser.allocator.newTracked<LSLListConstant>(nullptr);
  list_const->pushChild(parser.allocator.newTracked<LSLIntegerConstant>(1));
  auto *vec_child = parser.allocator.newTracked<LSLVectorConstant>(1.0f, 2.0f, 3.0f);
  list_const->pushChild(vec_child);
  LSOHeapManager heap_manager;
  CHECK_EQ(heap_manager.writeConstant(list_const), 1);

  LSOBitStream heap_bs(std::move(heap_manager.mHeapBS));
  CHECK_EQ(heap_bs.size(), 49);
  CHECK_EQ(heap_bs.pos(), 49);
  heap_bs.moveTo(0);
  uint32_t size, len, child_1_idx, child_2_idx;
  LSLIType type;
  uint16_t ref_count;
  heap_bs >> size >> type >> ref_count >> len >> child_1_idx >> child_2_idx;
  // 2 elements, so two 32-bit heap ptrs
  CHECK_EQ(size, 12);
  CHECK_EQ(type, LST_LIST);
  CHECK_EQ(ref_count, 1);
  CHECK_EQ(len, 2);
  // + 1 gets added to all indexes
  CHECK_EQ(child_1_idx, 20);
  CHECK_EQ(child_2_idx, 31);

  // move to the vector value on the heap
  float z;
  const uint32_t vec_pos = 30 + 4 + 1 + 2;
  heap_bs.moveTo(vec_pos);
  Vector3 v;
  heap_bs >> v;
  // make sure the vector matches what we meant to write
  CHECK_EQ(*vec_child->getValue(), v);

  // z comes first under LSO serialization
  heap_bs.moveTo(vec_pos);
  heap_bs >> z;
  CHECK_EQ(z, 3.0f);

  Quaternion q(0, 1, 2, 3);
  Quaternion new_q;
  heap_bs.moveTo(0);
  heap_bs << q;
  heap_bs.moveTo(0);
  heap_bs >> new_q;
  CHECK_EQ(new_q.y, 1);
}

TEST_CASE("Simple Global Var Serialization") {
  // so we don't have to set up the context on the allocator
  ScopedScriptParser parser(nullptr);

  // some global vars do implicit heap allocs
  LSOHeapManager heap_manager;
  LSOGlobalVarManager global_var_manager(&heap_manager);
  global_var_manager.writeVar(parser.allocator.newTracked<LSLIntegerConstant>(2));
  global_var_manager.writeVar(parser.allocator.newTracked<LSLKeyConstant>("foobar"));

  LSOBitStream heap_bs(std::move(heap_manager.mHeapBS));
  LSOBitStream globals_bs(std::move(global_var_manager.mGlobalsBS));
  CHECK_EQ(heap_bs.size(), 14);
  CHECK_EQ(globals_bs.size(), 20);

  uint32_t offset, val;
  LSLIType type;
  char nil;
  globals_bs.moveTo(0);
  globals_bs >> offset >> type >> nil >> val;
  CHECK_EQ(offset, 6);
  CHECK_EQ(type, LST_INTEGER);
  CHECK_EQ(nil, 0);
  CHECK_EQ(val, 2);

  globals_bs >> offset >> type >> nil >> val;
  CHECK_EQ(offset, 6);
  CHECK_EQ(type, LST_KEY);
  CHECK_EQ(nil, 0);
  // first entry on the heap
  CHECK_EQ(val, 1);

  heap_bs.moveTo(0);
  uint32_t size;
  uint16_t ref_count;
  heap_bs >> size >> type >> ref_count;
  CHECK_EQ(size, 7);  // foobar + nul
  // all keys get string heap entries
  CHECK_EQ(type, LST_STRING);
  CHECK_EQ(ref_count, 1);
  CHECK(!memcmp(heap_bs.current(), "foobar", 6));
}

TEST_CASE("Stack-Heap Collision") {
  auto script = runConformance("stack_heap_collide.lsl");
  LSOScriptCompiler visitor(&script->allocator);
  script->script->visit(&visitor);
  CHECK(getFilteredMessages(script).empty());
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("LSO conformance");

#define SIMPLE_LSO_CONFORMANCE(name) TEST_CASE((name)) { checkLSOOutput((name)); }

SIMPLE_LSO_CONFORMANCE("event_handler_order.lsl")
SIMPLE_LSO_CONFORMANCE("two_states.lsl")
SIMPLE_LSO_CONFORMANCE("various_globals.lsl")
SIMPLE_LSO_CONFORMANCE("lso_jump_behavior.lsl")
SIMPLE_LSO_CONFORMANCE("simple_function_defs.lsl")
SIMPLE_LSO_CONFORMANCE("constant_expressions.lsl")
SIMPLE_LSO_CONFORMANCE("branching.lsl")
SIMPLE_LSO_CONFORMANCE("simple_declarations.lsl")
SIMPLE_LSO_CONFORMANCE("implied_return.lsl")
SIMPLE_LSO_CONFORMANCE("lvalue.lsl")
SIMPLE_LSO_CONFORMANCE("xnary_expressions.lsl")
SIMPLE_LSO_CONFORMANCE("print_expression.lsl")
SIMPLE_LSO_CONFORMANCE("function_calls.lsl")
SIMPLE_LSO_CONFORMANCE("more_expressions.lsl")
SIMPLE_LSO_CONFORMANCE("desugaring.lsl")
SIMPLE_LSO_CONFORMANCE("state_change.lsl")
SIMPLE_LSO_CONFORMANCE("assignments.lsl")
SIMPLE_LSO_CONFORMANCE("casts.lsl")
SIMPLE_LSO_CONFORMANCE("event_handlers.lsl")
SIMPLE_LSO_CONFORMANCE("lsl_conformance.lsl")
SIMPLE_LSO_CONFORMANCE("lsl_conformance2.lsl")

TEST_SUITE_END();

}
