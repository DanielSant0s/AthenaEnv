/**
 * AthenaEnv — ODE physics bindings (`ODE` global namespace).
 *
 * Wraps the Open Dynamics Engine. `ODE` is a plain namespace object;
 * `World`, `Space`, `Body` and `JointGroup` are factory functions on it
 * (not `new`-able classes at the JS call-site, even though they return
 * class instances). Verified against `src/js_api/ath_ode.c`.
 */

/** `[x, y, z]` */
type Vec3Tuple = [number, number, number];
/** Row-major 3x3 rotation matrix as a flat 9-element array. */
type ODEMatrix3 = number[];

interface ODEContact {
    position: Vec3Tuple;
    normal: Vec3Tuple;
    depth: number;
    /** Present on {@link ODESpace.collide} / {@link ODEWorld.stepWithContacts} results; absent from {@link ODENamespace.geomCollide}. */
    geom1?: ODEGeom;
    geom2?: ODEGeom;
}

declare class ODEWorld {
    setGravity(x: number, y: number, z: number): void;
    getGravity(): Vec3Tuple;
    /** Constraint Force Mixing parameter. */
    setCFM(value: number): void;
    /** Error Reduction Parameter. */
    setERP(value: number): void;
    /** Advances the simulation by `dt` seconds using the standard (accurate, slower) integrator. */
    step(dt: number): void;
    /** Advances the simulation by `dt` seconds using the quick (iterative, faster) integrator. */
    quickStep(dt: number): void;
    setQuickStepIterations(iterations: number): void;
    /**
     * Runs collision detection on `space`, creates contact joints in
     * `jointGroup`, steps the world by `dt`, then empties `jointGroup`.
     * @param callback Optional; invoked once per contact with the contact object.
     * @returns every contact found this step.
     * @remarks Parameter order is `(space, jointGroup, dt, callback?)`.
     */
    stepWithContacts(space: ODESpace, jointGroup: ODEJointGroup, dt: number, callback?: (contact: ODEContact) => void): ODEContact[];
    /** Frees all resources associated with the world. */
    destroyWorld(): void;
}

declare class ODEBody {
    setPosition(x: number, y: number, z: number): void;
    getPosition(): Vec3Tuple;
    /** Accepts a flat 9-element row-major rotation matrix. */
    setRotation(matrix: ODEMatrix3): void;
    getRotation(): ODEMatrix3;
    setLinearVel(x: number, y: number, z: number): void;
    getLinearVel(): Vec3Tuple;
    setAngularVel(x: number, y: number, z: number): void;
    getAngularVel(): Vec3Tuple;
    /** @remarks Internally treats the body as a unit sphere for inertia purposes — only the total mass value is guaranteed accurate; prefer {@link setMassBox}/{@link setMassSphere} when inertia shape matters. */
    setMass(mass: number): void;
    setMassBox(density: number, lx: number, ly: number, lz: number): void;
    setMassSphere(density: number, radius: number): void;
    addForce(x: number, y: number, z: number): void;
    addTorque(x: number, y: number, z: number): void;
    enable(): void;
    disable(): void;
    enabled(): boolean;
    free(): void;
}

declare class ODEGeom {
    setPosition(x: number, y: number, z: number): void;
    setRotation(matrix: ODEMatrix3): void;
    getPosition(): Vec3Tuple;
    getRotation(): ODEMatrix3;
    setBody(body: ODEBody): void;
    /** @returns the attached body, or `null` if this geom has none. */
    getBody(): ODEBody | null;
    free(): void;
}

/** A Geom created via {@link ODENamespace.GeomRay}; adds ray-specific methods not present on plain {@link ODEGeom} instances. */
declare class ODERayGeom extends ODEGeom {
    raySetLength(length: number): void;
    rayGetLength(): number;
    raySet(px: number, py: number, pz: number, dx: number, dy: number, dz: number): void;
    rayGet(): { start: Vec3Tuple; direction: Vec3Tuple };
    /**
     * @param firstContact If true, stop at the first contact found.
     * @param backfaceCull If true, ignore back-facing surfaces.
     */
    raySetParams(firstContact: boolean, backfaceCull: boolean): void;
    rayGetParams(): { firstContact: boolean; backfaceCull: boolean };
    raySetClosestHit(closestHit: boolean): void;
    rayGetClosestHit(): boolean;
}

declare class ODESpace {
    /**
     * Runs collision detection between all geometries in the space.
     * @param callback Optional; invoked once per contact found (not once per pair) with the contact object.
     * @returns every contact found.
     */
    collide(callback?: (contact: ODEContact) => void): ODEContact[];
    free(): void;
}

declare class ODEJointGroup {
    /** Removes all joints from the group. */
    empty(): void;
    free(): void;
}

