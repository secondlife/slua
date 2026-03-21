assert(lljson.encode(lljson.null) == "null")
-- nil is unambiguous at the top level, so we can treat it as `null`
assert(lljson.encode(nil) == "null")
-- Empty tables default to arrays
assert(lljson.encode({}) == "[]")
-- But you can specify you _really_ want a table to be treated as an array
-- by setting the array_mt metatable
assert(lljson.encode(setmetatable({}, lljson.array_mt)) == "[]")
-- Sentinel frozen tables for empty array/object
assert(lljson.encode(lljson.empty_array) == "[]")
assert(lljson.encode(lljson.empty_object) == "{}")
assert(lljson.encode({1}) == "[1]")
assert(lljson.encode({integer(1)}) == "[1]")
assert(lljson.encode(true) == "true")
assert(lljson.encode({"foo"}) == '["foo"]')
-- UUIDs are basically just strings, encode them as such.
assert(lljson.encode({uuid("00000000-0000-0000-0000-000000000001")}) == '["00000000-0000-0000-0000-000000000001"]')
assert(lljson.encode({foo="bar"}) == '{"foo":"bar"}')
-- key -> nil is the same as deleting a key in Lua, we have no
-- way to distinguish between a key that has a nil value and
-- a non-existent key.
assert(lljson.encode({foo=nil}) == '[]')
-- But we can represent it explicitly with `lljson.null`
assert(lljson.encode({foo=lljson.null}) == '{"foo":null}')
assert(lljson.encode(vector(1, 2.5, 22.0 / 7.0)) == '"<1,2.5,3.142857>"')
assert(lljson.encode(quaternion(1, 2.5, 22.0 / 7.0, 4)) == '"<1,2.5,3.142857,4>"')

-- metatables without __jsonhint/__tojson are ignored
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
-- Unless it's specifically an array
local sparse_result = `[{string.rep("null,", 199)}1]`
assert(lljson.encode(setmetatable({[200]=1}, lljson.array_mt)) == sparse_result)
-- Or via the allow_sparse option
assert(lljson.encode({[200]=1}, {allow_sparse=true}) == sparse_result)

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
-- decode() sets array_mt/object_mt for round-trippability
assert(getmetatable(lljson.decode('[]')) == lljson.array_mt)
assert(getmetatable(lljson.decode('{}')) == lljson.object_mt)
assert(getmetatable(lljson.decode('[1,2]')) == lljson.array_mt)
assert(getmetatable(lljson.decode('{"a":1}')) == lljson.object_mt)
-- round-trips preserve shape
assert(lljson.encode(lljson.decode('[]')) == '[]')
assert(lljson.encode(lljson.decode('{}')) == '{}')
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

-- NaN encodes as null in standard JSON (matches JSON.stringify)
assert(lljson.encode(0/0) == "null")
-- slencode uses tagged float for NaN round-trip
assert(lljson.slencode(0/0) == '"!fNaN"')
do
    local nan_rt = lljson.sldecode(lljson.slencode(0/0))
    assert(nan_rt ~= nan_rt, "NaN should round-trip through slencode/sldecode")
end
-- NaN in table values: encodes as null
assert(lljson.encode({0/0}) == '[null]')
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

-- ============================================
-- SL-specific tagged JSON (slencode/sldecode)
-- ============================================

-- Basic tagged encoding of values
assert(lljson.slencode(vector(1, 2, 3)) == '"!v<1,2,3>"')
assert(lljson.slencode(quaternion(1, 2, 3, 4)) == '"!q<1,2,3,4>"')
assert(lljson.slencode(uuid("12345678-1234-1234-1234-123456789abc")) == '"!u12345678-1234-1234-1234-123456789abc"')

-- Strings starting with ! get escaped
assert(lljson.slencode("!dangerous") == '"!!dangerous"')
assert(lljson.slencode("!") == '"!!"')
-- Normal strings are unchanged
assert(lljson.slencode("normal") == '"normal"')

