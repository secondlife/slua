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
assert(lljson.encode({uuid("00000000-0000-0000-0000-000000000001")}) == '["00000000-0000-0000-0000-000000000001"]')
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

-- Tagged keys: numeric keys become !f
local float_key_table = {[3.14] = "pi"}
local float_key_json = lljson.slencode(float_key_table)
assert(float_key_json == '{"!f3.14":"pi"}')
local float_key_decoded = lljson.sldecode(float_key_json)
assert(float_key_decoded[3.14] == "pi")

-- Sparse tables: integer keys become !f tagged, avoiding sparse array issues
-- (contrast with regular encode which would fail or fill with nulls)
local sparse_table = {[1] = "first", [100] = "hundredth"}
local sparse_json = lljson.slencode(sparse_table)
-- Encoded as object with !f keys, not as array
-- Okay, this is a little obnoxious, but we don't really know in which order the
-- keys would be serialized. Just accept either/or.
assert(sparse_json == '{"!f1":"first","!f100":"hundredth"}' or sparse_json == '{"!f100":"hundredth","!f1":"first"}')
local sparse_decoded = lljson.sldecode(sparse_json)
assert(sparse_decoded[1] == "first")
assert(sparse_decoded[100] == "hundredth")

-- Tagged keys: vector keys
local vec_key_table = {[vector(1, 2, 3)] = "vec"}
local vec_key_json = lljson.slencode(vec_key_table)
assert(vec_key_json == '{"!v<1,2,3>":"vec"}')
local vec_key_decoded = lljson.sldecode(vec_key_json)
assert(vec_key_decoded[vector(1, 2, 3)] == "vec")

-- Tagged keys: quaternion keys (note: table lookup uses reference identity, but == uses value equality)
local quat_key_table = {[quaternion(0, 0, 0, 1)] = "identity"}
local quat_key_json = lljson.slencode(quat_key_table)
assert(quat_key_json == '{"!q<0,0,0,1>":"identity"}')
local quat_key_decoded = lljson.sldecode(quat_key_json)
-- Can't lookup by value since table keys use reference identity, need to iterate
local found_quat_key = false
for k, v in quat_key_decoded do
    if k == quaternion(0, 0, 0, 1) and v == "identity" then
        found_quat_key = true
    end
end
assert(found_quat_key)

-- Tagged keys: UUID keys
local uuid_key_table = {[uuid("12345678-1234-1234-1234-123456789abc")] = "some_uuid"}
local uuid_key_json = lljson.slencode(uuid_key_table)
assert(uuid_key_json == '{"!u12345678-1234-1234-1234-123456789abc":"some_uuid"}')
local uuid_key_decoded = lljson.sldecode(uuid_key_json)
assert(uuid_key_decoded[uuid("12345678-1234-1234-1234-123456789abc")] == "some_uuid")

-- Tagged keys: string keys starting with ! get escaped
local bang_key_table = {["!bang"] = "value"}
local bang_key_json = lljson.slencode(bang_key_table)
assert(bang_key_json == '{"!!bang":"value"}')
local bang_key_decoded = lljson.sldecode(bang_key_json)
assert(bang_key_decoded["!bang"] == "value")

-- Infinity in tagged floats
local inf_key_table = {[math.huge] = "infinity"}
local inf_key_json = lljson.slencode(inf_key_table)
assert(inf_key_json == '{"!f1e9999":"infinity"}')
local inf_key_decoded = lljson.sldecode(inf_key_json)
assert(inf_key_decoded[math.huge] == "infinity")

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

-- Buffer as object key
local buf_key_table = {[buf] = "data"}
local buf_key_json = lljson.slencode(buf_key_table)
assert(buf_key_json == '{"!dAQD/AA==":"data"}')

-- ============================================
-- Tight encoding mode (second arg = true)
-- ============================================

-- Tight vectors: no angle brackets
assert(lljson.slencode(vector(1, 2, 3), true) == '"!v1,2,3"')

-- Tight vectors: zeros omitted
assert(lljson.slencode(vector(0, 0, 1), true) == '"!v,,1"')
assert(lljson.slencode(vector(0, 0, 0), true) == '"!v,,"')
assert(lljson.slencode(vector(1, 0, 0), true) == '"!v1,,"')
assert(lljson.slencode(vector(0, 2, 0), true) == '"!v,2,"')

