-- {"name": "Audio player", "author": "Daniel Santos", "version": "10182026", "icon": "sap_icon.png", "file": "sound.lua", "category": "media", "blurb": "Stream OGG and WAV tracks with seek and playlist.", "exit": "TRIANGLE"}

local app = require "athenaos.app"
local COLORS, rgba = app.COLORS, app.rgba

local ui = app.AppUI{ title = "Audio player", category = "media",
  hints = { { "START", "Play/Pause" }, { "L1", "Prev" }, { "R1", "Next" }, { "D-pad", "Seek" }, { "square", "Rewind" } } }

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
Sound.setVolume(100)

local sounds = {}
for _, entry in ipairs(System.listDir("/sounds")) do
  local name = entry.name
  local base = name:match("^(.*)%.ogg$") or name:match("^(.*)%.wav$")
  if base then sounds[#sounds + 1] = { name = base, file = name } end
end
table.sort(sounds, function(a, b) return a.name < b.name end)

local current = 1
local track = Sound.Stream("sounds/" .. sounds[current].file)
local duration = track.length
track:play()

local pad = Pads.get()

local icons = { play = Image("amp/play.png"), pause = Image("amp/pause.png"),
                next = Image("amp/next.png"), back = Image("amp/back.png") }
for _, icon in pairs(icons) do
  icon.width = icon.width / 4
  icon.height = icon.height / 4
  icon.filter = LINEAR
end

local function msToSecondMin(ms)
  local minutes = ms // 60000
  local seconds = math.floor(ms / 1000 + 0.5) - minutes * 60
  return string.format("%d:%02d", minutes, seconds)
end

local function switchTrack(index)
  track:pause()
  current = (index - 1) % #sounds + 1
  local new_track = Sound.Stream("sounds/" .. sounds[current].file)
  duration = new_track.length
  new_track:play()
  local old = track
  track = new_track
  old:free()
end

local function drawIcon(img, cx, cy, color)
  img.color = color
  img:draw(cx - img.width / 2, cy - img.height / 2)
end

while true do
  pad:update()
  if ui:homeRequested(pad) then break end

  if pad:justPressed(Pads.START) then
    if track:playing() then track:pause() else track:play() end
  end

  if pad:justPressed(Pads.L1) then switchTrack(current - 1) end
  if pad:justPressed(Pads.R1) then switchTrack(current + 1) end

  if pad:justPressed(Pads.RIGHT) and track:playing() then
    track.position = track.position + 5000
  elseif pad:justPressed(Pads.LEFT) and track:playing() then
    track.position = math.max(0, track.position - 5000)
  end

  if pad:justPressed(Pads.SQUARE) then track:rewind() end

  ui:background()
  local c = ui.content

  -- Playlist
  local listW = 200
  ui:panel(c.x, c.y, listW, c.h)
  ui:heading(c.x + 16, c.y + 20, "Playlist - " .. #sounds .. " tracks")
  for i, s in ipairs(sounds) do
    local ry = c.y + 40 + (i - 1) * 30
    if i == current then
      Draw.rect(c.x + 8, ry, listW - 16, 26, rgba(255, 255, 255, 0.10))
      Draw.rect(c.x + 8, ry, 3, 26, ui:accent())
    end
    ui:textV(c.x + 20, ry + 13, ui:fit(s.name, 0.46, listW - 40), 0.46, i == current and COLORS.text or COLORS.textDim)
  end

  -- Now playing
  local px, pw = c.x + listW + 16, c.w - listW - 16
  local cx = math.floor(px + pw / 2 + 0.5)
  ui:panel(px, c.y, pw, c.h)
  ui:heading(px + 20, c.y + 20, track:playing() and "Now playing" or "Paused")
  ui:textV(cx, c.y + 110, ui:fit(sounds[current].name, 0.8, pw - 40), 0.8, COLORS.text, Font.ALIGN_HCENTER)

  local position = track.position
  local t = duration > 0 and math.min(position / duration, 1) or 0
  local bx, bw, by = px + 30, pw - 60, c.y + 170
  Draw.rect(bx, by, bw, 6, rgba(255, 255, 255, 0.15))
  Draw.rect(bx, by, bw * t, 6, ui:accent())
  Draw.circle(bx + bw * t, by + 3, 7, COLORS.text, true)
  ui:textV(bx, by + 24, msToSecondMin(position), 0.44, COLORS.textDim)
  ui:textV(bx + bw, by + 24, msToSecondMin(duration), 0.44, COLORS.textDim, Font.ALIGN_RIGHT)

  local iy = c.y + 260
  drawIcon(icons.back, cx - 90, iy, rgba(255, 255, 255, 0.8))
  Draw.circle(cx, iy, 30, ui:accent(), true)
  drawIcon(track:playing() and icons.pause or icons.play, cx, iy, COLORS.text)
  drawIcon(icons.next, cx + 90, iy, rgba(255, 255, 255, 0.8))

  ui:chrome()
  Screen.flip()
end

track:pause()
track:free()
app.exitToShell()
