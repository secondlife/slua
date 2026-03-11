-- TypedJSON: branded serialization via lljson replacer/reviver
--
-- Wraps lljson to tag marked objects with a type name during encoding
-- and revive them during decoding. The caller provides an explicit
-- name <-> metatable registry. Sentinel tables ensure that incidental
-- "__type" fields on user data are caught as errors.

local TypedJSON = {}
TypedJSON.__index = TypedJSON

function TypedJSON.new(type_map: {[string]: table}, options: {[string]: any}?)
    options = options or {}
    if options.replacer then
        error("Replacer may not be specified on options")
    end

    local self = setmetatable({}, TypedJSON)
    self.name_to_mt = {}
    self.mt_to_name = {}
    self.name_to_sentinel = {}
    self.sentinel_to_name = {}

    -- Set up the sentinel tables so we can use unique table identities
    -- to "brand" serialized data through "__type"
    -- "name" is a bit of a misnomer since it needn't be a string, but
    -- it usually is.
    for name, mt in type_map do
        local sentinel = table.freeze({name})
        self.name_to_mt[name] = mt
        self.mt_to_name[mt] = name
        self.name_to_sentinel[name] = sentinel
        self.sentinel_to_name[sentinel] = name
    end

    self._replacer = self:_make_replacer()
    self._reviver = self:_make_reviver()

    local encode_opts = table.clone(options)
    encode_opts.replacer = self._replacer
    -- We'll manually call `__tojson()` ourselves where necessary, but we
    -- want to see the values before they're unwrapped.
    encode_opts.skip_tojson = true
    self._encode_opts = encode_opts

    return self
end

function TypedJSON:_make_replacer()
    local sentinel_to_name = self.sentinel_to_name
    local mt_to_name = self.mt_to_name
    local name_to_sentinel = self.name_to_sentinel
    -- Per-type reusable wrappers. __type is fixed (never mutated), only
    -- __value changes. This is safe even for same-type nesting: lua_next
    -- captures values onto the Lua stack before recursion, so the outer
    -- __value is unaffected by the inner write.
    --
    -- NOTE: This isn't _strictly_ necessary, but it reduces GC pressure
    -- by a lot, because we can avoid a temporary table alloc that we'd
    -- otherwise need for each individual object we need to wrap.
    local wrappers = {}
    for name, sentinel in name_to_sentinel do
        wrappers[name] = { __type = sentinel, __value = lljson.null }
    end

    return function(key, value, parent)
        -- Intercept __type fields if present, try to unwrap the branding table
        if key == "__type" then
            local name = sentinel_to_name[value]
            if name then
                return name
            end
            error("field '__type' is reserved for type branding")
        end

        -- Don't re-wrap values inside our wrappers, we might just be revisiting.
        -- TODO: Hmm, imagine there's a cheaper way to do this.
        if key == "__value" and parent then
            if sentinel_to_name[rawget(parent, "__type")] then
                return value
            end
        end

        -- Wrap potentially branded tables
        if type(value) == "table" then
            local mt = getmetatable(value)
            if mt then
                -- Call __tojson manually on the value, if we have one.
                local tojson = mt.__tojson
                value = if tojson then tojson(value) else value

                -- Then try to wrap the value in a branded table, if this is one of our known types
                -- otherwise just return the value directly.
                local name = mt_to_name[mt]
                if name then

                    -- Returning nil from __tojson here isn't allowed for complicated reasons. Mostly because
                    -- it'd change the shape of the wrapper table if we set it which could mess up iteration.
                    if value == nil then
                        error("__tojson must not return nil (maybe use lljson.null?)")
                    end
                    local w = wrappers[name]
                    w.__value = value
                    return w
                end
            end
        end

        return value
    end
end

