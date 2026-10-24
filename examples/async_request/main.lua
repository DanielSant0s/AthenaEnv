
require "athena_network.erl"
local app = require "athenaos.app"

local term = app.Terminal{ title = "Async Network", category = "network", subtitle = "Non-blocking HTTPS download - async/await" }

term:log("Resetting IOP and loading the network driver...")
term:draw()

IOP.reset()
IOP.loadModule("padman")
IOP.loadModule("SMAP")

Network.init()
term:log(JSON.stringify(Network.getConfig()))

local req = Request()
req.verifyTLS = true
req.headers = { "upgrade-insecure-requests: 1", "sec-fetch-dest: document", "sec-fetch-mode: navigate" }

-- The transfer runs on an EE thread; the task below sleeps in await(req)
-- while the loop keeps drawing (Screen.flip() pumps the scheduler).
local downloading = true
async(function()
  req:asyncGet("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json")
  local ok, err = pcall(await, req)
  downloading = false
  if not ok then
    term:log("Download failed: " .. tostring(err))
    print("ATHENA_ASYNC_ERROR", err)
    return
  end
  local body = req:getAsyncData() or ""
  term:log("Done: " .. #body .. " bytes transferred")
  print("ATHENA_ASYNC_DONE", #body)
  term:log(body:sub(1, 300))
  Network.deinit()
end)

local pad = Pads.get()
while true do
  pad:update()
  if term:homeRequested(pad) then break end
  term.status = downloading and (req:getAsyncSize() .. " bytes") or ""
  term:draw()
end

app.exitToShell()
