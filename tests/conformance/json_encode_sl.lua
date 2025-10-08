function json_decode(str)
  local pos = 1
  local skip_whitespace, parse_array, parse_string, parse_number, parse_object, parse_value

  skip_whitespace = function ()
    while pos <= #str do
      local c = str:sub(pos, pos)
      if c == " " or c == "\t" or c == "\n" or c == "\r" then
        pos = pos + 1
      else
        break
      end
    end
  end

  parse_array = function ()
    local arr = {}
    pos = pos + 1  -- skip '['
    skip_whitespace()
    if str:sub(pos, pos) == "]" then
      pos = pos + 1
      return arr
    end
    local index = 1
    while true do
      skip_whitespace()
      arr[index] = parse_value()
      index = index + 1
      skip_whitespace()
      local delimiter = str:sub(pos, pos)
      if delimiter == "]" then
        pos = pos + 1
        break
      elseif delimiter == "," then
        pos = pos + 1
      else
        print("ERROR: " .."Expected ',' or ']' at position " .. pos)
      end
    end
    return arr
  end

  parse_string = function ()
    pos = pos + 1  -- skip opening quote
    local start = pos
    local result = ""
    while pos <= #str do
      local c = str:sub(pos, pos)
      if c == '"' then
        result = result .. str:sub(start, pos - 1)
        pos = pos + 1  -- skip closing quote
        return result
      elseif c == "\\" then
        result = result .. str:sub(start, pos - 1)
        pos = pos + 1
        local escape = str:sub(pos, pos)
        if escape == "u" then
          local hex = str:sub(pos + 1, pos + 4)
          result = result .. utf8.char(tonumber(hex, 16))
          pos = pos + 4
        elseif escape == '"' or escape == "\\" or escape == "/" then
          result = result .. escape
          pos = pos + 1
        elseif escape == "b" then
          result = result .. "\b"
          pos = pos + 1
        elseif escape == "f" then
          result = result .. "\f"
          pos = pos + 1
        elseif escape == "n" then
          result = result .. "\n"
          pos = pos + 1
        elseif escape == "r" then
          result = result .. "\r"
          pos = pos + 1
        elseif escape == "t" then
          result = result .. "\t"
          pos = pos + 1
        else
          print("ERROR: " .."Invalid escape sequence at position " .. pos)
        end
        start = pos
      else
        pos = pos + 1
      end
    end
    print("ERROR: " .."Unterminated string starting at position " .. start)
  end

  parse_number = function ()
    local start = pos
    while pos <= #str and str:sub(pos, pos):match("[0-9+%-eE%.]") do
      pos = pos + 1
    end
    local num_str = str:sub(start, pos - 1)
    local number = tonumber(num_str)
    if not number then
      print("ERROR: " .."Invalid number: " .. num_str .. " at position " .. start)
    end
    return number
  end
  
  parse_object = function ()
    local obj = {}
    pos = pos + 1  -- skip '{'
    skip_whitespace()
    if str:sub(pos, pos) == "}" then
      pos = pos + 1
      return obj
    end
    while true do
      skip_whitespace()
      if str:sub(pos, pos) ~= '"' then
        print("ERROR: " .."Expected string for key at position " .. pos)
      end
      local key = parse_string()
      skip_whitespace()
      if str:sub(pos, pos) ~= ":" then
        print("ERROR: " .."Expected ':' after key at position " .. pos)
      end
      pos = pos + 1  -- skip ':'
      skip_whitespace()
      local value = parse_value()
      obj[key] = value
      skip_whitespace()
      local delimiter = str:sub(pos, pos)
      if delimiter == "}" then
        pos = pos + 1
        break
      elseif delimiter == "," then
        pos = pos + 1
      else
        print("ERROR: " .."Expected ',' or '}' at position " .. pos)
      end
    end
    return obj
  end

  parse_value = function()
    skip_whitespace()
    local c = str:sub(pos, pos)
    if c == "{" then
      return parse_object()
    elseif c == "[" then
      return parse_array()
    elseif c == '"' then
      return parse_string()
    elseif c == "-" or c:match("%d") then
      return parse_number()
    elseif str:sub(pos, pos+3) == "true" then
      pos = pos + 4
      return true
    elseif str:sub(pos, pos+4) == "false" then
      pos = pos + 5
      return false
    elseif str:sub(pos, pos+3) == "null" then
      pos = pos + 4
      return nil
    else
      print("ERROR: " .."Unexpected character at position " .. pos)
    end
  end

  return parse_value()
end

