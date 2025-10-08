local perms = {[coroutine.yield]="yield", [coroutine.wrap]="wrap", [coroutine.resume]="resume", [assert]="assert"}
local uperms = {yield=coroutine.yield, wrap=coroutine.wrap, resume=coroutine.resume, assert=assert}


-- so we can run these tests with eris too
if eris ~= nil then
    ares = eris
    -- don't serialize globals
    perms[_ENV] = "_ENV"
    uperms["_ENV"] = _ENV
end

ares.settings("path", true)

local j = "k"
function setter()
    j = "z"
    return 1
end

function yielder()
    coroutine.yield(j .. "1")
    coroutine.yield(j .. "2")
    coroutine.yield(j .. "3")
end

function assert_yields(co, val)
    local valid, ret = coroutine.resume(co)
    assert(valid)
    assert(ret == val)
end

local persisted = ares.persist(perms, {coroutine.create(yielder), coroutine.create(setter)})
local new_yielder, new_setter = table.unpack(ares.unpersist(uperms, persisted))

assert(coroutine.status(new_yielder) == 'suspended')

j = "x"

-- should still be the same even though we mutated "j"
assert_yields(new_yielder, "k1")
assert(coroutine.status(new_yielder) == 'suspended')

assert_yields(new_setter, 1)

local persisted_mid_execution = ares.persist(perms, new_yielder)
local unpersisted_mid_execution = ares.unpersist(uperms, persisted_mid_execution)

assert_yields(new_yielder, "z2")
-- should not have changed, this is the original!
assert(j == "x")
assert_yields(new_yielder, "z3")
-- coroutine exhausted
assert_yields(new_yielder, nil)

assert(coroutine.status(new_yielder) == 'dead')
assert(coroutine.status(new_setter) == 'dead')

-- dead coroutines should stay dead
local unpersisted_dead = ares.unpersist(uperms, ares.persist(perms, new_yielder))
assert(coroutine.status(unpersisted_dead) == 'dead')
assert(coroutine.resume(unpersisted_dead) == false)

-- this coroutine should be able to pick up where it left off
assert(coroutine.status(unpersisted_mid_execution) == 'suspended')
assert_yields(unpersisted_mid_execution, "z2")

-- Currently problematic because of the fact that these closures are ONLY dynamically created.
-- It's difficult to get a reference to an existing one without wrapping a function!
local persisted_wrapped = ares.persist(perms, coroutine.wrap(yielder))
local unpersisted_wrapped = ares.unpersist(uperms, persisted_wrapped)
assert(unpersisted_wrapped() == "x1")
assert(unpersisted_wrapped() == "x2")


local function openupval_test()
    -- two closures that close over a mutable local to force
    -- use of an open upvalue
    local baz = {bar="quux"}
    local function foo()
        coroutine.yield(baz.bar)
        baz = {bar="foo"}
        coroutine.yield(baz.bar)
    end

    local function quux()
        coroutine.yield(baz.bar)
        baz = {bar="bazzer"}
        coroutine.yield(baz.bar)
    end
    foo()
    quux()
end

local co_open = coroutine.create(openupval_test)
local i
local expected = {"quux", "foo", "foo", "bazzer"}
for i=1,4 do
    -- round-trip the coroutine
    co_open = ares.unpersist(uperms, ares.persist(perms, co_open))
    -- make sure we yielded what we expected
    assert(select(2, coroutine.resume(co_open)) == expected[i])
end

-- just exhausted the coroutine
assert(select(2, coroutine.resume(co_open)) == nil)
-- nothing left to yield, the coroutine is exhausted
assert(coroutine.resume(co_open) == false)

function coro_with_upval_captures()
    local one = {}
    local two = {}
    local function foz()
        local function bar()
            local three = {}
            one.two = two
            two.three = three
            three.one = one
            three.two = two
            local function foo()
                coroutine.yield()
                return {one, two, three}
            end
            local function quux()
                return {one, two, three}
            end

            return foo(), quux()
        end
        return bar()
    end
    return foz
end

local success, tabs, rtabs

co_open = coroutine.create(coro_with_upval_captures())
coroutine.resume(co_open)

success, tabs, rtabs = coroutine.resume(ares.unpersist(uperms, ares.persist(perms, co_open)))
assert(tabs[1] == rtabs[1])
assert(tabs[2] == rtabs[2])
assert(tabs[3] == rtabs[3])
assert(tabs[3].one == rtabs[3].one)
assert(tabs[2].three == rtabs[2].three)

print('OK')
return 'OK'
