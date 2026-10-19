-- AthenaOS shared UI helpers: text layout, clock and layout metrics.
-- Lua port of athenaos/ui.js.

local gfx = require "athenaos.gfx"
local textTop, round = gfx.textTop, gfx.round

local M = {}

-- Layout -------------------------------------------------------------------

-- Left/right margin shared by the shell and every app (48px at 640 wide).
function M.margin(width) return round(width * 0.075) end

M.BAR_H = 26     -- top app/status bar
M.FOOTER_H = 34  -- bottom hint bar in apps

-- Text ---------------------------------------------------------------------

function M.text(font, x, y, str, scale, color, align)
  font.scale = scale
  font.color = color
  font.align = align or Font.ALIGN_NONE
  font:print(x, y, str)
  font.align = Font.ALIGN_NONE
end

-- Same as text(), but vertically centered on cy.
function M.textV(font, x, cy, str, scale, color, align)
  M.text(font, round(x), textTop(cy, scale), str, scale, color, align)
end

function M.measure(font, str, scale)
  font.scale = scale
  return font:getTextSize(str).width
end

function M.fit(font, str, scale, maxW)
  if M.measure(font, str, scale) <= maxW then return str end
  while #str > 1 and M.measure(font, str .. "..", scale) > maxW do str = str:sub(1, -2) end
  return str:match("^%s*(.-)%s*$") .. ".."
end

function M.wrap(font, str, scale, maxW)
  local lines, line = {}, ""
  for word in str:gmatch("%S+") do
    local nxt = line ~= "" and (line .. " " .. word) or word
    if line ~= "" and M.measure(font, nxt, scale) > maxW then
      lines[#lines + 1] = line
      line = word
    else
      line = nxt
    end
  end
  if line ~= "" then lines[#lines + 1] = line end
  return lines
end

-- Clock --------------------------------------------------------------------

local DAYS = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" }
local MONTHS = { "January", "February", "March", "April", "May", "June", "July",
                 "August", "September", "October", "November", "December" }

function M.pad2(n) return (n < 10 and "0" or "") .. n end

-- The console clock shifted by the user's offset, as an os.date("!*t") table.
function M.now(settings)
  return os.date("!*t", os.time() + (settings.utcOffset or 0) * 3600)
end

function M.clockText(settings, d)
  d = d or M.now(settings)
  local h, m = d.hour, M.pad2(d.min)
  if settings.clock24 then return M.pad2(h) .. ":" .. m end
  return string.format("%d:%s %s", (h % 12 == 0) and 12 or (h % 12), m, h < 12 and "AM" or "PM")
end

function M.dateText(settings, d)
  d = d or M.now(settings)
  return string.format("%s, %s %d", DAYS[d.wday], MONTHS[d.month], d.day)
end

return M
