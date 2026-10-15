# 3D Rendering API

`Render`, `RenderData`, `RenderObject`, `Batch`, `SceneNode`, `AsyncLoader`, `AnimCollection`, `Camera`, `Lights`, `Shadows`.

P.S.: *Italic* parameters refer to optional parameters.

Always configure the screen z-buffer before rendering 3D:
```js
const canvas = Screen.getMode();
canvas.zbuffering = true;
canvas.psmz = Screen.Z16S;
Screen.setMode(canvas);
```

## Render module

* `Render.init()` – Initializes internal renderer state (GS/VU microprograms, materials cache). Call once during boot.
* `Render.begin()` – Starts a render pass and resets batched state. Call once per frame before issuing draw calls.
* `Render.setView(fov = 60, nearClip = 1.0, farClip = 2000.0, width = 0.0, height = 0.0)` – Configures the default projection matrix. Width/height override the auto-derived aspect ratio when non-zero.
* `Render.materialColor(r, g, b, alpha = 1.0)` – Convenience helper that returns a `{r,g,b,a}` color object for materials.
* `Render.material(ambient, diffuse, specular, emission, transmittance, shininess, refraction, transmission_filter, disolve, texture_id, bump_texture_id, ref_texture_id, decal_texture_id)` – Builds a material descriptor used by RenderData/RenderObject. Texture ids accept `-1` to disable a layer.
* `Render.materialIndex(index, end)` – Tags the vertex/material arrays so the renderer knows which faces should use each material slice.
* `Render.vertexList(positions, normals, texcoords, colors, materials, material_indices)` – Creates the structure expected by `new RenderData(...)`. Each typed array must use 4-component packing (xyzw, n1n2n3w, stqw, rgba).

**Constants:**
* `Render.PL_NO_LIGHTS`, `Render.PL_DEFAULT`, `Render.PL_SPECULAR` – Select the shading pipeline for RenderData.
* `Render.CULL_FACE_NONE`, `Render.CULL_FACE_BACK`, `Render.CULL_FACE_FRONT` – Face-culling modes consumed by RenderData/RenderObject.

## RenderData module

**Construction:**
```js
let data = new RenderData(mesh, *texture*)
/* Load simple WaveFront OBJ files or vertex arrays.
MTL is supported on OBJs (including per-vertex colors and multi-texturing).
If you don't have a MTL file but you want to bind a texture on it,
just pass the image as a second argument if you want to use it. */
```
**Methods:**

* `getTexture(id)` - Gets the nth texture object from the model.
* `setTexture(id, texture)` - Changes or sets the nth texture on models.
* `free()` - Free asset content immediately. This is a way to quickly free stuff from memory, but you can also wait for the garbage collector, so it's not mandatory.

**Properties:**

* `vertices` - Object holding the raw vertex attribute buffers, `{positions, normals, texcoords, colors}`. Each present key is an `ArrayBuffer`, packed as x/y/z/adc for positions, n1/n2/n3/adc for normals, s/t/q/w for texcoords and r/g/b/a for colors.
* `pipeline` - Rendering pipeline. Available pipelines:
  * `Render.PL_NO_LIGHTS` - Lights disabled, colors still working.
  * `Render.PL_SPECULAR` - Diffuse and specular lights and colors enabled.
  * `Render.PL_DEFAULT` - Default for textured models. Lights and colors enabled.
* `materials` - `Render.material` array.
* `material_indices` - `Render.materialIndex` array.
* `size` - Vertex quantity.
* `bounds` - Mesh bounding box.
* `accurate_clipping` - Toggle accurate clipping.
* `face_culling` - Face culling mode. Default: `Render.CULL_FACE_BACK`. Available modes: `Render.CULL_FACE_NONE`, `Render.CULL_FACE_FRONT`, `Render.CULL_FACE_BACK`
* `texture_mapping` - Toggle texture mapping (can be used to disable texturing on a whole textured model).
* `shade_model` - Flat = 0, Gouraud = 1.

**Properties (skinned):**
* `bones`: Array - Skeleton bones data.
  * `position`: Vector4 - Bone local position.
  * `rotation`: Vector4 - Bone local rotation.
  * `scale`: Vector4 - Bone local scale.
  * `inverse_bind`: Matrix4 - Bone inverse bind matrix.

