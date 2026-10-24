-- Hello world: the starting point of the AthenaEnv Lua package.
-- athena_lua.elf runs the script named by default_script in athena.ini -- this
-- one. Edit it and run athena_lua.elf again. The API is documented in docs/,
-- and the demos package has a sample of every module.

local font = Font("default")
local pad = Pads.get()
local background = Color.new(16, 20, 36)

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

local presses = 0

while true do
  pad:update()
  if pad:justPressed(Pads.CROSS) then presses = presses + 1 end

  Screen.clear(background)

  font.scale = 1.0
  font.color = Color.new(128, 128, 128)
  font:print(40, 40, "Hello world!")

  font.scale = 0.6
  font.color = Color.new(90, 110, 128)
  font:print(40, 90, "Edit main.lua to make it yours.")
  font:print(40, 120, ("Cross pressed %d time%s."):format(presses, presses == 1 and "" or "s"))

  Screen.flip()
end
