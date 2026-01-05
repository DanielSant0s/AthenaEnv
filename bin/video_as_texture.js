/**
 * Video as Texture Sample
 * 
 * This sample demonstrates how to use video frames as textures
 * for advanced effects like rotating video, multiple instances,
 * or applying to 3D objects.
 * 
 * Place a video.m2v file in the same directory as this script.
 */

// Load the video
const video = new Video("video.m2v");
video.loop = true;
video.play();

console.log("Video as Texture Demo");
console.log("  Resolution: " + video.width + "x" + video.height);

let angle = 0;
let scale = 1.0;
let scaleDir = 0.01;

const frame = video.frame;

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

// Main render loop
while (true) {
    Screen.clear(Color.new(20, 20, 40));

    // Update video frame
    video.update();

    // Get the current frame as an Image reference
    // This does NOT allocate new memory - it's a reference!

    if (frame) {
        // Draw main video in center
        const centerX = 320 - (video.width * scale) / 2;
        const centerY = 224 - (video.height * scale) / 2;

        frame.width = video.width * scale;
        frame.height = video.height * scale;
        frame.angle = angle;
        frame.draw(centerX, centerY);

        // Draw smaller thumbnails in corners
        frame.width = 80;
        frame.height = 60;
        frame.angle = 0;

        frame.draw(10, 10);       // Top-left
        frame.draw(550, 10);      // Top-right
        frame.draw(10, 380);      // Bottom-left
        frame.draw(550, 380);     // Bottom-right
    }

    // Animate rotation and scale
    angle += 0.01;
    if (angle >= 360) angle = 0;

    scale += scaleDir;
    if (scale > 1.5 || scale < 0.5) {
        scaleDir = -scaleDir;
    }

    Screen.flip();
}

video.free();
console.log("Demo finished!");
