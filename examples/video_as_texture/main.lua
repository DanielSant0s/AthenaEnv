--
-- Video frames as an Image: rotate, scale and draw the decoded texture
-- several times per frame. Place a video.m2v next to this script.

local app = require "athenaos.app"

local ui = app.AppUI{ title = "Texture video", category = "media" }

local video = Video("video.m2v")
video.loop = true
video:play()

print("Video as Texture Demo: " .. video.width .. "x" .. video.height)

local angle, scale, scaleDir = 0, 1.0, 0.01

-- A reference to the decoder's texture, not a copy: it updates in place.
local frame = video.frame

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

local pad = Pads.get()

while true do
  pad:update()
  if ui:homeRequested(pad) then break end

  ui:background()
  video:update()

  if frame then
    frame.width = video.width * scale
    frame.height = video.height * scale
    frame.angle = angle
    frame:draw(320 - frame.width / 2, 224 - frame.height / 2)

    -- thumbnails in the corners
    frame.width, frame.height, frame.angle = 80, 60, 0
    frame:draw(10, 10)
    frame:draw(550, 10)
    frame:draw(10, 380)
    frame:draw(550, 380)
  end

  angle = angle + 0.01
  if angle >= 360 then angle = 0 end

  scale = scale + scaleDir
  if scale > 1.5 or scale < 0.5 then scaleDir = -scaleDir end

  ui:autoChrome(pad)
  Screen.flip()
end

video:free()
app.exitToShell()
