-- Break propagation in callsandboxedrequire

-- Basic break propagation
local result = callsandboxedrequire(function()
    breaker()
    return "after break"
end)
assert(result == "after break", "Should continue after break")

-- Break with return values
local a, b, c = callsandboxedrequire(function()
    breaker()
    return 1, 2, 3
end)
assert(a == 1 and b == 2 and c == 3, "Should return values after break")

-- Nested breaks
local nested_result = callsandboxedrequire(function()
    breaker()
    return callsandboxedrequire(function()
        breaker()
        return "nested break"
    end)
end)
assert(nested_result == "nested break", "Should handle nested breaks")

return "OK"
