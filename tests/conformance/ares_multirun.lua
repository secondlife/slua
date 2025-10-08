assert(aGlobal == nil)

aGlobal = "foo"
assert(aGlobal == "foo")

coroutine.yield()

assert(aGlobal == "foo")

print('OK')
return 'OK'
