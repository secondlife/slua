local function prequire(name) local success, result = pcall(require, name); return success and result end
local bench = script and require(script.Parent.bench_support) or prequire("bench_support") or require("../bench_support")

-- Build test corpus: repeated realistic text with varied patterns
local paragraph = "The quick brown fox jumps over the lazy dog. Email: user@example.com, Phone: 555-1234. Price: $19.99. Date: 2026-02-28."
local corpus = string.rep(paragraph .. "\n", 200)

-- string.find: plain substring search
bench.runCode(function()
    local count = 0
    for i = 1, 500 do
        local pos = 1
        while true do
            local s, e = string.find(corpus, "fox", pos, true)
            if not s then break end
            count = count + 1
            pos = e + 1
        end
    end
    assert(count > 0)
end, "find-plain")

-- string.find: pattern search
bench.runCode(function()
    local count = 0
    for i = 1, 500 do
        local pos = 1
        while true do
            local s, e = string.find(corpus, "%d+%-%d+", pos)
            if not s then break end
            count = count + 1
            pos = e + 1
        end
    end
    assert(count > 0)
end, "find-pattern")

-- string.match: capture groups
bench.runCode(function()
    local count = 0
    for i = 1, 500 do
        local pos = 1
        while true do
            local y, m, d = string.match(corpus, "(%d%d%d%d)%-(%d%d)%-(%d%d)", pos)
            if not y then break end
            count = count + 1
            local s, e = string.find(corpus, "%d%d%d%d%-%d%d%-%d%d", pos)
            pos = e + 1
        end
    end
    assert(count > 0)
end, "match-capture")

-- string.gmatch: iteration
bench.runCode(function()
    local count = 0
    for i = 1, 500 do
        for word in string.gmatch(corpus, "%a+") do
            count = count + 1
        end
    end
    assert(count > 0)
end, "gmatch-words")

-- string.gsub: replacement
bench.runCode(function()
    local result
    for i = 1, 200 do
        result = string.gsub(corpus, "%d+", function(n)
            return tostring(tonumber(n) + 1)
        end)
    end
    assert(#result > 0)
end, "gsub-callback")

-- string.gsub: pattern replacement
bench.runCode(function()
    local result
    for i = 1, 500 do
        result = string.gsub(corpus, "%u%l+", "%1")
    end
    assert(#result > 0)
end, "gsub-pattern")

-- string.find: character class heavy
bench.runCode(function()
    local count = 0
    for i = 1, 500 do
        local pos = 1
        while true do
            local s, e = string.find(corpus, "[%w._%%+-]+@[%w.-]+%.%a%a+", pos)
            if not s then break end
            count = count + 1
            pos = e + 1
        end
    end
    assert(count > 0)
end, "find-email-pattern")
