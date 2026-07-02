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

local function check(params, expected_rules)
    llprim.ParticleSystem(params)
    local got, want = lljson.slencode(_captured_rules), lljson.slencode(expected_rules)
    assert(got == want, `expected {want}, got {got}`)
end

-- Link handling: nil params emits empty rules and defaults to LINK_THIS;
-- an explicit link number is forwarded.
llprim.ParticleSystem(nil)
assert(_captured_link == LINK_THIS, `default link: got {_captured_link}`)
assert(lljson.slencode(_captured_rules) == "[]", "nil params: rules should be empty")
llprim.ParticleSystem(nil, 3)
assert(_captured_link == 3, `explicit link: got {_captured_link}`)

-- Scalar float and vector properties are serialized with their tags.
check({ burst_rate = 2.5 }, {PSYS_SRC_BURST_RATE, 2.5})
check(
    { color_begin = vector(1, 0.5, 0.25) },
    {PSYS_PART_START_COLOR, vector(1, 0.5, 0.25)}
)

-- Flag booleans merge into a single flags field entry.
check({ color_interp = true }, {PSYS_PART_FLAGS, PSYS_PART_INTERP_COLOR_MASK})
check(
    { color_interp = true, bounce = true },
    {PSYS_PART_FLAGS, bit32.bor(PSYS_PART_INTERP_COLOR_MASK, PSYS_PART_BOUNCE_MASK)}
)

-- Raw flags integer and boolean flag properties are merged together.
check(
    { flags = PSYS_PART_EMISSIVE_MASK, bounce = true },
    {PSYS_PART_FLAGS, bit32.bor(PSYS_PART_EMISSIVE_MASK, PSYS_PART_BOUNCE_MASK)}
)

-- Multiple params serialize in ascending tag order (color_begin tag=1 before burst_rate tag=13).
check(
    { burst_rate = 1.0, color_begin = vector(1, 0, 0) },
    {PSYS_PART_START_COLOR, vector(1, 0, 0), PSYS_SRC_BURST_RATE, 1.0}
)

-- A sole flag set to false still emits PSYS_PART_FLAGS = 0 (the clear path).
check({ color_interp = false }, {PSYS_PART_FLAGS, 0})

-- A false flag clears its own bit from the raw flags integer, leaving other bits intact.
check(
    { flags = bit32.bor(PSYS_PART_INTERP_COLOR_MASK, PSYS_PART_BOUNCE_MASK), color_interp = false },
    {PSYS_PART_FLAGS, PSYS_PART_BOUNCE_MASK}
)

return "OK"
