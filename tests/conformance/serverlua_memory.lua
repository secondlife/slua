local foo = {}
-- The allocs should eventually fail internally, but we should be able to
-- catch those errors within user code assuming we still have enough memory
-- left after the alloc failure to do so.
local success, ret = xpcall(
		function()
		    local i
            for i = 1, 20000 do
                foo[i] = tostring(i)
            end
		end,
		function(err)
			return err
		end)
assert(not success)
assert(ret == "not enough memory")
assert(#foo < 20000)

return 'OK'