## AnimCollection module

**Construction:**
```js
let walk_anims = new AnimCollection("walk_anims.gltf");
```
Loads an array map containing the animations inside the file, so you can either load them by index or name — for a anim called `"run_fast"`, just pass as:
```js
character.playAnim(walk_anims["run_fast"], true);

// now you want it to be in idle animation, which can be the first animation in our array, so:
character.playAnim(walk_anims[0], true);
```

## RenderObject module

**Construction:**
```js
let model = new RenderObject(render_data)
/* You need to build a RenderData first, RenderObject keeps position, rotation
and the individual object matrices */
```
**Methods:**

* `render()` - Draws the object on screen.
* `renderBounds(color)` - Draws object bounding box in the given color.
* `setCollision(geom)` - Attaches a `Physics` collision geometry (`Geom`) to this object (pass `null` to detach).
* `setPhysics(body)` - Attaches a `Physics` rigid body (`Body`) to this object (pass `null` to detach).
* `free()` - Free asset content immediately. This is a way to quickly free stuff from memory, but you can also wait for the garbage collector, so it's not mandatory.

**Methods (skinned):**

* `playAnim(anim, loop)` - Play animation on a skinned RenderObject.
* `isPlayingAnim(anim)` - Returns true if anim is being played.

**Properties:**

* `position` - Object with x, y and z keys that stores the object position. Default is `{x:0, y:0, z:0}`.
* `rotation` - Object with x, y and z keys that stores the object rotation. Default is `{x:0, y:0, z:0}`.
* `scale` - Object with x, y and z keys that stores the object scale. Default is `{x:0, y:0, z:0}`.
* `transform`: Matrix4 - Object RTS transform matrix.

**Properties (skinned):**
* `bone_matrices`: Matrix4[] - Array of Matrix4 containing the current bone state.
* `bones`: Array - Current bones data.
  * `position`: Vector4 - Current bone local position.
  * `rotation`: Vector4 - Current bone local rotation.
  * `scale`: Vector4 - Current bone local scale.
  * `transform`: Matrix4 - Current bone RTS matrix.

## Batch (RenderBatch module)

**Construction:**

```js
let batch = new Batch(options);
```
* `options.autoSort` - Boolean (default: true). Enables internal state-aware sort.

**Methods:**

* `add(renderObject)` - Adds a RenderObject to the batch. Returns current count.
* `clear()` - Removes all objects from the batch.
* `render()` - Renders all objects in the batch using optimal state transitions. Returns number of draws.

**Properties:**

* `size` (read-only) - Number of objects currently in the batch.

**Example:**
```js
const batch = new Batch({ autoSort: true });
batch.add(new RenderObject(new RenderData("dragon.obj")));
batch.render();
```

## SceneNode (RenderSceneNode module)

**Construction:**

```js
let node = new SceneNode();
```

**Methods:**

* `addChild(node)` - Parents a child SceneNode.
* `removeChild(node)` - Unparents a specific child.
* `attach(renderObject)` - Attaches a RenderObject to this node.
* `detach([renderObject])` - Detaches the specific object, or all when omitted.
* `update()` - Recomputes world transforms (rotate → scale → translate) for this subtree.

**Properties:**

* `position { x, y, z }`
* `rotation { x, y, z }`
* `scale { x, y, z }`

**Example:**
```js
const root = new SceneNode();
const child = new SceneNode();
const obj = new RenderObject(new RenderData("monkey.obj"));
child.position = { x: 0, y: 2, z: 0 };
child.attach(obj);
root.addChild(child);
root.update();
```

## AsyncLoader (RenderAsyncLoader module)

Cooperative streaming helper executed on the main thread in small steps.

**Construction:**

```js
let loader = new AsyncLoader({ jobsPerStep });
```
* `jobsPerStep` - Number (default: 1). How many items to process per `process()` call when budget is omitted.

**Methods:**

* `enqueue(path, callback[, texture])` - Queues a model load.
  * `path`: String (e.g., `"BoxTextured.gltf"`).
  * `callback`: Function `(path, renderData)` invoked when the item is ready.
  * `texture`: optional Image to bind during load (may be null/omitted).