function TypedJSON:_make_reviver()
    local name_to_mt = self.name_to_mt

    return function(key, value)
        if type(value) == "table" then
            local type_name = rawget(value, "__type")
            if type_name ~= nil then
                local inner = rawget(value, "__value")
                if inner ~= nil then
                    local mt = name_to_mt[type_name]
                    if not mt then
                        error(`unknown branded type: {type_name}`)
                    end
                    local fromjson = mt.__fromjson
                    if fromjson then
                        return fromjson(inner)
                    end
                    return setmetatable(inner, mt)
                end
            end
        end
        return value
    end
end

function TypedJSON:encode(value)
    return lljson.encode(value, self._encode_opts)
end

function TypedJSON:decode(json_string)
    return lljson.decode(json_string, self._reviver)
end

function TypedJSON:slencode(value)
    return lljson.slencode(value, self._encode_opts)
end

function TypedJSON:sldecode(json_string)
    return lljson.sldecode(json_string, self._reviver)
end

-- ============================================
-- Tests
-- ============================================

-- Define some example types
local Vec2 = {}
Vec2.__index = Vec2
function Vec2.new(x, y)
    return setmetatable({ x = x, y = y }, Vec2)
end
function Vec2:magnitude()
    return math.sqrt(self.x * self.x + self.y * self.y)
end

local Player = {}
Player.__index = Player
function Player.new(name, pos)
    return setmetatable({ name = name, pos = pos }, Player)
end

-- This could just be a vector, but whatever, it's just for demonstration.
local Color = {}
Color.__index = Color
function Color.new(r, g, b)
    return setmetatable({ r = r, g = g, b = b }, Color)
end
function Color:__tojson()
    return string.format("#%02x%02x%02x", self.r, self.g, self.b)
end
function Color.__fromjson(s)
    return Color.new(
        tonumber(string.sub(s, 2, 3), 16),
        tonumber(string.sub(s, 4, 5), 16),
        tonumber(string.sub(s, 6, 7), 16)
    )
end

local tj = TypedJSON.new({ Vec2 = Vec2, Player = Player, [3] = Color })

-- Basic round-trip
do
    local v = Vec2.new(3, 4)
    local str = tj:encode(v)
    local decoded = lljson.decode(str)
    assert(decoded.__type == "Vec2")
    assert(decoded.__value.x == 3)
    assert(decoded.__value.y == 4)

    local revived = tj:decode(str)
    assert(getmetatable(revived) == Vec2)
    assert(revived.x == 3)
    assert(revived.y == 4)
    assert(revived:magnitude() == 5)
end

-- Source object not mutated
do
    local v = Vec2.new(1, 2)
    tj:encode(v)
    assert(rawget(v, "__type") == nil)
    assert(rawget(v, "__value") == nil)
    assert(getmetatable(v) == Vec2)
end

-- Nested branded objects
do
    local p = Player.new("Alice", Vec2.new(3, 4))
    local str = tj:encode(p)
    local revived = tj:decode(str)

    assert(getmetatable(revived) == Player)
    assert(revived.name == "Alice")
    assert(getmetatable(revived.pos) == Vec2)
    assert(revived.pos.x == 3)
    assert(revived.pos.y == 4)
    assert(revived.pos:magnitude() == 5)
end

