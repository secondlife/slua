ret = "FAIL"
local NESTING = 3


local function nested_coro(i)
    if i >= NESTING then
        -- Do it twice, just to be sure
        breaker()
        breaker()
        ret = "OK"
    else
        coroutine.wrap(function()
            nested_coro(i + 1)
        end)()
    end
end

nested_coro(0)
return ret