-- Tagged decoding of values
assert(lljson.sldecode('"!v<1,2,3>"') == vector(1, 2, 3))
assert(lljson.sldecode('"!q<1,2,3,4>"') == quaternion(1, 2, 3, 4))
assert(lljson.sldecode('"!u12345678-1234-1234-1234-123456789abc"') == uuid("12345678-1234-1234-1234-123456789abc"))

-- Invalid tagged UUIDs must error
assert(not pcall(lljson.sldecode, '"!uOops"'))
assert(not pcall(lljson.sldecode, '"!u1234"'))
-- long enough, but invalid.
assert(not pcall(lljson.sldecode, '"!u00000000-0000-0000-0000-00000000000g"'))
-- uppercase accepted, it'll be canonicalized
local uppercase_uuid_str = '00000000-0000-0000-0000-00000000000A'
assert(lljson.sldecode(`"!u{uppercase_uuid_str}"`) == uuid(uppercase_uuid_str))

-- Escaped ! strings decode correctly
assert(lljson.sldecode('"!!dangerous"') == "!dangerous")
assert(lljson.sldecode('"!!"') == "!")

-- Non-tagged strings still work
assert(lljson.sldecode('"normal"') == "normal")

-- Round-trip tests for values
local test_vec = vector(1.5, -2.25, 3.14159)
assert(lljson.sldecode(lljson.slencode(test_vec)) == test_vec)

local test_quat = quaternion(0.1, 0.2, 0.3, 0.9)
assert(lljson.sldecode(lljson.slencode(test_quat)) == test_quat)

local test_uuid = uuid("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee")
assert(lljson.sldecode(lljson.slencode(test_uuid)) == test_uuid)

-- Tagged key round-trip: encode key, check JSON, decode and verify lookup
local function check_key_roundtrip(key, expected_json)
    local json = lljson.slencode({[key] = "value"})
    assert(json == expected_json, `expected {expected_json}, got {json}`)
    local decoded = lljson.sldecode(json)
    -- Try direct lookup first, fall back to iteration (for quaternion reference identity)
    if decoded[key] == "value" then return end
    for k, v in decoded do
        if k == key and v == "value" then return end
    end
    error(`key round-trip failed for {expected_json}`)
end

check_key_roundtrip(3.14, '{"!f3.14":"value"}')
check_key_roundtrip(vector(1, 2, 3), '{"!v<1,2,3>":"value"}')
check_key_roundtrip(quaternion(0, 0, 0, 1), '{"!q<0,0,0,1>":"value"}')
check_key_roundtrip(uuid("12345678-1234-1234-1234-123456789abc"),
    '{"!u12345678-1234-1234-1234-123456789abc":"value"}')
check_key_roundtrip("!bang", '{"!!bang":"value"}')
check_key_roundtrip(math.huge, '{"!f1e9999":"value"}')

-- Sparse tables: integer keys become !f tagged, avoiding sparse array issues
-- (contrast with regular encode which would fail or fill with nulls)
local sparse_table = {[1] = "first", [100] = "hundredth"}
local sparse_json = lljson.slencode(sparse_table)
-- Okay, this is a little obnoxious, but we don't really know in which order the
-- keys would be serialized. Just accept either/or.
assert(sparse_json == '{"!f1":"first","!f100":"hundredth"}' or sparse_json == '{"!f100":"hundredth","!f1":"first"}')
local sparse_decoded = lljson.sldecode(sparse_json)
assert(sparse_decoded[1] == "first")
assert(sparse_decoded[100] == "hundredth")

-- NaN can't be used as a table key in Lua ("table index is NaN" error)
-- so we can't test NaN keys - but NaN values in vectors still work:

-- Vectors with special float values
local special_vec = vector(math.huge, -math.huge, 0/0)
local special_vec_json = lljson.slencode(special_vec)
assert(special_vec_json == '"!v<inf,-inf,nan>"')
local special_vec_decoded = lljson.sldecode(special_vec_json)
assert(special_vec_decoded.x == math.huge)
assert(special_vec_decoded.y == -math.huge)
assert(special_vec_decoded.z ~= special_vec_decoded.z) -- NaN check

