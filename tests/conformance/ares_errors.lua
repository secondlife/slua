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
-- test number, vector size and the reference count ahead of the two feature
-- masks. Bit 63 will never be a feature this build knows. Luau numbers are
-- doubles, so the u64 is packed as two halves.
local features_present_pos = 4 + 4 + 4 + 4 + 1 + 8 + 1 + 4
local features_required_pos = features_present_pos + 8
local unknown_bit = string.pack("<I4I4", 0, 0x80000000)
local function patch(data, pos, bytes)
    return data:sub(1, pos) .. bytes .. data:sub(pos + #bytes + 1)
end

-- A stream that requires a feature this reader doesn't know is refused before
-- the body is touched
assert_decode_fails_with(patch(tab_ser, features_required_pos, unknown_bit), "doesn't know")
-- A present but unrequired feature is a droppable field, skipped through its
-- record length, so the stream still loads
assert(ares.unpersist(uperms, patch(tab_ser, features_present_pos, unknown_bit)) == "foobar")
-- and so does the untouched stream
assert(ares.unpersist(uperms, tab_ser) == "foobar")

print("OK")
return "OK"