-- Array of branded objects
do
    local points = {
        Vec2.new(1, 0),
        Vec2.new(0, 1),
        Vec2.new(3, 4),
    }
    local str = tj:encode(points)
    local revived = tj:decode(str)

    assert(#revived == 3)
    for i, v in revived do
        assert(getmetatable(v) == Vec2)
    end
    assert(revived[1].x == 1)
    assert(revived[3]:magnitude() == 5)
end

-- Error on unbranded __type field in encode
do
    local bad = { __type = "Vec2", __value = {1,2} }
    local ok, err = pcall(function()
        tj:encode(bad)
    end)
    assert(not ok)
    assert(string.find(err, "reserved"))
end

-- Error on unknown type during decode
do
    local json_str = '{"__type":"Unknown","__value":{"a":1}}'
    local ok, err = pcall(function()
        tj:decode(json_str)
    end)
    assert(not ok)
    assert(string.find(err, "unknown branded type"))
end

-- Unbranded tables pass through fine
do
    local plain = { x = 1, y = 2 }
    local str = tj:encode(plain)
    local revived = tj:decode(str)
    assert(revived.x == 1)
    assert(revived.y == 2)
    assert(getmetatable(revived) == nil)
end

-- slencode/sldecode round-trip
do
    local v = Vec2.new(3, 4)
    local str = tj:slencode(v)
    local revived = tj:sldecode(str)
    assert(getmetatable(revived) == Vec2)
    assert(revived.x == 3)
    assert(revived.y == 4)
    assert(revived:magnitude() == 5)
end

-- slencode/sldecode with nested branded objects
do
    local p = Player.new("Bob", Vec2.new(10, 20))
    local str = tj:slencode(p)
    local revived = tj:sldecode(str)
    assert(getmetatable(revived) == Player)
    assert(revived.name == "Bob")
    assert(getmetatable(revived.pos) == Vec2)
    assert(revived.pos.x == 10)
    assert(revived.pos.y == 20)
end

-- Table with __value but no __type passes through (not a branded wrapper)
do
    local t = { __value = 42, other = "hi" }
    local str = tj:encode(t)
    local revived = tj:decode(str)
    assert(revived.__value == 42)
    assert(revived.other == "hi")
end

-- Branded object inside unbranded table
do
    local data = {
        label = "origin",
        point = Vec2.new(0, 0),
    }
    local str = tj:encode(data)
    local revived = tj:decode(str)
    assert(revived.label == "origin")
    assert(getmetatable(revived.point) == Vec2)
    assert(revived.point.x == 0)
    assert(revived.point.y == 0)
end

-- Deep nesting: exercises wrapper reuse at multiple depths
do
    local Team = {}
    Team.__index = Team
    function Team.new(name, members)
        return setmetatable({ name = name, members = members }, Team)
    end

    local tj2 = TypedJSON.new({ Vec2 = Vec2, Player = Player, Team = Team })

    local team = Team.new("Red", {
        Player.new("Alice", Vec2.new(1, 2)),
        Player.new("Bob", Vec2.new(3, 4)),
    })
    local str = tj2:encode(team)
    local revived = tj2:decode(str)

    assert(getmetatable(revived) == Team)
    assert(revived.name == "Red")
    assert(#revived.members == 2)
    assert(getmetatable(revived.members[1]) == Player)
    assert(getmetatable(revived.members[1].pos) == Vec2)
    assert(revived.members[1].pos.x == 1)
    assert(getmetatable(revived.members[2]) == Player)
    assert(revived.members[2].pos:magnitude() == 5)
end

-- ============================================
-- Color: branded type with __tojson
-- ============================================

-- Branding with __tojson/__fromjson: compact wire format
do
    local c = Color.new(255, 0, 0)
    local str = tj:encode(c)
    local decoded = lljson.decode(str)
    assert(decoded.__type == 3, `should be branded as 3, got: {str}`)
    assert(decoded.__value == "#ff0000", "should use __tojson compact form, got: " .. str)

    local revived = tj:decode(str)
    assert(getmetatable(revived) == Color)
    assert(revived.r == 255 and revived.g == 0 and revived.b == 0)
end

-- Without skip_tojson, replacer sees __tojson result (a string), can't brand it
do
    local seen_type
    local r = lljson.encode(Color.new(255, 0, 0), {
        replacer = function(key, value)
            if key == nil then
                seen_type = type(value)
            end
            return value
        end,
    })
    assert(seen_type == "string")
    assert(r == '"#ff0000"')
end

-- Round-trip with Color nested inside other branded types
do
    local data = { pos = Vec2.new(1, 2), color = Color.new(0, 128, 255) }
    local str = tj:encode(data)
    local revived = tj:decode(str)
    assert(getmetatable(revived.pos) == Vec2)
    assert(revived.pos.x == 1)
    assert(getmetatable(revived.color) == Color)
    assert(revived.color.r == 0 and revived.color.g == 128 and revived.color.b == 255)
end

return 'OK'
