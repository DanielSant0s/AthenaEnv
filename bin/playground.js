// {"name": "Playground", "author": "Daniel Santos", "version": "06022024", "file": "playground.js"}

let v1 = Vector2.new(15.0f, 15.0f);
let v2 = Vector2.new(30.0f, 30.0f);

Screen.log(`Vectors distance is ${v1.dist(v2)}`)
Screen.log(`Vectors add is ${Vector2.add(v1, v2).toString()}`);
Screen.log(`Vectors sub is ${Vector2.sub(v1, v2).toString()}`);
Screen.log(`Vectors mul is ${Vector2.mul(v1, v2).toString()}`);
Screen.log(`Vectors div is ${Vector2.div(v1, v2).toString()}`);

let pad = Pads.get();

pad.setEventHandler();

Pads.newEvent(Pads.LEFT, Pads.JUST_PRESSED, () => { 
    Screen.log(`DPad Left button was just pressed`); 
});