local _VALUE_LIKE = {
    ["nil"]= true,
    ["boolean"]= true,
    ["number"]=true,
    ["string"]=true,
    ["vector"]=true,
    ["uuid"]=true,
    ["lljson_constant"]=true,
}

local _NO_EQ = {
    ["LLEvents"]=true,
    ["LLTimers"]=true,
    ["DetectedEvent"]=true,
}

-- Things we want to ignore, but still use for traversal
-- These aren't normally reachable by users, and we don't want it as a permanent
-- so their equality is unlikely to match on deserialize.
local _IGNORE = {
    [getmetatable(getfenv())] = true,
    [getfenv()]=true,
}

local function visit_objects(root, visited, path)
    -- Skip if already visited
    if visited[root] then
        return
    end

    -- A handful of places keep a reference to _G around, don't try to alias it.
    if root == _G and path ~= "g" then
        return
    end

    local t = typeof(root)
    if _VALUE_LIKE[t] then
        return
    end

    -- Mark as visited with path (record where first encountered)
    visited[root] = path

    if t == "table" then
        local mt = getmetatable(root)
        if mt then
            local mt_path = path .. "/mt"
            visit_objects(mt, visited, mt_path)
        end

        for key, value in root do
            local child_path = `{path}/{key}`
            --
            if key == "__index" and rawequal(value, root) then
                continue
            end
            visit_objects(value, visited, child_path)
        end
    end
end

local function scavenge_all()
    -- Make sure these are included even though they wouldn't normally be scavenged
    local all_heap = {
        [vector(0, 0, 0)]="some_vector",
        [quaternion(0,0,0,0)]="some_quaternion",
        [uuid("")]="some_uuid",
    }
    visit_objects(_G, all_heap, "g")

    -- Scan type metatables with proper paths
    local type_instances = {
        quaternion(0,0,0,0),
        vector(0,0,0),
        "",
        false,
        1,
        nil,
    }
    for _, instance in type_instances do
        local mt = getmetatable(instance)
        if mt then
            local typename = typeof(instance)
            local mt_path = "type/" .. typename .. "/mt"
            visit_objects(mt, all_heap, mt_path)
        end
    end

    -- Scan userdata metatables with proper paths
    -- We need instances of each userdata type to get their metatables
    local udata_instances = {
        uuid(""),
        LLEvents,
        LLTimers,
        -- Add other userdata types as needed
    }
    for _, instance in udata_instances do
        local mt = getmetatable(instance)
        if mt then
            -- Use tag number in path (like C++ does: "udata/0/mt", etc.)
            local mt_path = "udata/" .. typeof(instance) .. "/mt"
            visit_objects(mt, all_heap, mt_path)
        end
    end

    -- Also scan current environment
    local env = getfenv()
    if env then
        visit_objects(env, all_heap, "user_globs")
    end

    -- strip out ignored objects
    for k, v in _IGNORE do
        all_heap[k] = nil
    end

    local all_heap_list = {}
    for k, v in all_heap do
        -- Put the table in path, obj order
        table.insert(all_heap_list, {v, k})
    end
    return all_heap_list
end

local function main()
    local all_heap_list = scavenge_all()
    local deser_heap_list = ares.unpersist(ares.persist(all_heap_list))

    local deduped = {}
    for i, obj in deser_heap_list do
        local path, val = obj[1], obj[2]
        local found = deduped[val]
        if found then
            error(`Deserialized {path} is a dupe of {found}`)
        end
        deduped[val] = path
    end

    assert(#deser_heap_list == #all_heap_list, `{#deser_heap_list} == {#all_heap_list}`)

    for i, obj in all_heap_list do
        local obj2 = deser_heap_list[i]
        local v, v2 = obj[2], obj2[2]
        local t = typeof(v)
        if not _NO_EQ[t] then
            assert(v == v2, `{obj[1]}: {v} == {v2}`)
        end
    end
end

main()
return "OK"
