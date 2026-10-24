local app = require "athenaos.app"

-- Gameplay owns the screen; the standard chrome shows while paused.
local ui = app.AppUI{ title = "Pong", category = "games", hints = { { "START", "Play/Pause" }, { "D-pad", "Move" } } }

Sound.setVolume(100)
local main_sfx_channel = Sound.findChannel()

local sounds = {
  unpause = Sound.Sfx("pong/unpause.adp"),
  score = Sound.Sfx("pong/score.adp"),
  over = Sound.Sfx("pong/over.adp"),
}

local screen = Screen.getMode()

local dark_gray = Color.new(32, 32, 32)
local gray = Color.new(64, 64, 64)
local white = Color.new(255, 255, 255)

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

local function rectRect(a, b)
  return a.x < b.x + b.w and a.x + a.w > b.x and a.y < b.y + b.h and a.h + a.y > b.y
end

local font = Font("fonts/minecraft.ttf")
font.color = gray

local bigfont = Font("fonts/minecraft.ttf")
bigfont.color = white
bigfont.scale = 2.0

-- Paddle -------------------------------------------------------------------

local Paddle = {}
Paddle.__index = Paddle

function Paddle.new(x, y)
  local p = setmetatable({ x = x or 285.0, y = y or 400.0, w = 70, h = 10, speed = 4, color = white }, Paddle)
  p.bak_x, p.bak_y, p.bak_speed = p.x, p.y, p.speed
  return p
end

function Paddle:moveLeft() if self.x > 0 then self.x = self.x - self.speed end end
function Paddle:moveRight() if self.x < screen.width - self.w then self.x = self.x + self.speed end end
function Paddle:reset() self.x, self.y, self.speed = self.bak_x, self.bak_y, self.bak_speed end
function Paddle:draw() Draw.rect(self.x, self.y, self.w, self.h, self.color) end

-- Ball ---------------------------------------------------------------------

local DIRECTION_UP, DIRECTION_DOWN, DIRECTION_LEFT, DIRECTION_RIGHT = -1, 1, -1, 1

local Ball = {}
Ball.__index = Ball

function Ball.new(size)
  size = size or 16
  local b = setmetatable({ size = size, w = size, h = size, speed = 2, color = white }, Ball)
  b.x, b.y = 320 - size / 2, 224 - size / 2
  b.bak_x, b.bak_y, b.bak_speed = b.x, b.y, b.speed
  return b
end

function Ball:move()
  if self.x >= screen.width - self.size then
    self.h_direction = DIRECTION_LEFT
  elseif self.x <= 0 then
    self.h_direction = DIRECTION_RIGHT
  end
  if self.y <= 0 then self.v_direction = DIRECTION_DOWN end

  self.x = self.x + self.h_direction * self.speed
  self.y = self.y + self.v_direction * self.speed
end

function Ball:collide(paddle)
  local col = rectRect(self, paddle)
  if col then self.v_direction = DIRECTION_UP end
  return col
end

function Ball:escape() return self.y >= screen.height end

function Ball:randomDirection()
  self.v_direction = math.random() < 0.5 and -1 or 1
  self.h_direction = math.random() < 0.5 and -1 or 1
end

function Ball:reset()
  self.x, self.y, self.speed = self.bak_x, self.bak_y, self.bak_speed
  self:randomDirection()
end

function Ball:draw() Draw.rect(self.x, self.y, self.size, self.size, self.color) end

-- Game ---------------------------------------------------------------------

math.randomseed(os.millis())

local player = Paddle.new()
local ball = Ball.new()
ball:randomDirection()

local pad = Pads.get()
local paused, game_over = true, false
local score, score_threshold = 0, 5

local function banner(text)
  local size = bigfont:getTextSize(text)
  return { text = text, width = size.width, height = size.height * bigfont.scale }
end
local over, pause = banner("GAME OVER"), banner("PRESS START")

while true do
  pad:update()
  if ui:homeRequested(pad) then app.exitToShell() end

  Screen.clear(dark_gray)

  if pad:justPressed(Pads.START) then
    paused = not paused
    if game_over then
      ball:reset()
      player:reset()
      score, score_threshold = 0, 5
    end
    game_over = false
    sounds.unpause:play(main_sfx_channel)
  end

  if not paused then
    if pad:pressed(Pads.LEFT) or pad.rx < -50 then player:moveLeft() end
    if pad:pressed(Pads.RIGHT) or pad.rx > 50 then player:moveRight() end

    if ball:collide(player) then
      score = score + 1
      sounds.score:play(main_sfx_channel)
      if score > score_threshold then
        score_threshold = score_threshold * 2
        ball.speed = ball.speed + 1
        player.speed = player.speed + 1
      end
    end

    if ball:escape() then
      game_over, paused = true, true
      sounds.over:play(main_sfx_channel)
    end

    ball:move()
  end

  local b = game_over and over or (paused and pause or nil)
  if b then bigfont:print(320 - b.width / 2, 224 - b.height / 2, b.text) end

  local s = tostring(score)
  font:print(600 - font:getTextSize(s).width, 10, s)
  player:draw()
  ball:draw()

  if paused then ui:chrome() end

  Screen.flip()
end
