ares.settings("path", true)
local j = "k"
function setter()
    j = "z"
    return 1
end

function getter()
    return j
end

-- Don't serialize globals
local perms = {}
local uperms = {}

local persisted = ares.persist(perms, {getter, setter})
local new_getter, new_setter = table.unpack(ares.unpersist(uperms, persisted))

j = "x"

-- should still be the same even though we mutated "j"
assert(new_getter() == "k")
new_setter()
assert(new_getter() == "z")
-- should not have changed, this is the original!
assert(j == "x")

-- pristine state again, from before `j` was ever modified
local new_getter2, new_setter2 = table.unpack(ares.unpersist(uperms, persisted))
assert(new_getter2() == "k")

function func_maker(i)
    local func = function()
        i += 1
        return i
    end
    i += 1
    return func
end

-- closed-over upvals should behave the same as before
local made_func = func_maker(2)
local persisted_made_func = ares.unpersist(uperms, ares.persist(perms, made_func))

assert(made_func() == 4)
assert(made_func() == 5)

assert(persisted_made_func() == 4)
assert(persisted_made_func() == 5)

-- Reference to self shouldn't be an issue
local fib
fib = function(n)
  if n == 0 then
		return 0
	elseif n == 1 then
		return 1
	else
		return fib(n-1) + fib(n-2)
	end
end

local fibDec = ares.unpersist(uperms, ares.persist(perms, fib))
assert(fibDec(15) == 610)

local retSelf
retSelf = function ()
    return retSelf
end
assert(retSelf == retSelf())

local decRetSelf = ares.unpersist(uperms, ares.persist(perms, retSelf))
assert(decRetSelf == decRetSelf())

assert(aGlobal == nil)
-- Globals aren't upvalues, so they don't get serialized along with closures
aGlobal = {1, 2, 3}
function retGlobal()
    return aGlobal
end

local decRetGlobal = ares.unpersist(uperms, ares.persist(perms, retGlobal))
-- referential inequality because they will be using different wrappers for the
-- globals object!
assert(decRetGlobal() ~= aGlobal)

-- This time make sure we persist with our global wrapper as a permanent
decRetGlobal = ares.unpersist({glob=getfenv()}, ares.persist({[getfenv()]="glob"}, retGlobal))
assert(decRetGlobal() == aGlobal)

local function GenerateObjects()
  local Table = {}

  function Table:Func()
    return { Table, self }
  end

  function uvcycle()
    return Table:Func()
  end
end

GenerateObjects()

ares.unpersist({glob=getfenv()}, ares.persist({[getfenv()]="glob"}, uvcycle))

print('OK')
return 'OK'
