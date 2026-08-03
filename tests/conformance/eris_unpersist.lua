permtable = { 1234 }

function testcounter(counter)
  local a = counter.cur()
  counter.inc()
  return counter.cur() == a + 1
end

function testuvinthread(func)
  local success, result = coroutine.resume(func)
  return success and result == 5
end

function test(rootobj)
  local passed = 0
  local total = 0
  -- `actual` and `expected` are optional; when both are omitted they compare
  -- equal and the test rests on `ok` alone.
  local dotest = function(name, ok, actual, expected)
    total = total + 1
    if ok and actual == expected then
      print(name, " PASSED")
      passed = passed + 1
    else
      print(name, "*FAILED", `expected {expected}, got {actual}`)
      error(`{name} failed: expected {expected}, got {actual}`, 0)
    end
  end

  dotest("Permanent value        ", rootobj.testperm == permtable)
  dotest("Nil value              ", rootobj.testnil == nil)
  dotest("Boolean FALSE          ", rootobj.testfalse == false)
  dotest("Boolean TRUE           ", rootobj.testtrue == true)
  dotest("Light userdata         ", checkludata(rootobj.testludata))
  dotest("Number 7               ", rootobj.testseven == 7)
  dotest("String 'foobar'        ", rootobj.testfoobar == "foobar")
  dotest("Table                  ", rootobj.testtbl.a == 2 and rootobj.testtbl[2] == 4)
  dotest("NaN value              ", rootobj.testnan[1] ~= rootobj.testnan[1])
  dotest("Looped tables          ", rootobj.testlooptable.testloopb.testloopa == rootobj.testlooptable)
  dotest("Table metatable        ", rootobj.testmt() == 21)
  dotest("__newindex metamethod  ", rootobj.testniinmt.a == 3)
  -- dotest("Udata literal persist  ", unboxinteger(rootobj.testliteraludata) == 71)
  dotest("Func returning 4       ", rootobj.testfuncreturnsfour() == 4)
  dotest("Lua closure            ", rootobj.testclosure() == 11)
  dotest("Function env           ", rootobj.testfenv() == 456)
  dotest("Nil in closure         ", rootobj.testnilclosure() == nil)
  dotest("Nested func            ", rootobj.testnest(1) == 6)
  dotest("Upvalue cycles         ", rootobj.testuvcycle()[1] == rootobj.testuvcycle()[2])
  -- dotest("Table special persist  ", rootobj.testsptable.a == 6)
  -- dotest("Udata special persist  ", unboxboolean(rootobj.testspudata1) == true and unboxboolean(rootobj.testspudata2) == false)
  dotest("Identical tables       ", rootobj.testsharedrefa ~= rootobj.testsharedrefb)
  dotest("Shared reference       ", rootobj.testsharedrefa.sharedref == rootobj.testsharedrefb.sharedref)
  dotest("Shared upvalues        ", testcounter(rootobj.testsharedupval))
  -- dotest("Debug info             ", (rootobj.testdebuginfo(2)) == "foo")
  -- `coroutine.resume(co) == true` would truncate the multret and drop the
  -- returned value, so capture both before handing them to dotest.
  local nok, nval = coroutine.resume(rootobj.testnthread)
  dotest("Thread start           ", nok, nval, 4)
  local rok, rval = coroutine.resume(rootobj.testthread)
  dotest("Thread resume          ", rok, rval, 14)
  dotest("Thread dead            ", coroutine.resume(rootobj.testdthread) == false)
  dotest("Open upvalues          ", testuvinthread(rootobj.testuvinthread))
  local pok, pval = coroutine.resume(rootobj.testprotthr)
  dotest("Yielded pcall          ", pok, pval, "test")
  local xok, xval = coroutine.resume(rootobj.testxprotthr)
  dotest("Yielded xpcall         ", xok, xval, "handler:test")
  -- Luau doesn't support yielding in metafunctions!
  -- local yok, yval = coroutine.resume(rootobj.testymtthr)
  -- dotest("Yielded metafunc       ", yok, yval, true)
  dotest("Dead thread            ", coroutine.status(rootobj.testymtthr) == 'dead')
  dotest("Deep callstack         ", rootobj.testdeep() == 100)
  dotest("Tail call              ", rootobj.testtail() == 100)

  print()
  if passed == total then
    print("All tests passed.")
  else
    print(passed .. "/" .. total .. " tests passed.")
  end

  if rootobj.testlife then
    print(select(2, coroutine.resume(rootobj.testlife)))
    print(select(2, coroutine.resume(rootobj.testlife)))
  end
end

uperms = {
  _ENV = getfenv(),
  [1] = coroutine.yield,
  [2] = permtable,
  [3] = pcall,
  [4] = xpcall,
  [5] = coroutine.resume,
}

-- `buf` must be set by the runner
rootobj = ares.unpersist(uperms, buf)

test(rootobj)

return "OK"
