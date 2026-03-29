-- Tests for array sizing cap behavior
-- The cap limits "wasted" array slots to at most 127 for large arrays

-- Build array by insertion to trigger rehash (table.create() pre-allocates, skipping resize)
local function make_array(n)
    local t = {}
    for i = 1, n do
        t[i] = true
    end
    return t
end

local function check_sizes(t, expected_arr, expected_hash, desc)
    local arr_size, hash_size = table_sizes(t)
    assert(arr_size == expected_arr, `{desc}: expected {expected_arr} array slots, got {arr_size}`)
    assert(hash_size == expected_hash, `{desc}: expected {expected_hash} hash slots, got {hash_size}`)
end

-- Basic cap behavior - 600 elements should get 640, not 1024
local t = make_array(600)
check_sizes(t, 640, 0, "600 elements")

t = make_array(512)
check_sizes(t, 512, 0, "512 threshold")

t = make_array(513)
check_sizes(t, 640, 0, "513 threshold")

t = make_array(1500)
check_sizes(t, 1536, 0, "1500 elements")

t = make_array(2000)
check_sizes(t, 2048, 0, "2000 elements")

-- Boundary invariant - sparse table with boundary at high index
-- Element at 1000 doesn't meet 50% threshold, goes to hash
t = {}
t[1] = true
t[1000] = true
check_sizes(t, 1, 1, "boundary invariant")

-- Mixed array/hash - string keys don't affect array sizing
t = make_array(600)
t["foo"] = "bar"
t["baz"] = "qux"
check_sizes(t, 640, 2, "mixed keys")

-- Small arrays (below threshold) - normal power-of-2 sizing
t = make_array(50)
check_sizes(t, 64, 0, "50 elements")

-- Incremental growth
t = make_array(520)
check_sizes(t, 640, 0, "520 elements")

-- Offset array - elements not starting from 1
-- 600 elements at indices 401-1000, meets 50% threshold for 1024
t = {}
for i = 401, 1000 do
    t[i] = true
end
check_sizes(t, 1024, 0, "offset array 401-1000")

-- Sparse with gap - should not cap due to high max_idx
t = make_array(1100)
for i = 1500, 1549 do
    t[i] = true
end
check_sizes(t, 2048, 0, "sparse with gap")

-- Sequential growth across cap threshold
t = make_array(400)
check_sizes(t, 512, 0, "pre-growth 400 elements")
for i = 401, 600 do
    t[i] = true
end
check_sizes(t, 640, 0, "post-growth 600 elements")

-- Index 0 goes to hash, not array
t = make_array(600)
t[0] = true
check_sizes(t, 640, 1, "with index 0")

-- Negative indices go to hash
t = make_array(600)
t[-1] = true
t[-100] = true
check_sizes(t, 640, 2, "with negative indices")

-- Non-integer keys go to hash
t = make_array(600)
t[1.5] = true
t[2.7] = true
check_sizes(t, 640, 2, "with float indices")

-- Exactly at power-of-2 boundaries
t = make_array(1024)
check_sizes(t, 1024, 0, "exactly 1024 elements")

t = make_array(2048)
check_sizes(t, 2048, 0, "exactly 2048 elements")

-- table.create pre-allocates without rehash(), allows creating tables with
--  sizes that rehash() would not normally create.
t = table.create(1535, true)
check_sizes(t, 1535, 0, "exactly 1535 elements")
-- But it'll go to a "normal" size if we overfill it.
t[1536] = 1
check_sizes(t, 1536, 0, "resized to next increment after insert")
t[1537] = 2
check_sizes(t, 1664, 0, "second bigger resize")
-- Double check we didn't muck up the data with either of those resizes
assert(t[1536] == 1)
assert(t[1537] == 2)

-- Exact cap boundary - 1536 elements should get exactly 1536 slots
t = make_array(1536)
check_sizes(t, 1536, 0, "exactly 1536 elements")

-- Just over cap boundary - 1537 elements should grow to 1664
t = make_array(1537)
check_sizes(t, 1664, 0, "1537 elements")

-- Very large array - 10000 elements should get 10112, not 16384
t = make_array(10000)
check_sizes(t, 10112, 0, "10000 elements")

-- Hash spillover triggers rehash and array growth
-- Inserting beyond array size into dummynode triggers rehash
t = make_array(1536)
check_sizes(t, 1536, 0, "before spillover")
-- These will go into hash because they're not sequential with array
t.foo = true
t.bar = true
t.baz = true
-- Should be one space free in `node`
check_sizes(t, 1536, 4, "node insert - node grew")
t[1538] = true
check_sizes(t, 1536, 4, "after num insert - placed in node")

-- This will overflow `t->node`, so it should try to resize array now
t[1537] = true
-- Note that node will NOT shrink, but the values will be moved!
check_sizes(t, 1664, 4, "after spillover - array grew")
assert(t[1538] == true, "1538 still present")

-- System tables (memcat < 10) use power-of-2 sizing, no cap
change_memcat(0)
t = make_array(600)
check_sizes(t, 1024, 0, "system table memcat 0")
change_memcat(10)

-- table.shrink tests

-- Shrink dense array after removing elements - shrinks to exact boundary
t = make_array(1000)
for i = 501, 1000 do t[i] = nil end
table.shrink(t)
check_sizes(t, 500, 0, "shrunk dense array to boundary")

