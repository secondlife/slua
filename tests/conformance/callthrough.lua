-- lua_call from a yieldable C function has three ways back: the callee
-- returns, it yields and the continuation finishes the call, or it errors
-- and the frame unwinds. A pcall inside the callee catching an error has to
-- look like the first of those, with the frame trimmed to the result, and
-- not like the second. The yield macros are built on this.

-- Runs callthrough(callee) on its own coroutine so yields come back here
local function run(callee)
    local co = coroutine.create(function()
        return callthrough(callee)
    end)
    while true do
        local res = table.pack(coroutine.resume(co))
        if coroutine.status(co) == "dead" then
            return table.unpack(res, 1, res.n)
        end
    end
end

local ok, tag, top, result

ok, tag, top, result = run(function()
    return "r"
end)
assert(ok and tag == "direct", "plain return comes straight back")
assert(top == 2 and result == "r", "plain return leaves the result on top")

ok, tag, top, result = run(function()
    coroutine.yield()
    return "r"
end)
assert(ok and tag == "cont", "a yield drops the frame and finishes through the continuation")
assert(top == 2 and result == "r", "the continuation finds the result on top")

ok, tag, top, result = run(function()
    local caught = pcall(function()
        error("boom")
    end)
    return caught
end)
assert(ok and tag == "direct", "a caught error comes straight back like a return")
assert(top == 2 and result == false, "a caught error leaves the result on top")

ok, tag, top, result = run(function()
    local caught = pcall(function()
        coroutine.yield()
        error("boom")
    end)
    return caught
end)
assert(ok and tag == "cont", "a catch after a yield finishes through the continuation")
assert(top == 2 and result == false, "the continuation finds the result after a catch")

ok, tag = run(function()
    error("boom")
end)
assert(not ok and string.find(tag, "boom"), "an uncaught error unwinds the frame with the error")

-- A metamethod is not yieldable, and being reached from a yieldable C frame
-- must not change that
local yielder = setmetatable({}, {
    __tostring = function()
        coroutine.yield()
        return "s"
    end,
})
ok, tag = run(function()
    return tostring_through(yielder)
end)
assert(not ok and string.find(tag, "across metamethod"), "a metamethod reached from a yieldable C frame cannot yield")

return "OK"