declare class ODEJoint {
    free(): void;
    attach(body1: ODEBody, body2: ODEBody): void;

    // Ball
    setBallAnchor(x: number, y: number, z: number): void;
    getBallAnchor(): Vec3Tuple;

    // Hinge
    setHingeAnchor(x: number, y: number, z: number): void;
    setHingeAxis(x: number, y: number, z: number): void;
    addHingeTorque(t: number): void;
    getHingeAnchor(): Vec3Tuple;
    getHingeAxis(): Vec3Tuple;
    getHingeAngle(): number;
    getHingeAngleRate(): number;

    // Slider
    setSliderAxis(x: number, y: number, z: number): void;
    addSliderForce(f: number): void;
    getSliderAxis(): Vec3Tuple;
    getSliderPosition(): number;
    getSliderPositionRate(): number;

    // Hinge2
    setHinge2Anchor(x: number, y: number, z: number): void;
    setHinge2Axis1(x: number, y: number, z: number): void;
    setHinge2Axis2(x: number, y: number, z: number): void;
    AddHinge2Torques(t1: number, t2: number): void;
    getHinge2Anchor(): Vec3Tuple;
    getHinge2Axis1(): Vec3Tuple;
    getHinge2Axis2(): Vec3Tuple;
    getHinge2Angle1(): number;
    getHinge2Angle1Rate(): number;
    getHinge2Angle2Rate(): number;

    // Universal
    setUniversalAnchor(x: number, y: number, z: number): void;
    setUniversalAxis1(x: number, y: number, z: number): void;
    setUniversalAxis2(x: number, y: number, z: number): void;
    setUniversalTorques(t1: number, t2: number): void;
    getUniversalAnchor(): Vec3Tuple;
    getUniversalAxis1(): Vec3Tuple;
    getUniversalAxis2(): Vec3Tuple;
    getUniversalAngle1(): number;
    getUniversalAngle2(): number;
    getUniversalAngle1Rate1(): number;
    getUniversalAngle2Rate2(): number;

    // Fixed
    /** Locks the joint at the two bodies' current relative pose. */
    setFixed(): void;

    // AMotor
    setAMotorNumAxes(n: number): void;
    setAMotorAxis(index: number, rel: number, x: number, y: number, z: number): void;
    setAMotorAngle(axis: number, angle: number): void;
    setAMotorMode(mode: number): void;
    setAMotorTorques(x: number, y: number, z: number): void;
    getAMotorNumAxes(): number;
    getAMotorAxis(index: number): Vec3Tuple;
    getAMotorAxisRel(index: number): number;
    getAMotorAngle(index: number): number;
    getAMotorAngleRate(index: number): number;
    getAMotorMode(): number;
}

interface ODENamespace {
    /** Finalizes ODE and releases global resources. */
    cleanup(): void;
    /** @remarks Parameter order is `(space, renderObject)`. */
    GeomRenderObject(space: ODESpace, renderObject: RenderObject): ODEGeom;
    GeomBox(space: ODESpace, width: number, height: number, depth: number): ODEGeom;
    GeomSphere(space: ODESpace, radius: number): ODEGeom;
    /** Defines a plane `Ax + By + Cz = D`. */
    GeomPlane(space: ODESpace, a: number, b: number, c: number, d: number): ODEGeom;
    GeomTransform(space: ODESpace, geom: ODEGeom): ODEGeom;
    /** Creates a ray geometry for raycasting. */
    GeomRay(space: ODESpace | null, length: number): ODERayGeom;
    /** Checks for collision between two geometries (or spaces). @returns every contact found (without `geom1`/`geom2` fields). */
    geomCollide(geom1: ODEGeom, geom2: ODEGeom): ODEContact[];

    JointBall(world: ODEWorld, group: ODEJointGroup): ODEJoint;
    JointHinge(world: ODEWorld, group: ODEJointGroup): ODEJoint;
    JointSlider(world: ODEWorld, group: ODEJointGroup): ODEJoint;
    JointHinge2(world: ODEWorld, group: ODEJointGroup): ODEJoint;
    JointUniversal(world: ODEWorld, group: ODEJointGroup): ODEJoint;
    JointFixed(world: ODEWorld, group: ODEJointGroup): ODEJoint;
    JointNull(world: ODEWorld, group: ODEJointGroup): ODEJoint;
    JointAMotor(world: ODEWorld, group: ODEJointGroup): ODEJoint;

    World(): ODEWorld;
    /** @param parent Optional parent space, for creating a subspace. */
    Space(parent?: ODESpace): ODESpace;
    Body(world: ODEWorld): ODEBody;
    JointGroup(): ODEJointGroup;
}

declare const ODE: ODENamespace;
