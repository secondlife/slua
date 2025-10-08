
t1 = {}
t2 = {}
t3 = {}

-- simply for speed under mono, appending to bigger lists gets slower
for i=1,20 do
    table.insert(t1, i)
    table.insert(t2, i)
    table.insert(t3, i)
end
ll.Sleep(0.01)
ll.OwnerSay(`memory used: {ll.GetUsedMemory()}`)
ll.GetFreeMemory()
