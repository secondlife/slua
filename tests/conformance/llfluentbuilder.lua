-- Test suite for the generic fluent builder runtime (llfluent_builder.cpp).
--
-- Uses a mock "testmodule.TestObj" registered by the SLConformance test harness:
--   fields:  flags (i,0), amount (f,1), color (v,2), name (k,3), count (i,4)
--   flags:   active (0x1, field 0), looping (0x2, field 0)
--   apply:   ll.TestApply(rules) / ll.LinkTestApply(link, rules)
--
-- No particle system or SL-specific constants anywhere in this file.

local TestObj = testmodule.TestObj

-- ── Construction ────────────────────────────────────────────────────────────

-- new() returns a fresh table with the metatable attached.
local obj = TestObj.new()
assert(type(obj) == "table")

-- setmetatable works identically to new() — both are valid.
local obj2 = setmetatable({}, TestObj)
assert(type(obj2) == "table")

-- ── Property round-trips ─────────────────────────────────────────────────────

-- float
obj = TestObj.new()
obj.amount = 0.5
assert(obj.amount == 0.5)

-- vector
obj = TestObj.new()
obj.color = vector(1, 0.5, 0)
assert(obj.color == vector(1, 0.5, 0))

-- integer
obj = TestObj.new()
obj.count = 7
assert(obj.count == 7)

-- key (accepts string)
obj = TestObj.new()
obj.name = "00000000-0000-0000-0000-000000000000"
assert(obj.name == "00000000-0000-0000-0000-000000000000")

-- key (accepts uuid)
obj = TestObj.new()
local id = uuid("00000000-0000-0000-0000-000000000001")
obj.name = id
assert(obj.name == id)

-- raw integer flags field
obj = TestObj.new()
obj.flags = 42
assert(obj.flags == 42)

-- ── Unset properties read as nil ─────────────────────────────────────────────

obj = TestObj.new()
assert(obj.amount == nil)
assert(obj.color  == nil)
assert(obj.count  == nil)
assert(obj.name   == nil)

-- ── Type errors on assignment ────────────────────────────────────────────────

local function assert_error(fn, snippet)
    local ok, err = pcall(fn)
    assert(not ok, "expected error for: " .. snippet)
end

obj = TestObj.new()
assert_error(function() obj.amount = "notanumber" end, "amount = string")
assert_error(function() obj.amount = vector(1,0,0)  end, "amount = vector")
assert_error(function() obj.color  = 1.0             end, "color = number")
assert_error(function() obj.color  = "notavec"       end, "color = string")
-- Note: luaL_checkinteger accepts any number (floats are truncated), so only
-- non-number types are rejected for integer fields.
assert_error(function() obj.count  = "three"          end, "count = string")
assert_error(function() obj.count  = vector(0,0,0)    end, "count = vector")

-- ── Unknown property errors ──────────────────────────────────────────────────

assert_error(function() obj.nonexistent = 1 end, "unknown property")

-- ── Flag properties: write ───────────────────────────────────────────────────

-- Setting a flag true ORs the bit into flags.
obj = TestObj.new()
obj.active = true
assert(obj.flags == 0x1)

obj = TestObj.new()
obj.looping = true
assert(obj.flags == 0x2)

-- Both flags together.
obj = TestObj.new()
obj.active  = true
obj.looping = true
assert(obj.flags == 0x3)

-- Setting false clears the bit.
obj = TestObj.new()
obj.flags = 0x3
obj.active = false
assert(obj.flags == 0x2)

-- Nil also clears the bit.
obj = TestObj.new()
obj.flags = 0x3
obj.looping = nil
assert(obj.flags == 0x1)

-- Integer 0 clears the bit; non-zero sets it.
obj = TestObj.new()
obj.active = 1
assert(obj.flags == 0x1)
obj.active = 0
assert(obj.flags == 0x0)

-- ── Flag properties: read ────────────────────────────────────────────────────

obj = TestObj.new()
obj.flags = 0x1
assert(obj.active  == true)
assert(obj.looping == false)

obj.flags = 0x3
assert(obj.active  == true)
assert(obj.looping == true)

obj.flags = 0x0
assert(obj.active  == false)
assert(obj.looping == false)

-- ── Flag + raw flags coexistence ─────────────────────────────────────────────

-- Writing raw flags and reading back via boolean alias.
obj = TestObj.new()
obj.flags = 0x2
assert(obj.active  == false)
assert(obj.looping == true)

-- Writing via alias reflects in raw flags.
obj = TestObj.new()
obj.flags  = 0x0
obj.active = true
assert(obj.flags == 0x1)

-- Flag type error: only boolean or integer accepted.
assert_error(function() obj.active = "yes" end,    "active = string")
assert_error(function() obj.active = vector(1,0,0) end, "active = vector")

-- ── new({...}) initializer table ─────────────────────────────────────────────

obj = TestObj.new({
    amount  = 1.5,
    color   = vector(0, 1, 0),
    count   = 3,
    active  = true,
})
assert(obj.amount  == 1.5)
assert(obj.color   == vector(0, 1, 0))
assert(obj.count   == 3)
assert(obj.active  == true)
assert(obj.flags   == 0x1)

-- Unknown keys in the initializer are silently ignored (no error).
obj = TestObj.new({
    amount      = 2.0,
    unknown_key = "ignored",
    another_one = 99,
})
assert(obj.amount == 2.0)

-- ── apply() dispatch ─────────────────────────────────────────────────────────

-- apply() with no argument calls ll.TestApply(rules).
captured_apply_link  = "sentinel"
captured_apply_rules = nil
obj = TestObj.new()
obj.amount = 0.25
obj:apply()
assert(captured_apply_link  == nil,  "no-arg apply should not set link")
assert(captured_apply_rules ~= nil,  "rules table should be captured")

-- apply(linkNum) calls ll.LinkTestApply(linkNum, rules).
captured_apply_link  = nil
captured_apply_rules = nil
obj:apply(3)
assert(captured_apply_link  == 3,   "link number should be captured")
assert(captured_apply_rules ~= nil, "rules table should be captured")

-- apply() with different link numbers.
obj:apply(0)
assert(captured_apply_link == 0)
obj:apply(-1)
assert(captured_apply_link == -1)

-- ── Serialisation order follows tag, not insertion order ─────────────────────

-- Set properties in reverse-tag order; verify the serialized list is tag-sorted.
obj = TestObj.new()
obj.count  = 9    -- tag 4
obj.name   = "aa" -- tag 3
obj.color  = vector(1,2,3) -- tag 2
obj.amount = 0.1  -- tag 1
-- flags (tag 0) not set, so omitted from output

obj:apply()
local rules = captured_apply_rules
-- rules is a list: {tag0val, tag1val, tag2val, ...} pairs
-- tags present: 1,2,3,4 → indices 1-8
assert(rules[1] == 1,               "first tag should be 1 (amount)")
assert(rules[2] == 0.1,             "amount value")
assert(rules[3] == 2,               "second tag should be 2 (color)")
assert(rules[4] == vector(1,2,3),   "color value")
assert(rules[5] == 3,               "third tag should be 3 (name)")
assert(rules[6] == "aa",            "name value")
assert(rules[7] == 4,               "fourth tag should be 4 (count)")
assert(rules[8] == 9,               "count value")
assert(rules[9] == nil,             "no more entries")

-- Unset properties are omitted entirely.
obj2 = TestObj.new()
obj2.count = 1
obj2:apply()
rules = captured_apply_rules
assert(rules[1] == 4)   -- only tag 4
assert(rules[2] == 1)
assert(rules[3] == nil)

return "OK"
