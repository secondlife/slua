local m = setmetatable({}, {__index=breakcheck_index, __newindex=breakcheck_index})

breakable()
LLEvents:on("touch_start",print)
-- The LOP_GETTABLE will happen before the call, which will trigger our "did break" check
nothing(m[1])
breakable()
m[2] = 1
-- Same as above, but for LOP_NAMECALL
breakable()
m:foo(1)

return 'OK'
