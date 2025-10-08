local array = setmetatable({}, lljson.array_mt)
local empty_array = setmetatable({}, lljson.empty_array_mt)


function roundtrip_persist(val)
    return ares.unpersist(ares.persist(val))
end

function unpersisted_metatable(val)
    return getmetatable(ares.unpersist(ares.persist(val)))
end

assert(unpersisted_metatable(array) == lljson.array_mt)
assert(unpersisted_metatable(empty_array) == lljson.empty_array_mt)

local vec_mul = getmetatable(vector(1,2,3)).__mul
local quat_mul = getmetatable(quaternion(1,2,3,4)).__mul

assert(roundtrip_persist(vec_mul) == vec_mul)
assert(roundtrip_persist(quat_mul) == quat_mul)

return 'OK'
