-- Test that interrupts are called between event handlers and timer handlers

-- Test LLEvents interrupt between handlers
local call_count = 0
LLEvents:on("listen", function() call_count += 1 end)
LLEvents:on("listen", function() call_count += 1 end)
LLEvents:on("listen", function() call_count += 1 end)

LLEvents:_handleEvent("listen", 0, "test", "key", "msg")

assert(call_count == 3)

-- Test LLTimers interrupt between handlers
call_count = 0
LLTimers:every(0.1, function() call_count += 1 end)
LLTimers:every(0.1, function() call_count += 1 end)
LLTimers:every(0.1, function() call_count += 1 end)

setclock(1.0)
LLTimers:_tick()

assert(call_count == 3)

return 'OK'
