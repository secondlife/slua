-- Basic execution with single return value
local result = callsandboxedrequire(function()
    return 42
end)
assert(result == 42, "Should return single value")

-- Multiple return values
local a, b, c = callsandboxedrequire(function()
    return 1, 2, 3
end)
assert(a == 1 and b == 2 and c == 3, "Should return multiple values")

-- No return value
local no_result = callsandboxedrequire(function()
    local x = 5
end)
assert(no_result == nil, "Should return nil when function returns nothing")

-- Global sandboxing - clean globals, isolated from caller
test_value = "original"
callsandboxedrequire(function()
    assert(test_value == nil, "Should have clean globals, not see caller's values")
    test_value = "modified"
    assert(test_value == "modified", "Should see own modified value")
end)
assert(test_value == "original", "Caller's value should remain unchanged")

-- Error propagation
local success, err = pcall(function()
    callsandboxedrequire(function()
        error("test error message")
    end)
end)
assert(not success, "Should propagate errors")
assert(string.match(err, "test error message"), "Should include error message")

-- Yield rejection
local yield_success, yield_err = pcall(function()
    callsandboxedrequire(function()
        coroutine.yield(123)
    end)
end)
assert(not yield_success, "Should error on yield")
assert(string.match(yield_err, "attempt to yield from sandboxed require"), "Should have specific yield error message")

-- Type checking - non-function argument
local type_success, type_err = pcall(function()
    callsandboxedrequire("not a function")
end)
assert(not type_success, "Should error on non-function")
assert(string.match(type_err, "function expected"), "Should have type error message")

-- Type checking - nil argument
local nil_success, nil_err = pcall(function()
    callsandboxedrequire(nil)
end)
assert(not nil_success, "Should error on nil")

-- Nested callsandboxedrequire
local nested = callsandboxedrequire(function()
    return callsandboxedrequire(function()
        return "deeply nested"
    end)
end)
assert(nested == "deeply nested", "Should support nested calls")

-- Read access to standard library
local stdlib_result = callsandboxedrequire(function()
    -- math.sqrt might be folded, but table.create definitely won't.
    local f = table.create(1)
    f[1] = 4
    return f
end)
assert(stdlib_result[1] == 4, "Should have access to read standard library")

-- Table return values
local tbl = callsandboxedrequire(function()
    return {a = 1, b = 2, c = 3}
end)
assert(tbl.a == 1 and tbl.b == 2 and tbl.c == 3, "Should handle table returns")

-- Function calls within sandboxed code
local call_result = callsandboxedrequire(function()
    local function helper(x)
        return x * 2
    end
    return helper(21)
end)
assert(call_result == 42, "Should support function calls within sandboxed code")

-- Upvalues should be rejected
local outer = {value = 100}
local upvalue_success, upvalue_err = pcall(function()
    callsandboxedrequire(function()
        return outer.value + 23
    end)
end)
assert(not upvalue_success, "Should error on function with upvalues")
assert(string.match(upvalue_err, "upvalues not allowed"), "Should have upvalue error message")

-- Returned functions should keep sandboxed env (isolated from caller)
a_global = "1"
good_global = "1"
local get_sandboxed_env = callsandboxedrequire(function()
    inner_global = "1"
    good_global = "2"
    return function()
        -- Return the function's view of the globals as a table
        local view = {inner_global, a_global, new_global, good_global, LLEvents}
        -- Drop the reference to that table
        good_global = nil
        return view
    end
end)

-- Make sure the returned environment matches what we'd expect (clean globals)
local v = get_sandboxed_env()
assert(v[1] == "1" and v[2] == nil and v[3] == nil and v[4] == "2" and v[5] == LLEvents)
assert(good_global == "1")

local old_llevents = LLEvents
LLEvents = nil

-- Change our env and see if the function notices (it shouldn't - isolated)
new_global = "1"
v = get_sandboxed_env()
assert(v[1] == "1" and v[2] == nil and v[3] == nil and v[4] == nil and v[5] == old_llevents)

return "OK"
