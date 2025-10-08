local z = "foo"
local tab_key_tab = {[{1,2,z}]=z .. "1", [{4, 5, 6}]=z .. "2", [z]=z .. "3", foz=z .. "4", qox=z .. "5"}
local expected_order = {}
local perms = {[getfenv()]="globs", [ares.persist]="persist"}
local uperms = {globs=getfenv(), persist=ares.persist}

local i = 1
for k, v in tab_key_tab do
    expected_order[i] = v
    i += 1
end

function next_yielder()
    local key = next(tab_key_tab, nil)
    while key ~= nil do
        coroutine.yield(tab_key_tab[key])
        key = next(tab_key_tab, key)
    end
end

function for_yielder()
    for j, v in expected_order do
        coroutine.yield(v)
    end
end

local wrapped_next_yielder = coroutine.wrap(next_yielder)

local persisted = ares.persist(perms, {wrapped_next_yielder, coroutine.wrap(for_yielder)})

-- Try the generator without serialization first
for j, val in expected_order do
    local actual = wrapped_next_yielder()
    print(val, actual)
    assert(val == actual)
end

local new_next_yielder, for_yielder = table.unpack(ares.unpersist(uperms, persisted))

for j, val in expected_order do
    local actual = for_yielder()
    print(val, actual)
    assert(val == actual)
end

for j, val in expected_order do
    local actual = new_next_yielder()
    print(val, actual)
    assert(val == actual)
end

-- make sure `nil`ing a key doesn't break iteration order, this is required
-- to satisfy the guarantees Luau makes for its generalized iteration!
local nillable_tab = {foo=1, bar=2, baz=3, quux=4, foozy=5, doz=6, f=7}
local nillable_order = {}
for k, v in nillable_tab do
    table.insert(nillable_order, k)
end

function nil_iter_yield(input_tab, order_tab)
    local our_tab = table.clone(input_tab)
    local cur_order = next(order_tab, nil)
    for k, v in our_tab do
        assert(k == order_tab[cur_order])
        our_tab[k] = nil
        cur_order = next(order_tab, cur_order)
        coroutine.yield(1)
    end
end

-- same as above but uses next() instead
function nil_next_iter_yield(input_tab, order_tab)
    local our_tab = table.clone(input_tab)
    local cur_order = next(order_tab, nil)
    local k = next(our_tab, nil)
    while k ~= nil do
        assert(k == order_tab[cur_order])
        our_tab[k] = nil
        cur_order = next(order_tab, cur_order)
        coroutine.yield(1)
        k = next(our_tab, k)
    end
end

-- same as above but `nil`s _after_ calling `next()`
function nil_after_next_iter_yield(input_tab, order_tab)
    local our_tab = table.clone(input_tab)
    local cur_order = next(order_tab, nil)
    local k = next(our_tab, nil)
    while k ~= nil do
        assert(k == order_tab[cur_order])
        local next_k = next(our_tab, k)
        our_tab[k] = nil
        cur_order = next(order_tab, cur_order)
        coroutine.yield(1)
        k = next_k
    end
end

local nilling_wrapper = coroutine.wrap(function() nil_iter_yield(nillable_tab, nillable_order) end)
local pers_nilling_wrapper = ares.persist(perms, nilling_wrapper)

local nilling_next_wrapper = coroutine.wrap(function() nil_next_iter_yield(nillable_tab, nillable_order) end)
local pers_nilling_next_wrapper = ares.persist(perms, nilling_next_wrapper)

local nilling_after_next_wrapper = coroutine.wrap(function() nil_after_next_iter_yield(nillable_tab, nillable_order) end)
local pers_nilling_after_next_wrapper = ares.persist(perms, nilling_after_next_wrapper)

-- test the base cases first, no persistence
while nilling_wrapper() ~= nil do
end
-- now test a version where we deserialize(serialize()) every iter
-- TODO: This is annoying to write tests for because the hash codes won't change...
nilling_wrapper = ares.unpersist(uperms, pers_nilling_wrapper)
while nilling_wrapper() ~= nil do
    nilling_wrapper = ares.unpersist(uperms, ares.persist(perms, nilling_wrapper))
end

-- same as above but for the wrapper that uses next(), should fail because we've
-- changed behavior to make table entries with `nil`ed values invalid keys for `next()`.
-- note that this _isn't_ consistent with lua proper!
local success, ret = pcall(function() while nilling_next_wrapper() ~= nil do end end)
assert(not success)

nilling_wrapper = ares.unpersist(uperms, pers_nilling_next_wrapper)
success, ret = pcall(function()
    while nilling_wrapper() ~= nil do
        nilling_wrapper = ares.unpersist(uperms, ares.persist(perms, nilling_wrapper))
    end
end)
assert(not success)

-- but obviously it's still fine if you call `next()` before you `nil`
while nilling_after_next_wrapper() ~= nil do end

nilling_wrapper = ares.unpersist(uperms, pers_nilling_after_next_wrapper)
while nilling_wrapper() ~= nil do
    nilling_wrapper = ares.unpersist(uperms, ares.persist(perms, nilling_wrapper))
end

print('OK')
return 'OK'