* `process([budget])` - Processes up to `budget` items from the queue (or `jobsPerStep` if omitted). Returns number processed.
* `clear()` - Clears pending items and releases their callbacks.
* `destroy()` - Clears and destroys the loader.
* `size()` - Returns number of items still pending.
* `getJobsPerStep()` - Returns current jobs-per-step.
* `setJobsPerStep(n)` - Sets jobs-per-step, clamped to at least 1. Returns the applied value.

**Example:**
```js
const loader = new AsyncLoader({ jobsPerStep: 2 });
loader.enqueue("BoxTextured.gltf", (path, data) => {
  const obj = new RenderObject(data);
  sceneBatch.add(obj);
});

// In your frame loop
loader.process();
```

## Camera

* `Camera.position(x, y, z)`
* `Camera.target(x, y, z)`
* `Camera.orbit(yaw, pitch)`
* `Camera.turn(yaw, pitch)`
* `Camera.pan(x, y)`
* `Camera.dolly(distance)`
* `Camera.zoom(distance)`
* `Camera.update()` - Update camera state (must be called every frame).

## Lights

You have 4 lights to use in 3D scenes, use `set` to configure them.

* `Lights.set(id, attribute, x, y, z)`
  * Available light attributes: `Lights.DIRECTION`, `Lights.AMBIENT`, `Lights.DIFFUSE`

## Shadows module

The Shadows module provides real-time shadow projection capabilities using a grid-based approach with optional `Physics` ray casting for accurate shadow placement on 3D geometry.

**Constants:**

* `Shadows.SHADOW_BLEND_DARKEN` - Darken blend mode (default)
* `Shadows.SHADOW_BLEND_ALPHA` - Alpha blend mode
* `Shadows.SHADOW_BLEND_ADD` - Additive blend mode

**Construction:**

```js
let projector = new Shadows.Projector(shadowTexture);
```
Creates a new shadow projector using the specified texture for the shadow appearance.

**Methods:**

* `setSize(width, height)` - Set the shadow projection area size in world units.
* `setGrid(gridX, gridZ)` - Set the grid resolution for shadow tessellation (minimum 2x2).
* `setLightDir(x, y, z)` - Set the directional light direction (automatically normalized).
* `setBias(bias)` - Set shadow bias to prevent z-fighting (default: 0.01).
* `setLightOffset(offset)` - Set offset along light direction to shift shadow center.
* `setSlopeLimit(maxSlopeCos)` - Set maximum slope angle for shadow projection (optional).
* `setColor(r, g, b, a)` - Set shadow color and alpha (0.0-1.0 range).
* `setBlend(mode)` - Set shadow blend mode (`SHADOW_BLEND_*` constants).
* `setUVRect(u0, v0, u1, v1)` - Set texture UV rectangle for shadow appearance.
* `enableRaycast(space, enable, rayLength)` - Enable/disable `Physics` ray casting for accurate shadow placement. See [Physics](PHYSICS_API.md) for `space`.
* `render()` - Render the shadow projector (call every frame).

**Properties:**

* `position` - Object with x, y, z keys for shadow projector world position.
* `rotation` - Object with x, y, z keys for shadow projector rotation.
* `scale` - Object with x, y, z keys for shadow projector scale.

**Usage Example:**

```js
// Create shadow render target
const shadowRT = new Image();
shadowRT.filter = LINEAR;
shadowRT.renderable = true;
shadowRT.bpp = 32;
shadowRT.texWidth = 256;
shadowRT.texHeight = 256;
shadowRT.width = 256;
shadowRT.height = 256;
shadowRT.lock();

// Create shadow projector
const projector = new Shadows.Projector(shadowRT);
projector.setSize(2.0, 2.0);
projector.setGrid(12, 12);
projector.setLightDir(0.0, 1.0, 1.0);
projector.setBias(-0.02);
projector.setColor(0.0, 0.0, 0.0, 0.65);
projector.setBlend(Shadows.SHADOW_BLEND_DARKEN);
projector.enableRaycast(space, 4, 12.0);
projector.setLightOffset(1.0);

// In render loop
projector.position = { x: object.x, y: 0.0, z: object.z };
projector.render();
```

**Performance Notes:**

* Higher grid resolution provides smoother shadows but uses more vertices
* Ray casting provides accurate shadows but has performance cost
* Shadow projectors use VU1 pipeline for optimal performance
* Consider using lower resolution for distant or less important shadows
