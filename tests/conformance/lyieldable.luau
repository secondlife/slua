-- Tests for lyieldable.h yieldable C function framework

-- Non-yielding paths
assert(yieldable_sum(function(i) return i end, 5) == 15)
assert(yieldable_sum(function(i) return i end, 0) == 0)
assert(yieldable_sum(function(i) return 10 end, 3) == 30)

-- Sequential calls: clean state between invocations
assert(yieldable_sum(function(i) return i end, 3) == 6)
assert(yieldable_sum(function(i) return i * 2 end, 4) == 20)

-- Yielding callback via coroutine
do
    local co = coroutine.create(function()
        return yieldable_sum(function(i)
            coroutine.yield(i)
            return i
        end, 5)
    end)

    for i = 1, 5 do
        local ok, val = coroutine.resume(co)
        assert(ok and val == i)
    end
    local ok, result = coroutine.resume(co)
    assert(ok and result == 15)
end

-- Yielding with varying counts
do
    local function run(n)
        local co = coroutine.create(function()
            return yieldable_sum(function(i)
                coroutine.yield()
                return i
            end, n)
        end)
        for j = 1, n do assert(coroutine.resume(co)) end
        local ok, result = coroutine.resume(co)
        assert(ok)
        return result
    end
    assert(run(1) == 1)
    assert(run(5) == 15)
    assert(run(10) == 55)
end

-- Ares round-trip: serialize mid-loop, restore, resume
do
    local co = coroutine.create(function()
        return yieldable_sum(function(i)
            coroutine.yield(i)
            return i
        end, 5)
    end)

    for i = 1, 3 do
        local ok, val = coroutine.resume(co)
        assert(ok and val == i)
    end

    local restored = ares.unpersist(ares.persist(co))
    co = nil
    collectgarbage()

    for i = 4, 5 do
        local ok, val = coroutine.resume(restored)
        assert(ok and val == i)
    end
    local ok, result = coroutine.resume(restored)
    assert(ok and result == 15)
end

-- Chained SlotManager: helper function with own slots
-- yieldable_chained(callback, start, count) calls callback(0) first (in parent),
-- then delegates to helper which calls callback(i) for i=start..start+count-1.
-- This tests that the helper's slots.isInit() returns true even when the parent
-- has already yielded & resumed before entering the helper.
assert(yieldable_chained(function(i) return i end, 1, 5) == 15)
assert(yieldable_chained(function(i) return i end, 1, 0) == 0)
assert(yieldable_chained(function(i) return 10 end, 1, 3) == 40)

-- Chained: yielding callback (yields 0 from parent pre-call, then 1..5 from helper)
do
    local co = coroutine.create(function()
        return yieldable_chained(function(i)
            coroutine.yield(i)
            return i
        end, 1, 5)
    end)

    -- First yield is callback(0) from the parent
    local ok, val = coroutine.resume(co)
    assert(ok and val == 0)
    -- Then 1..5 from the helper
    for i = 1, 5 do
        ok, val = coroutine.resume(co)
        assert(ok and val == i)
    end
    ok, result = coroutine.resume(co)
    assert(ok and result == 15)
end

-- Chained: ares round-trip after parent pre-call but before helper.
-- Serialize right after the parent's callback(0) yield — this is the critical
-- case where the parent has yielded & resumed, but the helper hasn't run yet.
-- Uses start=100 so the yielded values prove the helper's isInit() returned true:
-- seeing val==100 means the helper called callback(100), which requires init to
-- have read start=100 from the Lua stack. If isInit() were broken (returned false),
-- i and end would stay at default 0, the loop wouldn't run, and we'd get 0.
do
    local co = coroutine.create(function()
        return yieldable_chained(function(i)
            coroutine.yield(i)
            return i
        end, 100, 3)
    end)

    -- Resume through parent's callback(0)
    local ok, val = coroutine.resume(co)
    assert(ok and val == 0)

    -- Serialize and restore right here — parent has yielded, helper not yet entered
    local restored = ares.unpersist(ares.persist(co))
    co = nil
    collectgarbage()

    -- Each yielded value IS the callback argument. Seeing 100, 101, 102 proves
    -- the helper initialized correctly (read start=100 from the stack).
    for i = 100, 102 do
        ok, val = coroutine.resume(restored)
        assert(ok and val == i)
    end
    ok, result = coroutine.resume(restored)
    assert(ok and result == 303) -- 100 + 101 + 102
end

-- Chained: ares round-trip mid-helper-loop
do
    local co = coroutine.create(function()
        return yieldable_chained(function(i)
            coroutine.yield(i)
            return i
        end, 1, 5)
    end)

    -- Resume through parent's callback(0) and first 3 helper iterations
    local ok, val = coroutine.resume(co)
    assert(ok and val == 0)
    for i = 1, 3 do
        ok, val = coroutine.resume(co)
        assert(ok and val == i)
    end

    local restored = ares.unpersist(ares.persist(co))
    co = nil
    collectgarbage()

    for i = 4, 5 do
        ok, val = coroutine.resume(restored)
        assert(ok and val == i)
    end
    ok, result = coroutine.resume(restored)
    assert(ok and result == 15)
end

-- Recursive SlotManager: sum 1..n via recursion
-- yieldable_recursive(callback, n) calls callback(i) for i=n..1, accumulates results
assert(yieldable_recursive(function(i) return i end, 0) == 0)
assert(yieldable_recursive(function(i) return i end, 1) == 1)
assert(yieldable_recursive(function(i) return i end, 5) == 15)
assert(yieldable_recursive(function(i) return i end, 10) == 55)

-- Recursive: yielding callback via coroutine
do
    local co = coroutine.create(function()
        return yieldable_recursive(function(i)
            coroutine.yield(i)
            return i
        end, 5)
    end)

    -- Yields at each recursion level: 5, 4, 3, 2, 1
    for i = 5, 1, -1 do
        local ok, val = coroutine.resume(co)
        assert(ok and val == i)
    end
    local ok, result = coroutine.resume(co)
    assert(ok and result == 15)
end

-- Recursive: ares round-trip mid-recursion
do
    local co = coroutine.create(function()
        return yieldable_recursive(function(i)
            coroutine.yield(i)
            return i
        end, 5)
    end)

    -- Resume through first 2 levels (yields 5, 4)
    for i = 5, 4, -1 do
        local ok, val = coroutine.resume(co)
        assert(ok and val == i)
    end

    local restored = ares.unpersist(ares.persist(co))
    co = nil
    collectgarbage()

    -- Continue from level 3 (yields 3, 2, 1)
    for i = 3, 1, -1 do
        local ok, val = coroutine.resume(restored)
        assert(ok and val == i)
    end
    local ok, result = coroutine.resume(restored)
    assert(ok and result == 15)
end

return "OK"
