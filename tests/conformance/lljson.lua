assert(lljson.encode(lljson.null) == "null")
-- nil is unambiguous at the top level, so we can treat it as `null`
assert(lljson.encode(nil) == "null")
-- cjson encodes empty tables as objects by default
-- TODO: Is that what we want? You have to pick one or the other.
assert(lljson.encode({}) == "{}")
-- But you can specify you _really_ want a table to be treated as an array
-- by setting the array_mt metatable
assert(lljson.encode(setmetatable({}, lljson.array_mt)) == "[]")
-- `lljson.empty_array` is the same.
assert(lljson.encode(lljson.empty_array) == "[]")
assert(lljson.encode({1}) == "[1]")
assert(lljson.encode({integer(1)}) == "[1]")
assert(lljson.encode(true) == "true")
assert(lljson.encode({"foo"}) == '["foo"]')
-- UUIDs are basically just strings, encode them as such.
assert(lljson.encode({uuid("foo")}) == '["foo"]')
assert(lljson.encode({foo="bar"}) == '{"foo":"bar"}')
-- key -> nil is the same as deleting a key in Lua, we have no
-- way to distinguish between a key that has a nil value and
-- a non-existent key.
assert(lljson.encode({foo=nil}) == '{}')
-- But we can represent it explicitly with `lljson.null`
assert(lljson.encode({foo=lljson.null}) == '{"foo":null}')
assert(lljson.encode(vector(1, 2.5, 22.0 / 7.0)) == '"<1,2.5,3.14286>"')
assert(lljson.encode(quaternion(1, 2.5, 22.0 / 7.0, 4)) == '"<1,2.5,3.14286,4>"')

-- metatables are totally ignored
local SomeMT = {}
function SomeMT.whatever(...)
    error("Placeholder function called")
end
SomeMT.some_val = true
SomeMT.__index = SomeMT
assert(lljson.encode(setmetatable({foo="bar"}, SomeMT)) == '{"foo":"bar"}')

-- Can't have random types as keys, JSON only has string keys.
assert(not pcall(function() lljson.encode({[vector.one]=1}) end))
-- Sparse arrays are okay
assert(lljson.encode({[4]=1}) == '[null,null,null,1]')
-- But not _really_ sparse arrays
assert(not pcall(function() lljson.encode({[200]=1}) end))

-- Vector is allowed to have NaN because it turns into a string.
local nan_vec = vector(math.huge, -math.huge, 0/0)
assert(lljson.encode(nan_vec) == '"<inf,-inf,nan>"')

assert(lljson.decode('1') == 1)
assert(lljson.decode('null') == lljson.null)
-- null is always decoded as `lljson.null` so it's unambiguous. it's not `nil`!
assert(lljson.decode('null') ~= nil)
assert(lljson.decode("true") == true)
-- Yay, you can actually use unicode escapes.
assert(lljson.decode('"\\u0020"') == " ")
-- TODO: should this have the custom array metatable where appropriate?
--       could people override it if it did?
assert(getmetatable(lljson.decode('[]')) == nil)
assert(getmetatable(lljson.decode('{}')) == nil)
assert(lljson.decode('[5]')[1] == 5)
assert(lljson.decode('{"foo":5}').foo == 5)
-- Don't automatically cast these
assert(lljson.decode('"<1,1,1>"') == "<1,1,1>")

local nan_vec_roundtripped = tovector(lljson.decode(lljson.encode(nan_vec)))
assert(nan_vec.x == nan_vec_roundtripped.x)
assert(nan_vec.y == nan_vec_roundtripped.y)
-- self-inequality check for NaN
assert(nan_vec_roundtripped.z ~= nan_vec_roundtripped.z)

local self_ref = {a=string.rep("a", 10000)}
self_ref.b = self_ref
-- Should refuse due to size constraints
assert(not pcall(function() lljson.encode(self_ref) end))

self_ref.a = nil

-- Should still refuse due to recursion constraints
assert(not pcall(function() lljson.encode(self_ref) end))

-- Long string literal, not allowed, total payload is too large.
local long_str = '"' .. string.rep("a", 100000) .. '"'
assert(not pcall(function() lljson.decode(long_str) end))

-- Can encode NaNs (non-standard, NaN literal has no JSON representation)
assert(lljson.encode(0/0) == "NaN")
-- We can also decode them
local nan_decoded = lljson.decode("nan")
assert(nan_decoded ~= nan_decoded)

-- Infinity is also a little special due to not having a real JSON representation
assert(lljson.encode(math.huge) == "1e9999")
assert(lljson.decode("1e9999") == math.huge)
assert(lljson.encode(-math.huge) == "-1e9999")
-- I guess we'll parse a bare inf if we see it, why not.
assert(lljson.decode("inf") == math.huge)
assert(lljson.decode("-inf") == -math.huge)

-- Buffers are fine, they get base64 encoded.
local buf = buffer.create(4)
buffer.writeu8(buf, 0, 0x1)
buffer.writeu8(buf, 2, 0xff)
assert(lljson.encode(buf) == '"AQD/AA=="')

-- Can't encode a function, how would that even work?
assert(not pcall(function() lljson.encode(function()end) end))

local ReturnVecJsonMeta = {}
function ReturnVecJsonMeta:__tojson()
    return self.foo
end

local ReturnTableJsonMeta = {}
function ReturnTableJsonMeta:__tojson()
    return {self.foo, "baz"}
end

local sampleObj = {
    foo=vector(1, 2, 3),
    bar="ignored",
}

assert(lljson.encode(setmetatable(sampleObj, ReturnVecJsonMeta)), '"<1,2,3>"')
assert(lljson.encode(setmetatable(sampleObj, ReturnTableJsonMeta)), '["<1,2,3>","baz"]')
-- setmetatable mutates, so we don't need to call it again.
assert(lljson.encode({sampleObj, sampleObj}), '[["<1,2,3>","baz"],["<1,2,3>","baz"]]')

return 'OK'
