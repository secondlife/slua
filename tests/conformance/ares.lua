--
-- json.lua
--
-- Copyright (c) 2020 rxi
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy of
-- this software and associated documentation files (the "Software"), to deal in
-- the Software without restriction, including without limitation the rights to
-- use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.
--

-------------------------------------------------------------------------------
-- Encode
-------------------------------------------------------------------------------

local encode

local escape_char_map = {
  [ "\\" ] = "\\",
  [ "\"" ] = "\"",
  [ "\b" ] = "b",
  [ "\f" ] = "f",
  [ "\n" ] = "n",
  [ "\r" ] = "r",
  [ "\t" ] = "t",
}

local function escape_char(c)
  return "\\" .. (escape_char_map[c] or string.format("u%04x", c:byte()))
end


local function encode_nil(val)
  return "null"
end


local function encode_table(val)
  local res = {}
  for k, v in pairs(val) do
    table.insert(res, encode(k) .. ":" .. encode(v))
  end
  return "{" .. table.concat(res, ",") .. "}"
end


local function encode_string(val)
  return '"' .. val:gsub('[%z\1-\31\\"]', escape_char) .. '"'
end


local function encode_buffer(val)
  return encode(buffer.tostring(val))
end


local function encode_number(val)
  -- Check for NaN, -inf and inf
  if val ~= val then
    return "nan"
  elseif val <= -math.huge then
    return "-inf"
  elseif val >= math.huge then
    return "inf"
  end
  return string.format("%.14g", val)
end

local type_func_map = {
  [ "nil"     ] = encode_nil,
  [ "table"   ] = encode_table,
  [ "string"  ] = encode_string,
  [ "buffer"  ] = encode_buffer,
  [ "number"  ] = encode_number,
  [ "boolean" ] = tostring,
  [ "vector"  ] = tostring,
}


encode = function(val, stack)
  local t = type(val)
  local f = type_func_map[t]
  if f then
    return f(val, stack)
  end
  error("unexpected type '" .. t .. "'")
end

function round_trip(val)
    return ares.unpersist(ares.persist(val))
end

function assert_encoded_equals(expected, val)
    expected = encode(expected)
    val = encode(val)
    print(expected, val)
    assert(expected == val)
end

function assert_round_trips(expected)
    local actual = round_trip(expected)
    assert_encoded_equals(expected, actual)
end

function assert_mutation_keeps_order(tab, func)
    local mut = ares.unpersist(ares.persist(tab))
    -- mutate the unpersisted table somehow
    func(mut)

    -- make sure the keys are still in order
    local tab_val, tab_key, mut_key
    while true do
        tab_key, tab_val = next(tab, tab_key)
        mut_key, tab_val = next(mut, mut_key)
        -- they should still iterate in the same order
        assert(mut_key == tab_key)
        if tab_key == nil then
            break
        end
    end
    func(mut)
end

function assert_mutation_changes_order(tab, func)
    local mut = ares.unpersist(ares.persist(tab))
    -- mutate the unpersisted table somehow
    func(mut)

    -- make sure the keys aren't in the same order
    local tab_val, tab_key, mut_key
    repeat
        tab_key, tab_val = next(tab, tab_key)
        mut_key, tab_val = next(mut, mut_key)
        print(tab_key, mut_key)
        -- found a mismatch, this is good
        if mut_key ~= tab_key then
            return
        end
    until (tab_key == nil or mut_key == nil)
    -- if we fell off the bottom of the loop the current keys had
    -- better be different
    assert (tab_key ~= mut_key)
end

-- TODO: This is failing right now because we reuse global envs :(
-- assert(foobaz == nil)
-- foobaz = "bar"

assert_round_trips({1, 2, 3})
assert_round_trips({1, 2, 'foo'})
assert_round_trips({{1, 2, 3}, {4, 5, 6}})
assert_round_trips({foo="bar"})
assert_round_trips("foo")
assert_round_trips(true)
assert_round_trips(nil)
assert_round_trips(0/0)
assert_round_trips({1, 2, nil, 4})

-- test buffers
local b = buffer.create(10)
buffer.fill(b, 0, 0x61)
assert_round_trips(b)

-- This is dependent on iteration order not being invalidated!
assert_round_trips({foo=1, bar=2, quux=3})
assert_round_trips({foo=1, bar=2, quux=3, 1, nil, "2"})
-- Bucketing for keys of type Table will be based on pointer value,
-- iteration order should be the same even though pointer values change
-- when the values are unpersisted
local z = {1, 2, 3}
local tab_key_tab = {[{1,2,3}]=z, [{4, 5, 6}]=z, [z]=z}
assert_round_trips(tab_key_tab)

local tab = {1, 2, 3, foo=1, bar=2, quux=3}
-- changing an existing value shouldn't change iteration order
assert_mutation_keeps_order(tab, function (mut) mut['foo'] = 2 end)
assert_mutation_keeps_order(tab, function (mut) mut['bar'] = 2 end)
-- changing the array part shouldn't matter either
assert_mutation_keeps_order(tab, function (mut) mut[1] = 1 end)
-- Only assignments that flip `nil`ity or alloc invalidate the existing
-- iteration order. Assigning `nil` to a non-existing key doesn't
-- affect it.
-- This might actually matter for explicit `next()` calls in the
-- case of `next(mut, 'foz')`, but we're not required to return a
-- sensible result from `next()` in that case. `foz` would never
-- naturally show up in a `next()` call chain for the table!
assert_mutation_keeps_order(tab, function (mut) mut['foz'] = nil end)

-- these should all change the enforced iteration order
assert_mutation_changes_order(tab, function (mut) mut['foz'] = 'foo' end)
assert_mutation_changes_order(tab, function (mut) mut[4] = 'foo' end)
assert_mutation_changes_order(tab, function (mut) mut[3] = nil end)
assert_mutation_changes_order(tab, function (mut) mut['foo'] = nil end)

-- custom iteration order shouldn't be lost by double deserialize(serialize(val))
assert_encoded_equals(tab_key_tab, round_trip(round_trip(tab_key_tab)))

local ref = "foo"
local other_tab = {[{1,2,ref}]=ref .. "1", [{4, 5, 6}]=ref .. "2", [ref]=ref .. "3", foz=ref .. "4"}
assert_encoded_equals(other_tab, round_trip(round_trip(other_tab)))

-- reference cycles shouldn't be a problem to serialize
local self_referential = {1, 2, 3}
self_referential[4] = self_referential
local dec_self_referential = ares.unpersist(ares.persist(self_referential))
assert(dec_self_referential[4] == dec_self_referential)

-- Tables should remember they're read-only when deserialized
local ro_table = ares.unpersist(ares.persist(table.freeze({1, 2, 3})))
assert(not table.pack(pcall(function() ro_table['foo'] = 1 end))[1])

local userdata = newproxy(true)
assert(ares.unpersist(ares.persist(userdata)) ~= nil)

assert_round_trips(vector(1, 2, 3))

print('OK')
return 'OK'
