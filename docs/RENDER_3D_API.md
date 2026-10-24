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
* `Render.begin()` – Starts a render pass: resets `Render.stats()`, marks the VU1 view block stale, and re-uploads static lighting. Call it once per frame before the first 3D draw. Skeletal animation does not depend on it — a pose advances once per `Screen.flip()`, and every camera pass in that frame (the main view and a shadow view) shares that pose.
* `Render.setView(fov = 60, nearClip = 1.0, farClip = 2000.0, width = 0.0, height = 0.0)` – Configures the default projection matrix. Width/height override the auto-derived aspect ratio when non-zero.
* `Render.materialColor(r, g, b, alpha = 1.0)` – Convenience helper that returns a `{r,g,b,a}` color object for materials.
* `Render.material(ambient, diffuse, specular, emission, transmittance, shininess, refraction, transmission_filter, disolve, texture_id, bump_texture_id, ref_texture_id, decal_texture_id, normal_texture_id = -1)` – Builds a material descriptor used by RenderData/RenderObject. Texture ids accept `-1` to disable a layer. `normal_texture_id` is the tangent-space normal map [normal mapping](#normal-mapping-dot3) uses.
* `Render.materialIndex(index, end)` – Tags the vertex/material arrays so the renderer knows which faces should use each material slice.
* `Render.vertexList(positions, normals, texcoords, colors, materials, material_indices)` – Creates the structure expected by `new RenderData(...)`. Each typed array must use 4-component packing (xyzw, n1n2n3w, stqw, rgba).

**Constants:**
* `Render.PL_NO_LIGHTS`, `Render.PL_DEFAULT`, `Render.PL_SPECULAR` – Select the shading pipeline for RenderData.
* `Render.CULL_FACE_NONE`, `Render.CULL_FACE_BACK`, `Render.CULL_FACE_FRONT` – Face-culling modes consumed by RenderData/RenderObject.
* `Render.DOT3_OFF`, `Render.DOT3_2PASS`, `Render.DOT3_4PASS` – Values of `RenderData.normal_mapping`.

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
* `freeze()` - Drops the CPU float vertex source after the VU1 compact buffers are cooked. Returns `false` when a buffer is shared (a shallow `clone()`, or `shareBuffers`) and cannot be released. Use it for meshes that will never be read back and are not collision trimeshes. Skinned meshes are a normal case: they animate through bone matrices, not by rewriting `positions`. Leaving a mesh unfrozen is also fine — the automatic compactor LZ4-compresses those same arrays after they go idle and restores them if script or physics needs them. See [Automatic EE RAM compaction](CORE_API.md#automatic-ee-ram-compaction).

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
* `normal_mapping` - Per-pixel lighting from the materials' normal maps: `Render.DOT3_OFF` (default), `Render.DOT3_2PASS` or `Render.DOT3_4PASS`. See [Normal mapping](#normal-mapping-dot3).

### Normal mapping (DOT3)

Per-pixel N·L on the GS, after Morten Mikkelsen's *DOT3 Normal Mapping on the PS2* (2004). glTF `normalTexture` is loaded into the material's `normal_texture_id`; script-built meshes set it like any other texture id (`pushTexture`). Turning it on is per RenderData:

```js
const crate = new RenderData("box_bump.gltf");
crate.normal_mapping = Render.DOT3_4PASS;
```

* `Render.DOT3_2PASS` – two geometry passes. The light vector is interpolated between vertices without being renormalized, so a curved low-poly surface loses some light inside its triangles.
* `Render.DOT3_4PASS` – four geometry passes. The light vector is renormalized per pixel through a sphere map.

Both then add the three signed products with channel shuffles, turn the sum into a light level through a palette, and draw the object once more, unlit, multiplied by it. Those shuffles run over the object's screen rectangle, so the cost grows with its size on screen.

The normal map is quantized to 256 normals the first time it draws. Tangents are built from positions, normals and UVs the first time too, so the mesh must not be frozen before its first normal-mapped draw. Lighting comes from the scene's first light, directional or point (with its attenuation, per vertex), as one level — its diffuse color and the ambient are averaged to grey. It replaces the emboss bump (`bump_texture_id`) while on. Not supported, and drawn the usual way instead: skinned meshes, meshes without UVs, and draw buffers other than CT32 with a Z24 depth buffer (the top byte of Z24 is the scratch space).

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

* `playAnim(anim, loop)` - Play animation on a skinned RenderObject. The clip advances on the EE clock, once per displayed frame, the first time the object is drawn after `Screen.flip()`. Extra draws in the same frame reuse the bone palette.
* `isPlayingAnim(anim)` - Returns true if anim is being played. Omit `anim` to test whether any clip is current.

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
* `Shadows.SHADOW_BLEND_MULTIPLY` - Multiplies the scene by `1 - alpha` (texture alpha times the projector color's alpha); RGB is ignored. One pass, but two overlapping shadows darken twice

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
* `screenBounds()` - Screen rectangle `{x, y, width, height}` in pixels that the next `render()` covers under the current camera, or `null` when it is off-screen. Use it to skip off-screen casters, or to limit full-screen passes to where the shadows are (see `physics_collision.js`). With raycasting on, the grid is built here and the next `render()` reuses it, so the call costs no extra work; any setter discards that grid.

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

### Shadow volumes

Stencil shadow volumes: unlike a projector, the shadow lands on whatever lies inside the volume -- the floor, other objects, the caster itself. The GS has no stencil buffer, so the red channel of the frame buffer stands in for one: each volume adds 1 where its front faces pass the depth test and subtracts 1 where its back faces do, and the red that ends up non-zero is shadow.

```js
const volume = new Shadows.Volume(crateObject); // follows the RenderObject's transform
volume.extrusion = 30;                          // how far the silhouette is pushed from the light

// After the opaque scene:
if (Shadows.beginVolumes()) {
    volume.render();
    Shadows.endVolumes(0.55);                   // shadow opacity
}
```

* `new Shadows.Volume(renderObject)` - Builds the volume of a RenderObject's mesh: vertices welded by position, the edge list, and two buffers of one quad per edge plus two triangles per triangle. The mesh must not be frozen when it is created; it should be closed, or its open edges count as silhouette.
* `volume.render()` - Finds this frame's silhouette from the scene's first light -- directional, or a point light, pushed radially from it -- and draws the closed volume into the stencil: the sides, and the caster's faces turned from the light twice, pushed (the far end) and in place (the near end). Only between `beginVolumes` and `endVolumes`.
* `volume.extrusion` - Distance the silhouette is pushed away from the light. Default 20.
* `volume.edges` - Edge count, for reference: one quad each, most of them collapsed.
* `Shadows.beginVolumes(region)` - Saves the scene's red in the top byte of the Z24 buffer and clears it. `region` (`{x, y, width, height}`) limits the passes; the whole screen by default. Returns `false` -- and nothing is drawn until `endVolumes` -- unless the draw buffer is CT32 and the depth buffer Z24.
* `Shadows.endVolumes(opacity = 0.5)` - Turns the stencil into the shadow and restores red: what is inside a volume is darkened to `1 - opacity`. Cheap, but it darkens the ambient light too, and faces already turned from the light a second time.
* `Shadows.endVolumes(renderObjects)` - With an array of RenderObjects instead: what is inside a volume is drawn again from those objects, with the ambient light only (the scene's, averaged to grey), through the destination alpha test -- so only their shadowed pixels. That is what a light's shadow really leaves, Doom 3 style: in shadow and turned from the light come out the same. Costs one more unlit draw of the objects. Pass the objects the shadows can land on.

```js
if (Shadows.beginVolumes()) {
    for (const volume of volumes) volume.render();
    Shadows.endVolumes([floor, ...walls, ...casters]);
}
```

Depth-pass counting (the GS tests GEQUAL/GREATER, never "behind"). The camera never ends up inside a volume: each extruded vertex stops just in front of the near plane, and the caps close the volume there. A caster's own faces turned from the light are where the volume starts, so they are not shadowed by it. See [bin/torchlight.js](../bin/torchlight.js): a point light, DOT3 everywhere and six volumes.
