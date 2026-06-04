-- Conformance tests for 'C' (string-csv) and 'M' (string-map) fluent builder semantics.
--
-- The C++ harness registers testbuilder.apply(params) which serializes params
-- using a synthetic descriptor table:
--   tag 5  'C'  whitelist
--   tag 7  'M'  custom_headers
--   tag 9  's'  label

local function apply(params)
    testbuilder.apply(params)
    return _captured_rules
end

-- ─── 'C' string-csv ──────────────────────────────────────────────────────────

-- Single URL: emits {tag, "url"}.
local r = apply({ whitelist = {"https://example.com"} })
assert(#r == 2, "csv single: expected 2 slots, got " .. #r)
assert(r[1] == 5, "csv single: tag should be 5")
assert(r[2] == "https://example.com", "csv single: value mismatch")

-- Multiple URLs: comma-joined.
r = apply({ whitelist = {"https://a.com", "https://b.com"} })
assert(#r == 2, "csv multi: expected 2 slots")
assert(r[1] == 5, "csv multi: tag should be 5")
assert(r[2] == "https://a.com,https://b.com", "csv multi: value mismatch, got " .. tostring(r[2]))

-- URL containing a comma: comma must be backslash-escaped.
r = apply({ whitelist = {"https://a.com/p?x=1,y=2"} })
assert(#r == 2, "csv comma-escape: expected 2 slots")
assert(r[2] == "https://a.com/p?x=1\\,y=2", "csv comma-escape: value mismatch, got " .. tostring(r[2]))

-- URL containing a backslash: backslash must be doubled.
r = apply({ whitelist = {"https://a.com/a\\b"} })
assert(#r == 2, "csv backslash-escape: expected 2 slots")
assert(r[2] == "https://a.com/a\\\\b", "csv backslash-escape: value mismatch, got " .. tostring(r[2]))

-- Empty array: tag must not appear in output.
r = apply({ whitelist = {} })
assert(#r == 0, "csv empty: output should be empty, got " .. #r)

-- Nil / unset: tag must not appear in output.
r = apply({})
assert(#r == 0, "csv nil: output should be empty")

-- Array index order is preserved in the CSV.
r = apply({ whitelist = {"first", "second", "third", "fourth", "fifth"} })
assert(#r == 2, "csv order: expected 2 slots")
assert(r[2] == "first,second,third,fourth,fifth", "csv order: insertion order not preserved, got " .. tostring(r[2]))

-- ─── 'M' string-map ──────────────────────────────────────────────────────────

-- Single header: emits {tag, key, value}.
r = apply({ custom_headers = {["X-Foo"] = "bar"} })
assert(#r == 3, "map single: expected 3 slots, got " .. #r)
assert(r[1] == 7, "map single: tag should be 7")
assert(r[2] == "X-Foo", "map single: key mismatch")
assert(r[3] == "bar", "map single: value mismatch")

-- Multiple headers: tag appears once per pair, keys sorted.
r = apply({ custom_headers = {["Z-Last"] = "z", ["A-First"] = "a"} })
assert(#r == 6, "map multi: expected 6 slots, got " .. #r)
assert(r[1] == 7 and r[4] == 7, "map multi: both entries must start with tag 7")
assert(r[2] == "A-First", "map multi: first key should be A-First (sorted), got " .. tostring(r[2]))
assert(r[3] == "a",       "map multi: first value should be 'a'")
assert(r[5] == "Z-Last",  "map multi: second key should be Z-Last, got " .. tostring(r[5]))
assert(r[6] == "z",       "map multi: second value should be 'z'")

-- Empty table: tag must not appear in output.
r = apply({ custom_headers = {} })
assert(#r == 0, "map empty: output should be empty")

-- Nil / unset: tag must not appear in output.
r = apply({})
assert(#r == 0, "map nil: output should be empty")

-- ─── Co-existence with scalar property ───────────────────────────────────────

-- All three descriptor types in one call.
r = apply({
    whitelist      = {"https://x.com"},
    custom_headers = {["K"] = "V"},
    label          = "hello",
})
-- Expected output (tag-sorted: 5, 7, 9):
--   5, "https://x.com", 7, "K", "V", 9, "hello"
assert(#r == 7, "mixed: expected 7 slots, got " .. #r)
assert(r[1] == 5 and r[2] == "https://x.com", "mixed: csv slot mismatch")
assert(r[3] == 7 and r[4] == "K" and r[5] == "V", "mixed: map slot mismatch")
assert(r[6] == 9 and r[7] == "hello", "mixed: string slot mismatch")

return "OK"
