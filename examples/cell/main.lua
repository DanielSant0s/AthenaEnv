
local app = require "athenaos.app"
local COLORS, rgba = app.COLORS, app.rgba

-- Menus use the standard chrome; gameplay owns the screen.
local ui = app.AppUI{ title = "Cell.io", category = "games" }
local MENU = { "Play", "Options", "Quit" }

Screen.setFrameCounter(true)
Screen.setVSync(false)

local transparent = Color.new(255, 255, 255, 40)
local purple = Color.new(64, 0, 128)
local white = Color.new(255, 255, 255)

local font = Font("fonts/LEMONMILK-Regular.otf")
font.scale = 2

local MAIN_MENU, RUN_GAME, GAME_OVER = 0, 1, 3

local game_state = MAIN_MENU
local player = { x = 0, y = 0, r = 25, color = Color.new(128, 100, 255) }
local camera = { x = -320, y = -224 }
local enemies = {}
local random = math.random
math.randomseed(os.millis())

local function world2Screen(rect)
  return rect.x - camera.x, rect.y - camera.y
end

local function drawCell(x, y, radius, color, hollow)
  Draw.circle(x, y, radius, color)
  if hollow then Draw.circle(x, y, radius, white, false) end
end

local function circleCircleColl(c1, c2)
  local dx, dy = c1.x - c2.x, c1.y - c2.y
  return math.sqrt(dx * dx + dy * dy) <= c1.r + c2.r
end

local function startGame()
  game_state = RUN_GAME
  player.x, player.y, player.r = 0, 0, 25
  camera.x, camera.y = -320, -224

  local enemies_qt = random(8, 15)
  while enemies_qt > #enemies do
    local enemy = { color = Color.new(random(0, 255), random(0, 255), random(0, 255)),
                    x = random(0, 640), y = random(0, 448), r = random(5, 75) }
    local found_collision = circleCircleColl(enemy, player)
    for _, other in ipairs(enemies) do
      if circleCircleColl(enemy, other) then found_collision = true end
    end
    if not found_collision then enemies[#enemies + 1] = enemy end
  end
  font.scale = 1
end

local function gameOver()
  game_state = GAME_OVER
  font.scale = 2
end

local main_menu_ptr = 1
local pad = Pads.get()

Screen.clearColor(purple)
pad:setEventHandler()

Screen.display(function()
  if ui:homeRequested(pad) then app.exitToShell() end

  if game_state == MAIN_MENU then
    if pad:justPressed(Pads.UP) and main_menu_ptr > 1 then main_menu_ptr = main_menu_ptr - 1 end
    if pad:justPressed(Pads.DOWN) and main_menu_ptr < #MENU then main_menu_ptr = main_menu_ptr + 1 end

    if pad:justPressed(Pads.CROSS) then
      if main_menu_ptr == 1 then startGame()
      elseif main_menu_ptr == 3 then app.exitToShell() end
    end

    if game_state == MAIN_MENU then
      ui:background()
      local x, y = ui.content.x, ui.content.y

      font.scale = 1.6
      font.color = player.color
      font:print(x, y + 10, "Cell.io")

      for i, label in ipairs(MENU) do
        local cy = y + 130 + (i - 1) * 44
        local selected = i == main_menu_ptr
        if selected then
          Draw.rect(x, cy - 18, 260, 36, rgba(255, 255, 255, 0.10))
          Draw.rect(x, cy - 18, 3, 36, ui:accent())
        end
        ui:textV(x + 18, cy, label, 0.7, selected and COLORS.text or COLORS.textDim)
      end

      ui:chrome({ { "D-pad", "Select" }, { "cross", "Confirm" } })
    end

  elseif game_state == RUN_GAME then
    for i = -1500, 1500, 50 do
      local sx, sy = world2Screen{ x = i, y = -1500 }
      if sx > 0 and sx < 640 then
        local ex, ey = world2Screen{ x = i, y = 1500 }
        Draw.line(sx, sy, ex, ey, transparent)
      end

      sx, sy = world2Screen{ x = -1500, y = i }
      if sy > 0 and sy < 448 then
        local ex, ey = world2Screen{ x = 1500, y = i }
        Draw.line(sx, sy, ex, ey, transparent)
      end
    end

    if pad:pressed(Pads.LEFT) and player.x > -1500 then player.x, camera.x = player.x - 4, camera.x - 4 end
    if pad:pressed(Pads.RIGHT) and player.x < 1500 then player.x, camera.x = player.x + 4, camera.x + 4 end
    if pad:pressed(Pads.UP) and player.y > -1500 then player.y, camera.y = player.y - 4, camera.y - 4 end
    if pad:pressed(Pads.DOWN) and player.y < 1500 then player.y, camera.y = player.y + 4, camera.y + 4 end

    local i = 1
    while i <= #enemies do
      local enemy, eaten = enemies[i], false
      local ex, ey = world2Screen(enemy)
      if ex + enemy.r > 0 and ex - enemy.r < 640 and ey + enemy.r > 0 and ey - enemy.r < 448 then
        drawCell(ex, ey, enemy.r, enemy.color, false)
        if circleCircleColl(enemy, player) then
          if enemy.r < player.r then
            player.r = player.r + enemy.r / 2
            table.remove(enemies, i)
            eaten = true
            if #enemies == 0 then gameOver() end
          elseif enemy.r > player.r then
            gameOver()
          end
        end
      end
      if not eaten then i = i + 1 end
    end

    local px, py = world2Screen(player)
    drawCell(px, py, player.r, player.color, true)

    ui:hud({ Screen.getFPS(360) .. " FPS",
             string.format("Size %d  |  %d cells left", math.floor(player.r + 0.5), #enemies) })

  elseif game_state == GAME_OVER then
    font.scale = 2
    font.color = Color.new(128, 128, 128)
    font.align = Font.ALIGN_HCENTER
    font:print(ui.W / 2, 190, #enemies == 0 and "VICTORY!" or "GAME OVER")
    font.align = Font.ALIGN_NONE
    ui:chrome({ { "cross", "Menu" } })

    if pad:justPressed(Pads.CROSS) then
      game_state = MAIN_MENU
      enemies = {}
    end
  end
end)
