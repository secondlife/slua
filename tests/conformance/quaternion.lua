local quat = give_quaternion()
local yaw_ninety = quaternion(0,0,0.7071068,0.7071068)

assert(quat == quaternion(1, 2, 3, 4))

-- Test string cast is expected format and precision
assert(`{quat}` == "<1, 2, 3, 4>")
assert(`{yaw_ninety}` == "<0, 0, 0.707107, 0.707107>")

-- Test both quaternion and rotation modules exist and can be used for creation the same way
assert(quaternion(1, 2, 3, 4) == quaternion(1, 2, 3, 4))
assert(quaternion.create(4, 3, 2, 1) == rotation.create(4, 3, 2, 1))

-- Test quaternion module functions
assert(quaternion.normalize(quaternion(3, 5, 2, 1)) == quaternion(0.4803844690322876, 0.8006408214569092, 0.3202563226222992, 0.1601281613111496))

assert(quaternion.magnitude(quaternion(0, 0, 0, 1)) == 1)

assert(quaternion.dot(quaternion(1, 2, 3, 4),quaternion(0, 0, 0, 1)) == 4)

assert(quaternion.conjugate(quat) == quaternion(-quat.x, -quat.y, -quat.z, quat.s))

assert(`{quaternion.slerp(yaw_ninety, quaternion(0, 0, 0, 1), 0.5)}` == "<0, 0, 0.382683, 0.92388>")

-- string cast to deal with precision
assert(`{quaternion.tofwd(yaw_ninety)}` == "<0, 1, 0>")
assert(`{quaternion.toleft(yaw_ninety)}` == "<-1, 0, 0>")
assert(`{quaternion.toup(yaw_ninety)}` == "<0, 0, 1>")

-- Check rotation module has same implementation as quaternion module
for k,_ in quaternion do
    assert(rotation[k] ~= nil)
end

return "OK"