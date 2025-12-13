local quat = give_quaternion()
local nintey = quaternion(0,0,0.7071068,0.7071068)

local modules = {quaternion = quaternion, rotation = rotation}

for _, module in modules do

    assert(quat == module(1.0, 2.0, 3.0, 4.0))

    assert(module.create(1, 2, 3, 4) == module(1, 2, 3, 4))


    assert(`{module.normalize(module(3, 5, 2, 1))}` == "<0.480384, 0.800641, 0.320256, 0.160128>")
    assert(module.normalize(module(3, 5, 2, 1)) == module(0.4803844690322876, 0.8006408214569092, 0.3202563226222992, 0.1601281613111496))

    assert(module.magnitude(module(0, 0, 0, 1)) == 1)

    assert(module.dot(module(1, 2, 3, 4),module(0, 0, 0, 1)) == 4)

    assert(module.conjugate(nintey) == module(-nintey.x, -nintey.y, -nintey.z, nintey.s))

    assert(`{module.slerp(nintey, module(0, 0, 0, 1), 0.5)}` == "<0, 0, 0.382683, 0.92388>")

    --string cast to deal with precision
    assert(`{module.tofwd(nintey)}` == "<0, 1, 0>")
    assert(`{module.toleft(nintey)}` == "<-1, 0, 0>")
    assert(`{module.toup(nintey)}` == "<0, 0, 1>")

end
-- Check rotation module has same implementation as quaternion module
for k,_ in quaternion do
    assert(rotation[k] ~= nil)
end

return "OK"