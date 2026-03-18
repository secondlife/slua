-- ============================================
-- Reviver for decode()
-- ============================================

-- Basic: transform all strings to uppercase
do
    local t = lljson.decode('{"a":"hello","b":"world"}', function(key, value)
        if type(value) == "string" then return string.upper(value) end
        return value
    end)
    assert(t.a == "HELLO")
    assert(t.b == "WORLD")
end

-- lljson.remove from reviver: omit specific keys from objects
do
    local t = lljson.decode('{"keep":"yes","drop":"no","also":"yes"}', function(key, value)
        if key == "drop" then return lljson.remove end
        return value
    end)
    assert(t.keep == "yes")
    assert(t.also == "yes")
    assert(t.drop == nil)
end

-- lljson.remove from reviver: omit elements from arrays (result is compacted)
do
    local t = lljson.decode('[1,2,3,4,5]', function(key, value)
        if value == 2 or value == 4 then return lljson.remove end
        return value
    end)
    assert(#t == 3)
    assert(t[1] == 1)
    assert(t[2] == 3)
    assert(t[3] == 5)
end

-- nil return from reviver encodes as null (not an error)
do
    local t = lljson.decode('{"a":1}', function(key, value)
        if type(value) == "number" then return nil end
        return value
    end)
    assert(t.a == nil)
end

-- lljson.null return: stored as JSON null
do
    local t = lljson.decode('{"a":"hello"}', function(key, value)
        if type(value) == "string" then return lljson.null end
        return value
    end)
    assert(t.a == lljson.null)
end

-- Root reviver: wrap the root value
do
    local result = lljson.decode('42', function(key, value)
        if key == nil then return {wrapped = value} end
        return value
    end)
    assert(result.wrapped == 42)
end

-- Root lljson.remove: returns lljson.null
do
    local result = lljson.decode('"hello"', function(key, value)
        return lljson.remove
    end)
    assert(result == lljson.null)
end

-- Nested objects: verify bottom-up call order
do
    local order = {}
    lljson.decode('{"a":{"b":1}}', function(key, value)
        table.insert(order, key)
        return value
    end)
    -- bottom-up: "b" first (inner), then "a" (outer), then nil (root)
    assert(order[1] == "b")
    assert(order[2] == "a")
    assert(order[3] == nil)
end

-- Type reconstruction: setmetatable in reviver
do
    local Vec2 = {}
    Vec2.__index = Vec2
    function Vec2:magnitude() return math.sqrt(self.x * self.x + self.y * self.y) end

    local t = lljson.decode('{"x":3,"y":4}', function(key, value)
        if key == nil and type(value) == "table" and value.x and value.y then
            return setmetatable(value, Vec2)
        end
        return value
    end)
    assert(t:magnitude() == 5)
end

-- Too many args should error
assert(not pcall(lljson.decode, '"hello"', function() end, "extra"))

-- Non-function second arg should error if not a table
assert(not pcall(lljson.decode, '"hello"', "not a function"))

-- ============================================
-- Reviver for sldecode()
-- ============================================

-- Reviver sees vectors/UUIDs (post-tag-parsing), not raw tagged strings
do
    local saw_vector = false
    local saw_uuid = false
    local t = lljson.sldecode('{"v":"!v<1,2,3>","id":"!u12345678-1234-1234-1234-123456789abc"}', function(key, value)
        if type(value) == "vector" then saw_vector = true end
        if typeof(value) == "uuid" then saw_uuid = true end
        return value
    end)
    assert(saw_vector, "reviver should see parsed vector, not tagged string")
    assert(saw_uuid, "reviver should see parsed uuid, not tagged string")
    assert(t.v == vector(1, 2, 3))
    assert(t.id == uuid("12345678-1234-1234-1234-123456789abc"))
end

-- sldecode reviver can transform SL types
do
    local t = lljson.sldecode('{"pos":"!v<1,2,3>"}', function(key, value)
        if type(value) == "vector" then
            return value * 2
        end
        return value
    end)
    assert(t.pos == vector(2, 4, 6))
end

-- sldecode: backwards compat without reviver
assert(lljson.sldecode('"!v<1,2,3>"') == vector(1, 2, 3))

-- ============================================
-- Encode replacer
-- ============================================

-- Basic object replacer: transform values
do
    local r = lljson.encode({a = 1, b = 2}, {replacer = function(key, value)
        if type(value) == "number" then return value * 10 end
        return value
    end})
    local t = lljson.decode(r)
    assert(t.a == 10 and t.b == 20)
end

-- Object replacer with lljson.remove: omit keys
do
    local r = lljson.encode({keep = 1, drop = 2, also = 3}, {replacer = function(key, value)
        if key == "drop" then return lljson.remove end
        return value
    end})
    local t = lljson.decode(r)
    assert(t.keep == 1 and t.also == 3 and t.drop == nil)
end

-- Array replacer with lljson.remove: skip elements
do
    local r = lljson.encode({1, 2, 3, 4, 5}, {replacer = function(key, value, parent)
        if parent == nil then return value end
        if value % 2 == 0 then return lljson.remove end
        return value
    end})
    assert(r == "[1,3,5]")
end

-- nil return from replacer encodes as null (not an error)
do
    local r = lljson.encode({a = 1}, {replacer = function(key, value)
        if type(value) == "number" then return nil end
        return value
    end})
    assert(r == '{"a":null}')
end

-- Passthrough replacer preserves nil-as-null (no semantic change from adding replacer)
do
    local r = lljson.encode({1, nil, 3}, {replacer = function(key, value)
        return value
    end})
    assert(r == "[1,null,3]", "passthrough replacer should preserve nils as null, got: " .. r)
end

-- Replacer sees nil array elements and can transform them
do
    local r = lljson.encode({1, nil, 3}, {replacer = function(key, value, parent)
        if parent ~= nil and value == nil then return 0 end
        return value
    end})
    assert(r == "[1,0,3]", "replacer should be able to transform nil elements, got: " .. r)
end

-- slencode: nil return from replacer produces "!n" (preserves nil/null distinction)
do
    local r = lljson.slencode({1, nil, 3}, {replacer = function(key, value)
        return value
    end})
    assert(r == '[1,"!n",3]', "slencode passthrough replacer should preserve !n, got: " .. r)
end

-- Nested structures: replacer sees leaf values
do
    local r = lljson.encode({a = {1, 2}}, {replacer = function(key, value)
        if type(value) == "number" then return value + 100 end
        return value
    end})
    local t = lljson.decode(r)
    assert(t.a[1] == 101 and t.a[2] == 102)
end

-- Replacer + __tojson: __tojson resolves first, replacer sees result (JS compat)
do
    local mt = { __tojson = function(self)
        return self.v * 10
    end }
    local r = lljson.encode({a = setmetatable({v = 3}, mt)}, {replacer = function(key, value)
        -- replacer sees 30 (the __tojson-resolved value), not the table
        if type(value) == "number" then
            return value + 1
        end
        return value
    end})
    assert(r == '{"a":31}', "expected 31, got " .. r)
end

-- Replacer + __tojson in arrays: __tojson resolves first (JS compat)
do
    local mt = { __tojson = function(self)
        return self.v * 10
    end }
    local r = lljson.encode({setmetatable({v = 3}, mt)}, {replacer = function(key, value)
        if type(value) == "number" then
            return value + 1
        end
        return value
    end})
    assert(r == '[31]', "expected [31], got " .. r)
end

-- Root __tojson + replacer: __tojson resolves first, replacer sees result (consistent with non-root)
do
    local mt = { __tojson = function(self)
        return self.v * 10
    end }
    local r = lljson.encode(setmetatable({v = 5}, mt), {replacer = function(key, value)
        -- replacer should see 50 (the __tojson-resolved value), not the table
        if type(value) == "number" then
            return value + 1
        end
        return value
    end})
    assert(r == '51', "root __tojson + replacer: expected 51, got " .. r)
end

-- Root replacer: replacer receives (nil, value, nil) for root
do
    local r = lljson.encode({1, 2, 3}, {replacer = function(key, value, parent)
        if key == nil and parent == nil then
            -- root call: transform the root value
            return {10, 20, 30}
        end
        return value
    end})
    assert(r == '[10,20,30]', "root replacer transform failed: " .. r)
end

-- Root replacer remove: returning lljson.remove from root -> encodes as null
do
    local r = lljson.encode({1, 2}, {replacer = function(key, value, parent)
        if key == nil then
            return lljson.remove
        end
        return value
    end})
    assert(r == 'null', "root replacer remove failed: " .. r)
end

-- nil return from root replacer encodes as null
do
    local r = lljson.encode({1, 2}, {replacer = function(key, value, parent)
        if key == nil then
            return nil
        end
        return value
    end})
    assert(r == "null")
end

-- Replacer parent arg (object): third arg is the containing table
do
    local inner = {b = 1}
    local seen_parent
    local r = lljson.encode({a = inner}, {replacer = function(key, value, parent)
        if key == "b" then
            seen_parent = parent
        end
        return value
    end})
    assert(seen_parent == inner, "replacer parent should be the inner table")
end

-- Replacer parent arg (array): third arg is the containing array
do
    local inner = {10}
    local seen_parent
    local r = lljson.encode({inner}, {replacer = function(key, value, parent)
        if value == 10 then
            seen_parent = parent
        end
        return value
    end})
    assert(seen_parent == inner, "replacer array parent should be the inner array")
end

-- Reviver parent arg (object): third arg is the containing table
do
    local seen_parent
    local result = lljson.decode('{"a":{"b":1}}', function(key, value, parent)
        if key == "b" then
            seen_parent = parent
        end
        return value
    end)
    -- parent should be the inner table that contains key "b"
    assert(type(seen_parent) == "table", "reviver parent should be a table")
    assert(seen_parent.b == 1, "reviver parent should be the inner object")
end

-- Reviver parent arg (array): third arg is the containing array
do
    local seen_parent
    local result = lljson.decode('[["hello"]]', function(key, value, parent)
        if value == "hello" then
            seen_parent = parent
        end
        return value
    end)
    assert(type(seen_parent) == "table", "reviver array parent should be a table")
    assert(seen_parent[1] == "hello", "reviver array parent should be the inner array")
end

-- Root reviver: key and parent are both nil
do
    local root_key, root_parent, root_called
    local result = lljson.decode('42', function(key, value, parent)
        root_key = key
        root_parent = parent
        root_called = true
        return value
    end)
    assert(root_called, "root reviver should be called")
    assert(root_key == nil, "root reviver key should be nil")
    assert(root_parent == nil, "root reviver parent should be nil")
    assert(result == 42, "root reviver should pass through value")
end

-- slencode with replacer
do
    local r = lljson.slencode({a = 1, b = 2}, {replacer = function(key, value)
        if key == "b" then return lljson.remove end
        return value
    end})
    local t = lljson.sldecode(r)
    assert(t.a == 1 and t.b == nil)
end

-- encode with replacer
do
    local r = lljson.encode({x = 10}, {replacer = function(key, value)
        if type(value) == "number" then return value * 2 end
        return value
    end})
    assert(lljson.decode(r).x == 20)
end

-- ============================================
-- Reviver visitation order
-- ============================================

-- Helpers: collect visited keys, encode to JSON for easy comparison.
-- nil (root) is recorded as lljson.null so it survives in the array.
local function check_reviver_order(json_str, expected)
    local order = {}
    lljson.decode(json_str, function(key, value)
        table.insert(order, if key == nil then lljson.null else key)
        return value
    end)
    local got = lljson.encode(order)
    assert(got == expected, `expected {expected}, got {got}`)
end

local function check_replacer_order(value, expected)
    local order = {}
    lljson.encode(value, {replacer = function(key, value)
        table.insert(order, if key == nil then lljson.null else key)
        return value
    end})
    local got = lljson.encode(order)
    assert(got == expected, `expected {expected}, got {got}`)
end

-- Revivers do depth-first, leaf-first visitation
check_reviver_order('{"a":{"b":{"c":1}}}', '["c","b","a",null]')
check_reviver_order('[{"a":1},{"b":2}]', '["a",1,"b",2,null]')
check_reviver_order('{"x":[1,2],"y":[3,4]}', '[1,2,"x",1,2,"y",null]')
check_reviver_order('{"a":1,"b":2,"c":3}', '["a","b","c",null]')

-- Replacers do depth-first, container-first visitation
check_replacer_order({a = {b = 1, c = 2}}, '[null,"a","c","b"]')
check_replacer_order({{a = 1}, {b = 2}}, '[null,1,"a",2,"b"]')
check_replacer_order({a = 1, b = 2, c = 3}, '[null,"a","c","b"]')

-- ============================================
-- skip_tojson option
-- ============================================

-- skip_tojson suppresses __tojson: table encoded as plain object, not __tojson result
do
    local mt = { __tojson = function(self)
        return self.v * 10
    end }
    local obj = setmetatable({v = 3}, mt)
    local r = lljson.encode(obj, {
        skip_tojson = true,
        replacer = function(key, value) return value end,
    })
    local t = lljson.decode(r)
    assert(t.v == 3, "skip_tojson should encode raw table, got: " .. r)
end

-- skip_tojson: replacer sees original metatabled table, not __tojson result
do
    local mt = { __tojson = function(self)
        return self.v * 10
    end }
    local obj = setmetatable({v = 7}, mt)
    local seen_mt
    local r = lljson.encode({a = obj}, {
        skip_tojson = true,
        replacer = function(key, value)
            if key == "a" then
                seen_mt = getmetatable(value)
            end
            return value
        end,
    })
    assert(seen_mt == mt, "replacer should see original metatable with skip_tojson")
end

-- skip_tojson: replacer can invoke __tojson manually
do
    local mt = { __tojson = function(self)
        return self.v * 10
    end }
    local obj = setmetatable({v = 5}, mt)
    local r = lljson.encode(obj, {
        skip_tojson = true,
        replacer = function(key, value)
            if type(value) == "table" then
                local m = getmetatable(value)
                if m and m.__tojson then
                    return m.__tojson(value)
                end
            end
            return value
        end,
    })
    assert(r == "50", "manual __tojson invocation should work, got: " .. r)
end

-- skip_tojson = false: __tojson still resolves normally (explicit false)
do
    local mt = { __tojson = function(self)
        return self.v * 10
    end }
    local r = lljson.encode(setmetatable({v = 4}, mt), {
        skip_tojson = false,
        replacer = function(key, value)
            if type(value) == "number" then return value + 1 end
            return value
        end,
    })
    assert(r == "41", "skip_tojson=false should resolve __tojson normally, got: " .. r)
end

-- skip_tojson in arrays: replacer sees raw table, not __tojson result
do
    local mt = { __tojson = function(self)
        return self.v * 10
    end }
    local r = lljson.encode({setmetatable({v = 2}, mt)}, {
        skip_tojson = true,
        replacer = function(key, value) return value end,
    })
    local t = lljson.decode(r)
    assert(t[1].v == 2, "skip_tojson in array should encode raw table, got: " .. r)
end

-- skip_tojson without replacer: __tojson is still suppressed
do
    local mt = { __tojson = function(self)
        return self.v * 10
    end }
    local r = lljson.encode(setmetatable({v = 6}, mt), {
        skip_tojson = true,
    })
    local t = lljson.decode(r)
    assert(t.v == 6, "skip_tojson without replacer should suppress __tojson, got: " .. r)
end

-- ============================================
-- Interrupt tests for replacer/reviver
-- ============================================

enable_check_interrupt()

local function consume_impl(check, expect_yields, f, ...)
    clear_check_count()
    local co = coroutine.create(f)
    local yields = 0
    local ok, a, b, c = coroutine.resume(co, ...)
    assert(ok, a)
    while coroutine.status(co) ~= "dead" do
        yields += 1
        co = ares.unpersist(ares.persist(co))
        collectgarbage()
        ok, a, b, c = coroutine.resume(co)
        assert(ok, a)
    end
    if expect_yields then
        assert(yields > 0, "no yields occurred")
    end
    if check then
        assert(yields == get_check_count(),
            "yield count mismatch: " .. yields .. " actual vs " .. get_check_count() .. " interrupts")
    end
    return a, b, c, yields
end

local function consume(f, ...)
    return consume_impl(true, true, f, ...)
end

local function consume_nocheck(f, ...)
    return consume_impl(false, false, f, ...)
end

-- decode with reviver: exercises REVIVER_CHECK/REVIVER_CALL yield paths with Ares round-trip
consume(function()
    local src = '{"a":1,"b":2,"c":3,"d":4,"e":5,"f":6,"g":7,"h":8}'
    local t = lljson.decode(src, function(key, value)
        if type(value) == "number" then return value * 10 end
        return value
    end)
    assert(t.a == 10 and t.b == 20 and t.h == 80)
end)

-- decode array with reviver + lljson.remove: exercises compaction across yields
consume(function()
    local src = '[1,2,3,4,5,6,7,8,9,10]'
    local t = lljson.decode(src, function(key, value)
        -- remove even numbers
        if type(value) == "number" and value % 2 == 0 then return lljson.remove end
        return value
    end)
    assert(#t == 5)
    assert(t[1] == 1 and t[2] == 3 and t[3] == 5 and t[4] == 7 and t[5] == 9)
end)

-- encode with replacer: exercises REPLACER_CHECK/REPLACER_CALL yield paths
consume_nocheck(function()
    local r = lljson.encode({a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8}, {replacer = function(key, value)
        if type(value) == "number" then return value * 10 end
        return value
    end})
    local t = lljson.decode(r)
    assert(t.a == 10 and t.b == 20 and t.h == 80)
end)

-- encode array with replacer + lljson.remove across yields
consume_nocheck(function()
    local r = lljson.encode({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, {replacer = function(key, value)
        if type(value) == "number" and value % 2 == 0 then return lljson.remove end
        return value
    end})
    assert(r == "[1,3,5,7,9]")
end)

return 'OK'
