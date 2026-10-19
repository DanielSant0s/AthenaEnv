-- {"name": "Playground", "author": "Daniel Santos", "version": "10182026", "file": "playground.lua", "category": "system", "blurb": "Vector and matrix math API checks.", "exit": "TRIANGLE"}

local app = require "athenaos.app"

local term = app.Terminal{ title = "Playground", category = "system", subtitle = "Vector2, Vector3, Matrix4 and native calls",
                           hints = { { "D-pad", "Log input" } } }

local v3, v4 = Vector3(15.0, 15.0, 15.0), Vector3(30.0, 30.0, 30.0)
term:log("Vector3 " .. tostring(v3 + v4))
term:log("Vector3 dist " .. v3:dist(v4))

local v1, v2 = Vector2(15.0, 15.0), Vector2(30.0, 30.0)
term:log("Vectors distance is " .. v1:dist(v2))
term:log("Vectors add is " .. tostring(v1 + v2))
term:log("Vectors sub is " .. tostring(v1 - v2))
term:log("Vectors mul is " .. tostring(v1 * v2))
term:log("Vectors div is " .. tostring(v1 / v2))

local test_matrix = Matrix4()
term:log("Matrix4 " .. tostring(test_matrix))

local m1 = Matrix4()
if m1 == test_matrix then term:log("Matrix equal check for equal matrix values") end

for i = 1, #test_matrix do test_matrix[i] = math.random() end
term:log("Matrix4 " .. tostring(test_matrix))

if m1 ~= test_matrix then term:log("Matrix equal check for different matrix values") end

-- Calling a kernel function by its exported symbol, with a Buffer out-param.
local OsdConfigParam = Buffer(4)
System.nativeCall(System.findRelocObject("GetOsdConfigParam"), { { type = System.T_BUFFER, value = OsdConfigParam } })
term:log(string.format("GetOsdConfigParam: %u", OsdConfigParam:getU32(0)))

local pad = Pads.get()

while true do
  pad:update()
  if term:homeRequested(pad) then break end

  if pad:justPressed(Pads.LEFT) then term:log("DPad Left button was just pressed") end
  if pad:justPressed(Pads.RIGHT) then term:log("DPad Right button was just pressed") end

  term:draw()
end

app.exitToShell()
