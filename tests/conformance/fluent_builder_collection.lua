-- Conformance tests for 'C' (string-csv) and 'M' (string-map) fluent builder semantics.
--
-- The C++ harness registers testbuilder.apply(params) which serializes params
-- using a synthetic descriptor table:
--   tag 5  'C'  whitelist
--   tag 7  'M'  custom_headers
--   tag 9  's'  label

local function check(params, expected)
    testbuilder.apply(params)
    -- it's fine to compare the json-encoded rules because they will _always_
    -- be array-like tables with defined sort order. The whole intent of the functions
    -- is to turn mappings into flat lists.
    local got, want = lljson.slencode(_captured_rules), lljson.slencode(expected)
    assert(got == want, `expected {want}, got {got}`)
end

-- ─── 'C' string-csv ──────────────────────────────────────────────────────────

-- Single URL: emits {tag, "url"}.
check({ whitelist = {"https://example.com"} }, {5, "https://example.com"})

-- Multiple URLs: comma-joined, array index order preserved.
check(
    { whitelist = {"https://a.com", "https://b.com"} },
    {5, "https://a.com,https://b.com"}
)
check(
    { whitelist = {"first", "second", "third", "fourth", "fifth"} },
    {5, "first,second,third,fourth,fifth"}
)

-- Comma in a URL must be backslash-escaped; a backslash must be doubled.
check({ whitelist = {"https://a.com/p?x=1,y=2"} }, {5, "https://a.com/p?x=1\\,y=2"})
check({ whitelist = {"https://a.com/a\\b"} }, {5, "https://a.com/a\\\\b"})

-- Empty array or unset: tag must not appear in output.
check({ whitelist = {} }, {})
check({}, {})

-- ─── 'M' string-map ──────────────────────────────────────────────────────────

-- Single header: emits {tag, key, value}.
check({ custom_headers = {["X-Foo"] = "bar"} }, {7, "X-Foo", "bar"})

-- Multiple headers: tag appears once per pair, keys sorted.
check(
    { custom_headers = {["Z-Last"] = "z", ["A-First"] = "a"} },
    {7, "A-First", "a", 7, "Z-Last", "z"}
)

-- Empty table or unset: tag must not appear in output.
check({ custom_headers = {} }, {})
check({}, {})

-- ─── Co-existence with scalar property ───────────────────────────────────────

-- All three descriptor types in one call, tag-sorted output.
check(
    { whitelist = {"https://x.com"}, custom_headers = {["K"] = "V"}, label = "hello" },
    {5, "https://x.com", 7, "K", "V", 9, "hello"}
)

return "OK"
