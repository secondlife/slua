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
setclock(0.0)
local on_count = 0
local on_handler = LLTimers:on(0.1, function(scheduled_time, interval)
    on_count += 1
    assert(interval == 0.1, "on() timer should receive interval")
end)

assert(typeof(on_handler) == "function")

-- Simulate timer tick
setclock(0.05) -- Not time yet
-- `ll.GetTime()` should be using the clock provider.
assert(math.abs(ll.GetTime() - 0.05) < 0.01)
LLTimers:_tick()
assert(on_count == 0)

-- This should be using our fake clock
assert(math.abs(os.clock() - 0.05) < 0.000001)

incrementclock(0.05) -- Advance to past 0.1, should fire now
LLTimers:_tick()
assert(on_count == 1)

incrementclock(0.1) -- Advance by interval, should fire again
LLTimers:_tick()
assert(on_count == 2)

-- Clean up on() timer before testing once()
LLTimers:off(on_handler)

-- Test once() functionality
setclock(1.0)
local once_count = 0
local once_handler = LLTimers:once(0.1, function(scheduled_time, interval)
    once_count += 1
    assert(interval == nil, "once() timer should receive nil interval")
end)

incrementclock(0.1) -- Should fire the once handler
LLTimers:_tick()
assert(once_count == 1)

incrementclock(0.1) -- Should NOT fire again
LLTimers:_tick()
assert(once_count == 1)

-- Test off() functionality
setclock(2.0)
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

setclock(0.551) -- timer2 fires (at 0.55), reschedules to 0.60
LLTimers:_tick()
assert(timer1_count == 0)
assert(timer2_count == 1)

setclock(0.60001) -- timer1 fires (at 0.6), timer2 also fires (at 0.60)
LLTimers:_tick()
assert(timer1_count == 1)
assert(timer2_count == 2)

setclock(0.651) -- timer2 fires again (at 0.65), timer1 doesn't (at 0.7)
LLTimers:_tick()
assert(timer1_count == 1)
assert(timer2_count == 3)

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

-- Test zero interval (0 means "ASAP" and is valid)
timer1_count = 0
local function zero_interval_handler()
    timer1_count += 1
end
LLTimers:on(0, zero_interval_handler)
LLTimers:_tick()
assert(timer1_count == 1)
LLTimers:off(zero_interval_handler)

-- Test zero interval continues firing over multiple seconds (regression test for division-by-zero bug)
setclock(50.0)
local zero_continuous_count = 0
local zero_continuous_handler = LLTimers:on(0, function()
    zero_continuous_count += 1
end)

-- Fire several times with small time advances
for i = 1, 5 do
    incrementclock(0.01)
    LLTimers:_tick()
end
assert(zero_continuous_count == 5, "Zero-interval timer should fire 5 times")

-- Now advance past what would be the 2-second clamping threshold for interval>0 timers
-- For interval=0, this just schedules for "now" (ASAP semantics)
-- This used to trigger division by zero: ceil(time_behind / 0)
setclock(53.0)  -- More than 2 seconds later
LLTimers:_tick()
assert(zero_continuous_count == 6, "Zero-interval timer should continue after large time jump")

-- Verify it continues working normally
incrementclock(0.01)
LLTimers:_tick()
assert(zero_continuous_count == 7, "Zero-interval timer should still work after clamp")

LLTimers:off(zero_continuous_handler)

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
LLEvents:_handleEvent('timer')

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

local tick_coro = coroutine.create(function() LLEvents:_handleEvent('timer') end)

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
LLTimers:off(yield_timer2)
LLTimers:off(yield_timer3)

-- Test reentrancy detection
setclock(0.0)
local reentrant_handler = LLTimers:on(0.1, function()
    -- This should error because we're already inside _tick()
    LLTimers:_tick()
end)

setclock(1.0)
assert_errors(
    function() LLTimers:_tick() end,
    "Recursive call to LLTimers:_tick%(%) detected"
)

