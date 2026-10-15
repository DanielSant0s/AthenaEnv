# Audio API

`Sound`. For MPEG video playback, see [Video](VIDEO.md).

## Sound module

* `Sound.setVolume(volume)` - Set master volume.
* `Sound.findChannel()` - Returns the first free channel found to be used on sound effect playback.
* `const bgm = Sound.Stream(path)` - Loads an audio stream file (WAV, OGG)

  **Methods:**
  * `play()` - Play (or resume) audio stream.
  * `free()` - Free audio stream from memory.
  * `pause()` - Pause audio stream.
  * `playing()` - Check if the audio stream is being played.
  * `rewind()` - Restart audio to its beginning (should call `play()` again if it's not the current track).

  **Properties:**
  * `position` - Current track playtime in msec, you can get or change.
  * `length` - Current track duration in msec, read-only property.
  * `loop` - If the track is played in a loop, you can get or change.

* `const shoot_sfx = Sound.Sfx(path)` - Loads a sound effect (ADPCM)

  **Methods:**
  * `play(*channel*)` - Play sound effect, returns the channel index used. If channel isn't specified, it will automatically use a free channel.
  * `free()` - Free sound effect from memory.
  * `playing(channel)` - Check if the sound effect is being played on the specified channel.

  **Properties:**
  * `volume` - Current sound effect volume, you can get or change from 0 to 100.
  * `pan` - Sound effect spatial setting, you can get or change from -100 (left) to 100 (right), 0 is the center.
  * `pitch` - Sound effect pitch, you can get or change from -100 to 100, 0 is the default value.
