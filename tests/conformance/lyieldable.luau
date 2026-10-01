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

-- A coroutine suspended three callbacks into yieldable_sum, persisted
local function saved_mid_sum()
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
    return ares.persist(co)
end

local function finish_sum(restored)
    for i = 4, 5 do
        local ok, val = coroutine.resume(restored)
        assert(ok and val == i)
    end
    local ok, result = coroutine.resume(restored)
    assert(ok and result == 15)
end

-- A frame older than the build loads and resumes, its bytes read as they are
do
    local saved = saved_mid_sum()
    set_test_abi_version(1)
    finish_sum(ares.unpersist(saved))
    set_test_abi_version(0)
end

-- A frame newer than the build is refused at load
do
    set_test_abi_version(1)
    local newer = saved_mid_sum()
    set_test_abi_version(0)
    local ok, err = pcall(ares.unpersist, newer)
    assert(not ok and string.find(err, "test_yieldable_sum", 1, true) ~= nil, err)
end

-- The resume path checks a buffer that never went through Ares.
-- call_test_sum_k runs the continuation over hand-built bytes with the
-- callback's pending result on the stack.
do
    local function state(version, region_len, phase, innermost)
        return string.pack("<BI2Bi4i4i4B", version, region_len, phase, 4, 5, 6, innermost)
    end
    local function add(i) return i end
    -- i=4 of n=5 with 6 banked and callback(4)'s result of 4 pending
    assert(call_test_sum_k(state(0, 14, 1, 1), add, 5, 4) == 15)

    local function assert_resume_fails(bytes, pattern)
        local ok, err = pcall(call_test_sum_k, bytes, add, 5, 4)
        assert(not ok and string.find(err, pattern, 1, true) ~= nil, err)
    end
    assert_resume_fails(state(1, 14, 1, 1), "newer version")
    assert_resume_fails(state(0, 200, 1, 1), "corrupt yield state")
    assert_resume_fails(state(0, 14, 9, 1), "corrupt yield state")
    assert_resume_fails(state(0, 14, 1, 2), "corrupt yield state")

    -- A region that stops short of the accumulator slot leaves it at its
    -- default, so only the pending 4 and callback(5) count
    assert(call_test_sum_k(string.pack("<BI2Bi4i4B", 0, 10, 1, 4, 5, 1), add, 5, 4) == 9)
    -- A region with a trailing slot this build doesn't know is read as its prefix
    assert(call_test_sum_k(string.pack("<BI2Bi4i4i4i4B", 0, 18, 1, 4, 5, 6, 99, 1), add, 5, 4) == 15)
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

-- A chain whose regions don't fit the uint16 offsets is refused up front,
-- before any yield could write through wrapped offsets
do
    local ok, err = pcall(yieldable_wide)
    assert(not ok, "wide yield state was accepted")
    assert(string.find(err, "yield state too large", 1, true), err)
end

return "OK"
