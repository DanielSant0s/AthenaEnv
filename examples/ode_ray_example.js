// Example of using ODE Ray functions
// This example demonstrates how to create and use rays for collision detection

// Create world and space
const world = ODE.World();
const space = ODE.Space();

// Configure gravity
world.setGravity(0, -9.81, 0);

// Create some objects to test collision
const groundPlane = ODE.GeomPlane(space, 0, 1, 0, 0); // Ground plane
const sphere = ODE.GeomSphere(space, 1.0); // Sphere with radius 1.0
sphere.setPosition(0, 5, 0);

// Create a ray
const ray = ODE.GeomRay(space, 100.0); // Ray with length of 100 units

// Set ray position and direction (top to bottom)
// Start position: (0, 10, 0), Direction: (0, -1, 0) - pointing downward
ray.raySet(
    0, 10, 0,  // Start position (x, y, z)
    0, -1, 0   // Direction (dx, dy, dz)
);

// Configure ray parameters
ray.raySetParams(
    1,  // firstContact: return at first contact
    0   // backfaceCull: don't eliminate back faces
);

// Enable closest hit mode
ray.raySetClosestHit(1);

// Get ray information
const rayInfo = ray.rayGet();
console.log('Ray start position:', rayInfo.start);
console.log('Ray direction:', rayInfo.direction);
console.log('Ray length:', ray.rayGetLength());

const params = ray.rayGetParams();
console.log('First Contact:', params.firstContact);
console.log('Backface Cull:', params.backfaceCull);
console.log('Closest Hit:', ray.rayGetClosestHit());

// Perform collision detection between ray and other objects
const contacts = space.collide((contact) => {
    console.log('Collision detected!');
    console.log('Position:', contact.position);
    console.log('Normal:', contact.normal);
    console.log('Depth:', contact.depth);
});

// Modify ray length
ray.raySetLength(50.0);
console.log('New length:', ray.rayGetLength());

// Change ray direction (now pointing forward)
ray.raySet(
    0, 10, 0,   // Start position
    0, 0, 1     // Direction (forward on Z axis)
);

// Cleanup
ray.free();
sphere.free();
groundPlane.free();
space.free();
world.destroyWorld();
ODE.cleanup();

console.log('Example completed!');

