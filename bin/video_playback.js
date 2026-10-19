// {"name": "Video Playback", "author": "Daniel Santos", "version": "08022026", "file": "video_playback.js", "category": "media", "blurb": "Fullscreen MPEG-1/2 playback.", "exit": "TRIANGLE"}

/**
 * MPEG Video Playback Sample
 * 
 * This sample demonstrates how to use the Video class to play
 * MPEG1/2 videos on the PS2, including using video as a texture.
 * 
 * Place a video.m2v file in the same directory as this script.
 * 
 * MPEG2 elementary stream (.m2v) can be created with ffmpeg:
 *   ffmpeg -i input.mp4 -vf scale=320:240 -c:v mpeg2video -b:v 2000k video.m2v
 */

import { Video } from 'athena_mpeg.erl';
import { AppUI, exitToShell } from 'athenaos/app.js';

const ui = new AppUI({ title: "Video Playback", category: "media", hints: [["square", "Loop"]] });

// Load the video file
const video = new Video("video.m2v");


// Enable looping (optional)
video.loop = false;

// Start playback
video.play();

console.log("Video loaded:");
console.log("  Width: " + video.width);
console.log("  Height: " + video.height);
console.log("  FPS: " + video.fps);

let should_finish = false;

const pad = Pads.get();

// Main loop
while (!video.ended && !should_finish) {
    pad.update();

    if (ui.homeRequested(pad)) {
        should_finish = true;
    } else if (pad.justPressed(Pads.SQUARE)) {
        video.loop ^= 1;
    }

    // Clear screen with black background
    Screen.clear(Color.new(0, 0, 0));

    // Update video - handles frame timing and decoding
    video.update();

    // Draw video fullscreen
    video.draw(0, 0, video.width, video.height);

    // Chrome hides itself after a few idle seconds; any button brings it back.
    ui.status = video.loop ? "Loop ON" : "";
    ui.autoChrome(pad);

    Screen.flip();
}

// Clean up
video.free();
console.log("Video playback finished!");

exitToShell();
