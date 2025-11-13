# TileMap Runtime

AthenaEnv ships with a tilemap renderer purpose-built for PlayStation 2 hardware. It streams sprite data to a custom VU1 microprogram, so you get thousands of quads per frame with minimal EE involvement. This document explains how to work with the API exposed through the `TileMap` module.

## Concepts

| Component | Description |
|-----------|-------------|
| `TileMap.Descriptor` | Immutable render description: texture list, materials (texture index + blend equation), and layout metadata. Create it once per tileset or level. |
| `TileMap.Instance` | A drawable that references a descriptor plus a sprite buffer. Multiple instances can share the same descriptor. |
| `TileMap.SpriteBuffer` | Helpers to create native buffers. Use `create(count)` for zeroed memory or `fromObjects(array)` to convert structured JS into the native layout. |
| `TileMap.layout` | Exposes `stride` and per-field offsets (`x`, `y`, `w`, …) so you can edit `ArrayBuffer`s directly with `DataView`/TypedArray without guessing offsets. |

The renderer batches sprites by material order, binds textures lazily, and reuses DMA packets each frame. Your job is to keep sprite data contiguous and aligned to the layout.

## Minimal Example

```js
import TileMap from 'TileMap';

const descriptor = new TileMap.Descriptor({
  textures: ['simple_atlas.png'],
  materials: [{
    texture_index: 0,
    blend_mode: Screen.alphaEquation(Screen.ZERO_RGB, Screen.SRC_RGB, Screen.SRC_ALPHA, Screen.DST_RGB, 0),
    end_offset: 23, // last sprite index handled by this material
  }],
});

const sprites = TileMap.SpriteBuffer.fromObjects([
  { x: 0, y: 0, zindex: 1, w: 32, h: 32, u1: 0, v1: 0, u2: 32, v2: 32, r: 255, g: 255, b: 255, a: 255 },
  // ... more quads ...
]);

const map = new TileMap.Instance({ descriptor, spriteBuffer: sprites });
```

You still call `TileMap.init()` once at startup and `TileMap.begin()` each frame before drawing instances.

## Updating Sprites In-Place

Grab the native buffer and mutate it through `DataView` or TypedArrays using the shared layout description:

```js
const layout = TileMap.layout;
const view = new DataView(map.getSpriteBuffer());

function setFloat(idx, offset, value) {
  view.setFloat32(idx * layout.stride + offset, value, true);
}

const basePositions = [...];
let time = 0;

function animate() {
  time += 0.05;
  for (let i = 0; i < basePositions.length; i++) {
    const phase = time + i * 0.2;
    const wobble = Math.sin(phase) * 4;
    setFloat(i, layout.offsets.x, basePositions[i].x + wobble);
    setFloat(i, layout.offsets.y, basePositions[i].y);
  }
}
```

Because you operate on the underlying buffer, no C-side allocation happens when tiles move, and the renderer simply reuses the updated data on the next `render` call.

## Replacing or Streaming Buffers

`TileMap.Instance` exposes two mutators:

* `replaceSpriteBuffer(arrayBuffer)` – swap the pointer to an entirely new buffer (must match the native layout).
* `updateSprites(dstOffset, srcBuffer[, spriteCount])` – copy a partial range from another buffer (useful for streaming tiles in chunks).

Use these to implement streaming tilemaps, network-loaded levels, or background workers that build buffers off-thread.

## Reference

* Sprite layout: `athena_sprite_data` in `src/include/tile_render.h`.
* Renderer implementation: `src/tile_render.c` (shows batching, alpha handling, texture binding).
* JavaScript binding: `src/js_api/ath_sprite.c`.
* Example: `bin/tilemap.js` animates a grid by writing into a `DataView` every frame.

With the descriptor/buffer split you can keep large tilesets in memory without re-uploading textures and still enjoy low-cost sprite mutations.
