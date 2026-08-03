local int_band_result = bit32.band(integer.create(3), integer.create(1))
assert(int_band_result == integer.create(1))
assert(typeof(int_band_result) == 'integer')

local num_band_result = bit32.band(integer.create(3), 1)
assert(num_band_result == 1)
assert(typeof(num_band_result) == 'number')

return "OK"
