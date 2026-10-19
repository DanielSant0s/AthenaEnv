-- JSON.parse / JSON.stringify for AthenaEnv Lua (parity with the JS global).
-- Embedded into the runtime by scripts/lua2c.py; keep it dependency free.
--
-- Objects decode to tables, arrays to 1-based sequences, null to JSON.null.
-- stringify() treats a table as an array when it has a contiguous 1..n
-- sequence and no other keys (an empty table encodes as {} unless it is
-- tagged with JSON.array()).

local JSON = {}

local null = setmetatable({}, { __tostring = function() return "null" end })
JSON.null = null

local array_mt = {}
function JSON.array(t) return setmetatable(t or {}, array_mt) end

local escapes = { ['"'] = '\\"', ['\\'] = '\\\\', ['\b'] = '\\b', ['\f'] = '\\f',
                  ['\n'] = '\\n', ['\r'] = '\\r', ['\t'] = '\\t' }

local function encode_string(s)
  return '"' .. s:gsub('[%c"\\]', function(c)
    return escapes[c] or string.format("\\u%04x", c:byte())
  end) .. '"'
end

local function is_array(t)
  if getmetatable(t) == array_mt then return true end
  local n = 0
  for k in pairs(t) do
    if math.type(k) ~= "integer" or k < 1 then return false end
    n = n + 1
  end
  for i = 1, n do
    if t[i] == nil then return false end
  end
  return n > 0
end

local encode

local function encode_value(v, indent, depth, seen)
  local kind = type(v)
  if v == nil or v == null then return "null" end
  if kind == "boolean" then return tostring(v) end
  if kind == "number" then
    if v ~= v or v == math.huge or v == -math.huge then return "null" end
    if math.type(v) == "integer" then return string.format("%d", v) end
    return string.format("%.7g", v)
  end
  if kind == "string" then return encode_string(v) end
  if kind == "table" then return encode(v, indent, depth, seen) end
  error("JSON.stringify: cannot encode a " .. kind, 0)
end

