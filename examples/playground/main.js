
import { Terminal, exitToShell } from 'athenaos/app.js';

const term = new Terminal({ title: "Playground", category: "system", subtitle: "Vector2, Vector3, Matrix4 and native calls",
                            hints: [["D-pad", "Log input"]] });

let v3 = new Vector3(15.0f, 15.0f, 15.0f);
let v4 = new Vector3(30.0f, 30.0f, 30.0f);

term.log(`Vector3 ${v3.add(v4)}`);
term.log(`Vector3 dist ${v3.dist(v4)}`);

let v1 = new Vector2(15.0f, 15.0f);
let v2 = new Vector2(30.0f, 30.0f);

term.log(`Vectors distance is ${v1.dist(v2)}`);
term.log(`Vectors add is ${v1.add(v2)}`);
term.log(`Vectors sub is ${v1.sub(v2)}`);
term.log(`Vectors mul is ${v1.mul(v2)}`);
term.log(`Vectors div is ${v1.div(v2)}`);

const test_matrix = new Matrix4();
term.log(`Matrix4 ${test_matrix}`);

const m1 = new Matrix4();

if (m1.equals(test_matrix)) {
    term.log("Matrix equal check for equal matrix values");
}

for (let i = 0; i < test_matrix.length; i++) {
    test_matrix[i] = Math.randomf(0.0f, 1.0f);
}
term.log(`Matrix4 ${test_matrix}`);

if (!m1.equals(test_matrix)) {
    term.log("Matrix equal check for different matrix values");
}

const OsdConfigParam = new ArrayBuffer(4);
System.nativeCall(System.findRelocObject("GetOsdConfigParam"), [{type: System.JS_BUFFER, value: OsdConfigParam}]);

term.log(`GetOsdConfigParam: ${(new Uint32Array(OsdConfigParam))[0].toString()}`);

const pad = Pads.get();

while (true) {
    pad.update();
    if (term.homeRequested(pad)) break;

    if (pad.justPressed(Pads.LEFT)) term.log("DPad Left button was just pressed");
    if (pad.justPressed(Pads.RIGHT)) term.log("DPad Right button was just pressed");

    term.draw();
}

exitToShell();
