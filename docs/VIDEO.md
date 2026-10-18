# Video playback

AthenaEnv can decode MPEG-1/2 video through the `Video` class, backed by the
vendored `libmpeg` (`src/libmpeg`) and the IPU (Image Processing Unit) hardware
decoder. This document covers what kind of file to feed it and how to use the
JS API.

By default `Video` lives in `athena_mpeg.erl`. Importing it also installs the global, so `new Video(...)` works after the import.

## Source format requirements

The decoder expects a **raw MPEG-1/2 elementary video stream** (`.m2v`/`.mpg`
video-only stream) — it does **not** demux containers. Don't point it at an
`.mp4`/`.mkv`/`.avi` file directly; extract the raw video stream first.

Hard requirements (from the underlying MPEG decoder, not configurable):

- **4:2:0 chroma subsampling only.** 4:2:2/4:4:4 streams are not supported.
- **No MPEG-2 scalable extensions** (SNR/spatial/temporal scalability).
- Raw elementary stream only — no PS/TS/PES container, no audio.

### Recommended encode

```bash
ffmpeg -i input.mp4 \
  -vf scale=640:360 \
  -c:v mpeg2video -b:v 2000k \
  -g 15 \
  video.m2v
```

- `-g 15` — GOP size of 15 frames.

Keep the resolution modest (e.g. 640x360 or smaller) and the bitrate within a
few Mbps — the EE has to color-convert and upload every decoded frame as a
texture each frame, on top of running your script.

## JS API

```js
import { Video } from 'athena_mpeg.erl';

const video = new Video("video.m2v");

video.loop = false;   // restart automatically on end
video.play();

while (!video.ended) {
    Screen.clear(Color.new(0, 0, 0));

    video.update();               // advances decoding on its own timing
    video.draw(0, 0, 640, 448);   // 0 width/height = use the video's own size

    Screen.flip();
}

video.free();
```

### Constructor

- `new Video(path)` — opens `path` and decodes the sequence header. Throws if
  the file can't be opened or isn't a valid MPEG-1/2 elementary stream.

### Methods

| Method | Description |
|---|---|
| `play()` | Starts/resumes playback. |
| `pause()` | Pauses at the current frame. |
| `stop()` | Stops and rewinds to the beginning. |
| `update()` | Advances decoding by one frame when enough time has passed (paced by the stream's own frame rate). Returns `true` if a new frame was decoded. Call this once per your own render loop iteration while playing. |
| `draw(x, y, w, h)` | Draws the current frame. `w`/`h` default to the video's native size when `0`. |
| `free()` | Releases the decoder and its buffers. |

### Properties

| Property | Type | Description |
|---|---|---|
| `width`, `height` | `number` | Video dimensions, valid once `ready` is `true`. |
| `fps` | `number` | Frame rate read from the sequence header. |
| `ready` | `boolean` | `true` once the sequence header has been parsed and the first frame decoded. |
| `ended` | `boolean` | `true` once playback has reached the end of the stream (and isn't looping). |
| `playing` | `boolean` | `true` while actively playing (not paused/stopped/ended). |
| `loop` | `boolean` (r/w) | Restart from the beginning instead of ending. |
| `frame` | `Image` | The current decoded frame as an `Image`, for use as a texture instead of/in addition to `draw()`. |
| `currentFrame` | `number` | Index of the currently decoded frame. |

## Troubleshooting

- **Colors look wrong:** confirm the source was encoded 4:2:0, not 4:2:2/4:4:4.
- **Nothing decodes / constructor throws:** the file is probably a container
  (`.mp4` etc.), not a raw elementary stream — demux it first.
