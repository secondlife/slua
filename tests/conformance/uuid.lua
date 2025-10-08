local key = give_uuid()

local expected_str = "12345678-9abc-def0-1234-56789abcdef0"
local expected_key = uuid(expected_str)

assert(key == expected_key)
assert(tostring(key) == expected_str)

-- These two should be equal within tables
local tab = {}
tab[key] = 1
tab[expected_key] = 2

assert(tab[key] == 2)
assert(tab[expected_key] == 2)

return "OK"