-- Error on malformed tags
assert(not pcall(function() lljson.sldecode('"!x<invalid>"') end))
assert(not pcall(function() lljson.sldecode('"!v<1,2>"') end)) -- missing component
assert(not pcall(function() lljson.sldecode('"!q<1,2,3>"') end)) -- missing component

-- Complex nested structure round-trip
local complex = {
    vec = vector(1, 2, 3),
    quat = quaternion(0, 0, 0, 1),
    id = uuid("00000000-0000-0000-0000-000000000001"),
    str = "hello",
    bang_str = "!escaped",
    nested = {
        inner_vec = vector(4, 5, 6),
    },
}
local complex_json = lljson.slencode(complex)
local complex_decoded = lljson.sldecode(complex_json)
assert(complex_decoded.vec == complex.vec)
assert(complex_decoded.quat == complex.quat)
assert(complex_decoded.id == complex.id)
assert(complex_decoded.str == complex.str)
assert(complex_decoded.bang_str == complex.bang_str)
assert(complex_decoded.nested.inner_vec == complex.nested.inner_vec)

-- Tagged buffers: !d with base64 data
local buf = buffer.create(4)
buffer.writeu8(buf, 0, 0x1)
buffer.writeu8(buf, 2, 0xff)
assert(lljson.slencode(buf) == '"!dAQD/AA=="')

-- Round-trip buffer
local buf_decoded = lljson.sldecode('"!dAQD/AA=="')
assert(buffer.len(buf_decoded) == 4)
assert(buffer.readu8(buf_decoded, 0) == 0x1)
assert(buffer.readu8(buf_decoded, 2) == 0xff)

-- Empty buffer
local empty_buf = buffer.create(0)
assert(lljson.slencode(empty_buf) == '"!d"')
local empty_decoded = lljson.sldecode('"!d"')
assert(buffer.len(empty_decoded) == 0)

-- Buffer as object key (no round-trip - buffers use reference identity)
assert(lljson.slencode({[buf] = "value"}) == '{"!dAQD/AA==":"value"}')

-- ============================================
-- Tight encoding mode ({tight = true})
-- ============================================

-- Tight vectors: no angle brackets
assert(lljson.slencode(vector(1, 2, 3), {tight = true}) == '"!v1,2,3"')

-- Tight vectors: zeros omitted
assert(lljson.slencode(vector(0, 0, 1), {tight = true}) == '"!v,,1"')
assert(lljson.slencode(vector(0, 0, 0), {tight = true}) == '"!v"')
assert(lljson.slencode(vector(1, 0, 0), {tight = true}) == '"!v1,,"')
assert(lljson.slencode(vector(0, 2, 0), {tight = true}) == '"!v,2,"')

-- Tight quaternions: no angle brackets, zeros omitted
assert(lljson.slencode(quaternion(0, 0, 0, 1), {tight = true}) == '"!q"')
assert(lljson.slencode(quaternion(1, 0, 0, 0), {tight = true}) == '"!q1,,,"')
assert(lljson.slencode(quaternion(0, 0, 0, 0), {tight = true}) == '"!q,,,"')

