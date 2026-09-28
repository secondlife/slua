-- Every yieldable C function parked mid-call at once. The leaf ones stop at
-- their next stdlib yield check (arm_stdlib_yield), the callback ones inside
-- their callback, and the timer handler itself inside _tick. The golden
-- fixture test checks the phase of every frame, so the parked table's order
-- matters.
local hay = string.rep("a", 10000) .. "b"
local needle = string.rep("a", 600) .. "b"
local src = string.rep("a", 3000) .. "b" .. string.rep("a", 3000) .. "b"

local sort_input = {}
for i = 1, 200 do
    sort_input[i] = (i * 7919) % 200
end

local eq_mt = { __eq = function(a, b) return a.v == b.v end }
local tfind_input = {}
for i = 1, 500 do
    tfind_input[i] = setmetatable({ v = i }, eq_mt)
end
local tfind_needle = setmetatable({ v = 432 }, eq_mt)

parked = {}
results = {}
held = nil

local function park(f)
    local co = coroutine.create(f)
    assert(coroutine.resume(co))
    assert(coroutine.status(co) == "suspended")
    table.insert(parked, co)
end

LLTimers:every(0.1, function()
    park(function()
        arm_stdlib_yield()
        results.find = string.find(hay, needle, 1, true)
    end)
    park(function()
        arm_stdlib_yield()
        results.match = #string.match(hay, "(a+)b")
    end)
    park(function()
        arm_stdlib_yield()
        local _, n = string.gsub(src, "a+b", "x")
        results.gsub_match = n
    end)
    park(function()
        arm_stdlib_yield()
        local count = 0
        for w in string.gmatch(src, "a+b") do
            count += #w
        end
        results.gmatch = count
    end)
    park(function()
        arm_stdlib_yield()
        results.tfind = table.find(tfind_input, tfind_needle)
    end)
    park(function()
        results.gsub_repl = string.gsub("aa bb cc", "%w+", function(w)
            if w == "bb" then
                coroutine.yield()
            end
            return string.upper(w)
        end)
    end)
    park(function()
        local yielded = false
        table.sort(sort_input, function(a, b)
            if not yielded then
                yielded = true
                coroutine.yield()
            end
            return a < b
        end)
        local sorted = true
        for i = 2, #sort_input do
            if sort_input[i - 1] > sort_input[i] then
                sorted = false
            end
        end
        results.sort = sorted
    end)
    park(function()
        results.encode = lljson.encode({ 10, 20, 30 }, { replacer = function(key, value)
            if key == 2 then
                coroutine.yield()
            end
            return value
        end })
    end)
    park(function()
        local t = lljson.decode("[1,2,3]", function(key, value)
            if key == 2 then
                coroutine.yield()
            end
            return value
        end)
        results.decode = t[1] == 1 and t[2] == 2 and t[3] == 3
    end)
    -- An iterator held between iterations has no frame, only upvalues
    held = string.gmatch("aa bb cc", "%w+")
    results.held = #held()
    preempt()
end)

function LLEvents.moving_start()
    for _, co in parked do
        assert(coroutine.resume(co))
        assert(coroutine.status(co) == "dead")
    end
    local ok = results.find == 9401
        and results.match == 10000
        and results.gsub_match == 2
        and results.gmatch == 6002
        and results.tfind == 432
        and results.gsub_repl == "AA BB CC"
        and results.sort == true
        and results.encode == "[10,20,30]"
        and results.decode == true
        and results.held == 2 and #held() == 2 and #held() == 2 and held() == nil
    print(if ok then "yieldables ok" else "yieldables bad")
end
