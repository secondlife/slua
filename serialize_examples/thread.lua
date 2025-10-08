function foo()
    coroutine.yield(1)
end

return coroutine.create(foo)
