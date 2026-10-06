local perms = {[coroutine.yield]="yield", [coroutine.wrap]="wrap", [coroutine.resume]="resume", [assert]="assert"}
local uperms = {yield=coroutine.yield, wrap=coroutine.wrap, resume=coroutine.resume, assert=assert}

local tab_ser = ares.persist(perms, "foobar")

local function assert_decode_fails(data)
    local success, err
    success, err = pcall(function() ares.unpersist(uperms, data) end)
    print(err)
    assert(err ~= nil)
end


local function assert_decode_fails_with(data, pattern)
    local success, err = pcall(function() ares.unpersist(uperms, data) end)
    print(err)
    assert(not success and string.find(err, pattern) ~= nil)
end

assert_decode_fails(tab_ser:sub(#tab_ser - 2))
assert_decode_fails(tab_ser:gsub("ARES", "ARTS"))

-- The header is "ARES", major, minor and the record length, then number size,
-- test number, vector size and the reference count ahead of the required
-- feature mask. Bit 63 will never be a feature this build knows. Luau numbers
-- are doubles, so the u64 is packed as two halves.
local features_required_pos = 4 + 4 + 4 + 4 + 1 + 8 + 1 + 4
local unknown_bit = string.pack("<I4I4", 0, 0x80000000)
local function patch(data, pos, bytes)
    return data:sub(1, pos) .. bytes .. data:sub(pos + #bytes + 1)
end

-- A stream that requires a feature this reader doesn't know is refused before
-- the body is touched
assert_decode_fails_with(patch(tab_ser, features_required_pos, unknown_bit), "doesn't know")
-- and the untouched stream loads
assert(ares.unpersist(uperms, tab_ser) == "foobar")

-- With testpad every record ends in two blocks no reader knows: feature 66
-- holding the padding, then an empty feature 67. The root table's record is
-- the last thing in the stream, so they're its final bytes.
ares.settings("testpad", 3)
local padded_ser = ares.persist(perms, {1, 2, 3})
ares.settings("testpad", 0)
assert(padded_ser:sub(-5) == string.char(67) .. string.pack("<I4", 0))
assert(#ares.unpersist(uperms, padded_ser) == 3)

-- Send an ares test object back and forth through the serialization mechanism.
-- This is meant to exercise the optional "blocks" extension mechanism in ares.cpp.
local function test_round_trip(fields)
    return ares_test_fields(ares.unpersist(uperms, ares.persist(perms, ares_test_object(fields))))
end

local fields = test_round_trip({base = 1})
assert(fields.base == 1 and fields.added == 0)
fields = test_round_trip({base = 1, added = 5})
assert(fields.base == 1 and fields.added == 5)
-- An unknown block ahead of added's is skipped
fields = test_round_trip({base = 1, added = 5, newer_block = 3})
assert(fields.added == 5 and fields.newer_block == 0)
-- and so is the unread end of added's own block
fields = test_round_trip({base = 1, added = 5, newer_tail = 3})
assert(fields.added == 5 and fields.newer_tail == 0)
-- and so are the testpad blocks after it, with or without added's
ares.settings("testpad", 1)
fields = test_round_trip({base = 1, newer_block = 3})
ares.settings("testpad", 0)
assert(fields.base == 1 and fields.added == 0)

-- added's block ends the stream, and reading it can't run past its length
local added_ser = ares.persist(perms, ares_test_object({base = 1, added = 5}))
assert(added_ser:sub(-6) == string.char(65) .. string.pack("<I4", 1) .. string.char(5))
assert_decode_fails_with(patch(added_ser, #added_ser - 5, string.pack("<I4", 0)), "unterminated varint")
assert_decode_fails_with(patch(added_ser, #added_ser - 5, string.pack("<I4", 2)), "block exceeds enclosing record")

print("OK")
return "OK"
