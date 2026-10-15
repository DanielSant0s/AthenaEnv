# Physics API (ODE)

The `ODE` module provides JavaScript bindings for the **Open Dynamics Engine (ODE)**. It allows creating and manipulating worlds, rigid bodies, geometries, spaces, and physical joints.

## Table of Contents

1. [Class `World`](#world)
2. [Class `Body`](#body)
3. [Class `Geom`](#geom)
4. [Class `Space`](#space)
5. [Class `JointGroup`](#jointgroup)
6. [Class `Joint`](#joint)
   * [Ball Joint](#ball-joint)
   * [Hinge Joint](#hinge-joint)
   * [Slider Joint](#slider-joint)
   * [Hinge2 Joint](#hinge2-joint)
   * [Universal Joint](#universal-joint)
   * [Fixed Joint](#fixed-joint)
   * [AMotor Joint](#amotor-joint)
7. [Global Functions (`ODE` namespace)](#global-functions-ode-namespace)
   * [Geometry Creation](#geometry-creation)
   * [Ray Methods](#ray-methods)
   * [Joint Creation](#joint-creation)
   * [Structure Creation](#structure-creation)

---

## World

Represents the physical simulation world. It defines global properties such as gravity and integration parameters.

### Methods

* `setGravity(x, y, z)` — Set the world gravity vector.
* `getGravity()` — Returns the gravity vector `[x, y, z]`.
* `setCFM(value)` — Set the Constraint Force Mixing parameter.
* `setERP(value)` — Set the Error Reduction Parameter.
* `step(dt)` — Advances the simulation by `dt` seconds.
* `quickStep(dt)` — Advances the simulation using the quick step integrator.
* `setQuickStepIterations(iter)` — Set the number of iterations for the quick step solver.
* `stepWithContacts(space, jointGroup, dt, *callback*)` — Runs collision detection on `space`, creates contact joints in `jointGroup`, advances the simulation by `dt`, then empties `jointGroup`. Returns the array of contacts found. If given, `callback` is invoked once per contact.
* `destroyWorld()` — Frees all resources associated with the world.

---

## Body

Represents a rigid body that can move, receive forces, and participate in collisions.

### Methods

* `setPosition(x, y, z)`
* `getPosition()` — Returns `[x, y, z]`.
* `setRotation(matrix)` — Set the rotation using a 3x3 matrix or quaternion.
* `getRotation()` — Returns the current rotation matrix.
* `setLinearVel(x, y, z)` — Set linear velocity.
* `getLinearVel()` — Returns `[vx, vy, vz]`.
* `setAngularVel(x, y, z)` — Set angular velocity.
* `getAngularVel()` — Returns `[wx, wy, wz]`.
* `setMass(mass)`
* `setMassBox(density, lx, ly, lz)` — Set mass from a box shape.
* `setMassSphere(density, radius)` — Set mass from a sphere shape.
* `addForce(x, y, z)` — Apply a linear force.
* `addTorque(x, y, z)` — Apply a torque.
* `enable()` — Enable simulation for the body.
* `disable()` — Disable simulation for the body.
* `enabled()` — Returns `true` if enabled, otherwise `false`.
* `free()` — Free the body resources.

---

## Geom

Represents a collision geometry. A geometry can optionally be attached to a `Body`.

### Methods

* `setPosition(x, y, z)`
* `setRotation(matrix)`
* `getPosition()`
* `getRotation()`
* `setBody(body)` — Attach this geometry to a body.
* `getBody()` — Returns the attached body.
* `free()`

---

## Space

A space groups geometries for collision detection.

### Methods

* `collide(*callback*)` — Runs collision detection between all geometries in the space. Returns the array of contacts found. If given, `callback` is invoked once per contact (not once per colliding pair) with the contact object.
* `free()`

---

## JointGroup

Represents a group of temporary joints, often used for collision contacts.

### Methods

* `empty()` — Removes all joints from the group.
* `free()` — Frees the group resources.

---

## Joint

Represents a constraint between two bodies.

### General Methods

* `free()` — Frees the joint.
* `attach(body1, body2)` — Attaches the joint to two bodies.

### Ball Joint

* `setBallAnchor(x, y, z)`
* `getBallAnchor()`

### Hinge Joint

* `setHingeAnchor(x, y, z)`
* `setHingeAxis(x, y, z)`
* `addHingeTorque(t)`
* `getHingeAnchor()`
* `getHingeAxis()`
* `getHingeAngle()`
* `getHingeAngleRate()`

### Slider Joint

* `setSliderAxis(x, y, z)`
* `addSliderForce(f)`
* `getSliderAxis()`
* `getSliderPosition()`
* `getSliderPositionRate()`

### Hinge2 Joint

* `setHinge2Anchor(x, y, z)`
* `setHinge2Axis1(x, y, z)`
* `setHinge2Axis2(x, y, z)`
* `AddHinge2Torques(t1, t2)`
* `getHinge2Anchor()`
* `getHinge2Axis1()`
* `getHinge2Axis2()`
* `getHinge2Angle1()`
* `getHinge2Angle1Rate()`
* `getHinge2Angle2Rate()`

### Universal Joint

* `setUniversalAnchor(x, y, z)`
* `setUniversalAxis1(x, y, z)`
* `setUniversalAxis2(x, y, z)`
* `setUniversalTorques(t1, t2)`
* `getUniversalAnchor()`
* `getUniversalAxis1()`
* `getUniversalAxis2()`
* `getUniversalAngle1()`
* `getUniversalAngle2()`
* `getUniversalAngle1Rate1()`
* `getUniversalAngle2Rate2()`

### Fixed Joint

* `setFixed()`

### AMotor Joint

* `setAMotorNumAxes(n)`
* `setAMotorAxis(index, rel, x, y, z)`
* `setAMotorAngle(axis, angle)`
* `setAMotorMode(mode)`
* `setAMotorTorques(x, y, z)`
* `getAMotorNumAxes()`
* `getAMotorAxis(index)`
* `getAMotorAxisRel(index)`
* `getAMotorAngle(index)`
* `getAMotorAngleRate(index)`
* `getAMotorMode()`

---

## Global Functions (`ODE` namespace)

* `cleanup()` — Finalize ODE and release global resources.
* `geomCollide(geom1, geom2)` — Check for collision between two geometries.

### Geometry Creation

* `GeomRenderObject(space, renderObj)`
* `GeomBox(space, lx, ly, lz)`
* `GeomSphere(space, radius)`
* `GeomPlane(space, a, b, c, d)` — Defines a plane `Ax + By + Cz = D`.
* `GeomTransform(space, geom)`
* `GeomRay(space, length)` — Creates a ray geometry for raycasting. See [Ray Methods](#ray-methods) below.

### Ray Methods

Ray geometries support specialized methods for raycasting and collision detection:

* `raySetLength(length)` — Set the ray length.
* `rayGetLength()` — Returns the current ray length.
* `raySet(px, py, pz, dx, dy, dz)` — Set ray position `(px, py, pz)` and direction `(dx, dy, dz)`.
* `rayGet()` — Returns an object with `start` and `direction` arrays: `{start: [x,y,z], direction: [dx,dy,dz]}`.
* `raySetParams(firstContact, backfaceCull)` — Configure ray behavior:
  * `firstContact` — If `true`, returns only the first contact found.
  * `backfaceCull` — If `true`, ignores back-facing surfaces.
* `rayGetParams()` — Returns current parameters: `{firstContact: boolean, backfaceCull: boolean}`.
* `raySetClosestHit(closestHit)` — If `true`, returns only the closest hit.
* `rayGetClosestHit()` — Returns whether closest hit mode is enabled.

**Ray Contact Information:**

When a ray collides with another geometry, contact data follows special conventions:

* **position** — Point where the ray intersects the surface.
* **normal** — Surface normal at the contact point (oriented for reflection if ray is first argument).
* **depth** — Distance from ray start to contact point.

### Joint Creation

* `JointBall(world, group)`
* `JointHinge(world, group)`
* `JointSlider(world, group)`
* `JointHinge2(world, group)`
* `JointUniversal(world, group)`
* `JointFixed(world, group)`
* `JointNull(world, group)`
* `JointAMotor(world, group)`

### Structure Creation

* `World()`
* `Space()`
* `Body(world)`
* `JointGroup()`
