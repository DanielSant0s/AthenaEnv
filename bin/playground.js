// {"name": "Playground", "author": "Daniel Santos", "version": "06022024", "file": "playground.js"}

let v3 = new Vector3(15.0f, 15.0f, 15.0f);
let v4 = new Vector3(30.0f, 30.0f, 30.0f);

console.log(`Vector3 ${v3 + v4}`);
console.log(`Vector3 dist ${v3.dist(v4)}`);

let v1 = new Vector2(15.0f, 15.0f);
let v2 = new Vector2(30.0f, 30.0f);

console.log(`Vectors distance is ${v1.dist(v2)}`);
console.log(`Vectors add is ${v1 + v2}`);
console.log(`Vectors sub is ${v1 - v2}`);
console.log(`Vectors mul is ${v1 * v2}`);
console.log(`Vectors div is ${v1 / v2}`);

const test_matrix = new Matrix4();
console.log(`Matrix4 ${test_matrix}`);
for (let i = 0; i < test_matrix.length; i++) {
    console.log(`m[${i}] = ${test_matrix[i]}`);
}

let pad = Pads.get();

const m1 = new Matrix4();

if (m1 == test_matrix) {
    console.log("Matrix equal check for equal matrix values");
}

for (let i = 0; i < test_matrix.length; i++) {
    test_matrix[i] = Math.randomf(0.0f, 1.0f);
}
console.log(`Matrix4 ${test_matrix}`);

if (m1 != test_matrix) {
    console.log("Matrix equal check for different matrix values");
}

pad.setEventHandler();

Pads.newEvent(Pads.LEFT, Pads.JUST_PRESSED, () => { 
    console.log(`DPad Left button was just pressed`); 
});

Pads.newEvent(Pads.RIGHT, Pads.JUST_PRESSED, () => { 
    console.log(`DPad Right button was just pressed`); 
});

Pads.newEvent(Pads.START, Pads.JUST_PRESSED, () => { 
    std.reload("main.js");
});

