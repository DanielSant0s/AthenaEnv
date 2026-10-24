export class Element {
    render  = (ctx) => {};
    control = (ctx, pad) => {};

    constructor(setup, render, control) {
        this.render = () => render(this);
        this.control = (pad) => control(this, pad);
        setup(this);
    }
}

export class Component {
    elements = [];

    constructor(elements) {
        if (typeof elements !== "undefined") {
            this.elements.push(...elements);
        }
    }

    run(pad) {
        this.elements.forEach((element) => { 
            element.control(pad);
            element.render(pad);
        });
    }
}

export class Menu {
    components = [];
    title = '';

    constructor(title, entries, setup, draw, controls) {
        this.title = title;
        this.components.push(new Component([
            new Element(
                (ctx) => { 
                    ctx.title = title;
                    ctx.entries = entries;
                    ctx.ptr = 0;

                    setup(ctx);
                }, 
                draw, 
                (ctx, pad) => {
                    controls.forEach((control) => { control(ctx, pad) });
                },
            )
        ]));
    }

    run(pad) {
        this.components.forEach((component) => component.run(pad));
    }
}

export class MenuList {
    menus = [];
    draw = () => {};
    menu_run = () => {};
    control = () => {};

    constructor(menus, setup, draw, control, menu_run) {
        this.menus.push(...menus);
        this.draw = () => draw(this);
        this.control = (pad) => control(this, pad);
        this.menu_run = (pad) => menu_run(this, pad);
        setup(this);
    }

    run(pad) {
        this.draw();
        this.control(pad);
        this.menu_run(pad);
    }
}

export class Interface {
    components = [];
    pad = undefined;

    constructor(pad, components) {
        this.pad = pad;
        this.components.push(...components);
    }

    run() {
        this.components.forEach((component) => component.run(this.pad));
    }
}