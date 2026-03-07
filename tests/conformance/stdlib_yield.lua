-- ServerLua: yieldability tests for pattern matching and table.sort
-- All pattern matching functions and table.sort are DEFINE_YIELDABLE and can be used in coroutines.

-- yield between find/match calls
do
  local co = coroutine.create(function()
    local i, j = string.find("hello world", "(%w+)")
    coroutine.yield(i, j)
    local m = string.match("hello world", "(%w+)", 7)
    coroutine.yield(m)
    return "done"
  end)
  local ok, i, j = coroutine.resume(co)
  assert(ok and i == 1 and j == 5)
  local ok2, m = coroutine.resume(co)
  assert(ok2 and m == "world")
  local ok3, r = coroutine.resume(co)
  assert(ok3 and r == "done")
end

-- yield from inside gmatch for-in loop
do
  local co = coroutine.create(function()
    local results = {}
    for w in string.gmatch("one two three", "%w+") do
      table.insert(results, w)
      coroutine.yield(w)
    end
    return table.concat(results, ",")
  end)
  local ok1, v1 = coroutine.resume(co)
  assert(ok1 and v1 == "one")
  local ok2, v2 = coroutine.resume(co)
  assert(ok2 and v2 == "two")
  local ok3, v3 = coroutine.resume(co)
  assert(ok3 and v3 == "three")
  local ok4, final = coroutine.resume(co)
  assert(ok4 and final == "one,two,three")
end

-- complex patterns in coroutines: frontiers, balanced match, multi-capture gmatch
do
  local co = coroutine.create(function()
    local r1 = string.gsub("aaa aa a aaa a", "%f[%w]a", "x")
    coroutine.yield(r1)
    local r2 = string.match("((hello)(world))", "%b()")
    coroutine.yield(r2)
    local t = {}
    for k, v in string.gmatch("from=world, to=Lua", "(%w+)=(%w+)") do
      table.insert(t, k .. ":" .. v)
      coroutine.yield(k)
    end
    return table.concat(t, ",")
  end)
  local ok1, v1 = coroutine.resume(co)
  assert(ok1 and v1 == "xaa xa x xaa x")
  local ok2, v2 = coroutine.resume(co)
  assert(ok2 and v2 == "((hello)(world))")
  local ok3, v3 = coroutine.resume(co)
  assert(ok3 and v3 == "from")
  local ok4, v4 = coroutine.resume(co)
  assert(ok4 and v4 == "to")
  local ok5, final = coroutine.resume(co)
  assert(ok5 and final == "from:world,to:Lua")
end

-- yield from gsub replacement callback
do
  local co = coroutine.create(function()
    local s = string.gsub("a b c", "%w+", function(w)
      coroutine.yield("saw:" .. w)
      return string.upper(w)
    end)
    return s
  end)
  local ok1, v1 = coroutine.resume(co)
  assert(ok1 and v1 == "saw:a")
  local ok2, v2 = coroutine.resume(co)
  assert(ok2 and v2 == "saw:b")
  local ok3, v3 = coroutine.resume(co)
  assert(ok3 and v3 == "saw:c")
  local ok4, result = coroutine.resume(co)
  assert(ok4 and result == "A B C")
end

