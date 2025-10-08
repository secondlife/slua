change_memcat()
local t = {}

function do_some_allocs(num, callback)
    for i=1, 10 do
        for j=1,num do
            table.insert(t, buffer.create(100))
            if callback then
                callback()
            end
        end
        table.clear(t)
    end
    -- Just make sure `t` doesn't get optimized away
    assert(t)
end

-- This one should _never_ fail
do_some_allocs(80, function() collectgarbage("collect") end)
-- This one might if the GC doesn't naturally try and catch up based on the limit
do_some_allocs(80, nil)
-- This one should absolutely fail, it allocates too much.
assert(not pcall(function() do_some_allocs(200, nil) end))

return 'OK'