function json_encode(val)
  -- Helper function to determine if a table is an array.
  -- Returns true if all keys are positive integers and they form a consecutive sequence.
  local function is_array(t)
    local max = 0
    local count = 0
    for k, _ in pairs(t) do
      if type(k) ~= "number" or k <= 0 or math.floor(k) ~= k then
        return false
      end
      if k > max then max = k end
      count = count + 1
    end
    return max == count
  end

  local function escape_string(s)
    s = s:gsub('\\', '\\\\')
    s = s:gsub('"', '\\"')
    s = s:gsub('\b', '\\b')
    s = s:gsub('\f', '\\f')
    s = s:gsub('\n', '\\n')
    s = s:gsub('\r', '\\r')
    s = s:gsub('\t', '\\t')
    return s
  end

  local t = type(val)
  if t == "nil" then
    return "null"
  elseif t == "boolean" then
    return val and "true" or "false"
  elseif t == "number" then
    return tostring(val)
  elseif t == "string" then
    return '"' .. escape_string(val) .. '"'
  elseif t == "table" then
    local items = {}
    -- Decide whether to encode the table as a JSON array or object.
    if is_array(val) then
      for i = 1, #val do
        table.insert(items, json_encode(val[i]))
      end
      return "[" .. table.concat(items, ",") .. "]"
    else
      for k, v in pairs(val) do
        if type(k) ~= "string" then
          error("JSON object keys must be strings")
        end
        table.insert(items, json_encode(k) .. ":" .. json_encode(v))
      end
      return "{" .. table.concat(items, ",") .. "}"
    end
  else
    error("Unsupported type: " .. t)
  end
end


local function show_table(indent, my_table)
    local i_str = ""
    for count = 1, indent do 
        i_str = i_str .. " "
    end

    for k, v in pairs(my_table) do
        if v == nil then
            print(i_str .. k .. ": null")
        elseif type(v) == "table" then
            print(i_str .. k .. ": {")
            show_table(indent + 1, v)
            print(i_str .. "}")
        elseif type(v) == "boolean" then
            print(i_str .. k .. ": " .. if v then "true" else "false" )
        else
            print(i_str .. k .. ": " .. v)
        end            
    end
end    

-- Example usage:
--local jsonString = [[
--{
--  "name": "Alice",
--  "age": 30,
--  "isMember": true,
--  "favorites": {
--    "color": "blue",
--    "numbers": [1, 2, 3]
--  },
--  "score": null
--}
--]]

local jsonString = [[
{
  "company": "Acme Corp",
  "employees": [
    {
      "id": 1,
      "name": "Alice",
      "age": 29,
      "isActive": true,
      "roles": ["developer", "team lead"],
      "contact": {
        "email": "alice@example.com",
        "phone": "+1234567890"
      },
      "address": {
        "street": "123 Maple Street",
        "city": "Springfield",
        "zipcode": "12345"
      }
    },
    {
      "id": 2,
      "name": "Bob",
      "age": 35,
      "isActive": false,
      "roles": ["designer"],
      "contact": {
        "email": "bob@example.com",
        "phone": null
      },
      "address": {
        "street": "456 Oak Avenue",
        "city": "Shelbyville",
        "zipcode": "54321"
      }
    },
    {
      "id": 3,
      "name": "Carol",
      "age": 42,
      "isActive": true,
      "roles": ["manager", "HR"],
      "contact": {
        "email": "carol@example.com",
        "phone": "+1987654321"
      },
      "address": {
        "street": "789 Pine Road",
        "city": "Capital City",
        "zipcode": "67890"
      }
    }
  ],
  "departments": {
    "development": {
      "manager": "Alice",
      "budget": 150000.75,
      "projects": [
        {
          "projectId": "dev001",
          "name": "Website Redesign",
          "deadline": "2025-06-30",
          "completed": false
        },
        {
          "projectId": "dev002",
          "name": "Mobile App",
          "deadline": "2025-12-31",
          "completed": false
        }
      ]
    },
    "design": {
      "manager": "Bob",
      "budget": 80000,
      "projects": [
        {
          "projectId": "des001",
          "name": "Brand Refresh",
          "deadline": "2025-04-15",
          "completed": true
        }
      ]
    },
    "human_resources": {
      "manager": "Carol",
      "budget": 50000,
      "initiatives": [
        "Employee Engagement",
        "Recruitment Drive",
        "Training Programs"
      ]
    }
  },
  "metadata": {
    "generatedAt": "2025-02-25T12:00:00Z",
    "version": "1.0.0",
    "notes": "This JSON represents company data including employees, departments, and project details."
  }
}
]]

-- function touch_start(number)
    print("Decoding: " .. jsonString)
    print("-----")
    local luaTable = json_decode(jsonString)

    local count = 0
    for _ in pairs(luaTable) do count = count + 1 end;
    print("count = " .. count)

    show_table(1, luaTable)
    print("-----")
    print("encoding back...")
    local reencoded = json_encode(luaTable)

    print("reencoded: " .. reencoded)
-- end

return "OK"
