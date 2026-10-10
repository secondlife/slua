-- Test suite for the llprim module, currently just llprim.ParamSetter.

local ParamSetter = llprim.ParamSetter

-- `setmetatable()` should work just fine, `.new()` is just sugar for it.
local rules = setmetatable({}, ParamSetter)

-- Chaining returns self, the table is the rules list
assert(rules:size(vector(1, 2, 3)) == rules)
assert(#rules == 2)
assert(rules[1] == PRIM_SIZE)
assert(rules[2] == vector(1, 2, 3))

-- Multiple rules accumulate in order.
rules = ParamSetter.new()
  :pos(vector(0, 0, 0))
  :size(vector(1, 1, 1))
  :physicsMaterial(2)
assert(#rules == 6)
assert(rules[1] == PRIM_POSITION and rules[2] == vector(0, 0, 0))
assert(rules[3] == PRIM_SIZE and rules[4] == vector(1, 1, 1))
assert(rules[5] == PRIM_MATERIAL and rules[6] == 2)

-- face_target rules take a face index as the first arg.
rules = ParamSetter.new():color(0, vector(1, 0.5, 0), 0.75)
assert(#rules == 4)
assert(rules[1] == PRIM_COLOR)
assert(rules[2] == 0)
assert(rules[3] == vector(1, 0.5, 0))
assert(rules[4] == 0.75)

-- PRIM_TYPE variants prepend the discriminator automatically
rules = ParamSetter.new():primTypeBox(
    0,
    vector(0, 1, 0),
    0.0,
    vector(0, 0, 0),
    vector(1, 1, 0),
    vector(0, 0, 0)
)
assert(#rules == 8)
assert(rules[1] == PRIM_TYPE and rules[2] == PRIM_TYPE_BOX)
assert(rules[3] == 0 and rules[8] == vector(0, 0, 0))

-- Boolean semantic accepts bool or int and stores native integer.
-- Technically allows any integer, though, as that's the case with
-- the underlying SPP implementation.
rules = ParamSetter.new():physical(true)
assert(rules[1] == PRIM_PHYSICS and rules[2] == 1)
rules = ParamSetter.new():physical(false)
assert(rules[1] == PRIM_PHYSICS and rules[2] == 0)
rules = ParamSetter.new():physical(1)
assert(rules[1] == PRIM_PHYSICS and rules[2] == 1)

-- String semantic.
rules = ParamSetter.new():name("foo")
assert(rules[1] == PRIM_NAME and rules[2] == "foo")

-- Rotation semantic
rules = ParamSetter.new():rot(quaternion(0, 0, 0, 1))
assert(rules[1] == PRIM_ROTATION)
assert(rules[2] == quaternion(0, 0, 0, 1))

-- Type mismatches are raised from the wrapper itself.
assert(not pcall(function() ParamSetter.new():size("not a vector") end))
assert(not pcall(function() ParamSetter.new():color("face", vector(1,1,1), 1.0) end))
-- Make sure we don't do the stupid number->string coercion that Lua likes to do
assert(not pcall(function() ParamSetter.new():name(42) end))

-- Non-nullable rules reject nil.
assert(not pcall(function() ParamSetter.new():size(nil) end))

-- The GLTF rulesets allow specifying `""` to clear overrides for basically all rules.
-- Mixed: some args real, others "".
rules = ParamSetter.new():gltfBaseColor(1, "sometexture", "", "", 0.0,
    vector(1, 1, 1), "", 0, "", true)
assert(rules[1] == PRIM_GLTF_BASE_COLOR)
assert(rules[2] == 1)
assert(rules[3] == "sometexture")
assert(rules[4] == "" and rules[5] == "")
assert(rules[6] == 0.0)
assert(rules[7] == vector(1, 1, 1))
assert(rules[8] == "")
assert(rules[9] == 0)
assert(rules[10] == "")
assert(rules[11] == 1)  -- true -> 1

-- extend() appends a plain rule list and returns self for chaining.
rules = ParamSetter.new():pos(vector(1, 2, 3))
assert(rules:extend({PRIM_SIZE, vector(1, 1, 1)}) == rules)
assert(#rules == 4)
assert(rules[3] == PRIM_SIZE and rules[4] == vector(1, 1, 1))

-- Another ParamSetter is just a rule list too, and the source is untouched.
local other = ParamSetter.new():physicsMaterial(2)
assert(rules:extend(other):name("foo") == rules)
assert(#rules == 8)
assert(rules[5] == PRIM_MATERIAL and rules[6] == 2)
assert(rules[7] == PRIM_NAME and rules[8] == "foo")
assert(#other == 2)

-- Only tables are accepted as the source.
assert(not pcall(function() ParamSetter.new():extend("nope") end))
assert(not pcall(function() ParamSetter.new():extend() end))

-- Coverage check: every method advertised by the typed interface is a
-- function on the metatable.
local expected_methods = {
    "targetLink", "physicsMaterial", "physical", "temporary", "phantom",
    "pos", "size", "rot",
    "primTypeBox", "primTypeCylinder", "primTypePrism", "primTypeSphere",
    "primTypeTorus", "primTypeTube", "primTypeRing", "primTypeSculpt",
    "texture", "color", "shinyBump", "fullbright", "flexible", "texgen",
    "pointLight", "glow", "text", "name", "description", "rotLocal",
    "physicsShapeType", "omega", "posLocal", "slice", "specular", "normal",
    "alphaMode", "allowUnsit", "scriptedSitOnly", "sitTarget", "projector",
    "clickAction", "reflectionProbe", "gltfNormal", "gltfEmissive",
    "gltfMetallicRoughness", "gltfBaseColor", "renderMaterial", "sitFlags",
    "damage", "health", "collisionSound",
    "new", "apply", "extend",
}
for _, name in expected_methods do
    assert(type(ParamSetter[name]) == "function",
           `expected ParamSetter.{name} to be a function`)
end

-- apply() routes the rule list through ll.SetLinkPrimitiveParamsFast on
-- the base globals. The test harness installs a stub that captures its
-- args in the globals `captured_apply_link` / `captured_apply_rules`.
captured_apply_link = nil
captured_apply_rules = nil

rules = ParamSetter.new():pos(vector(1, 2, 3)):physicsMaterial(2)
rules:apply()
assert(captured_apply_link == LINK_THIS)
assert(captured_apply_rules == rules)

-- `apply()` shouldn't have cleared out the rules!
assert(#rules ~= 0)

-- Try again with a different target link in `apply()`.
captured_apply_link = nil
captured_apply_rules = nil

rules:apply(LINK_ROOT)
assert(captured_apply_link == LINK_ROOT)
assert(captured_apply_rules == rules)

return "OK"
