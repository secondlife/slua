local foo = table.create(1000, 1)
change_memcat()
-- The allocs inside table.clone() should fail, and it should not break freeing the GCO
assert(not pcall(table.clone, foo))
return "OK"
