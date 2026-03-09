local foo = integer(5)
local bar = integer(5)
local baz = integer(6)
local vec = vector(1, 2, 3)

assert(foo == bar)
assert(foo ~= baz)
assert(foo + integer(1) == baz)
assert(foo ~= 5)
assert(tonumber(foo) == 5)
assert(tostring(foo) == "5")
assert(integer("5") == foo)
assert(typeof(integer("5")) == "integer")
assert(integer(true) == integer(1))
assert(integer(false) == integer(0))
assert(integer(false, "nonsense") == integer(0))

assert(tovector("<1,2,3>") == vector(1, 2, 3))
assert(tovector("<1,2,4>") ~= vector(1, 2, 3))
assert(toquaternion("<1,2,3,4>") == quaternion(1, 2, 3, 4))
assert(toquaternion("<1,2,3,5>") ~= quaternion(1, 2, 3, 4))
-- Unlike Mono, we allow weird spacing around components
assert(toquaternion(" < 1 , 2 , 3 , 4 > ") == quaternion(1, 2, 3, 4))
assert(tovector(" < 1 , 2 , 3 > ") == vector(1, 2, 3))
-- Unlike mono, we detect trailing junk.
assert(tovector("<1,2,3foobar123") == nil)
assert(tovector("<1,2,3>c") == nil)
assert(tovector("<1,2,3,4>") == nil)
assert(tovector("<1,2,3> c") == nil)
assert(toquaternion("<1,2,3,4>c") == nil)

assert(typeof(quaternion(0, 0, 0, 1)) == "quaternion")
assert(tovector("", "nonsense") == nil)
assert(toquaternion("", "nonsense") == nil)
local nan_quat = quaternion(1, 2, 3, tonumber('nan'))
assert(nan_quat ~= nan_quat)
assert(tostring(nan_quat) == "<1, 2, 3, nan>")
local nan_vec = vector(1, 2, tonumber('nan'))
assert(nan_vec ~= nan_vec)
assert(tostring(nan_vec) == "<1, 2, nan>")

assert(not (uuid("00000000-0000-0000-0000-000000000001") == uuid("00000000-0000-0000-0000-000000000002")))
assert(uuid("00000000-0000-0000-0000-000000000001") ~= uuid("00000000-0000-0000-0000-000000000002"))
assert(uuid("00000000-0000-0000-0000-000000000001") == uuid("00000000-0000-0000-0000-000000000001"))
assert(not (uuid("00000000-0000-0000-0000-000000000001") ~= uuid("00000000-0000-0000-0000-000000000001")))
assert(typeof(uuid("00000000-0000-0000-0000-000000000001") == "uuid"))
assert(not uuid("00000000-0000-0000-0000-000000000000").istruthy)
assert(not uuid("00000000-0000-0000-0000-000000000000").istruthy)
assert(uuid("00000000-0000-0000-0000-00000000000a").istruthy)
-- Uppercase is now canonicalized to lowercase and compressed
assert(uuid("00000000-0000-0000-0000-00000000000A").istruthy)
-- uuid(uuid()) is a the identity function
assert(uuid(uuid("00000000-0000-0000-0000-00000000000a")).istruthy)

-- This is an integer in LSL, should be a float in Lua
assert(typeof(ll.Ceil(5)) == "number")

-- These should all return nil gracefully
assert(tovector(tovector) == nil)
assert(tovector(nil) == nil)
assert(toquaternion(nil) == nil)
assert(touuid(nil) == nil)
assert(touuid(ZERO_ROTATION) == nil)

assert(not pcall(function() tovector() end))
assert(not pcall(function() toquaternion() end))
assert(not pcall(function() integer() end))

-- Vector / quaternion operations should still be supported as in LSL
assert(vec * quaternion(0, 0, 0, 1) == vec)
-- Make sure our ll.StringLength is unicode-aware
assert(ll.StringLength("草") == 1)

return "OK"
