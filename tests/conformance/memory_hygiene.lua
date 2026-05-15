local TVALUE_SIZE = 16

-- Clear out temporaries for args after `LOP_CALL` so that they aren't
-- reachable by the GC in any following expressions.
-- This makes sure that `ll.GetUsedMemory()` returns something that people can
-- reason about, without having to know too much about register allocation.

-- Our `ll.GetUsedMemory()` is kind of hacky, and doesn't subsidize things the way
-- that the SL server will, so grab a baseline to diff off of.
local mem = ll.GetUsedMemory()
local tab = table.create(32, true)
local growth = ll.GetUsedMemory() - mem

assert(growth >= 32 * TVALUE_SIZE, `expected >=512 byte growth from table.create, got {growth}`)

-- Replace the `tab` local with a 1-elem slice of itself.
-- The old `tab` will be referenced as the first arg of `table.move()`, but we should
-- sweep it up after the call is done.
tab = table.move(tab, 1, 1, 1, {})

local function check_is_small(a, b)
    assert(a < 32 * 8, `expected small first delta after table.move, got {a}`)
    assert(b < 32 * 8, `expected small second delta after table.move, got {b}`)
    assert(a == b, "successive memory checks returned different values")
end

-- Slots will be reserved for the result of `ll.GetUsedMemory() - mem`, but won't be written to
-- until `ll.GetUsedMemory()` actually returns. Incidentally, that's likely the slot that the temporary
-- for the old `tab` was living in. If we haven't swept up, this will let us know.
check_is_small(ll.GetUsedMemory() - mem, ll.GetUsedMemory() - mem)


-- Check sizes for stack reallocs.

-- Run each subtest in a fresh coroutine so its stack starts at BASIC_STACK_SIZE
-- and growth is attributable. Returns the memory delta observed inside the
-- coroutine across the test body.
local function measure_coro_mem(body)
    local co = coroutine.create(function()
        local before = ll.GetUsedMemory()
        body()
        return ll.GetUsedMemory() - before
    end)
    local ok, delta = coroutine.resume(co)
    assert(ok, delta)
    return delta
end

local SLACK = 128 * TVALUE_SIZE

-- capacity should round to next 128-slot boundary above demand.
local d = measure_coro_mem(function()
    local t = table.create(1000, true)
    local _ = table.unpack(t)
end)
assert(d >= (1000 * TVALUE_SIZE) - SLACK, `unpack: too small ({d})`)
assert(d < (1000 * TVALUE_SIZE) + (4 * SLACK), `unpack: exceeds cap ({d})`)

-- Deep recursion to depth 1200, size should stay close to demand rather than doubling.
local function recurse(n) if n <= 0 then return end return recurse(n - 1) end
local d = measure_coro_mem(function() recurse(1200) end)
assert(d < 220 * 1024, `recurse: uncapped doubling ({d})`)

return 'OK'
