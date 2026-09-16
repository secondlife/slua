counter = 1
names = {"a", "b", c = {nested = true}, [10] = "ten"}
buf = buffer.create(16)
buffer.writeu32(buf, 0, 0xdeadbeef)
vec = vector.create(1, 2, 3)

local captured = 10
function bump()
    captured += 1
    return captured
end

co = coroutine.create(function(x)
    local y = coroutine.yield(x + 1)
    return y * 2
end)
coroutine.resume(co, 1)

function LLEvents.timer()
    counter += bump()
end

local function report(name, ok)
    print(name .. (if ok then " ok" else " bad"))
end

function LLEvents.moving_start()
    report("counter", counter == 1)
    report("table", names[1] == "a" and names.c.nested == true and names[10] == "ten")
    report("buffer", buffer.readu32(buf, 0) == 0xdeadbeef)
    report("vector", vec == vector.create(1, 2, 3))

    -- The coroutine was left suspended inside its yield, so resuming it has to
    -- pick up where it stopped rather than start over
    local resumed, value = coroutine.resume(co, 21)
    report("coroutine", resumed and value == 42)

    -- The closure's upvalue kept counting from where the fixture left it
    report("upvalue", bump() == 11)
end