-- Shrink empty table
t = make_array(1000)
for i = 1, 1000 do t[i] = nil end
table.shrink(t)
check_sizes(t, 0, 0, "shrunk to an empty table")

-- Shrink hash-only table (dense path since boundary=0, no array elements beyond)
t = {}
t.a = 1
t.b = 2
t.c = 3
table.shrink(t)
check_sizes(t, 0, 4, "shrunk hash only")  -- 4 is smallest power-of-2 >= 3

-- Shrink is no-op when hash is already at minimal capacity (no useless rebuild)
t = {}
for i = 1, 12 do t[`key {i}`] = true end
check_sizes(t, 0, 16, "12 elements in 16-slot hash")
local order_before = {}
for k in t do table.insert(order_before, k) end
table.shrink(t)
check_sizes(t, 0, 16, "hash capacity unchanged after shrink")
local order_after = {}
for k in t do table.insert(order_after, k) end
for i = 1, #order_before do
    assert(order_before[i] == order_after[i], "iteration order preserved when hash cannot shrink")
end

-- Shrink mixed array/hash - dense path shrinks to exact boundary
t = make_array(100)
t.foo = "bar"
t.baz = "qux"
table.shrink(t)
check_sizes(t, 100, 2, "shrunk mixed array/hash to boundary")

-- Dense array stays at boundary, no wasted space
t = make_array(600)
table.shrink(t)
-- Dense array shrinks to exactly 600 (boundary), not pow2 or 128-aligned
check_sizes(t, 600, 0, "shrink dense to exact boundary")

-- Shrink sparse array moves high indices to hash
t = {}
for i = 901, 1000 do
    t[i] = true
end
table.shrink(t)
-- 100 elements at 901-1000 don't meet 50% threshold for any array size
-- They all go to hash
check_sizes(t, 0, 128, "sparse high indices to hash")

-- Shrink removes unused hash slots
t = {}
t.a = 1
t.b = 2
t.c = 3
t.d = 4
t.e = 5
-- This creates hash with some size
t.a = nil
t.b = nil
t.c = nil
-- Now only 2 elements remain
table.shrink(t)
check_sizes(t, 0, 2, "shrank hash after deletions")

-- Verify data integrity after shrink
t = make_array(100)
t.key = "value"
table.shrink(t)
assert(t[1] == true, "t[1] preserved after shrink")
assert(t[100] == true, "t[100] preserved after shrink")
assert(t.key == "value", "hash data preserved after shrink")

-- Default shrink keeps elements in their current part (no elements move to hash)
t = table.create(2000)
for i = 1, 600 do t[i] = true end
t[1500] = true  -- Sparse element in array at high index
table.shrink(t)
check_sizes(t, 1500, 0, "default shrink - preserves order")

-- With shrink_sparse=true, sparse elements move to hash for better memory usage
t = table.create(2000)
for i = 1, 600 do t[i] = true end
t[1500] = true
table.shrink(t, true)
check_sizes(t, 600, 1, "shrink_sparse - sparse element to hash")
assert(t[1500] == true, "sparse element preserved after move to hash")

-- Reorder with element closer to boundary
t = table.create(1000)
for i = 1, 600 do t[i] = true end
t[700] = true  -- Sparse element
table.shrink(t, true)
-- boundary=600, element at 700 moves to hash, array shrinks to 600
check_sizes(t, 600, 1, "shrink_sparse - element near boundary to hash")

-- Even with shrink_sparse=true, if moving elements to hash would grow the table,
-- we fall back to shrinking to max_used_idx instead.
t = table.create(100)
t[1] = true
for i = 3, 52 do t[i] = true end
check_sizes(t, 100, 0, "starts at 100 array elements")
table.shrink(t, true)
-- Falls back to max_used_idx since boundary shrink would grow table
check_sizes(t, 52, 0, "shrink_sparse fallback - boundary too expensive")

-- shrink_sparse with large hash and few sparse elements doesn't OOM
-- When hash capacity wouldn't change, elements are inserted directly without resize
t = {}
for i = 1, 16 do t[i] = i end
for i = 1, 1025 do t[`k{i}`] = i end
for i = 2, 15 do t[i] = nil end
-- boundary=0, max_used_idx=16, hash=1025 in 2048-capacity node
assert(#t == 16, "len before shrink")
table.shrink(t, true)
-- Sparse element t[16] moves to hash, array shrinks to 1 (preserving first elem)
check_sizes(t, 1, 2048, "shrink_sparse without hash resize")
assert(t[16] == 16, "sparse element preserved")
assert(t[1] == 1, "left element preserved")
assert(t.k1 == 1, "hash element preserved")
assert(#t == 1, "boundary correct after sparse elements moved to hash")

-- Idempotent - second shrink is no-op
t = make_array(1000)
for i = 501, 1000 do t[i] = nil end
table.shrink(t)
check_sizes(t, 500, 0, "first shrink")
table.shrink(t)
check_sizes(t, 500, 0, "idempotent shrink")

-- Empty table with no array and no hash - no-op, doesn't crash
t = {}
check_sizes(t, 0, 0, "empty table before shrink")
table.shrink(t)
check_sizes(t, 0, 0, "empty table no-op")

return "OK"
