-- {"name": "Sockets demo", "author": "Daniel Santos", "version": "10182026", "file": "sockets.lua", "category": "network", "blurb": "Raw TCP socket talking HTTP to a web server.", "exit": "TRIANGLE"}

require "athena_network.erl"
local app = require "athenaos.app"

local term = app.Terminal{ title = "Sockets demo", category = "network", subtitle = "Raw TCP socket to www.google.com:80" }

term:log("Resetting IOP and loading the network driver...")
term:draw()

IOP.reset()
IOP.loadModule("padman")
IOP.loadModule("SMAP")

Network.init()

local cfg = Network.getConfig()
term:log("IP Address: " .. cfg.ip)
term:log("Netmask: " .. cfg.netmask)
term:log("Gateway: " .. cfg.gateway)
term:log("DNS: " .. cfg.dns)
term:draw()

local s = Socket(Socket.AF_INET, Socket.SOCK_STREAM)
local host = Network.getHostbyName("www.google.com")
term:log("www.google.com -> " .. host)
term:draw()

s:connect(host, 80)
s:send("GET / HTTP/1.1\r\nHost:www.google.com\r\n\r\n")
term:log(s:recv(1024))
s:close()

Network.deinit()

local pad = Pads.get()
while true do
  pad:update()
  if term:homeRequested(pad) then break end
  term:draw()
end

app.exitToShell()