-- Tight UUIDs: base64 encoded (22 chars instead of 36)
local test_uuid = uuid("12345678-1234-1234-1234-123456789abc")
local tight_uuid_json = lljson.slencode(test_uuid, {tight = true})
-- Should be "!u" + 22 chars of base64
assert(#tight_uuid_json == 26)  -- 2 quotes + !u + 22 base64
assert(tight_uuid_json:sub(1, 3) == '"!u')

-- Tight null UUID: just "!u" with no payload
local null_uuid = uuid("00000000-0000-0000-0000-000000000000")
assert(lljson.slencode(null_uuid, {tight = true}) == '"!u"')
assert(lljson.sldecode('"!u"') == null_uuid)

-- Decoding tight formats
assert(lljson.sldecode('"!v1,2,3"') == vector(1, 2, 3))
assert(lljson.sldecode('"!v,,1"') == vector(0, 0, 1))
assert(lljson.sldecode('"!v,,"') == vector(0, 0, 0))
assert(lljson.sldecode('"!v"') == vector(0, 0, 0))
assert(lljson.sldecode('"!q,,,1"') == quaternion(0, 0, 0, 1))
assert(lljson.sldecode('"!q"') == quaternion(0, 0, 0, 1))
assert(lljson.sldecode('"!q,,,"') == quaternion(0, 0, 0, 0))

-- Normal format still works after tight implementation
assert(lljson.sldecode('"!v<1,2,3>"') == vector(1, 2, 3))
assert(lljson.sldecode('"!q<0,0,0,1>"') == quaternion(0, 0, 0, 1))

-- Round-trip with tight encoding
local vec_rt = lljson.sldecode(lljson.slencode(vector(1.5, 0, -2.5), {tight = true}))
assert(vec_rt == vector(1.5, 0, -2.5))

local quat_rt = lljson.sldecode(lljson.slencode(quaternion(0, 0, 0, 1), {tight = true}))
assert(quat_rt == quaternion(0, 0, 0, 1))

local uuid_rt = lljson.sldecode(lljson.slencode(test_uuid, {tight = true}))
assert(uuid_rt == test_uuid)

-- Complex structure with tight encoding
local tight_complex = {
    pos = vector(0, 0, 10),
    rot = quaternion(0, 0, 0, 1),
    id = uuid("00000000-0000-0000-0000-000000000001"),
}
local tight_json = lljson.slencode(tight_complex, {tight = true})
local tight_decoded = lljson.sldecode(tight_json)
assert(tight_decoded.pos == tight_complex.pos)
assert(tight_decoded.rot == tight_complex.rot)
assert(tight_decoded.id == tight_complex.id)

-- Boolean keys
local bool_key_table = {[true] = "yes", [false] = "no"}
local bool_key_json = lljson.slencode(bool_key_table)
local bool_key_decoded = lljson.sldecode(bool_key_json)
assert(bool_key_decoded[true] == "yes")
assert(bool_key_decoded[false] == "no")

-- Recursive table with uuid as value
local recurse = {
    -- This will use the most stack space of any of the encode paths
    a=uuid(''),
}
recurse.b = recurse
-- Should fail due to limits
assert(not pcall(lljson.encode, recurse))

-- Non-string input should error, not crash
assert(not pcall(lljson.decode, {"1,2,3"}))

-- ============================================
-- lljson.remove sentinel
-- ============================================
assert(lljson.remove ~= nil)
assert(lljson.remove ~= lljson.null)
assert(type(lljson.remove) == "userdata")

-- ============================================
-- slencode/sldecode !n tag for nil
-- ============================================

-- slencode emits "!n" for nil holes in arrays
assert(lljson.slencode({1, nil, 3}) == '[1,"!n",3]')

-- slencode top-level nil
assert(lljson.slencode(nil) == '"!n"')

-- sldecode "!n" produces nil (hole in array)
do
    local t = lljson.sldecode('[1,"!n",3]')
    assert(t[1] == 1)
    assert(t[2] == nil)
    assert(t[3] == 3)
end

-- lljson.null still round-trips as null, not !n
do
    local t = lljson.sldecode(lljson.slencode({1, lljson.null, 3}))
    assert(t[1] == 1)
    assert(t[2] == lljson.null)
    assert(t[3] == 3)
end

-- standard encode still uses null for nil (no !n)
assert(lljson.encode({1, nil, 3}) == '[1,null,3]')

-- ============================================
-- __tojson context table
-- ============================================
do
    local captured_ctx
    local ctx_mt = { __tojson = function(self, ctx)
        captured_ctx = ctx
        return self.val
    end }

    -- encode() passes mode="json", tight=false
    lljson.encode(setmetatable({val = 1}, ctx_mt))
    assert(captured_ctx.mode == "json")
    assert(captured_ctx.tight == false)

    -- slencode() passes mode="sljson", tight=false
    captured_ctx = nil
    lljson.slencode(setmetatable({val = 1}, ctx_mt))
    assert(captured_ctx.mode == "sljson")
    assert(captured_ctx.tight == false)

    -- slencode(val, {tight = true}) passes tight=true
    captured_ctx = nil
    lljson.slencode(setmetatable({val = 1}, ctx_mt), {tight = true})
    assert(captured_ctx.mode == "sljson")
    assert(captured_ctx.tight == true)

    -- This should definitely be read-only
    assert(table.isfrozen(captured_ctx))
end


-- slencode with table arg: tight option
do
    local r = lljson.slencode(vector(1, 2, 3), {tight = true})
    assert(r == '"!v1,2,3"')
end

-- No options: still works
assert(lljson.encode(42) == "42")
assert(lljson.slencode(42) == "42")

-- Bad arg types error
assert(not pcall(lljson.encode, 1, "string"))
assert(not pcall(lljson.slencode, 1, "string"))
assert(not pcall(lljson.slencode, 1, true))

-- sldecode does not set metatables (slencode ignores them, so attaching would be dishonest)
do
    assert(getmetatable(lljson.sldecode("[]")) == nil)
    assert(getmetatable(lljson.sldecode("[1,2,3]")) == nil)
    assert(getmetatable(lljson.sldecode("{}")) == nil)
    assert(getmetatable(lljson.sldecode('{"a":1}')) == nil)
    -- Standard decode sets metatables
    assert(getmetatable(lljson.decode("[]")) == lljson.array_mt)
    assert(getmetatable(lljson.decode("[1,2]")) == lljson.array_mt)
    assert(getmetatable(lljson.decode("{}")) == lljson.object_mt)
    assert(getmetatable(lljson.decode('{"a":1}')) == lljson.object_mt)
    -- sldecode round-trip: non-empty array auto-detected, no metatable needed
    local decoded = lljson.sldecode(lljson.slencode({1, 2, 3}))
    assert(decoded[1] == 1 and decoded[2] == 2 and decoded[3] == 3)
    assert(getmetatable(decoded) == nil)
end

-- slencode ignores shape metatables - auto-detects from data
do
    -- array_mt is ignored by slencode, auto-detects as array anyway
    local t = setmetatable({1, 2, 3}, lljson.array_mt)
    assert(lljson.slencode(t) == "[1,2,3]")
    -- object_mt is ignored by slencode, auto-detects as array
    local t2 = setmetatable({10, 20}, lljson.object_mt)
    assert(lljson.slencode(t2) == "[10,20]")
    -- empty table with object_mt: slencode ignores it, encodes as []
    local t3 = setmetatable({}, lljson.object_mt)
    assert(lljson.slencode(t3) == "[]")
end

-- object_mt forces object encoding
do
    -- Sequential integer keys become stringified
    local t = setmetatable({10, 20, 30}, lljson.object_mt)
    local json = lljson.encode(t)
    local decoded = lljson.decode(json)
    assert(decoded["1"] == 10)
    assert(decoded["2"] == 20)
    assert(decoded["3"] == 30)
    -- Empty table with object_mt encodes as {}
    assert(lljson.encode(setmetatable({}, lljson.object_mt)) == "{}")
    -- object_mt is accessible
    assert(lljson.object_mt ~= nil)
    assert(type(lljson.object_mt) == "table")
end

-- ============================================
-- __jsonhint metamethod
-- ============================================
do
    -- Custom metatable with __jsonhint = "array"
    local arr_mt = {__jsonhint = "array"}
    assert(lljson.encode(setmetatable({}, arr_mt)) == "[]")
    assert(lljson.encode(setmetatable({1, 2, 3}, arr_mt)) == "[1,2,3]")

    -- Custom metatable with __jsonhint = "object"
    local obj_mt = {__jsonhint = "object"}
    assert(lljson.encode(setmetatable({}, obj_mt)) == "{}")
    assert(lljson.encode(setmetatable({1, 2}, obj_mt)) == '{"1":1,"2":2}')

    -- __jsonhint + __index: metamethods used for element access
    local proxy_mt = {
        __jsonhint = "array",
        __len = function() return 3 end,
        __index = function(_, k) return k * 10 end,
    }
    assert(lljson.encode(setmetatable({}, proxy_mt)) == "[10,20,30]")

    -- __jsonhint + __len (custom length)
    local len_mt = {
        __jsonhint = "array",
        __len = function() return 2 end,
    }
    assert(lljson.encode(setmetatable({10, 20, 30}, len_mt)) == "[10,20]")

    -- __tojson provides content, __jsonhint provides shape (orthogonal)
    -- scalar __tojson result: shape is irrelevant
    local scalar_mt = {
        __jsonhint = "object",
        __tojson = function(self) return self.a end,
    }
    assert(lljson.encode(setmetatable({a = 1}, scalar_mt)) == '1')

    -- table __tojson result + __jsonhint = "array": shape applied to result
    local arr_tojson_mt = {
        __jsonhint = "array",
        __tojson = function(self) return {self[1] * 10} end,
    }
    assert(lljson.encode(setmetatable({5}, arr_tojson_mt)) == '[50]')

    -- __tojson converts string-keyed table to array-compatible result
    local convert_mt = {
        __jsonhint = "array",
        __tojson = function(self) return {self.x, self.y} end,
    }
    assert(lljson.encode(setmetatable({x = 1, y = 2}, convert_mt)) == '[1,2]')

    -- table __tojson result + __jsonhint = "object": shape applied to result
    local obj_tojson_mt = {
        __jsonhint = "object",
        __tojson = function() return {1, 2} end,
    }
    assert(lljson.encode(setmetatable({}, obj_tojson_mt)) == '{"1":1,"2":2}')

    -- __jsonhint = "array" on table with string keys: hint ignored, encoded as object
    assert(lljson.encode(setmetatable({x = 1}, {__jsonhint = "array"})) == '{"x":1}')

    -- __jsonhint = "array" + __tojson returning string-keyed table: hint ignored
    local fallback_tojson_mt = {
        __jsonhint = "array",
        __tojson = function() return {x = 1} end,
    }
    assert(lljson.encode(setmetatable({}, fallback_tojson_mt)) == '{"x":1}')

    -- Original's __jsonhint wins over __tojson result's metatable (both directions)
    local arr_orig_mt = {
        __jsonhint = "array",
        __tojson = function() return setmetatable({10, 20}, lljson.object_mt) end,
    }
    assert(lljson.encode(setmetatable({}, arr_orig_mt)) == '[10,20]')

    local obj_orig_mt = {
        __jsonhint = "object",
        __tojson = function() return setmetatable({10, 20}, lljson.array_mt) end,
    }
    assert(lljson.encode(setmetatable({}, obj_orig_mt)) == '{"1":10,"2":20}')

    -- Invalid __jsonhint value errors
    local bad_mt = {__jsonhint = "invalid"}
    assert(not pcall(lljson.encode, setmetatable({}, bad_mt)))

    -- Non-string __jsonhint value errors
    assert(not pcall(lljson.encode, setmetatable({}, {__jsonhint = 42})))
    assert(not pcall(lljson.encode, setmetatable({}, {__jsonhint = true})))

    -- slencode ignores __jsonhint
    local slen_mt = {__jsonhint = "object"}
    assert(lljson.slencode(setmetatable({1, 2}, slen_mt)) == "[1,2]")
    assert(lljson.slencode(setmetatable({}, slen_mt)) == "[]")
end

-- empty_array / empty_object are frozen tables with shape metatables
assert(type(lljson.empty_array) == "table")
assert(type(lljson.empty_object) == "table")
assert(table.isfrozen(lljson.empty_array))
assert(table.isfrozen(lljson.empty_object))
assert(lljson.empty_object ~= nil)
assert(lljson.empty_object ~= lljson.null)
assert(lljson.empty_object ~= lljson.empty_array)
-- Metatables are shared with array_mt/object_mt
assert(getmetatable(lljson.empty_array) == lljson.array_mt)
assert(getmetatable(lljson.empty_object) == lljson.object_mt)
assert(getmetatable(lljson.empty_array).__jsonhint == "array")
assert(getmetatable(lljson.empty_object).__jsonhint == "object")

-- UUID table keys should encode as their string form
assert(
    lljson.encode({[uuid("12345678-1234-1234-1234-123456789abc")]="hello" }) ==
    '{"12345678-1234-1234-1234-123456789abc":"hello"}'
)

-- Enable interrupt-driven yields for remaining tests.
enable_check_interrupt()

local function consume_impl(check, expect_yields, f, ...)
    clear_check_count()
    local co = coroutine.create(f)
    local yields = 0
    local ok, a, b, c = coroutine.resume(co, ...)
    assert(ok, a)
    while coroutine.status(co) ~= "dead" do
        yields += 1
        co = ares.unpersist(ares.persist(co))
        collectgarbage()
        ok, a, b, c = coroutine.resume(co)
        assert(ok, a)
    end
    if expect_yields then
        assert(yields > 0, "no yields occurred")
    end
    if check then
        assert(yields == get_check_count(),
            "yield count mismatch: " .. yields .. " actual vs " .. get_check_count() .. " interrupts")
    end
    return a, b, c, yields
end

local function consume(f, ...)
    return consume_impl(true, true, f, ...)
end

local function consume_nocheck(f, ...)
    return consume_impl(false, false, f, ...)
end

-- Trailing garbage in tagged values should error
assert(not pcall(lljson.sldecode, '"!f3.14$$$$"'))
assert(not pcall(lljson.sldecode, '"!v1,2,3junk"'))
assert(not pcall(lljson.sldecode, '"!q1,2,3,4junk"'))
assert(not pcall(lljson.sldecode, '"!q2e3,,0x16,,xyzzz"'))
assert(not pcall(lljson.sldecode, '"!v<1,2,3>junk"'))
assert(not pcall(lljson.sldecode, '"!q<1,2,3,4>junk"'))
-- Whitespace around components/delimiters is OK (matches tonumber())
assert(lljson.sldecode('"!f3.14 "') == 3.14)
assert(lljson.sldecode('"!f 3.14"') == 3.14)
assert(lljson.sldecode('"!v< 1 , 2 , 3 >"') == vector(1, 2, 3))
assert(lljson.sldecode('"!q< 1 , 2 , 3 , 4 >"') == quaternion(1, 2, 3, 4))

-- Shared metatables for yield tests
local yield_tojson_mt = { __tojson = function(self)
    coroutine.yield()
    return self.val
end }
local yield_len_mt = { __jsonhint = "array", __len = function(self)
    coroutine.yield()
    return self.n
end, __tojson = function(self)
    local t = {}
    for i = 1, self.n do t[i] = self[i] end
    return t
end }

-- encode flat array: exercises ELEMENT/NEXT_ELEMENT yield path
assert(consume(function()
    return lljson.encode({1, 2, 3, 4, 5})
end) == "[1,2,3,4,5]")

-- encode object: exercises VALUE/NEXT_PAIR yield path
consume(function()
    local r = lljson.encode({foo = "bar", baz = 42})
    local t = lljson.decode(r)
    assert(t.foo == "bar" and t.baz == 42)
end)

-- encode nested structure: exercises recursive YIELD_HELPER
consume(function()
    local r = lljson.encode({a = {1, 2}, b = {c = "d"}})
    local t = lljson.decode(r)
    assert(t.a[1] == 1 and t.a[2] == 2 and t.b.c == "d")
end)

-- __tojson that yields: exercises TOJSON_CALL yield path with ares round-trip
assert(consume_nocheck(function()
    local mt = { __tojson = function(self)
        coroutine.yield()
        return tostring(self.val)
    end }
    return lljson.encode({setmetatable({val = 42}, mt)})
end) == "[\"42\"]")

-- __tojson returning a table: exercises TOJSON_RECURSE (recursive encode of result)
assert(consume_nocheck(function()
    local mt = { __tojson = function(self)
        coroutine.yield()
        return {self.x, self.y}
    end }
    return lljson.encode(setmetatable({x = 1, y = 2}, mt))
end) == "[1,2]")

-- multiple __tojson in one encode: two yielding metamethods in the same array
assert(consume_nocheck(function()
    return lljson.encode({setmetatable({val = 10}, yield_tojson_mt), setmetatable({val = 20}, yield_tojson_mt)})
end) == "[10,20]")

-- encode large array: sustained interrupt-driven yields
consume(function()
    local t = {}
    for i = 1, 200 do t[i] = i end
    local r = lljson.encode(t)
    local t2 = lljson.decode(r)
    assert(#t2 == 200 and t2[1] == 1 and t2[200] == 200)
end)

-- decode with interrupt-driven yields
consume(function()
    local src = lljson.encode({a = {1, 2, 3}, b = {c = "d"}})
    local t = lljson.decode(src)
    assert(t.a[1] == 1 and t.a[2] == 2 and t.a[3] == 3 and t.b.c == "d")
end)

-- slencode with interrupt-driven yields
consume(function()
    local r = lljson.slencode({pos = vector(1, 2, 3), id = uuid("12345678-1234-1234-1234-123456789abc")})
    local t = lljson.sldecode(r)
    assert(t.pos == vector(1, 2, 3) and t.id == uuid("12345678-1234-1234-1234-123456789abc"))
end)

-- __len that yields: exercises LEN_CHECK/LEN_CALL yield path
assert(consume_nocheck(function()
    return lljson.encode(setmetatable({10, 20, 30, n = 3}, yield_len_mt))
end) == "[10,20,30]")

-- deeply nested encode: arrays of objects of arrays with __tojson and __len at multiple levels
consume_nocheck(function()
    local r = lljson.encode({
        items = {
            {name = "a", tags = {1, 2, 3}},
            {name = "b", tags = setmetatable({10, 20, n = 2}, yield_len_mt)},
            {name = "c", custom = setmetatable({val = "hello"}, yield_tojson_mt)},
        },
        meta = {
            nested = {
                deep = {{true, false}, {lljson.null}},
            },
        },
    })
    local t = lljson.decode(r)
    assert(t.items[1].name == "a" and t.items[1].tags[2] == 2)
    assert(t.items[2].name == "b" and t.items[2].tags[1] == 10 and t.items[2].tags[2] == 20)
    assert(t.items[3].name == "c" and t.items[3].custom == "hello")
    assert(t.meta.nested.deep[1][1] == true and t.meta.nested.deep[1][2] == false)
end)

-- deeply nested decode: exercises recursive json_process_value -> parse_object/parse_array at depth
consume(function()
    local src = '{"a":[{"b":[[1,2],[3,4]]},{"c":{"d":[5,6,7],"e":{"f":true}}}],"g":[[[8]]]}'
    local t = lljson.decode(src)
    assert(t.a[1].b[1][1] == 1 and t.a[1].b[2][2] == 4)
    assert(t.a[2].c.d[3] == 7 and t.a[2].c.e.f == true)
    assert(t.g[1][1][1] == 8)
end)


-- __tojson(self, ctx) that yields and uses ctx
assert(consume_nocheck(function()
    local mt = { __tojson = function(self, ctx)
        coroutine.yield()
        if ctx.mode == "json" then
            return tostring(self.val)
        end
        return self.val
    end }
    return lljson.encode({setmetatable({val = 42}, mt)})
end) == "[\"42\"]")

-- slencode with __tojson ctx: mode should be "sljson"
consume_nocheck(function()
    local captured_mode
    local captured_ctx
    local mt = { __tojson = function(self, ctx)
        captured_ctx = ctx
        captured_mode = type(ctx) == "table" and ctx.mode or nil
        return self.val
    end }
    lljson.slencode(setmetatable({val = 1}, mt))
    assert(captured_mode == "sljson", "expected sljson, got " .. tostring(captured_mode) .. " (ctx type=" .. type(captured_ctx) .. ")")
end)

-- slencode with __tojson ctx that explicitly yields: exercises Ares round-trip
consume_nocheck(function()
    local captured_mode
    local mt = { __tojson = function(self, ctx)
        coroutine.yield()
        captured_mode = ctx.mode
        return self.val
    end }
    lljson.slencode(setmetatable({val = 1}, mt))
    assert(captured_mode == "sljson")
end)

-- Numbers exceeding int64 range should parse correctly, not clamp to LLONG_MAX
assert(lljson.decode("100000000000000000000") == 1e20)
assert(lljson.decode("-100000000000000000000") == -1e20)

return 'OK'
