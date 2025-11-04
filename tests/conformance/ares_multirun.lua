assert(aGlobal == nil)

aGlobal = "foo"
assert(aGlobal == "foo")

coroutine.yield()

assert(aGlobal == "foo")

-- Test C closure with upvalues on call stack during serialization
-- Create a wrapped coroutine (auxwrapy C closure with 1 upvalue)
local wrapped = coroutine.wrap(function()
    -- Triggers LUA_BREAK, auxwrapy returns -1 and stays on stack
    breaker()
    return "I still worked"
end)

-- Call the wrapped coroutine
-- When breaker() is called, it breaks the calling thread all the way to the top.
-- At this point, auxwrapy (with 1 upvalue: the wrapped thread) is on our call stack
-- The forkserver will serialize at the break point, hopefully exercising
-- the code path that handles C closures with upvalues in CallInfo
assert(wrapped() == "I still worked")

return 'OK'
