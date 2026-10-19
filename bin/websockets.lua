-- {"name": "Websockets", "author": "Daniel Santos", "version": "10182026", "file": "websockets.lua", "category": "network", "blurb": "Secure WebSocket echo round-trip.", "exit": "TRIANGLE"}

require "athena_network.erl"
local app = require "athenaos.app"

local term = app.Terminal{ title = "Websockets", category = "network", subtitle = "wss://echo.websocket.org" }

term:log("Resetting IOP and loading the network driver...")
term:draw()

IOP.reset()
IOP.loadModule("padman")
IOP.loadModule("SMAP")

Network.init()

term:log(JSON.stringify(Network.getConfig()))
term:log("Connecting to wss://echo.websocket.org ...")
term:draw()

local ws = WebSocket("wss://echo.websocket.org")
term:log(ws:recv() or "(no message)") -- server greeting
ws:send("hello from Lua", false)       -- text frame
term:log("echo: " .. (ws:recv() or "(no reply)"))
ws:close()

Network.deinit()

local pad = Pads.get()
while true do
  pad:update()
  if term:homeRequested(pad) then break end
  term:draw()
end

app.exitToShell()