-- Tight quaternions: no angle brackets, zeros omitted
assert(lljson.slencode(quaternion(0, 0, 0, 1), true) == '"!q,,,1"')
assert(lljson.slencode(quaternion(1, 0, 0, 0), true) == '"!q1,,,"')
assert(lljson.slencode(quaternion(0, 0, 0, 0), true) == '"!q,,,"')

-- Tight UUIDs: base64 encoded (22 chars instead of 36)
local test_uuid = uuid("12345678-1234-1234-1234-123456789abc")
local tight_uuid_json = lljson.slencode(test_uuid, true)
-- Should be "!u" + 22 chars of base64
assert(#tight_uuid_json == 26)  -- 2 quotes + !u + 22 base64
assert(tight_uuid_json:sub(1, 3) == '"!u')

-- Tight null UUID: just "!u" with no payload
local null_uuid = uuid("00000000-0000-0000-0000-000000000000")
assert(lljson.slencode(null_uuid, true) == '"!u"')
assert(lljson.sldecode('"!u"') == null_uuid)

-- Decoding tight formats
assert(lljson.sldecode('"!v1,2,3"') == vector(1, 2, 3))
assert(lljson.sldecode('"!v,,1"') == vector(0, 0, 1))
assert(lljson.sldecode('"!v,,"') == vector(0, 0, 0))
assert(lljson.sldecode('"!q,,,1"') == quaternion(0, 0, 0, 1))
assert(lljson.sldecode('"!q,,,"') == quaternion(0, 0, 0, 0))

-- Normal format still works after tight implementation
assert(lljson.sldecode('"!v<1,2,3>"') == vector(1, 2, 3))
assert(lljson.sldecode('"!q<0,0,0,1>"') == quaternion(0, 0, 0, 1))

-- Round-trip with tight encoding
local vec_rt = lljson.sldecode(lljson.slencode(vector(1.5, 0, -2.5), true))
assert(vec_rt == vector(1.5, 0, -2.5))

local quat_rt = lljson.sldecode(lljson.slencode(quaternion(0, 0, 0, 1), true))
assert(quat_rt == quaternion(0, 0, 0, 1))

local uuid_rt = lljson.sldecode(lljson.slencode(test_uuid, true))
assert(uuid_rt == test_uuid)

-- Complex structure with tight encoding
local tight_complex = {
    pos = vector(0, 0, 10),
    rot = quaternion(0, 0, 0, 1),
    id = uuid("00000000-0000-0000-0000-000000000001"),
}
local tight_json = lljson.slencode(tight_complex, true)
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
assert(not pcall(function() lljson.encode(recurse) end))

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
    local mt = { __tojson = function(self)
        coroutine.yield()
        return self.v
    end }
    return lljson.encode({setmetatable({v = 10}, mt), setmetatable({v = 20}, mt)})
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
    local mt = { __len = function(self)
        coroutine.yield()
        return self.n
    end }
    return lljson.encode(setmetatable({10, 20, 30, n = 3}, mt))
end) == "[10,20,30]")

-- deeply nested encode: arrays of objects of arrays with __tojson and __len at multiple levels
consume_nocheck(function()
    local tojson_mt = { __tojson = function(self)
        coroutine.yield()
        return self.val
    end }
    local len_mt = { __len = function(self)
        coroutine.yield()
        return self.n
    end }
    local r = lljson.encode({
        items = {
            {name = "a", tags = {1, 2, 3}},
            {name = "b", tags = setmetatable({10, 20, n = 2}, len_mt)},
            {name = "c", custom = setmetatable({val = "hello"}, tojson_mt)},
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

-- deeply nested decode: exercises recursive json_process_value → parse_object/parse_array at depth
consume(function()
    local src = '{"a":[{"b":[[1,2],[3,4]]},{"c":{"d":[5,6,7],"e":{"f":true}}}],"g":[[[8]]]}'
    local t = lljson.decode(src)
    assert(t.a[1].b[1][1] == 1 and t.a[1].b[2][2] == 4)
    assert(t.a[2].c.d[3] == 7 and t.a[2].c.e.f == true)
    assert(t.g[1][1][1] == 8)
end)

return 'OK'