-- Clean up
LLTimers:off(reentrant_handler)

-- Test automatic registration with LLEvents when first timer is added
setclock(4.0)
assert(#LLEvents:listeners("timer") == 0)

local auto_reg_timer1 = LLTimers:on(1.0, function() end)
assert(#LLEvents:listeners("timer") == 1)

-- Adding second timer should not add another listener
local auto_reg_timer2 = LLTimers:on(2.0, function() end)
assert(#LLEvents:listeners("timer") == 1)

-- Removing first timer should keep listener (still have timer2)
LLTimers:off(auto_reg_timer1)
assert(#LLEvents:listeners("timer") == 1)

-- Removing last timer should auto-deregister
LLTimers:off(auto_reg_timer2)
assert(#LLEvents:listeners("timer") == 0)

-- Test that timer wrapper in listeners() cannot be called directly
local guard_timer = LLTimers:on(1.0, function() end)
local timer_listeners = LLEvents:listeners("timer")
assert(#timer_listeners == 1)

local guard_func = timer_listeners[1]
assert_errors(function()
    guard_func()
end, "Cannot call internal timer wrapper directly")

-- Verify guard function exists
assert(guard_func ~= nil)

-- Clean up
LLTimers:off(guard_timer)

-- Test that LLEvents:_handleEvent('timer') drives LLTimers:_tick()
setclock(5.0)
local integration_count = 0
local integration_timer = LLTimers:on(0.5, function()
    integration_count += 1
end)

-- Manually trigger timer event via LLEvents (no arguments)
setclock(5.6)
LLEvents:_handleEvent('timer')
assert(integration_count == 1)

-- Trigger again
incrementclock(0.5)
LLEvents:_handleEvent('timer')
assert(integration_count == 2)

-- Clean up
LLTimers:off(integration_timer)

-- Test callable tables (tables with __call metamethod) as timer handlers
setclock(6.0)
local callable_count = 0
local callable_table = nil
callable_table = setmetatable({}, {
    __call = function(self)
        assert(self == callable_table)
        callable_count += 1
    end
})

-- Register callable table as timer handler
local callable_timer = LLTimers:on(0.3, callable_table)
assert(callable_timer ~= nil)

-- Advance time and trigger timer
setclock(6.4)
LLEvents:_handleEvent('timer')
assert(callable_count == 1)

-- Trigger again
incrementclock(0.3)
LLEvents:_handleEvent('timer')
assert(callable_count == 2)

-- Test unregistering by passing the same table reference
local off_result = LLTimers:off(callable_table)
assert(off_result == true)

-- Verify timer no longer fires
incrementclock(0.3)
LLEvents:_handleEvent('timer')
assert(callable_count == 2)

-- make sure serialization still works
setclock(0)
-- error() is the poor man's long-return.
local function throw_error() error("called!") end
LLTimers:on(0.5, throw_error)

-- In reality you wouldn't give users primitives to clone these, but just for testing!
local timers_clone = ares.unpersist(ares.persist(LLTimers))
setclock(0.6)

assert_errors(function() timers_clone:_tick() end, "called!")
assert_errors(function() LLTimers:_tick() end, "called!")

incrementclock(0.6)

LLTimers:off(throw_error)
-- Only one of them now has the problematic handler
LLTimers:_tick()
assert_errors(function() timers_clone:_tick() end, "called!")

-- Test that setTimerEventCb is called with correct intervals
-- Single timer should schedule with correct interval
setclock(10.0)
local interval_timer1 = LLTimers:every(0.5, function() end)
assert(math.abs(get_last_interval() - 0.5) < 0.001)

-- Adding an earlier timer should reschedule to shorter interval
local interval_timer2 = LLTimers:on(0.3, function() end)
assert(math.abs(get_last_interval() - 0.3) < 0.001)

-- Adding a later timer should not change the interval
local interval_timer3 = LLTimers:on(1.0, function() end)
-- Should still be 0.3 (the earliest timer)
assert(math.abs(get_last_interval() - 0.3) < 0.001)

-- Scenario 4: Removing the earliest timer should reschedule to next timer
LLTimers:off(interval_timer2)
-- Now earliest should be 0.5
assert(math.abs(get_last_interval() - 0.5) < 0.001)

-- Scenario 5: Removing all timers should call with 0.0
LLTimers:off(interval_timer1)
LLTimers:off(interval_timer3)
assert(math.abs(get_last_interval() - 0.0) < 0.001)

-- Oh also we should make sure that `:once()` behaves correctly.
LLTimers:once(0.5, interval_timer1)
assert(math.abs(get_last_interval() - 0.5) < 0.001)

-- Test that scheduled time parameter is passed to timer handlers
-- Basic parameter passing
setclock(10.0)
local received_scheduled_time = nil
local test_handler = LLTimers:on(0.5, function(scheduled_time)
    received_scheduled_time = scheduled_time
end)

setclock(10.6)  -- Fire at 10.5 + some slop
LLEvents:_handleEvent('timer')
assert(received_scheduled_time ~= nil, "Handler should receive scheduled_time parameter")
assert(math.abs(received_scheduled_time - 10.5) < 0.001, "Scheduled time should be 10.5")
LLTimers:off(test_handler)

-- Verify it's scheduled time, not current time
setclock(15.0)
local scheduled_vs_current = {}
local timing_handler = LLTimers:on(0.3, function(scheduled_time)
    local current_time = getclock()
    table.insert(scheduled_vs_current, {
        scheduled = scheduled_time,
        current = current_time,
    })
end)

-- Intentionally call tick "late" to create a gap
setclock(15.4)  -- Should have fired at 15.3
LLEvents:_handleEvent('timer')

assert(#scheduled_vs_current == 1)
local timing = scheduled_vs_current[1]
-- Scheduled time should be 15.3, current should be 15.4
assert(math.abs(timing.scheduled - 15.3) < 0.001, "Scheduled should be 15.3")
assert(math.abs(timing.current - 15.4) < 0.001, "Current should be 15.4")
assert(timing.current > timing.scheduled, "Current time should be later than scheduled")

LLTimers:off(timing_handler)

-- Test diff calculation for delay detection
setclock(20.0)
local delays = {}
local delay_handler = LLTimers:on(0.1, function(scheduled_time)
    local current_time = getclock()
    local delay = current_time - scheduled_time
    table.insert(delays, delay)
end)

-- First call on time
setclock(20.11)
LLEvents:_handleEvent('timer')
assert(#delays == 1)
assert(delays[1] < 0.02, "First delay should be minimal")

-- Second call late
setclock(20.3)  -- Should have fired at 20.2, we're 0.1 late
LLEvents:_handleEvent('timer')
assert(#delays == 2)
assert(delays[2] > 0.09, "Second delay should be ~0.1")
assert(delays[2] < 0.11, "Second delay should be ~0.1")

LLTimers:off(delay_handler)

-- Test that once() timers also receive scheduled_time parameter
setclock(25.0)
local once_scheduled_time = nil
local once_handler = LLTimers:once(0.7, function(scheduled_time)
    once_scheduled_time = scheduled_time
end)

setclock(25.8)
LLEvents:_handleEvent('timer')
assert(once_scheduled_time ~= nil, "once() handler should receive parameter")
assert(math.abs(once_scheduled_time - 25.7) < 0.001, "once() scheduled time should be 25.7")

-- Test multiple timers receive correct individual scheduled times
setclock(30.0)
local timer_results = {}
local handler1 = LLTimers:on(0.2, function(scheduled_time)
    table.insert(timer_results, {id = 1, scheduled = scheduled_time})
end)
local handler2 = LLTimers:on(0.3, function(scheduled_time)
    table.insert(timer_results, {id = 2, scheduled = scheduled_time})
end)

setclock(30.35)  -- Both should fire (at 30.2 and 30.3)
LLEvents:_handleEvent('timer')

assert(#timer_results == 2)
-- Find which is which
local result1 = timer_results[1].id == 1 and timer_results[1] or timer_results[2]
local result2 = timer_results[1].id == 2 and timer_results[1] or timer_results[2]

assert(math.abs(result1.scheduled - 30.2) < 0.001)
assert(math.abs(result2.scheduled - 30.3) < 0.001)

LLTimers:off(handler1)
LLTimers:off(handler2)

-- Test that on() timers get new scheduled_time for each invocation
setclock(35.0)
local repeat_scheduled_times = {}
local repeat_handler = nil
repeat_handler = LLTimers:on(0.5, function(scheduled_time)
    table.insert(repeat_scheduled_times, scheduled_time)
    if #repeat_scheduled_times >= 3 then
        LLTimers:off(repeat_handler)
    end
end)

setclock(35.6)
LLEvents:_handleEvent('timer')  -- Should fire at 35.5
setclock(36.1)
LLEvents:_handleEvent('timer')  -- Should fire at 36.0
setclock(36.6)
LLEvents:_handleEvent('timer')  -- Should fire at 36.5

assert(#repeat_scheduled_times == 3)
assert(math.abs(repeat_scheduled_times[1] - 35.5) < 0.001)
assert(math.abs(repeat_scheduled_times[2] - 36.0) < 0.001)
assert(math.abs(repeat_scheduled_times[3] - 36.5) < 0.001)

-- Test clamped catch-up: timers >2s late skip ahead instead of rapid-firing
setclock(40.0)
local catchup_fires = 0
local catchup_scheduled_times = {}
local catchup_handler = LLTimers:on(0.1, function(scheduled_time)
    catchup_fires += 1
    table.insert(catchup_scheduled_times, scheduled_time)
end)

-- Make timer VERY late (4.9 seconds, exceeds 2-second threshold)
setclock(45.0)
LLEvents:_handleEvent('timer')

-- Should fire ONCE per _handleEvent call, not 49 times
assert(catchup_fires == 1, "Timer should fire once per handleEvent call")

-- scheduled_time parameter shows when it WAS scheduled (40.1), not when it got rescheduled to
assert(math.abs(catchup_scheduled_times[1] - 40.1) < 0.001, "First fire shows original schedule time")

-- Fire again to verify logical schedule syncs when clamping
setclock(45.1)
LLEvents:_handleEvent('timer')
assert(catchup_fires == 2, "Should fire again on next handleEvent")
-- When we clamp, we sync the logical schedule to the new reality
-- This means handlers see the initial delay (first fire), then return to normal
assert(catchup_scheduled_times[2] > 44.9, "Second fire shows synced schedule (~45.0)")
assert(catchup_scheduled_times[2] < 45.2, "Second fire shows synced schedule (~45.0)")
-- Handler sees normal delay now: getclock() - scheduled_time = 45.1 - 45.0 = ~0.1s

-- Clean up
LLTimers:off(catchup_handler)


-- Verify that user code can't get a reference to the timers tick function through `debug`
local expected = {
    "lltimers.lua:false",
    -- Should not be able to get a reference to `LLTimers:_tick()`
    "[C]:true",
    "lltimers.lua:false",
}
local passed = false
LLTimers:once(0, function()
    local i = 0
    repeat
        local name, func = debug.info(i + 1, "sf")
        if name == nil then
            break
        end
        i += 1
        local actual = `{name}:{func==nil}`
        assert(expected[i] == actual, `{expected[i]} == {actual}`)
    until false
    assert(i == #expected, `Had same number of calls ({i} == {#expected})`)
    passed = true
end)
LLTimers:_tick()
assert(passed)

return "OK"
