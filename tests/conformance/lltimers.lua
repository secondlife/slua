-- Test suite for LLTimers API

assert(typeof(LLTimers) == "LLTimers")

-- Helper to increment clock with epsilon to avoid floating point precision issues
local function incrementclock(delta)
    setclock(getclock() + delta + 0.001)
end

local function assert_errors(func, expected_str)
    local success, ret = pcall(func)
    assert(not success)
    local is_match = typeof(ret) == "string" and ret:find(expected_str) ~= nil
    if not is_match then
        print(ret, "!=", expected_str)
    end
    assert(is_match)
end

-- Test basic on() functionality
local on_count = 0
local on_handler = LLTimers:on(0.1, function()
    on_count += 1
end)

assert(typeof(on_handler) == "function")

-- Simulate timer tick
setclock(0.05) -- Not time yet
LLTimers:_tick()
assert(on_count == 0)

-- This should be using our fake clock
assert(os.clock() - 0.05 < 0.000001)

incrementclock(0.05) -- Advance to past 0.1, should fire now
LLTimers:_tick()
assert(on_count == 1)

incrementclock(0.1) -- Advance by interval, should fire again
LLTimers:_tick()
assert(on_count == 2)

-- Clean up on() timer before testing once()
LLTimers:off(on_handler)

-- Test once() functionality
local once_count = 0
local once_handler = LLTimers:once(0.1, function()
    once_count += 1
end)

incrementclock(0.1) -- Should fire the once handler
LLTimers:_tick()
assert(once_count == 1)

incrementclock(0.1) -- Should NOT fire again
LLTimers:_tick()
assert(once_count == 1)

-- Test off() functionality
-- Create a new timer to test removal
local new_on_handler = LLTimers:on(0.1, function()
    on_count += 1
end)

local result = LLTimers:off(new_on_handler)
assert(result == true)

incrementclock(0.1) -- Should not increment on_count anymore
LLTimers:_tick()
assert(on_count == 2) -- Still 2 from before

-- Test off() with non-existent handler
local fake_handler = function() end
result = LLTimers:off(fake_handler)
assert(result == false)

-- Test multiple timers
local timer1_count = 0
local timer2_count = 0

setclock(0.5)
local timer1 = LLTimers:on(0.1, function()
    timer1_count += 1
end)

local timer2 = LLTimers:on(0.05, function()
    timer2_count += 1
end)

setclock(0.551) -- timer2 fires (at 0.55), reschedules to 0.601
LLTimers:_tick()
assert(timer1_count == 0)
assert(timer2_count == 1)

setclock(0.60001) -- timer1 fires (at 0.6), timer2 doesn't yet (still at 0.601)
LLTimers:_tick()
assert(timer1_count == 1)
assert(timer2_count == 1)

setclock(0.651) -- timer2 fires (at 0.601), timer1 doesn't (at 0.70001)
LLTimers:_tick()
assert(timer1_count == 1)
assert(timer2_count == 2)

-- Clean up
LLTimers:off(timer1)
LLTimers:off(timer2)

-- Test cancelling a once timer before it fires
local cancelled_count = 0
setclock(0.7)
local cancel_handler = LLTimers:once(0.5, function()
    cancelled_count += 1
end)

result = LLTimers:off(cancel_handler)
assert(result == true)

incrementclock(0.5) -- Should not fire
LLTimers:_tick()
assert(cancelled_count == 0)

-- Test negative interval
local success, err = pcall(function()
    LLTimers:on(-1, function() end)
end)
assert(success == false)

-- Test zero interval
success, err = pcall(function()
    LLTimers:on(0, function() end)
end)
assert(success == false)

-- Test invalid handler type
success, err = pcall(function()
    LLTimers:on(1, "not a function")
end)
assert(success == false)

-- Test tostring
local str = tostring(LLTimers)
assert(type(str) == "string")
assert(string.find(str, "LLTimers"))

-- Test that timers can be removed during tick
local removal_test_count = 0
local remover = nil
setclock(1.2)
remover = LLTimers:on(0.1, function()
    removal_test_count += 1
    if removal_test_count >= 2 then
        LLTimers:off(remover)
    end
end)

incrementclock(0.1)
LLTimers:_tick()
assert(removal_test_count == 1)

incrementclock(0.1)
LLTimers:_tick()
assert(removal_test_count == 2)

incrementclock(0.1)
LLTimers:_tick()
assert(removal_test_count == 2) -- Should not increment

-- Test that timers can handle internal `lua_break()`s due to the scheduler
local breaker_call_order = {}
setclock(2.0)

local breaker_timer1 = LLTimers:on(0.01, function()
    table.insert(breaker_call_order, 1)
    breaker()
end)

-- This should work with :once() too :)
local breaker_timer2 = LLTimers:once(0.01, function()
    breaker()
    table.insert(breaker_call_order, 2)
end)

local breaker_timer3 = LLTimers:on(0.01, function()
    table.insert(breaker_call_order, 3)
    breaker()
    table.insert(breaker_call_order, 4)
end)

setclock(2.1) -- All timers should fire
LLTimers:_tick()

assert(lljson.encode(breaker_call_order) == "[1,2,3,4]")

-- Clean up
LLTimers:off(breaker_timer1)
LLTimers:off(breaker_timer3)

-- Test that timers can handle coroutine yields
breaker_call_order = {}
local yield_order = {}
setclock(3.0)

local yield_timer1 = LLTimers:on(0.01, function()
    table.insert(breaker_call_order, 1)
    coroutine.yield(1)
end)

-- This should work with :once() too :)
local yield_timer2 = LLTimers:once(0.01, function()
    coroutine.yield(2)
    table.insert(breaker_call_order, 2)
end)

local yield_timer3 = LLTimers:on(0.01, function()
    table.insert(breaker_call_order, 3)
    coroutine.yield(3)
    table.insert(breaker_call_order, 4)
end)

setclock(3.1) -- All timers should fire

local tick_coro = coroutine.create(function() LLTimers:_tick() end)

while true do
    local co_status, yielded_val = coroutine.resume(tick_coro)
    if not co_status then
        break
    end
    table.insert(yield_order, yielded_val)
end

assert(lljson.encode(breaker_call_order) == "[1,2,3,4]")
assert(lljson.encode(yield_order) == "[1,2,3]")

-- Clean up
LLTimers:off(yield_timer1)
LLTimers:off(yield_timer3)

-- Run this last, check that we can block _tick() calls
set_may_call_tick(false)
LLTimers:on(0.1, function() assert(false) end)
assert_errors(
    function() LLTimers:_tick() end,
    "Not allowed to call LLTimers:_tick()"
)

return "OK"
