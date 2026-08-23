-- Canny repro: a large object whose only remaining reference is a weak
-- table entry must not count against the memory limit once the strong
-- reference dies. Before eager weak clearing this OOMs on the second fill.

-- Keep the real GC from completing a cycle on its own; the memory limit
-- callback's heap walk must do the clearing. Don't use collectgarbage("stop"),
-- it disables the beforeallocate callback entirely.
collectgarbage("setgoal", 100000)
collectgarbage("collect")

change_memcat(10)

local tab1 = table.create(2000, 1)
local weak = setmetatable({}, { __mode = "kv" })
weak[1] = tab1

-- Both tables can't fit under the limit at once
tab1 = nil

local tab2 = table.create(2000, 2)
assert(tab2[2000] == 2)
assert(weak[1] == nil)

return 'OK'