-- Ares round-trip: serialize gsub coroutine with empty then partial buffer
do
  local co = coroutine.wrap(function()
    return string.gsub("aa bb cc dd", "%w+", function(w)
      coroutine.yield(w)
      return string.upper(w)
    end)
  end)

  -- First yield: buffer is empty (callback hasn't returned yet)
  assert(co() == "aa")
  co = ares.unpersist(ares.persist(co))
  collectgarbage()

  -- Second yield: buffer now has "AA " after first replacement
  assert(co() == "bb")
  co = ares.unpersist(ares.persist(co))
  collectgarbage()

  -- Resume from restored coroutine
  assert(co() == "cc")
  assert(co() == "dd")
  local result, n = co()
  assert(result == "AA BB CC DD" and n == 4)
end

-- unfinished captures: find/match should error
assert(not pcall(string.find, "hello", "("))
assert(not pcall(string.match, "hello", "("))

-- unfinished captures in gsub: string replacement without %N should succeed
assert(string.gsub("hello", "(", "") == "hello")
assert(string.gsub("*10|1", "(", "") == "*10|1")
-- %0 uses match pointers directly (not captures), so it also succeeds
assert(string.gsub("hello", "(", "%0") == "hello")

-- unfinished captures in gsub should error when actually accessed
assert(not pcall(string.gsub, "hello", "(", "%1"))
assert(not pcall(string.gsub, "hello", "(", function(c) return c end))
assert(not pcall(string.gsub, "hello", "(", {}))

-- mixed captures: finished + unfinished
assert(string.gsub("abc", "(a)(", "%1") == "abc")
assert(not pcall(string.gsub, "abc", "(a)(", "%2"))

-- max captures (32) with find/match/gmatch to exercise stack headroom
do
  local pat = string.rep("(.)", 32)
  local src = string.rep("a", 32)
  local i, j = string.find(src, pat)
  assert(i == 1 and j == 32)
  local c1 = string.match(src, pat)
  assert(c1 == "a")
  local caps = {}
  for c in string.gmatch(src, pat) do
    table.insert(caps, c)
  end
  assert(#caps == 1 and caps[1] == "a")
end

-- max captures (32) with function replacement to exercise stack growth
do
  local pat = string.rep("(.)", 32)
  local src = string.rep("a", 32)
  local ncalls = 0
  local r = string.gsub(src, pat, function(...)
    ncalls = ncalls + 1
    assert(select("#", ...) == 32)
    return string.upper(table.concat({...}))
  end)
  assert(r == string.rep("A", 32) and ncalls == 1)
end

-- max captures (32) with string replacement referencing captures
do
  local pat = string.rep("(.)", 32)
  local src = string.rep("a", 32)
  -- Replace with first and last capture
  local r = string.gsub(src, pat, "%1_%9")
  assert(r == "a_a")
  -- Replace with all 9 addressable captures (%1-%9)
  local r2 = string.gsub(src, pat, "%1%2%3%4%5%6%7%8%9")
  assert(r2 == "aaaaaaaaa")
end

-- max captures (32) with table replacement
do
  local pat = string.rep("(.)", 32)
  local src = string.rep("a", 32)
  local r = string.gsub(src, pat, {a = "X"})
  assert(r == "X")
end

-- lazy quantifier backtrack failure: exercises IMATCH_MIN_EXPAND exhaustion
-- when the lazy expansion hits a char that doesn't match the class
do
  local i, j, cap = string.find("1ab", "(%d-)b")
  assert(i == 3 and j == 3 and cap == "")
end

-- too many captures: 33 exceeds LUA_MAXCAPTURES (32)
assert(not pcall(string.find, "a", string.rep("()", 33)))

-- pattern too complex: 201 quantifiers exceed LUAI_MAXCCALLS (200) backtrack frames
assert(not pcall(string.find, string.rep("a", 300), string.rep("a?", 201)))

-- Enable interrupt-driven yields from YIELD_CHECK for remaining tests.
-- The interrupt handler (installed by C++ test fixture) calls lua_yield
-- on every YIELD_CHECK hit, and we Ares round-trip on each yield.
enable_check_interrupt()

local function consume_impl(check, expect_yields, f, ...)
  clear_check_count()
  local co = coroutine.create(f)
  local yields = 0
  local ok, a, b, c = coroutine.resume(co, ...)
  assert(ok, a)
  while coroutine.status(co) ~= "dead" do
    yields += 1
    if not is_codegen then
      co = ares.unpersist(ares.persist(co))
      collectgarbage()
    end
    ok, a, b, c = coroutine.resume(co)
    assert(ok, a)
  end
  if expect_yields then
    assert(yields > 0, "no yields occurred")
  end
  if check then
    assert(yields == get_check_count(),
      "yield count mismatch: " .. yields .. " actual vs " .. get_check_count() .. " interrupts")
  end
  return a, b, c, yields
end

local function consume(f, ...)
  return consume_impl(true, true, f, ...)
end

local function consume_nocheck(f, ...)
  return consume_impl(false, false, f, ...)
end

-- deep backtracking with interrupt-driven yields
do
  -- a*a*a*a*b on "aaaaaaa" fails after combinatorial backtracking;
  -- every backtrack step hits YIELD_CHECK in iterative_match_helper.
  local r = consume(function()
    return string.find("aaaaaaa", "a*a*a*a*b")
  end)
  assert(r == nil)

  -- same but with a successful match at the end
  local i, j = consume(function()
    return string.find("aaaab", "a*a*a*b")
  end)
  assert(i == 1 and j == 5)
end

-- plain find with long pattern: yield mid-search and ares round-trip.
-- Scans 5000 non-matching positions (memchr hit + O(5K) memcmp each)
-- before finding the match, exercising yield/resume throughout.
do
  local s = string.rep("a", 10000) .. "b"
  local p = string.rep("a", 5000) .. "b"
  local i, j = consume(function()
    return string.find(s, p, 1, true)
  end)
  assert(i == 5001 and j == 10001, `plain find match failed: i={i} j={j}`)
end

-- zero-length match advancement with interrupts
do
  local r = consume(function()
    return string.gsub("ab", ".-", "x")
  end)
  assert(r == "xaxbx")

  -- The same for empty pattern
  local r2 = consume(function()
    return string.gsub("ab", "", "x")
  end)
  assert(r2 == "xaxbx")
end

-- gsub replacement function returning nil/false (keep-original path)
do
  local r = consume(function()
    return string.gsub("abc", "%w", function() return nil end)
  end)
  assert(r == "abc")

  local r2 = consume(function()
    return string.gsub("abc", "%w", function() return false end)
  end)
  assert(r2 == "abc")
end

-- many sequential matches with interrupt-driven yields
do
  local src = string.rep("a", 100)
  local r, n = consume(function()
    return string.gsub(src, "a", "b")
  end)
  assert(r == string.rep("b", 100) and n == 100)
end

-- gsub with max_s boundary values
do
  -- max_s = 0: no replacements (no yields expected)
  local r, n = consume_nocheck(function()
    return string.gsub("aaa", "a", "b", 0)
  end)
  assert(r == "aaa" and n == 0)

  -- max_s = 1: single replacement
  local r2, n2 = consume(function()
    return string.gsub("aaa", "a", "b", 1)
  end)
  assert(r2 == "baa" and n2 == 1)
end

-- position captures with interrupts (zero-length matches) via for-in
-- skipped under codegen: JITed code can't handle yielding from for-in iterators
if not is_codegen then
  local positions = consume(function()
    local pos = {}
    for p in string.gmatch("abc", "()") do
      table.insert(pos, p)
    end
    return pos
  end)
  assert(#positions == 4)
  assert(positions[1] == 1 and positions[2] == 2 and positions[3] == 3 and positions[4] == 4)
end

-- find/match with interrupts: anchored pattern
do
  local i, j = consume(function()
    return string.find("hello world", "^hello")
  end)
  assert(i == 1 and j == 5)

  local r = consume(function()
    return string.find("hello world", "^world")
  end)
  assert(r == nil)
end

-- gsub with anchored pattern and interrupts
do
  local r, n = consume(function()
    return string.gsub("aaa", "^a", "b")
  end)
  assert(r == "baa" and n == 1)
end

-- Run fn() under timing interrupt and assert max gap between yields is bounded.
local function assert_interrupt_bounded(label, max_delta, fn)
  collectgarbage()
  collectgarbage()
  enable_timing_interrupt()
  fn()
  local delta = get_max_interrupt_delta()
  if not skip_timing_tests then
    assert(delta < max_delta, `max delta between yields too large ({label}): {delta}s`)
  end
end

local N = 100000

do
  local s = string.rep("a", N)
  assert_interrupt_bounded("greedy", 0.0001, function()
    local i, j = string.find(s, "^.*$")
    assert(i == 1 and j == N, `greedy match failed: i={i} j={j}`)
  end)
end

do
  local half = N // 2
  local s = string.rep("(", half) .. string.rep(")", half)
  assert_interrupt_bounded("balance", 0.0001, function()
    local i, j = string.find(s, "%b()")
    assert(i == 1 and j == N, `balance match failed: i={i} j={j}`)
  end)
end

do
  local pat = string.rep("a", N) .. "(b)"
  local s = string.rep("a", N) .. "b"
  assert_interrupt_bounded("forward", 0.0001, function()
    local i, j, c = string.find(s, pat)
    assert(i == 1 and j == N + 1 and c == "b", `forward match failed: i={i} j={j} c={c}`)
  end)
end

-- Adversarial plain find: O(N*M) memcmp with no match.
-- memchr hits at every position, memcmp scans ~N/2 bytes each time.
do
  local half = N // 2
  local s = string.rep("a", N)
  local p = string.rep("a", half) .. "b"
  assert_interrupt_bounded("plain find (adversarial)", 0.0001, function()
    local i = string.find(s, p, 1, true)
    assert(i == nil, "should not match")
  end)
end

do
  local pat = "(" .. string.rep("a", 254) .. "b)"
  local s = string.rep("a", N)
  assert_interrupt_bounded("outer loop", 0.0001, function()
    local r = string.find(s, pat)
    assert(r == nil, "should not match")
  end)
end

-- ==========================================================================
-- table.sort yieldability tests
-- ==========================================================================

-- basic sort with yielding comparator
do
  local t = consume_nocheck(function()
    local t = {3, 1, 2}
    table.sort(t, function(a, b)
      coroutine.yield()
      return a < b
    end)
    return t
  end)
  assert(t[1] == 1 and t[2] == 2 and t[3] == 3)
end

-- sort edge cases with yielding comparator: 0, 1, 2, 3 elements
do
  for _, input in {{}, {1}, {2, 1}, {3, 1, 2}} do
    local t = consume_nocheck(function()
      local t = table.clone(input)
      table.sort(t, function(a, b)
        coroutine.yield()
        return a < b
      end)
      return t
    end)
    for i = 2, #t do
      assert(t[i - 1] <= t[i], "sort edge case failed")
    end
  end
end

-- sort with yielding comparator and ares round-trip
do
  local t = consume_nocheck(function()
    local t = {5, 3, 4, 1, 2}
    table.sort(t, function(a, b)
      coroutine.yield()
      return a < b
    end)
    return t
  end)
  assert(t[1] == 1 and t[2] == 2 and t[3] == 3 and t[4] == 4 and t[5] == 5)
end

-- interrupt-driven yields with custom comparator
do
  local t = consume(function()
    local t = {}
    for i = 1, 100 do t[i] = 101 - i end
    table.sort(t, function(a, b) return a < b end)
    return t
  end)
  for i = 1, 100 do
    assert(t[i] == i, `sort with comparator failed at index {i}: got {t[i]}`)
  end
end

-- interrupt-driven yields with default comparator
do
  local t = consume(function()
    local t = {}
    for i = 1, 100 do t[i] = 101 - i end
    table.sort(t)
    return t
  end)
  for i = 1, 100 do
    assert(t[i] == i, `default sort failed at index {i}: got {t[i]}`)
  end
end

-- large input with yielding comparator
do
  local t = consume_nocheck(function()
    local t = {}
    for i = 1, 200 do t[i] = 201 - i end
    table.sort(t, function(a, b)
      coroutine.yield()
      return a < b
    end)
    return t
  end)
  for i = 1, 200 do
    assert(t[i] == i, `large sort failed at index {i}: got {t[i]}`)
  end
end

-- quicksort killer that triggers heapsort fallback (see sort.luau)
-- yields in comparator ensure YIELD_DISPATCH branches in sort_siftheap are taken
do
  local t = consume_nocheck(function()
    -- discover quicksort killer sequence (non-yielding)
    local keys = {}
    local candidate = 0
    local next = 0
    local t = table.create(100, 0)
    for k in t do t[k] = k end
    table.sort(t, function(x, y)
      coroutine.yield()
      if keys[x] == nil and keys[y] == nil then
        if x == candidate then keys[x] = next else keys[y] = next end
        next += 1
      end
      if keys[x] == nil then candidate = x; return true end
      if keys[y] == nil then candidate = y; return false end
      return keys[x] < keys[y]
    end)
    -- build the killer array
    local arr = table.create(#t)
    for k, v in t do arr[v] = k end
    -- re-sort with yielding comparator to exercise heapsort with yields
    table.sort(arr, function(a, b)
      coroutine.yield()
      return a < b
    end)
    return arr
  end)
  for i = 1, 100 do
    assert(t[i] == i, `heapsort fallback sort failed at index {i}: got {t[i]}`)
  end
end

-- sort (default comparator) yields frequently
do
  local t = {}
  for i = 1, 10000 do t[i] = 10001 - i end
  assert_interrupt_bounded("sort default", 0.0001, function()
    table.sort(t)
  end)
end

-- sort (function comparator) yields frequently
do
  local t = {}
  for i = 1, 10000 do t[i] = 10001 - i end
  assert_interrupt_bounded("sort comparator", 0.0001, function()
    table.sort(t, function(a, b) return a < b end)
  end)
end

return('OK')
