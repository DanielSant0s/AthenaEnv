---@meta
-- AthenaEnv Lua runtime -- LuaLS (lua-language-server) definitions.
--
-- Editor-only: nothing here runs. `.luarc.json` at the repository root adds
-- this folder to the workspace library. The JavaScript equivalents live in
-- types/*.d.ts; docs/LUA.md lists the differences between the two runtimes.
--
-- Conventions: tables and collections are 1-based; hardware ids, texture ids,
-- byte offsets, sprite records and material indices stay 0-based.

-- ---------------------------------------------------------------------------
-- Runtime prelude
-- ---------------------------------------------------------------------------

---@class Task
---@field done boolean?
---@field result table? packed return values once done
local Task = {}
---@return boolean
function Task:ready() end

---Runs `fn` as a coroutine task. The main loop resumes it whenever what it awaits is ready.
---@param fn fun(...): ...
---@return Task
function async(fn, ...) end

---Inside `async`: waits for `ms` milliseconds, until a function returns truthy,
---or until an object's `:ready()` returns truthy (Task, Request...). Errors raised
---by `ready()` are re-raised in the task.
---@param cond number|fun(): any|{ ready: fun(self): any }
---@return any
function await(cond) end

---`await(ms)`.
---@param ms number
function sleep(ms) end

---@class console
console = {}
---Prints its arguments space-separated; tables are printed as JSON.
function console.log(...) end

---@class JSON
---@field null table sentinel for JSON null
JSON = {}
---@param text string
---@return any
function JSON.parse(text) end
---@param value any
---@param indent? integer|string
---@return string
function JSON.stringify(value, indent) end
---Marks a table so it encodes as an array even when empty.
---@param t? table
---@return table
function JSON.array(t) end

---@param fn function
---@param ms? number
---@return integer id
function os.setTimeout(fn, ms, ...) end
---@param fn function
---@param ms? number
---@return integer id
function os.setInterval(fn, ms, ...) end
---@param id integer?
function os.clearTimeout(id) end
---@param id integer?
function os.clearInterval(id) end
---Milliseconds since boot.
---@return number
function os.millis() end
---@param path string
---@return boolean
function os.chdir(path) end
---@return string
function os.getcwd() end
---@param ms integer
function os.sleepms(ms) end

---@param path string
---@return string? data
function io.readfile(path) end
---@param path string
---@param data string
---@return boolean ok
function io.writefile(path, data) end
---@param path string
---@return boolean
function io.exists(path) end

-- ---------------------------------------------------------------------------
-- System, IOP, Timer, Mutex, Thread
-- ---------------------------------------------------------------------------

---@class System
---@field boot_path string
---@field engine "lua"
System = {
  T_LONG = 0, T_ULONG = 0, T_INT = 0, T_UINT = 0, T_SHORT = 0, T_USHORT = 0, T_CHAR = 0, T_UCHAR = 0,
  T_BOOL = 0, T_FLOAT = 0, T_PTR = 0, T_STRING = 0, T_BUFFER = 0, READ_ONLY = 0, SELECT = 0,
}
---@param path? string
---@return { name: string, size: integer, dir: boolean }[]
function System.listDir(path) end
function System.removeDirectory(path) end
function System.moveFile(src, dst) end
function System.copyFile(src, dst) end
function System.threadCopyFile(src, dst) end
---@return { current: integer, final: integer }
function System.getFileProgress() end
function System.rename(src, dst) end
function System.mount(path, device, mode) end
function System.umount(path) end
function System.devices() end
function System.getBDMInfo(device) end
function System.sleep(seconds) end
function System.delay(seconds) end
function System.exitToBrowser() end
---Restarts the Lua VM running `script` (defaults to the current one). Never returns.
---@param script? string
function System.reload(script) end
function System.getMCInfo(slot) end
---@param path string
---@param args? string[]
---@param reboot_iop? boolean
function System.loadELF(path, args, reboot_iop) end
function System.checkValidDisc() end
function System.getDiscType() end
function System.checkDiscTray() end
function System.setDarkMode(enabled) end
function System.getCPUInfo() end
function System.getGPUInfo() end
---@return table stats includes `luaHeap`
function System.getMemoryStats() end
function System.setMemoryCompression(enabled) end
function System.compactMemory() end
---@return Buffer
function System.lz4Compress(data) end
---@return Buffer
function System.lz4Decompress(data, size) end
function System.getTemperature() end
---Starts the statistical PC profiler (`hz` samples per second, default 1000).
---@return boolean
function System.profileStart(hz) end
---Stops it and writes the raw u32 PCs to `path` (see scripts/profile_report.py).
---@return integer samples
function System.profileStop(path) end
---Calls a native function. `args` is a list of `{ type = System.T_*, value = v }`.
---@param fn integer address
---@param args? { type: integer, value: any }[]
---@param result_type? integer
function System.nativeCall(fn, args, result_type) end
function System.loadReloc(path) end
function System.unloadReloc(handle) end
---@return integer address
function System.findRelocObject(name) end
function System.findRelocLocalObject(handle, name) end

---@class IOP
IOP = {}
function IOP.newModule(path, args) end
---@param name string
function IOP.loadModule(name, ...) end
function IOP.getModule(name) end
function IOP.getModules() end
function IOP.reset() end
function IOP.getMemoryStats() end

---@class Timer
---@overload fun(): Timer
Timer = {}
---@return number ms
function Timer:getTime() end
function Timer:setTime(ms) end
function Timer:destroy() end
function Timer:pause() end
function Timer:resume() end
function Timer:reset() end
---@return boolean
function Timer:isPlaying() end

---@class Mutex
---@overload fun(): Mutex
Mutex = {}
function Mutex:lock() end
function Mutex:unlock() end

---Cooperative: `Thread(fn, name)` runs fn as a scheduler task that advances
---whenever it yields (`sleep`, `await`, `Thread.yield`).
---@class Thread
---@overload fun(fn: function, name?: string): Thread
Thread = {}
---@return { id: integer, name: string, status: integer }[]
function Thread.list() end
function Thread.kill(id) end
function Thread.yield() end
function Thread.new(fn, name) end
function Thread:start() end
function Thread:stop() end
---@return boolean
function Thread:running() end

-- ---------------------------------------------------------------------------
-- Buffer, Color, Draw
-- ---------------------------------------------------------------------------

---16-byte aligned byte buffer (the ArrayBuffer of the JS runtime). Offsets are 0-based bytes.
---@class Buffer
---@overload fun(size: integer): Buffer
---@operator len: integer
---@field size integer
---@field byteLength integer
Buffer = {}
function Buffer:getU8(offset) end
function Buffer:setU8(offset, v) end
function Buffer:getI8(offset) end
function Buffer:setI8(offset, v) end
function Buffer:getU16(offset) end
function Buffer:setU16(offset, v) end
function Buffer:getI16(offset) end
function Buffer:setI16(offset, v) end
---@return integer
function Buffer:getU32(offset) end
function Buffer:setU32(offset, v) end
function Buffer:getI32(offset) end
function Buffer:setI32(offset, v) end
---@return number
function Buffer:getF32(offset) end
function Buffer:setF32(offset, v) end
---@return string
function Buffer:toString(offset, length) end
function Buffer:fill(byte, offset, length) end
---@param src Buffer|string
---@param dst_offset? integer
---@param src_offset? integer
---@param length? integer
function Buffer:copy(src, dst_offset, src_offset, length) end

---A packed RGBA integer.
---@alias ColorValue integer

---@class Color
Color = {}
---@return ColorValue
function Color.new(r, g, b, a) end
function Color.getR(c) end
function Color.getG(c) end
function Color.getB(c) end
function Color.getA(c) end
function Color.setR(c, v) end
function Color.setG(c, v) end
function Color.setB(c, v) end
function Color.setA(c, v) end

---@class Draw
Draw = {}
function Draw.point(x, y, color) end
function Draw.line(x1, y1, x2, y2, color) end
function Draw.triangle(x1, y1, x2, y2, x3, y3, c1, c2, c3) end
function Draw.quad(x1, y1, x2, y2, x3, y3, x4, y4, c1, c2, c3, c4) end
function Draw.rect(x, y, w, h, color) end
---@param filled? boolean defaults to true
function Draw.circle(x, y, radius, color, filled) end

-- ---------------------------------------------------------------------------
-- Image, ImageList, Font, Screen
-- ---------------------------------------------------------------------------

NEAREST = 0
LINEAR = 1

---@class Image
---@overload fun(path?: string, list?: ImageList): Image
---@field width number
---@field height number
---@field startx number
---@field starty number
---@field endx number
---@field endy number
---@field angle number
---@field color ColorValue
---@field filter integer NEAREST or LINEAR
---@field renderable boolean
---@field texWidth integer
---@field texHeight integer
---@field bpp integer
---@field pixels Buffer
---@field palette Buffer
---@field size integer
---@field delayed boolean
Image = { NEAREST = 0, LINEAR = 1 }
function Image:draw(x, y) end
---@return boolean
function Image:ready() end
function Image:optimize() end
function Image:free() end
function Image:lock() end
function Image:unlock() end
function Image:locked() end
function Image.copyVRAMBlock(src, sx, sy, dst, dx, dy, w, h) end

---Loads images on a background thread.
---@class ImageList
---@overload fun(): ImageList
ImageList = {}
function ImageList:process() end

---@class Font
---@overload fun(path?: string): Font
---@field scale number
---@field outline number
---@field dropshadow number
---@field color ColorValue
---@field outline_color ColorValue
---@field dropshadow_color ColorValue
---@field align integer
Font = {
  ALIGN_TOP = 0, ALIGN_BOTTOM = 0, ALIGN_VCENTER = 0, ALIGN_LEFT = 0, ALIGN_RIGHT = 0,
  ALIGN_HCENTER = 0, ALIGN_NONE = 0, ALIGN_CENTER = 0,
}
function Font:print(x, y, text) end
---@return FontRender
function Font:render(text) end
---@return { width: number, height: number }
function Font:getTextSize(text) end

---@class FontRender
---@field width number
---@field height number
local FontRender = {}
function FontRender:print(x, y) end

---@class VideoMode
---@field mode integer
---@field width integer
---@field height integer
---@field psm integer
---@field interlace integer
---@field field integer
---@field psmz integer
---@field zbuffering boolean
---@field double_buffering boolean
---@field pass_count integer

---Alpha blend equation `{ a, b, c, d, fix }`.
---@alias AlphaEquation integer[]

---@class Screen
Screen = {}
function Screen.flip() end
function Screen.clear(color) end
function Screen.clearColor(color) end
---Registers the render function; the main loop calls it once per frame and flips.
---@param fn fun()
function Screen.display(fn) end
function Screen.getMemoryStats(kind) end
---@param interval? integer sampling period in frames, default 60
---@return number
function Screen.getFPS(interval) end
function Screen.waitVblankStart() end
function Screen.setVSync(enabled) end
function Screen.setFrameCounter(enabled) end
function Screen.setFrameOverlap(enabled) end
function Screen.getFrameOverlap() end
function Screen.getProfile() end
---@return VideoMode
function Screen.getMode() end
---@param mode VideoMode
function Screen.setMode(mode) end
function Screen.getParam(param) end
function Screen.setParam(param, value) end
---@return AlphaEquation
function Screen.alphaEquation(a, b, c, d, fix) end
function Screen.initBuffers() end
function Screen.resetBuffers() end
---@return Image
function Screen.getBuffer(which) end
function Screen.setBuffer(which, image) end
function Screen.createShuffleLUT(...) end
function Screen.updateShuffleLUT(...) end
function Screen.buildDepthFogLUT(...) end
function Screen.channelShuffle(...) end
function Screen.depthFog(...) end
function Screen.depthBitShift(...) end
function Screen.depthInvertBlue(...) end
function Screen.switchContext() end
function Screen.flush() end

-- ---------------------------------------------------------------------------
-- Math
-- ---------------------------------------------------------------------------

---@class Vector2
---@overload fun(x?: number, y?: number): Vector2
---@operator add(Vector2): Vector2
---@operator sub(Vector2): Vector2
---@operator mul(Vector2|number): Vector2
---@operator div(Vector2|number): Vector2
---@operator unm: Vector2
---@field x number
---@field y number
Vector2 = {}
function Vector2:norm() end
function Vector2:dot(v) end
function Vector2:cross(v) end
function Vector2:dist(v) end
function Vector2:distsqr(v) end
---@return number, number
function Vector2:unpack() end

---@class Vector3
---@overload fun(x?: number, y?: number, z?: number): Vector3
---@operator add(Vector3): Vector3
---@operator sub(Vector3): Vector3
---@operator mul(Vector3|number): Vector3
---@operator div(Vector3|number): Vector3
---@operator unm: Vector3
---@field x number
---@field y number
---@field z number
Vector3 = {}
function Vector3:norm() end
function Vector3:normalize() end
function Vector3:dot(v) end
---@return Vector3
function Vector3:cross(v) end
function Vector3:dist(v) end
function Vector3:distsqr(v) end
---@return number, number, number
function Vector3:unpack() end

---@class Vector4
---@overload fun(x?: number, y?: number, z?: number, w?: number): Vector4
---@operator add(Vector4): Vector4
---@operator sub(Vector4): Vector4
---@operator mul(Vector4|number): Vector4
---@operator div(Vector4|number): Vector4
---@operator unm: Vector4
---@field x number
---@field y number
---@field z number
---@field w number
Vector4 = {}
function Vector4:norm() end
function Vector4:dot(v) end
function Vector4:cross(v) end
function Vector4:distance(v) end
function Vector4:distance2(v) end
---@return number, number, number, number
function Vector4:unpack() end

---4x4 float matrix. Elements are `m[1]`..`m[16]`.
---@class Matrix4
---@overload fun(): Matrix4
---@operator mul(Matrix4): Matrix4
---@operator len: integer
---@field [integer] number
Matrix4 = {}
---@return number[]
function Matrix4:toArray() end
function Matrix4:fromArray(t) end
---@return Matrix4
function Matrix4:clone() end
function Matrix4:copy(src) end
function Matrix4:transpose() end
function Matrix4:invert() end
function Matrix4:identity() end
function Matrix4:mul(m) end

-- ---------------------------------------------------------------------------
-- Input
-- ---------------------------------------------------------------------------

---@class Pad
---@field port integer
---@field btns integer
---@field old_btns integer
---@field lx integer
---@field ly integer
---@field rx integer
---@field ry integer
---@field old_lx integer
---@field old_ly integer
---@field old_rx integer
---@field old_ry integer
local Pad = {}
function Pad:update() end
---@return boolean
function Pad:pressed(button) end
---@return boolean
function Pad:justPressed(button) end
---Lets the main loop update this pad each frame (for `Screen.display` scripts).
function Pad:setEventHandler() end

---@class Pads
Pads = {
  SELECT = 0, START = 0, UP = 0, RIGHT = 0, DOWN = 0, LEFT = 0, TRIANGLE = 0, CIRCLE = 0, CROSS = 0, SQUARE = 0,
  L1 = 0, R1 = 0, L2 = 0, R2 = 0, L3 = 0, R3 = 0, PRESSED = 0, JUST_PRESSED = 0, NONPRESSED = 0,
}
---@param port? integer
---@return Pad
function Pads.get(port) end
function Pads.getType(port) end
function Pads.getActiveType(port) end
function Pads.setMode(port, mode, lock) end
function Pads.getState(port) end
function Pads.getPressure(port, button) end
function Pads.rumble(port, small, large) end
function Pads.setLED(port, ...) end
function Pads.isActive(port) end
function Pads.getConnected() end
function Pads.getConnectedCount() end
---@param button integer
---@param kind integer Pads.PRESSED / JUST_PRESSED / NONPRESSED
---@param fn fun()
---@return integer id
function Pads.newEvent(button, kind, fn) end
function Pads.deleteEvent(id) end

---@class Keyboard
Keyboard = {}
function Keyboard.init() end
function Keyboard.get() end
function Keyboard.getRaw() end
function Keyboard.setReadMode(mode) end
function Keyboard.setRepeatRate(ms) end
function Keyboard.setBlockingMode(enabled) end
function Keyboard.setKeymap(...) end
function Keyboard.resetKeymap() end
function Keyboard.deinit() end

---@class Mouse
Mouse = {}
function Mouse.init() end
---@return { x: integer, y: integer, wheel: integer, buttons: integer }
function Mouse.get() end
function Mouse.setBoundary(minx, maxx, miny, maxy) end
function Mouse.getBoundary() end
function Mouse.getMode() end
function Mouse.setMode(mode) end
function Mouse.getAccel() end
function Mouse.setAccel(v) end
function Mouse.setPosition(x, y) end
function Mouse.getThreshold() end
function Mouse.setThreshold(v) end
function Mouse.getDoubleClickTime() end
function Mouse.setDoubleClickTime(v) end
function Mouse.getVersion() end
function Mouse.enumerate() end
function Mouse.reset() end

---@class RemoteState
---@field button integer
---@field old_button integer
---@field status integer
local RemoteState = {}
function RemoteState:update() end
function RemoteState:pressed(button) end
function RemoteState:justPressed(button) end
function RemoteState:released(button) end

---@class Remote
Remote = {}
function Remote.init() end
function Remote.deinit() end
---@return RemoteState
function Remote.get() end
function Remote.isActive() end

---USB camera.
---@class EyeToy
EyeToy = {}
function EyeToy.init() end
function EyeToy.count() end
function EyeToy.open(index) end
function EyeToy.close(index) end
function EyeToy.getStatus(index) end
function EyeToy.setBandwidth(index, mode) end
function EyeToy.readPacket(index, buffer) end
function EyeToy.setLED(index, on) end
function EyeToy.setConfig(index, ...) end

-- ---------------------------------------------------------------------------
-- Audio
-- ---------------------------------------------------------------------------

---@class SoundStream
---@field loop boolean
---@field position number
---@field length number
local SoundStream = {}
function SoundStream:play() end
function SoundStream:pause() end
function SoundStream:rewind() end
---@return boolean
function SoundStream:playing() end
function SoundStream:free() end

---@class SoundSfx
---@field length integer
---@field volume integer
---@field pan integer
---@field loop boolean
---@field pitch integer
local SoundSfx = {}
---@param channel? integer
---@return integer channel
function SoundSfx:play(channel) end
function SoundSfx:playing(channel) end
function SoundSfx:free() end

---@class Sound
Sound = {}
function Sound.setVolume(volume, channel) end
function Sound.findChannel() end
---@return SoundStream
function Sound.Stream(path) end
---@return SoundSfx
function Sound.Sfx(path) end

-- ---------------------------------------------------------------------------
-- TileMap, Particles, Shadows
-- ---------------------------------------------------------------------------

---@class TileMapDescriptor
---@field materialCount integer
local TileMapDescriptor = {}
function TileMapDescriptor:free() end

---@class TileMapInstance
local TileMapInstance = {}
function TileMapInstance:render(x, y, zindex) end
function TileMapInstance:replaceSpriteBuffer(buffer) end
---@return Buffer
function TileMapInstance:getSpriteBuffer() end
function TileMapInstance:updateSprites(start, sprites) end
function TileMapInstance:free() end

---@class TileMap
---@field layout { stride: integer, offsets: table<string, integer> }
TileMap = {}
function TileMap.init() end
function TileMap.begin() end
function TileMap.setCamera(x, y) end
---@param desc { textures: string[], materials: { texture_index: integer, blend_mode: AlphaEquation, end_offset: integer }[] }
---@return TileMapDescriptor
function TileMap.Descriptor(desc) end
---@param opts { descriptor: TileMapDescriptor, spriteBuffer: Buffer }
---@return TileMapInstance
function TileMap.Instance(opts) end
TileMap.SpriteBuffer = {}
---@return Buffer
function TileMap.SpriteBuffer.create(count) end
---@return Buffer
function TileMap.SpriteBuffer.fromObjects(sprites) end
---@param index integer 0-based
function TileMap.SpriteBuffer.set(buffer, index, fields) end
---@param index integer 0-based
---@return table
function TileMap.SpriteBuffer.get(buffer, index) end

---@class ParticleEmitter
local ParticleEmitter = {}
function ParticleEmitter:setPosition(x, y, z) end
function ParticleEmitter:burst(count) end
function ParticleEmitter:start() end
function ParticleEmitter:stop() end
function ParticleEmitter:configure(opts) end

---@class ParticleSystem
---@field activeCount integer
---@field capacity integer
local ParticleSystem = {}
---@return ParticleEmitter
function ParticleSystem:createEmitter(opts) end
function ParticleSystem:update(dt) end
function ParticleSystem:draw() end
function ParticleSystem:clear() end
function ParticleSystem:dispose() end
function ParticleSystem:stats() end

---@class Particles
Particles = {}
---@return ParticleSystem
function Particles.System(opts) end

---@class ShadowProjector
---@field position table
---@field rotation table
---@field scale table
local ShadowProjector = {}
function ShadowProjector:setTransform(...) end
function ShadowProjector:setSize(w, h) end
function ShadowProjector:setGrid(x, y) end
function ShadowProjector:setLightDir(x, y, z) end
function ShadowProjector:setBias(v) end
function ShadowProjector:setLightOffset(x, y, z) end
function ShadowProjector:setSlopeLimit(v) end
function ShadowProjector:setColor(color) end
function ShadowProjector:setBlend(mode) end
function ShadowProjector:setUVRect(u1, v1, u2, v2) end
function ShadowProjector:enableRaycast(space) end
function ShadowProjector:render() end
function ShadowProjector:free() end

---@class Shadows
Shadows = { SHADOW_BLEND_DARKEN = 0, SHADOW_BLEND_ALPHA = 1, SHADOW_BLEND_ADD = 2 }
---@return ShadowProjector
function Shadows.Projector(opts) end

-- ---------------------------------------------------------------------------
-- 3D rendering
-- ---------------------------------------------------------------------------

---@class Vec3Table
---@field x number
---@field y number
---@field z number

---@class Render
Render = { PL_NO_LIGHTS = 0, PL_DEFAULT = 0, PL_SPECULAR = 0 }
function Render.init() end
function Render.begin() end
function Render.setView(fov, near, far) end
function Render.stats() end
function Render.resetStats() end
---Vertex attributes are Buffers or flat number tables (4 floats per vertex).
---@return table
function Render.vertexList(positions, normals, texcoords, colors, materials, material_indices, shareBuffers) end
---@return { r: number, g: number, b: number, a: number }
function Render.materialColor(r, g, b, a) end
---@return table
function Render.material(ambient, diffuse, specular, emission, transmittance, shininess, refraction,
                         transmission_filter, disolve, texture_id, bump_texture_id, ref_texture_id, decal_texture_id) end
---@param index integer 0-based material
---@param last integer last vertex of the group
function Render.materialIndex(index, last) end

---@class RenderData
---@overload fun(path_or_vertexlist: string|table, texture?: Image, tristrip?: boolean): RenderData
---@field vertices table
---@field materials table
---@field material_indices table
---@field accurate_clipping boolean
---@field face_culling integer
---@field texture_mapping boolean
---@field shade_model integer
---@field pipeline integer
---@field bounds table
---@field size integer
---@field textures Image[]
RenderData = {}
function RenderData:setTexture(index, image) end
---@return Image
function RenderData:getTexture(index) end
function RenderData:pushTexture(image) end
---@param index integer 0-based
function RenderData:updateMaterial(index, fields) end
---@return RenderData
function RenderData:clone() end
function RenderData:freeze() end
function RenderData:free() end
function RenderData:dispose() end

---@class RenderObject
---@overload fun(data: RenderData): RenderObject
---@field position Vec3Table
---@field rotation Vec3Table
---@field scale Vec3Table
---@field frustumCull boolean
RenderObject = {}
function RenderObject:render() end
function RenderObject:renderBounds() end
function RenderObject:setPosition(x, y, z) end
function RenderObject:setRotation(x, y, z) end
function RenderObject:setScale(x, y, z) end
---@return Matrix4
function RenderObject:getBoneTransform(index) end
---@param anim Animation
---@param loop? boolean
---@param blend? number seconds, default 0.15
function RenderObject:playAnim(anim, loop, blend) end
function RenderObject:isPlayingAnim(anim) end
function RenderObject:setCollision(geom) end
function RenderObject:setPhysics(body) end
function RenderObject:free() end
function RenderObject:dispose() end

---@class Batch
---@overload fun(opts?: table): Batch
---@field size integer
Batch = {}
function Batch:add(ro) end
function Batch:clear() end
function Batch:render() end
function Batch:free() end

---@class SceneNode
---@overload fun(): SceneNode
---@field position Vec3Table
---@field rotation Vec3Table
---@field scale Vec3Table
SceneNode = {}
function SceneNode:addChild(node) end
function SceneNode:removeChild(node) end
function SceneNode:attach(ro) end
function SceneNode:detach(ro) end
function SceneNode:update() end
function SceneNode:free() end

---@class AsyncLoader
---@overload fun(opts?: table): AsyncLoader
AsyncLoader = {}
---@param fn fun(data: RenderData)
function AsyncLoader:enqueue(path, fn, texture) end
function AsyncLoader:clear() end
function AsyncLoader:process() end
function AsyncLoader:destroy() end
function AsyncLoader:size() end
function AsyncLoader:getJobsPerStep() end
function AsyncLoader:setJobsPerStep(n) end

---@class Animation
---@field name string

---Indexed 1..n or by animation name.
---@class AnimCollection
---@overload fun(path: string): AnimCollection
---@operator len: integer
---@field [integer|string] Animation
AnimCollection = {}
function AnimCollection:free() end

---@class Lights
Lights = { AMBIENT = 0, DIFFUSE = 0, SPECULAR = 0, DIRECTION = 0, POSITION = 0, ATTENUATION = 0 }
---@return integer id
function Lights.new() end
function Lights.set(id, attribute, x, y, z) end

---@class Camera
Camera = {}
function Camera.position(x, y, z) end
---Pans by delta on each call: set it once, not every frame.
function Camera.target(x, y, z) end
function Camera.orbit(yaw, pitch) end
function Camera.turn(yaw, pitch) end
function Camera.dolly(d) end
function Camera.zoom(d) end
function Camera.pan(x, y) end
function Camera.save() end
function Camera.restore() end
function Camera.update() end

-- ---------------------------------------------------------------------------
-- Physics (getters return multiple values: local x, y, z = body:getPosition())
-- ---------------------------------------------------------------------------

---@class PhysicsWorld
local PhysicsWorld = {}
function PhysicsWorld:setGravity(x, y, z) end
---@return number, number, number
function PhysicsWorld:getGravity() end
function PhysicsWorld:setCFM(v) end
function PhysicsWorld:setERP(v) end
function PhysicsWorld:step(dt) end
function PhysicsWorld:quickStep(dt) end
function PhysicsWorld:setQuickStepIterations(n) end
---Without a callback, returns the number of contacts.
---@return integer?
function PhysicsWorld:stepWithContacts(space, group, dt, callback) end
function PhysicsWorld:destroyWorld() end
function PhysicsWorld:free() end

---@class PhysicsSpace
local PhysicsSpace = {}
function PhysicsSpace:collide(callback) end
function PhysicsSpace:add(geom) end
function PhysicsSpace:remove(geom) end
function PhysicsSpace:raycast(ox, oy, oz, dx, dy, dz, length) end
function PhysicsSpace:free() end

---@class PhysicsGeom
local PhysicsGeom = {}
function PhysicsGeom:setPosition(x, y, z) end
---@return number, number, number
function PhysicsGeom:getPosition() end
function PhysicsGeom:setRotation(...) end
function PhysicsGeom:getRotation() end
function PhysicsGeom:setBody(body) end
---@return PhysicsBody?
function PhysicsGeom:getBody() end
function PhysicsGeom:free() end

---@class PhysicsBody
local PhysicsBody = {}
function PhysicsBody:setPosition(x, y, z) end
---@return number, number, number
function PhysicsBody:getPosition() end
function PhysicsBody:setRotation(...) end
function PhysicsBody:getRotation() end
function PhysicsBody:setLinearVel(x, y, z) end
---@return number, number, number
function PhysicsBody:getLinearVel() end
function PhysicsBody:setAngularVel(x, y, z) end
---@return number, number, number
function PhysicsBody:getAngularVel() end
function PhysicsBody:setMass(...) end
function PhysicsBody:setMassBox(density, x, y, z) end
function PhysicsBody:setMassSphere(density, radius) end
function PhysicsBody:addForce(x, y, z) end
function PhysicsBody:addTorque(x, y, z) end
function PhysicsBody:enable() end
function PhysicsBody:disable() end
function PhysicsBody:enabled() end
function PhysicsBody:free() end

---@class Physics
Physics = {}
---@return PhysicsWorld
function Physics.World() end
---@return PhysicsSpace
function Physics.Space(parent) end
---@return PhysicsBody
function Physics.Body(world) end
function Physics.JointGroup() end
---@return PhysicsGeom
function Physics.GeomBox(space, x, y, z) end
---@return PhysicsGeom
function Physics.GeomSphere(space, radius) end
---@return PhysicsGeom
function Physics.GeomPlane(space, a, b, c, d) end
---@return PhysicsGeom
function Physics.GeomRay(space, length) end
---@return PhysicsGeom
function Physics.GeomTransform(space) end
---@return PhysicsGeom
function Physics.GeomRenderObject(space, ro) end
function Physics.geomCollide(a, b) end
function Physics.cleanup() end

-- ---------------------------------------------------------------------------
-- Network, Archive, Video (relocatable modules, loaded on first use)
-- ---------------------------------------------------------------------------

---@class Network
Network = {}
function Network.init(ip, netmask, gateway, dns) end
---@return { ip: string, netmask: string, gateway: string, dns: string }
function Network.getConfig() end
function Network.getHostbyName(host) end
function Network.deinit() end

---@class RequestResponse
---@field text string
---@field status_code integer
---@field headers string?

---@class Request
---@overload fun(): Request
---@field keepalive boolean
---@field useragent string
---@field userpwd string
---@field timeout integer
---@field verifyTLS boolean
---@field followRedirects boolean
---@field headers string[]
Request = {}
---@return RequestResponse
function Request:get(url) end
---@return RequestResponse
function Request:head(url) end
---@return RequestResponse
function Request:post(url, data) end
function Request:download(url, path) end
function Request:asyncGet(url) end
function Request:asyncPost(url, data) end
function Request:asyncDownload(url, path) end
---@return string?
function Request:getAsyncData() end
---@return integer
function Request:getAsyncSize() end
---Raises the transfer error, if any. Usable with `await(req)`.
---@param timeout? integer
---@return boolean
function Request:ready(timeout, conn_timeout) end

---@class Socket
---@overload fun(family?: integer, kind?: integer): Socket
Socket = { AF_INET = 2, SOCK_STREAM = 1, SOCK_DGRAM = 2, SOCK_RAW = 3 }
function Socket:connect(host, port) end
function Socket:bind(host, port) end
function Socket:listen() end
function Socket:send(data) end
---@return string?
function Socket:recv(size) end
function Socket:close() end

---@class WebSocket
---@overload fun(url: string, opts?: { verifyTLS?: boolean }): WebSocket
---@field verifyTLS boolean
WebSocket = {}
function WebSocket:send(data) end
---@return string?
function WebSocket:recv() end
function WebSocket:close() end

---@class ArchiveHandle
local ArchiveHandle = {}
---@return { name: string, size: integer, mtime: integer }[]
function ArchiveHandle:list() end
---ZIP: extracts to disk and returns true. GZ: returns the data as a Buffer.
function ArchiveHandle:extractAll() end
function ArchiveHandle:close() end

---@class Archive
Archive = {}
---@return ArchiveHandle?
function Archive.open(path) end
function Archive.list(archive) end
function Archive.extractAll(archive) end
function Archive.close(archive) end
---@return boolean
function Archive.untar(path) end

---@class Video
---@overload fun(path: string): Video
---@field width integer
---@field height integer
---@field fps number
---@field ready boolean
---@field ended boolean
---@field playing boolean
---@field loop boolean
---@field frame Image? the decoder texture, updated in place
---@field currentFrame integer
Video = {}
function Video:play() end
function Video:pause() end
function Video:stop() end
function Video:update() end
function Video:draw(x, y, w, h) end
function Video:free() end