encode = function(t, indent, depth, seen)
  if seen[t] then error("JSON.stringify: circular reference", 0) end
  seen[t] = true
  local pad, inner, sep = "", "", ","
  if indent then
    pad = "\n" .. string.rep(indent, depth)
    inner = "\n" .. string.rep(indent, depth + 1)
  end
  local parts = {}
  local out
  if is_array(t) then
    for i = 1, #t do
      parts[i] = inner .. encode_value(t[i], indent, depth + 1, seen)
    end
    out = #parts > 0 and ("[" .. table.concat(parts, sep) .. pad .. "]") or "[]"
  else
    local keys = {}
    for k in pairs(t) do
      if type(k) == "string" or type(k) == "number" then keys[#keys + 1] = tostring(k) end
    end
    table.sort(keys)
    local colon = indent and ": " or ":"
    for i, k in ipairs(keys) do
      local v = t[k]
      if v == nil then v = t[tonumber(k)] end
      if type(v) ~= "function" then
        parts[#parts + 1] = inner .. encode_string(k) .. colon .. encode_value(v, indent, depth + 1, seen)
      end
    end
    out = #parts > 0 and ("{" .. table.concat(parts, sep) .. pad .. "}") or "{}"
  end
  seen[t] = nil
  return out
end

-- JSON.stringify(value [, indent]) -- indent: number of spaces or a string
function JSON.stringify(value, indent)
  if type(indent) == "number" then indent = string.rep(" ", indent) end
  if indent == "" then indent = nil end
  return encode_value(value, indent, 0, {})
end

-- decoder -----------------------------------------------------------------

local function decode_error(str, pos, msg)
  local line, col = 1, 1
  for i = 1, pos - 1 do
    if str:sub(i, i) == "\n" then line, col = line + 1, 1 else col = col + 1 end
  end
  error(string.format("JSON.parse: %s at line %d col %d", msg, line, col), 0)
end

local function skip(str, pos)
  return str:find("[^ \t\r\n]", pos) or #str + 1
end

local parse

local function parse_string(str, pos)
  local buf, i = {}, pos + 1
  while true do
    local c = str:sub(i, i)
    if c == "" then decode_error(str, pos, "unterminated string") end
    if c == '"' then return table.concat(buf), i + 1 end
    if c == "\\" then
      local e = str:sub(i + 1, i + 1)
      local map = { b = "\b", f = "\f", n = "\n", r = "\r", t = "\t", ['"'] = '"', ["\\"] = "\\", ["/"] = "/" }
      if map[e] then
        buf[#buf + 1] = map[e]
        i = i + 2
      elseif e == "u" then
        local hex = str:sub(i + 2, i + 5)
        local code = tonumber(hex, 16)
        if not code then decode_error(str, i, "bad unicode escape") end
        if code >= 0xD800 and code <= 0xDBFF and str:sub(i + 6, i + 7) == "\\u" then
          local low = tonumber(str:sub(i + 8, i + 11), 16)
          if low and low >= 0xDC00 and low <= 0xDFFF then
            code = 0x10000 + (code - 0xD800) * 0x400 + (low - 0xDC00)
            i = i + 6
          end
        end
        buf[#buf + 1] = utf8.char(code)
        i = i + 6
      else
        decode_error(str, i, "bad escape")
      end
    else
      local j = str:find('["\\]', i)
      if not j then decode_error(str, pos, "unterminated string") end
      buf[#buf + 1] = str:sub(i, j - 1)
      i = j
    end
  end
end

local function parse_number(str, pos)
  local s, e = str:find("^-?%d+%.?%d*[eE]?[-+]?%d*", pos)
  if not s then decode_error(str, pos, "bad number") end
  local text = str:sub(s, e)
  local n = math.tointeger(tonumber(text)) or tonumber(text)
  if n == nil then decode_error(str, pos, "bad number") end
  return n, e + 1
end

parse = function(str, pos)
  pos = skip(str, pos)
  local c = str:sub(pos, pos)
  if c == "{" then
    local obj = {}
    pos = skip(str, pos + 1)
    if str:sub(pos, pos) == "}" then return obj, pos + 1 end
    while true do
      if str:sub(pos, pos) ~= '"' then decode_error(str, pos, "expected key") end
      local key
      key, pos = parse_string(str, pos)
      pos = skip(str, pos)
      if str:sub(pos, pos) ~= ":" then decode_error(str, pos, "expected ':'") end
      obj[key], pos = parse(str, pos + 1)
      pos = skip(str, pos)
      local d = str:sub(pos, pos)
      if d == "}" then return obj, pos + 1 end
      if d ~= "," then decode_error(str, pos, "expected ',' or '}'") end
      pos = skip(str, pos + 1)
    end
  elseif c == "[" then
    local arr = JSON.array()
    pos = skip(str, pos + 1)
    if str:sub(pos, pos) == "]" then return arr, pos + 1 end
    while true do
      arr[#arr + 1], pos = parse(str, pos)
      pos = skip(str, pos)
      local d = str:sub(pos, pos)
      if d == "]" then return arr, pos + 1 end
      if d ~= "," then decode_error(str, pos, "expected ',' or ']'") end
      pos = pos + 1
    end
  elseif c == '"' then
    return parse_string(str, pos)
  elseif c == "-" or c:match("%d") then
    return parse_number(str, pos)
  elseif str:sub(pos, pos + 3) == "true" then
    return true, pos + 4
  elseif str:sub(pos, pos + 4) == "false" then
    return false, pos + 5
  elseif str:sub(pos, pos + 3) == "null" then
    return null, pos + 4
  end
  decode_error(str, pos, "unexpected '" .. c .. "'")
end

function JSON.parse(str)
  if type(str) ~= "string" then error("JSON.parse expects a string", 2) end
  local value, pos = parse(str, 1)
  pos = skip(str, pos)
  if pos <= #str then decode_error(str, pos, "trailing characters") end
  return value
end

return JSON
