-- Conformance test for llprim.ParticleSystem().
-- The C++ test harness registers a mock ll.LinkParticleSystem that stores
-- _captured_link and _captured_rules as globals, and registers llprim
-- with a subset of particle descriptors:
--   flags      'i' tag 0  (backing field for flag bits)
--   color_begin 'v' tag 1
--   alpha_begin 'f' tag 2
--   burst_rate  'f' tag 13
-- Flag descriptors (all backed by tag 0):
--   color_interp  0x001
--   scale_interp  0x002
--   bounce        0x004
--   emissive      0x100

-- Helper: call and return captured state.
local function dispatch(params, link)
    if link ~= nil then
        llprim.ParticleSystem(params, link)
    else
        llprim.ParticleSystem(params)
    end
    return _captured_link, _captured_rules
end

-- Test 1: nil params emits empty rules list, link defaults to LINK_THIS.
local link, rules = dispatch(nil)
assert(link == LINK_THIS, "nil params: link should be LINK_THIS")
assert(type(rules) == "table", "nil params: rules should be a table")
assert(#rules == 0, "nil params: rules should be empty")

-- Test 2: explicit link number is forwarded.
link, rules = dispatch(nil, 3)
assert(link == 3, "explicit link=3 should be forwarded")

-- Test 3: scalar float property is serialized with its tag.
link, rules = dispatch({ burst_rate = 2.5 })
assert(#rules == 2, "burst_rate: expected 2 elements")
assert(rules[1] == PSYS_SRC_BURST_RATE, "burst_rate: tag should be PSYS_SRC_BURST_RATE")
assert(rules[2] == 2.5, "burst_rate: value should be 2.5")

-- Test 4: vector property is serialized correctly.
local col = vector(1, 0.5, 0.25)
link, rules = dispatch({ color_begin = col })
assert(#rules == 2, "color_begin: expected 2 elements")
assert(rules[1] == PSYS_PART_START_COLOR, "color_begin: tag should be PSYS_PART_START_COLOR")
assert(rules[2] == col, "color_begin: value should be the vector")

-- Test 5: single flag boolean is merged into the flags integer field.
link, rules = dispatch({ color_interp = true })
assert(#rules == 2, "color_interp: expected 2 elements (flags field only)")
assert(rules[1] == PSYS_PART_FLAGS, "color_interp: tag should be PSYS_PART_FLAGS")
assert(rules[2] == PSYS_PART_INTERP_COLOR_MASK, "color_interp: value should be PSYS_PART_INTERP_COLOR_MASK")

-- Test 6: multiple flag booleans accumulate into a single flags field entry.
link, rules = dispatch({ color_interp = true, bounce = true })
assert(#rules == 2, "two flags: should still be one flags field entry")
assert(rules[1] == PSYS_PART_FLAGS, "two flags: tag should be PSYS_PART_FLAGS")
assert(rules[2] == bit32.bor(PSYS_PART_INTERP_COLOR_MASK, PSYS_PART_BOUNCE_MASK),
    "two flags: both bits should be set")

-- Test 7: raw flags integer and boolean flag properties are merged together.
link, rules = dispatch({ flags = PSYS_PART_EMISSIVE_MASK, bounce = true })
assert(#rules == 2, "raw+boolean flags: should be one flags field entry")
assert(rules[1] == PSYS_PART_FLAGS, "raw+boolean flags: tag should be PSYS_PART_FLAGS")
assert(rules[2] == bit32.bor(PSYS_PART_EMISSIVE_MASK, PSYS_PART_BOUNCE_MASK),
    "raw+boolean flags: both bits should be set")

-- Test 8: multiple params serialize in ascending tag order.
link, rules = dispatch({ burst_rate = 1.0, color_begin = vector(1, 0, 0) })
-- color_begin tag=1, burst_rate tag=13 — tag 1 must come first.
assert(#rules == 4, "two params: expected 4 elements")
assert(rules[1] == PSYS_PART_START_COLOR, "tag order: color_begin should come before burst_rate")
assert(rules[3] == PSYS_SRC_BURST_RATE, "tag order: burst_rate should be second")

-- Test 9: a sole flag set to false still emits PSYS_PART_FLAGS = 0.
-- Validates the clear path: the flags field is emitted even when the merged value is 0.
link, rules = dispatch({ color_interp = false })
assert(#rules == 2, "false flag only: expected 2 elements (flags field)")
assert(rules[1] == PSYS_PART_FLAGS, "false flag only: tag should be PSYS_PART_FLAGS")
assert(rules[2] == 0, "false flag only: value should be 0 (bit was cleared)")

-- Test 10: a false flag clears its bit from the raw flags integer.
-- Validates that false both removes its own mask and leaves other bits intact.
link, rules = dispatch({ flags = bit32.bor(PSYS_PART_INTERP_COLOR_MASK, PSYS_PART_BOUNCE_MASK), color_interp = false })
assert(#rules == 2, "false flag with raw: expected 2 elements")
assert(rules[1] == PSYS_PART_FLAGS, "false flag with raw: tag should be PSYS_PART_FLAGS")
assert(rules[2] == PSYS_PART_BOUNCE_MASK,
    "false flag with raw: color_interp bit should be cleared, bounce bit should remain")

return "OK"
