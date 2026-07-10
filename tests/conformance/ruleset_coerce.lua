-- Conformance tests for slua_ruleset_coerce().
--
-- The C++ harness registers test_coerce(value) which calls slua_ruleset_coerce
-- on the input and returns (did_coerce, result).
-- Descriptor table: alpha='f'/1, name='s'/2, count='i'/3

-- ─── Dict tables should be coerced ───────────────────────────────────────────

-- Basic dict with multiple fields
local did, result = test_coerce({alpha = 0.5, name = "test", count = 42})
assert(did == true, "dict should be coerced")
assert(type(result) == "table", "result should be a table")
-- Verify serialized output: tags in order 1, 2, 3
assert(result[1] == 1, "first tag should be alpha (1)")
assert(result[2] == 0.5, "alpha value")
assert(result[3] == 2, "second tag should be name (2)")
assert(result[4] == "test", "name value")
assert(result[5] == 3, "third tag should be count (3)")
assert(result[6] == 42, "count value")

-- Single field dict
local did2, result2 = test_coerce({name = "solo"})
assert(did2 == true, "single-field dict should be coerced")
assert(result2[1] == 2 and result2[2] == "solo", "single field serialized correctly")

-- ─── Sequential lists should pass through unchanged ──────────────────────────

-- Flat rules list (has integer key 1)
local did3, result3 = test_coerce({1, 0.5, 2, "test"})
assert(did3 == false, "sequential list should not be coerced")
assert(result3[1] == 1, "list unchanged")
assert(result3[2] == 0.5, "list unchanged")
assert(result3[3] == 2, "list unchanged")
assert(result3[4] == "test", "list unchanged")

-- ─── Empty table should pass through unchanged ───────────────────────────────

local did4, result4 = test_coerce({})
assert(did4 == false, "empty table should not be coerced")
assert(type(result4) == "table", "empty table passed through")
assert(next(result4) == nil, "table is still empty")

-- ─── Non-table values should pass through unchanged ──────────────────────────

local did5, result5 = test_coerce(nil)
assert(did5 == false, "nil should not be coerced")
assert(result5 == nil, "nil passed through")

local did6, result6 = test_coerce(123)
assert(did6 == false, "number should not be coerced")
assert(result6 == 123, "number passed through")

local did7, result7 = test_coerce("hello")
assert(did7 == false, "string should not be coerced")
assert(result7 == "hello", "string passed through")

return "OK"
