-- {"name": "Header request", "author": "Daniel Santos", "version": "10192026", "file": "request_header.lua", "category": "network", "blurb": "HTTP HEAD request and response headers.", "exit": "TRIANGLE"}

require "athena_network.erl" -- registers the NETMAN/SMAP drivers loaded below
local app = require "athenaos.app"

local term = app.Terminal{ title = "Header request", category = "network", subtitle = "HTTP HEAD request" }

term:log("Resetting IOP and loading the network driver...")
term:draw()

IOP.reset()
IOP.loadModule("padman") -- keep the pad driver after the reset so Home works
IOP.loadModule("SMAP")

Network.init()

term:log(JSON.stringify(Network.getConfig()))
term:log("HEAD https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json")
term:draw()

local req = Request()
req.followRedirects = true
req.headers = { "upgrade-insecure-requests: 1", "sec-fetch-dest: document", "sec-fetch-mode: navigate" }

local ok, result = pcall(req.head, req, "https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json")
if ok then
  print("ATHENA_HTTP_STATUS", result.status_code, result.headers and #result.headers)
  term:log("HTTP " .. result.status_code)
  -- a HEAD response has no body: show the response headers instead
  for line in (result.headers or result.text):gmatch("[^\r\n]+") do term:log(line) end
else
  print("ATHENA_HTTP_ERROR", result)
  term:log("Request failed: " .. tostring(result))
end

Network.deinit()

local pad = Pads.get()
while true do
  pad:update()
  if term:homeRequested(pad) then break end
  term:draw()
end

app.exitToShell()
