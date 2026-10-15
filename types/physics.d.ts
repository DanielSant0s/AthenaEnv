/**
 * AthenaEnv — Physics and collision engine (`Physics`).
 *
 * Verified against src/js_api/ath_physics.c.
 */

/** 3×3 rotation matrix represented as a 12-element float array (3 rows × 4 floats, last column padded). */
type PhysicsMatrix3 = [
  number, number, number, number,
  number, number, number, number,
  number, number, number, number,
];

/** Contact point generated during collision detection. */
interface PhysicsContact {
  position: XYZ;
  normal: XYZ;
  depth: number;
}

/** Physics dynamics world. */
interface PhysicsWorld {
  setGravity(x: number, y: number, z: number): void;
  getGravity(): XYZ;
  setCFM(cfm: number): void;
  getCFM(): number;
  setERP(erp: number): void;
  getERP(): number;
  step(stepSize: number): void;
  quickStep(stepSize: number): void;
  setQuickStepNumIterations(num: number): void;
  getQuickStepNumIterations(): number;
  stepWithContacts(space: PhysicsSpace, jointGroup: PhysicsJointGroup, stepSize: number): void;
  destroyWorld(): void;
}

/** Rigid body in a physics world. */
interface PhysicsBody {
  setPosition(x: number, y: number, z: number): void;
  getPosition(): XYZ;
  setRotation(matrix: PhysicsMatrix3): void;
  getRotation(): PhysicsMatrix3;
  setLinearVel(vx: number, vy: number, vz: number): void;
  getLinearVel(): XYZ;
  setAngularVel(wx: number, wy: number, wz: number): void;
  getAngularVel(): XYZ;
  setMass(mass: number): void;
  setMassBox(density: number, lx: number, ly: number, lz: number): void;
  setMassSphere(density: number, radius: number): void;
  getMass(): number;
  addForce(fx: number, fy: number, fz: number): void;
  addTorque(tx: number, ty: number, tz: number): void;
  enable(): void;
  disable(): void;
  enabled(): boolean;
  free(): void;
}

/** Collision geometry. */
interface PhysicsGeom {
  setPosition(x: number, y: number, z: number): void;
  getPosition(): XYZ;
  setRotation(matrix: PhysicsMatrix3): void;
  getRotation(): PhysicsMatrix3;
  setBody(body: PhysicsBody): void;
  getBody(): PhysicsBody | null;
  enable(): void;
  disable(): void;
  enabled(): boolean;
  free(): void;
}

/** Ray geometry with ray-specific methods (extends PhysicsGeom). */
interface PhysicsRayGeom extends PhysicsGeom {
  raySetLength(length: number): void;
  rayGetLength(): number;
  raySet(px: number, py: number, pz: number, dx: number, dy: number, dz: number): void;
  rayGet(): { start: XYZ; direction: XYZ };
  raySetParams(firstContact: number | boolean, backfaceCull: number | boolean): void;
  rayGetParams(): { firstContact: boolean; backfaceCull: boolean };
  raySetClosestHit(closestHit: number | boolean): void;
  rayGetClosestHit(): boolean;
}

/** Collision space containing geometries. */
interface PhysicsSpace {
  /**
   * Detect collisions within this space.
   * @param callback Optional function called for each contact point.
   * @returns Total number of contacts detected.
   */
  collide(callback?: (contact: PhysicsContact) => void): number;
  free(): void;
}

/** Constraint joint connecting two bodies. */
interface PhysicsJoint {
  attach(body1: PhysicsBody | null, body2: PhysicsBody | null): void;
  setBallAnchor(x: number, y: number, z: number): void;
  setHingeAnchor(x: number, y: number, z: number): void;
  setHingeAxis(x: number, y: number, z: number): void;
  setSliderAxis(x: number, y: number, z: number): void;
  getBallAnchor(): XYZ;
  getHingeAnchor(): XYZ;
  getHingeAxis(): XYZ;
  getHingeAngle(): number;
  getHingeAngleRate(): number;
  getSliderPosition(): number;
  getSliderPositionRate(): number;
  free(): void;
}

/** Group of joints (typically used for contact joints). */
interface PhysicsJointGroup {
  empty(): void;
  free(): void;
}

/** Physics and collision detection module. */
declare namespace Physics {
  /** Create a new dynamics world. */
  function World(): PhysicsWorld;
  /** Create a rigid body in a world. */
  function Body(world: PhysicsWorld): PhysicsBody;
  /** Create a collision space, optionally inside a parent space. */
  function Space(parent?: PhysicsSpace): PhysicsSpace;
  /** Create a joint group. */
  function JointGroup(): PhysicsJointGroup;

  // Geometry constructors
  /** Create a box collision geometry. */
  function GeomBox(space: PhysicsSpace | null | undefined, lx: number, ly: number, lz: number): PhysicsGeom;
  /** Create a sphere collision geometry. */
  function GeomSphere(space: PhysicsSpace | null | undefined, radius: number): PhysicsGeom;
  /** Create a plane collision geometry (`ax + by + cz = d`). */
  function GeomPlane(space: PhysicsSpace | null | undefined, a: number, b: number, c: number, d: number): PhysicsGeom;
  /** Create a capsule (capped cylinder) collision geometry. */
  function GeomCapsule(space: PhysicsSpace | null | undefined, radius: number, length: number): PhysicsGeom;
  /** Create a cylinder collision geometry. */
  function GeomCylinder(space: PhysicsSpace | null | undefined, radius: number, length: number): PhysicsGeom;
  /** Create a ray collision geometry. */
  function GeomRay(space: PhysicsSpace | null | undefined, length: number): PhysicsRayGeom;
  /** Create a triangle mesh collision geometry from a `RenderData` resource. */
  function GeomTriangleMesh(space: PhysicsSpace | null | undefined, model: RenderData): PhysicsGeom;
  /** Create a triangle mesh collision geometry from a `RenderObject` instance. */
  function GeomRenderObject(space: PhysicsSpace | null | undefined, obj: RenderObject): PhysicsGeom;
  /** Create a geometry transform wrapper. */
  function GeomTransform(space: PhysicsSpace | null | undefined, childGeom: PhysicsGeom): PhysicsGeom;

  // Joint constructors
  /** Create a ball-and-socket joint. */
  function JointBall(world: PhysicsWorld, group?: PhysicsJointGroup): PhysicsJoint;
  /** Create a hinge joint. */
  function JointHinge(world: PhysicsWorld, group?: PhysicsJointGroup): PhysicsJoint;
  /** Create a slider (prismatic) joint. */
  function JointSlider(world: PhysicsWorld, group?: PhysicsJointGroup): PhysicsJoint;
  /** Create a fixed joint. */
  function JointFixed(world: PhysicsWorld, group?: PhysicsJointGroup): PhysicsJoint;

  /** Test collision between two individual geometries. */
  function collide(geom1: PhysicsGeom, geom2: PhysicsGeom): PhysicsContact[];
  /** Clean up global physics resources. */
  function cleanup(): void;
}
