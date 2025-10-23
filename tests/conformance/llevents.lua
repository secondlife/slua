assert(#LLEvents:eventNames() == 0)

local touch_handler = LLEvents:on("touch_start", function(detected) print(#detected) end)
assert(#LLEvents:eventNames() == 1)
assert(#LLEvents:listeners("touch_start") == 1)
assert(LLEvents:listeners("touch_start")[1] == touch_handler)

assert(LLEvents:off("touch_start", touch_handler))

assert(#LLEvents:eventNames() == 0)
assert(#LLEvents:listeners("touch_start") == 0)

-- Only returns true when it was actually subscribed before
assert(not LLEvents:off("touch_start", touch_handler))

local once_handler = LLEvents:once("touch_start", function(detected) print(#detected) end)
assert(#LLEvents:eventNames() == 1)
assert(#LLEvents:listeners("touch_start") == 1)
assert(LLEvents:listeners("touch_start")[1] == once_handler)

-- Clean up before handleEvent tests
assert(LLEvents:off("touch_start", once_handler))


-- handleEvent with no handlers (should be no-op)
-- Note that _handleEvent is NOT part of the public API and can't be
-- called directly except in tests.
LLEvents:_handleEvent("listen", 0, "test", "key", "msg")

-- Basic non-multi event handling
local listen_called = false
local listen_args = nil
LLEvents:on("listen", function(channel, name, id, msg)
    listen_called = true
    listen_args = {channel, name, id, msg}
end)

LLEvents:_handleEvent("listen", 0, "test", "key", "msg")
assert(listen_called)
assert(listen_args[1] == 0)
assert(listen_args[2] == "test")
assert(listen_args[3] == "key")
assert(listen_args[4] == "msg")

-- Multiple handlers for same event
local call_order = {}
LLEvents:on("listen", function() table.insert(call_order, 1) end)
LLEvents:on("listen", function() table.insert(call_order, 2) end)
LLEvents:on("listen", function() table.insert(call_order, 3) end)

LLEvents:_handleEvent("listen", 0, "test", "key", "msg")
assert(#call_order == 3)
assert(call_order[1] == 1)
assert(call_order[2] == 2)
assert(call_order[3] == 3)

-- Clean up listen handlers
local function unreg_all(event_name)
    for _, event_name in LLEvents:eventNames() do
        for _, handler in LLEvents:listeners(event_name) do
            LLEvents:off(event_name, handler)
        end
        assert(#LLEvents:listeners(event_name) == 0)
    end
    assert(#LLEvents:eventNames() == 0)
end

unreg_all()


-- Multi-event handling (touch_start)
local detected_table = nil
LLEvents:on("touch_start", function(detected)
    detected_table = detected
    assert(detected[1].valid)
    -- Should return the 0-indexed event index we used.
    assert(detected[3]:getTouchFace() == 2)
end)

LLEvents:_handleEvent("touch_start", 5)
assert(detected_table ~= nil)
assert(#detected_table == 5)
assert(typeof(detected_table[1]) == "DetectedEvent")
-- We're outside the event handler, so this shouldn't be valid anymore
assert(not detected_table[1].valid)
LLEvents:off("touch_start", LLEvents:listeners("touch_start")[1])

-- Once handler with handleEvent
local once_call_count = 0
local once_args = nil
local once_test_handler = LLEvents:once("listen", function(...)
    once_call_count = once_call_count + 1
    once_args = table.pack(...)
end)

assert(#LLEvents:listeners("listen") == 1)
LLEvents:_handleEvent("listen", 0, "test", "key", "msg")
assert(once_call_count == 1)
assert(#LLEvents:listeners("listen") == 0)
assert(#once_args == 4)
assert(once_args[4] == "msg")

-- Call again, should not increment
LLEvents:_handleEvent("listen", 0, "test", "key", "msg")
assert(once_call_count == 1)

-- Try a `function LLEvents.event` style handler
local call_count = 0
function LLEvents.listen(chan, name, id, msg)
    call_count += 1
    assert(msg == "msg")
end

LLEvents:_handleEvent("listen", 0, "test", "key", "msg")
assert(call_count == 1)

unreg_all()

-- Errors bubble up and interrupt following event handlers, but they should be recoverable
-- if whatever is driving `LLEvents` decides to make them so.
LLEvents:on("listen", function() table.insert(call_order, 1) end)
LLEvents:on("listen", function() error('foo'); table.insert(call_order, 2) end)
LLEvents:on("listen", function() table.insert(call_order, 3) end)

-- Do this twice in a row so we can be sure errors don't hose the state somehow
for i=1,2 do
    call_order = {}
    local success, ret = pcall(function() LLEvents:_handleEvent("listen", 0, "test", "key", "msg") end)
    assert(not success)
    assert(#call_order == 1)
end

unreg_all()

-- Make sure we can handle internal `lua_break()`s due to the scheduler
call_order = {}
LLEvents:on("listen", function() table.insert(call_order, 1); breaker() end)
-- This should work with :once() too :)
LLEvents:once("listen", function() breaker(); table.insert(call_order, 2) end)
LLEvents:on("listen", function() table.insert(call_order, 3); breaker(); table.insert(call_order, 4); end)

LLEvents:_handleEvent("listen", 0, "test", "key", "msg")

assert(lljson.encode(call_order) == "[1,2,3,4]")

unreg_all()

-- should be the same with lua_yield()

call_order = {}
local yield_order = {}
LLEvents:on("listen", function() table.insert(call_order, 1); coroutine.yield(1) end)
-- This should work with :once() too :)
LLEvents:once("listen", function() coroutine.yield(2); table.insert(call_order, 2) end)
LLEvents:on("listen", function() table.insert(call_order, 3); coroutine.yield(3); table.insert(call_order, 4); end)

local handle_coro = coroutine.create(function() LLEvents:_handleEvent("listen", 0, "test", "key", "msg") end)

while true do
    local co_status, yielded_val = coroutine.resume(handle_coro)
    if not co_status then
        break
    end
    table.insert(yield_order, yielded_val)
end
assert(lljson.encode(call_order) == "[1,2,3,4]")
assert(lljson.encode(yield_order) == "[1,2,3]")

-- Check that we allow filtering calls to :on() and :off()

local function assert_errors(func, expected_str)
    local success, ret = pcall(func)
    assert(not success)
    local is_match = typeof(ret) == "string" and ret:find(expected_str) ~= nil
    if not is_match then
        print(ret, "!=", expected_str)
    end
    assert(is_match)
end

call_order = {}

-- This should still be called because it was in the handlers table
-- at the point the event was triggered.
local function second_handler()
    table.insert(call_order, 2)
end

local function third_handler()
    table.insert(call_order, 3)
end

local function fourth_handler()
    table.insert(call_order, 4)
end


local function first_handler()
    table.insert(call_order, 1)
    -- This will only take effect for the _next_ event.
    LLEvents:off('touch_start', second_handler)
    -- This should never get called during this cycle because
    -- it was registered _during_ the event handling.
    LLEvents:on('touch_start', fourth_handler)
end

LLEvents:on('touch_start', first_handler)
LLEvents:on('touch_start', second_handler)
LLEvents:on('touch_start', second_handler)
LLEvents:on('touch_start', second_handler)
LLEvents:on('touch_start', third_handler)

-- This reflects desired behavior.
LLEvents:_handleEvent('touch_start', 1)
assert(lljson.encode(call_order) == "[1,2,2,3]")

unreg_all()

assert_errors(
    function() LLEvents:on("disallowed", function() return end) end,
    "'disallowed' is not a supported event name"
)
assert_errors(
    function() LLEvents:off("disallowed", function() return end) end,
    "'disallowed' is not a supported event name"
)

-- Don't define event handlers with the method style declaration. It
-- adds an implicit self parameter.
assert_errors(
    function()
        function LLEvents:touch_start(detected) print(detected) end
    end,
    "Event handler defined with ':' syntax; use '.'"
)

-- Run this last, check that we can block handle event calls
set_may_call_handle_event(false)
LLEvents:on('listen', function() assert(false) end)
assert_errors(
    function() LLEvents:_handleEvent("listen", 0, "test", "key", "msg") end,
    "Not allowed to call LLEvents:_handleEvent()"
)

return 'OK'
