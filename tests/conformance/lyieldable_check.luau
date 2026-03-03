-- Tests for YIELD_CHECK interrupt-driven yields in lyieldable framework.
-- The interrupt handler is installed globally by the C++ test fixture
-- and calls lua_yield(L, 0) on every YIELD_CHECK hit (reason -100).

-- Wraps f in a coroutine, fully consumes it, returns (result, yield_count).
-- Asserts that actual yield count matches the interrupt handler's count.
local function consume(f, ...)
    clear_check_count()
    local co = coroutine.create(f)
    local yields = 0
    local ok, result = coroutine.resume(co, ...)
    assert(ok, result)
    while coroutine.status(co) ~= "dead" do
        yields += 1
        ok, result = coroutine.resume(co)
        assert(ok, result)
    end
    assert(yields == get_check_count(),
        "yield count mismatch: " .. yields .. " actual vs " .. get_check_count() .. " interrupts")
    return result, yields
end

-- Simple YIELD_CHECK: sum 1..n
local result, yields = consume(yieldable_check_sum, 5)
assert(result == 15 and yields == 5)

result, yields = consume(yieldable_check_sum, 0)
assert(result == 0 and yields == 0)

result, yields = consume(yieldable_check_sum, 1)
assert(result == 1 and yields == 1)

-- Nested YIELD_CHECK: 1 parent PRE_CHECK + n child loop iterations
result, yields = consume(yieldable_chained_check, 1, 5)
assert(result == 15 and yields == 6)

result, yields = consume(yieldable_chained_check, 1, 0)
assert(result == 0 and yields == 1)

result, yields = consume(yieldable_chained_check, 100, 3)
assert(result == 303 and yields == 4)

-- Same tests inside xpcall
result, yields = consume(function()
    local ok, r = xpcall(yieldable_check_sum, debug.traceback, 5)
    assert(ok, r)
    return r
end)
assert(result == 15 and yields == 5)

result, yields = consume(function()
    local ok, r = xpcall(yieldable_chained_check, debug.traceback, 1, 5)
    assert(ok, r)
    return r
end)
assert(result == 15 and yields == 6)

result, yields = consume(function()
    local ok, r = xpcall(yieldable_chained_check, debug.traceback, 100, 3)
    assert(ok, r)
    return r
end)
assert(result == 303 and yields == 4)

return "OK"
