-- Test that interrupt handlers are called before metamethods and library callbacks
-- The C side sets up an interrupt handler that clears a flag,
-- and callbacks that throw if the flag wasn't cleared.

-- Create a test object with test_callback as various metamethods
local function create_test_object()
    local obj = {}
    local mt = {
        __tostring = test_callback,
        __tojson = test_callback,
        __len = test_callback,
        __add = test_callback,
        __sub = test_callback,
        __mul = test_callback,
        __div = test_callback,
        __unm = test_callback,
        __index = test_callback,
        __newindex = test_callback,
    }
    return setmetatable(obj, mt)
end

-- Create test object once to avoid intermediate function calls
local obj = create_test_object()
local obj_len = {}
setmetatable(obj_len, {__jsontype = "array", __len = test_callback})

-- Test __tostring (via luaL_callmeta path)
reset_interrupt_test()
local result = tostring(obj)
assert(result == "ok", "__tostring should return 'ok'")
assert(check_callback_ran(), "__tostring metamethod should have run")

-- Test __tojson (via JSON encoder)
reset_interrupt_test()
result = lljson.encode(obj)
assert(result == '"ok"', "__tojson should return '\"ok\"'")
assert(check_callback_ran(), "__tojson metamethod should have run")

-- Test __len (via JSON encoder when no __tojson)
reset_interrupt_test()
result = lljson.encode(obj_len)
assert(check_callback_ran(), "__len metamethod should have run")

-- Test arithmetic metamethods (__add, __sub, __mul, __div, __unm)
-- These go through callTMres or luaV_callTM paths
reset_interrupt_test()
local _ = obj + obj
assert(check_callback_ran(), "__add metamethod should have run")

reset_interrupt_test()
_ = obj - obj
assert(check_callback_ran(), "__sub metamethod should have run")

reset_interrupt_test()
_ = obj * obj
assert(check_callback_ran(), "__mul metamethod should have run")

reset_interrupt_test()
_ = obj / obj
assert(check_callback_ran(), "__div metamethod should have run")

reset_interrupt_test()
_ = -obj
assert(check_callback_ran(), "__unm metamethod should have run")

-- Test __index (via callTMres path)
reset_interrupt_test()
_ = obj.some_field
assert(check_callback_ran(), "__index metamethod should have run")

-- Test __newindex (via callTM path)
reset_interrupt_test()
obj.some_field = 123
assert(check_callback_ran(), "__newindex metamethod should have run")

-- Test library function callbacks

-- Create test table and data
local test_table = {1, 2, 3, 4, 5}
local test_dict = {a = 1, b = 2, c = 3, d = 4, e = 5}
local test_string = "aabbbcccc"

reset_interrupt_test()
table.sort(test_table, test_sort_callback)
assert(check_callback_ran(), "table.sort comparison function should have run")

reset_interrupt_test()
table.foreachi(test_table, test_callback)
assert(check_callback_ran(), "table.foreachi callback should have run")

reset_interrupt_test()
table.foreach(test_dict, test_callback)
assert(check_callback_ran(), "table.foreach callback should have run")

reset_interrupt_test()
local result = string.gsub(test_string, "a", test_callback)
assert(check_callback_ran(), "string.gsub replacement function should have run")

return "OK"
