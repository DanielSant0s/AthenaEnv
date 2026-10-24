--
-- Place a video.m2v next to this script. MPEG2 elementary streams can be
-- made with: ffmpeg -i input.mp4 -vf scale=320:240 -c:v mpeg2video -b:v 2000k video.m2v

local app = require "athenaos.app"

local ui = app.AppUI{ title = "Video Playback", category = "media", hints = { { "square", "Loop" } } }

-- Touching the Video global loads athena_mpeg_lua.erl on demand.
local video = Video("video.m2v")
video.loop = false
video:play()

print(string.format("ATHENA_VIDEO %dx%d @ %.2f fps", video.width, video.height, video.fps))

local pad = Pads.get()
local black = Color.new(0, 0, 0)

while not video.ended do
  pad:update()
  if ui:homeRequested(pad) then break end
  if pad:justPressed(Pads.SQUARE) then video.loop = not video.loop end

  Screen.clear(black)
  video:update()
  video:draw(0, 0, video.width, video.height)

  -- Chrome hides itself after a few idle seconds; any button brings it back.
  ui.status = video.loop and "Loop ON" or ""
  ui:autoChrome(pad)

  Screen.flip()
end

video:free()
app.exitToShell()
