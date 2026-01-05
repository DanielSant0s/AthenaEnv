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


Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
// Load the video file
const video = new Video("video.m2v");


// Enable looping (optional)
video.loop = true;

// Start playback
video.play();

console.log("Video loaded:");
console.log("  Width: " + video.width);
console.log("  Height: " + video.height);
console.log("  FPS: " + video.fps);

// Main loop
while (!video.ended) {
    // Clear screen with black background
    Screen.clear(Color.new(0, 0, 0));

    // Update video - handles frame timing and decoding
    video.update();

    // Draw video fullscreen
    video.draw(0, 0, 640, 448);

    // Optional: Display some info
    // Font.print(font, 10, 10, "Frame: " + video.currentFrame);

    Screen.flip();
}

// Clean up
video.free();
console.log("Video playback finished!");
