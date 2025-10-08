local perms = {[coroutine.yield]="yield", [coroutine.wrap]="wrap", [coroutine.resume]="resume", [assert]="assert"}
local uperms = {yield=coroutine.yield, wrap=coroutine.wrap, resume=coroutine.resume, assert=assert}

local tab_ser = ares.persist(perms, "foobar")

local function assert_decode_fails(data)
    local success, err
    success, err = pcall(function() ares.unpersist(uperms, data) end)
    print(err)
    assert(err ~= nil)
end


assert_decode_fails(tab_ser:sub(#tab_ser - 2))
assert_decode_fails(tab_ser:gsub("ARES", "ARTS"))

print("OK")
return "OK"
