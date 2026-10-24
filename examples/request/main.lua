
require "athena_network.erl" -- registers the NETMAN/SMAP drivers loaded below
local app = require "athenaos.app"

local term = app.Terminal{ title = "Network demo", category = "network", subtitle = "HTTPS GET with TLS verification" }

term:log("Resetting IOP and loading the network driver...")
term:draw()

IOP.reset()
IOP.loadModule("padman") -- keep the pad driver after the reset so Home works
IOP.loadModule("SMAP")

Network.init()

term:log(JSON.stringify(Network.getConfig()))
term:log("GET https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json")
term:draw()

local req = Request()
req.verifyTLS = true
req.followRedirects = true
req.headers = { "upgrade-insecure-requests: 1", "sec-fetch-dest: document", "sec-fetch-mode: navigate" }

local ok, result = pcall(req.get, req, "https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json")
if ok then
  print("ATHENA_HTTP_STATUS", result.status_code, #result.text)
  term:log("HTTP " .. result.status_code .. " - " .. #result.text .. " bytes")
  -- the body is JSON: decode it and list a few entries
  local okj, data = pcall(JSON.parse, result.text)
  if okj and type(data) == "table" then
    local shown = 0
    for _, entry in ipairs(data) do
      if type(entry) == "table" and entry.name then
        term:log(" - " .. tostring(entry.name))
        shown = shown + 1
        if shown == 8 then break end
      end
    end
  else
    term:log(result.text)
  end
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
