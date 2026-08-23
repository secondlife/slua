-- Weak table entries whose contents are only weakly reachable must be
-- cleared by the heap walk itself, without the real
-- GC ever reaching its atomic phase.

-- Keep the real GC from completing a cycle on its own so any clearing we
-- observe is attributable to the heap walk. Crucially, we _do not_ fully
-- disable GC since that also disables our allocation hooks.
collectgarbage("setgoal", 100000)
collectgarbage("collect")

local function entry_count(t)
    local n = 0
    for _ in pairs(t) do
        n += 1
    end
    return n
end

-- mode "v": dead values are cleared from both the array and node parts
do
    local wv = setmetatable({}, { __mode = "v" })
    local keep = {}
    wv[1] = { "array-part garbage" }
    wv.x = { "node-part garbage" }
    wv.y = keep
    used_memory()
    assert(wv[1] == nil)
    assert(wv.x == nil)
    assert(wv.y == keep)
end

-- mode "k": a dead key removes the whole entry, a live key keeps it
do
    local wk = setmetatable({}, { __mode = "k" })
    local dead_key = {}
    local live_key = {}
    wk[dead_key] = "payload for dead key"
    wk[live_key] = "payload for live key"
    dead_key = nil
    used_memory()
    assert(entry_count(wk) == 1)
    assert(wk[live_key] == "payload for live key")
end

-- mode "kv": either side dying removes the entry
do
    local wkv = setmetatable({}, { __mode = "kv" })
    local live = {}
    wkv[{}] = live
    wkv[live] = {}
    wkv.z = live
    used_memory()
    assert(entry_count(wkv) == 1)
    assert(wkv.z == live)
end

-- The size reported by the walk must drop when the only strong reference
-- to a large object dies, without any full collection.
do
    local weak = setmetatable({}, { __mode = "kv" })
    local tab = table.create(2000, 1)
    weak[1] = tab
    local before = used_memory()
    tab = nil
    local after = used_memory()
    assert(weak[1] == nil)
    -- 2000 array slots at 16 bytes logical cost each
    assert(before - after >= 2000 * 16)
end

-- Strings and UUIDs have value semantics in user weak tables: never cleared.
-- Build the string dynamically so it isn't a fixed proto constant.
do
    local wv = setmetatable({}, { __mode = "kv" })
    wv[1] = string.rep("weakly held string", 2)
    wv[2] = uuid("12345678-9abc-def0-1234-56789abcdef0")
    wv[uuid("12345678-9abc-def0-1234-56789abcdee0")] = true
    used_memory()
    assert(wv[1] == string.rep("weakly held string", 2))
    assert(wv[2] == uuid("12345678-9abc-def0-1234-56789abcdef0"))
    assert(wv[uuid("12345678-9abc-def0-1234-56789abcdee0")] == true)
end

-- Clearing must not rehash: capacity is untouched and iteration over a
-- cleared table just skips the dead entries.
do
    local wv = setmetatable({}, { __mode = "v" })
    for i = 1, 8 do
        wv[`key{i}`] = { i }
    end
    wv.live = wv
    local arr_before, hash_before = table_sizes(wv)
    used_memory()
    local arr_after, hash_after = table_sizes(wv)
    assert(arr_before == arr_after)
    assert(hash_before == hash_after)
    assert(entry_count(wv) == 1)
    assert(wv.live == wv)
end

return 'OK'
