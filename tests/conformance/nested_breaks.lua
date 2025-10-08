status = "FAIL"
local NESTING = 3


local function nested_coro(i)
    i = i or 0
    if i >= NESTING then
        -- Do it twice, just to be sure
        breaker()
        coroutine.yield(i)
        breaker()
        coroutine.yield(i)
        status = "OK"
    else
        local coro = coroutine.create(function() nested_coro(i + 1) end)
        while coroutine.status(coro) ~= "dead" do
            local success, ret = coroutine.resume(coro)
            if ret ~= nil then
                coroutine.yield(ret)
            end
        end
    end
end

local coro = coroutine.create(nested_coro)
for i=1,2 do
    local success, ret = coroutine.resume(coro)
    assert(ret == NESTING)
end

local success, ret = coroutine.resume(coro)
assert(success)
assert(ret == nil)

return status
