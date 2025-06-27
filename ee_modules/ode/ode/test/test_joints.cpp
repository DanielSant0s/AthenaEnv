/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

/*

perform tests on all the joint types.
this should be done using the double precision version of the library.

usage:
  test_joints [test_number] [g] [i] [e]

if a test number is given then that specific test is performed, otherwise
all the tests are performed. the tests are numbered `xxyy', where xx
corresponds to the joint type and yy is the sub-test number. not every
number maps to an actual test.

flags:
  i: the test is interactive.
  g: turn off graphical display (can't use this with `i').
  e: turn on occasional error purturbations

some tests compute and display error values. these values are scaled so
<1 is good and >1 is bad. other tests just show graphical results which
you must verify visually.

*/


#include <ode/ode.h>
#include <drawstuff/drawstuff.h>

#ifdef _MSC_VER
#pragma warning(disable:4244 4305)  // for VC++, no precision loss complaints
#endif

// select correct drawing functions
#ifdef dDOUBLE
#define dsDrawBox dsDrawBoxD
#endif


// some constants
#define NUM_JOINTS 10	// number of joints to test (the `xx' value)
#define SIDE (0.5f)	// side length of a box - don't change this
#define MASS (1.0)	// mass of a box
#define STEPSIZE 0.05


// dynamics objects
static dWorldID world;
static dBodyID body[2];
static dJointID joint;


// data from the command line arguments
static int cmd_test_num = -1;
static int cmd_interactive = 0;
static int cmd_graphics = 1;
static int cmd_occasional_error = 0;	// perturb occasionally


// info about the current test
struct TestInfo;
static int test_num = 0;		// number of the current test
static int iteration = 0;
static int max_iterations = 0;
static dReal max_error = 0;

//****************************************************************************
// utility stuff

static char loCase (char a)
{
  if (a >= 'A' && a <= 'Z') return a + ('a'-'A');
  else return a;
}


static dReal length (dVector3 a)
{
  return dSqrt (a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
}


// get the max difference between a 3x3 matrix and the identity

dReal cmpIdentity (const dMatrix3 A)
{
  dMatrix3 I;
  dSetZero (I,12);
  I[0] = 1;
  I[5] = 1;
  I[10] = 1;
  return dMaxDifference (A,I,3,3);
}

//****************************************************************************
// test world construction and utilities

void constructWorldForTest (dReal gravity, int bodycount,
 /* body 1 pos */           dReal pos1x, dReal pos1y, dReal pos1z,
 /* body 2 pos */           dReal pos2x, dReal pos2y, dReal pos2z,
 /* body 1 rotation axis */ dReal ax1x, dReal ax1y, dReal ax1z,
 /* body 1 rotation axis */ dReal ax2x, dReal ax2y, dReal ax2z,
 /* rotation angles */      dReal a1, dReal a2)
{
  // create world
  world = dWorldCreate();
  dWorldSetERP (world,0.2);
  dWorldSetCFM (world,1e-6);
  dWorldSetGravity (world,0,0,gravity);

  dMass m;
  dMassSetBox (&m,1,SIDE,SIDE,SIDE);
  dMassAdjust (&m,MASS);

  body[0] = dBodyCreate (world);
  dBodySetMass (body[0],&m);
  dBodySetPosition (body[0], pos1x, pos1y, pos1z);
  dQuaternion q;
  dQFromAxisAndAngle (q,ax1x,ax1y,ax1z,a1);
  dBodySetQuaternion (body[0],q);

  if (bodycount==2) {
    body[1] = dBodyCreate (world);
    dBodySetMass (body[1],&m);
    dBodySetPosition (body[1], pos2x, pos2y, pos2z);
    dQFromAxisAndAngle (q,ax2x,ax2y,ax2z,a2);
    dBodySetQuaternion (body[1],q);
  }
  else body[1] = 0;
}


// add an oscillating torque to body 0

void addOscillatingTorque (dReal tscale)
{
  static dReal a=0;
  dBodyAddTorque (body[0],tscale*cos(2*a),tscale*cos(2.7183*a),
		  tscale*cos(1.5708*a));
  a += 0.01;
}


void addOscillatingTorqueAbout(dReal tscale, dReal x, dReal y, dReal z)
{
  static dReal a=0;
  dBodyAddTorque (body[0], tscale*cos(a) * x, tscale*cos(a) * y,
		  tscale * cos(a) * z);
  a += 0.02;
}


// damp the rotational motion of body 0 a bit

void dampRotationalMotion (dReal kd)
{
  const dReal *w = dBodyGetAngularVel (body[0]);
  dBodyAddTorque (body[0],-kd*w[0],-kd*w[1],-kd*w[2]);
}


// add a spring force to keep the bodies together, otherwise they may fly
// apart with some joints.

void addSpringForce (dReal ks)
{
  const dReal *p1 = dBodyGetPosition (body[0]);
  const dReal *p2 = dBodyGetPosition (body[1]);
  dBodyAddForce (body[0],ks*(p2[0]-p1[0]),ks*(p2[1]-p1[1]),ks*(p2[2]-p1[2]));
  dBodyAddForce (body[1],ks*(p1[0]-p2[0]),ks*(p1[1]-p2[1]),ks*(p1[2]-p2[2]));
}

//****************************************************************************
// stuff specific to the tests
//
//   0xx : fixed
//   1xx : ball and socket
//   2xx : hinge
//   3xx : slider
//   4xx : hinge 2
//   5xx : contact
//   6xx : amotor
//   7xx : universal joint


// setup for the given test. return 0 if there is no such test

int setupTest (int n)
{
  switch (n) {

  // ********** fixed joint

  case 0: {			// 2 body
    constructWorldForTest (0,2,
			   0.5*SIDE,0.5*SIDE,1, -0.5*SIDE,-0.5*SIDE,1,
			   1,1,0, 1,1,0,
			   0.25*M_PI,0.25*M_PI);
    joint = dJointCreateFixed (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetFixed (joint);
    return 1;
  }

  case 1: {			// 1 body to static env
    constructWorldForTest (0,1,
			   0.5*SIDE,0.5*SIDE,1, 0,0,0,
			   1,0,0, 1,0,0,
			   0,0);
    joint = dJointCreateFixed (world,0);
    dJointAttach (joint,body[0],0);
    dJointSetFixed (joint);
    return 1;
  }

  case 2: {			// 2 body with relative rotation
    constructWorldForTest (0,2,
			   0.5*SIDE,0.5*SIDE,1, -0.5*SIDE,-0.5*SIDE,1,
			   1,1,0, 1,1,0,
			   0.25*M_PI,-0.25*M_PI);
    joint = dJointCreateFixed (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetFixed (joint);
    return 1;
  }

  case 3: {			// 1 body to static env with relative rotation
    constructWorldForTest (0,1,
			   0.5*SIDE,0.5*SIDE,1, 0,0,0,
			   1,0,0, 1,0,0,
			   0.25*M_PI,0);
    joint = dJointCreateFixed (world,0);
    dJointAttach (joint,body[0],0);
    dJointSetFixed (joint);
    return 1;
  }

  // ********** hinge joint

  case 200:			// 2 body
    constructWorldForTest (0,2,
			   0.5*SIDE,0.5*SIDE,1, -0.5*SIDE,-0.5*SIDE,1,
			   1,1,0, 1,1,0, 0.25*M_PI,0.25*M_PI);
    joint = dJointCreateHinge (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetHingeAnchor (joint,0,0,1);
    dJointSetHingeAxis (joint,1,-1,1.41421356);
    return 1;

  case 220:			// hinge angle polarity test
  case 221:			// hinge angle rate test
    constructWorldForTest (0,2,
			   0.5*SIDE,0.5*SIDE,1, -0.5*SIDE,-0.5*SIDE,1,
			   1,0,0, 1,0,0, 0,0);
    joint = dJointCreateHinge (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetHingeAnchor (joint,0,0,1);
    dJointSetHingeAxis (joint,0,0,1);
    max_iterations = 50;
    return 1;

  case 230:			// hinge motor rate (and polarity) test
  case 231:			// ...with stops
    constructWorldForTest (0,2,
			   0.5*SIDE,0.5*SIDE,1, -0.5*SIDE,-0.5*SIDE,1,
			   1,0,0, 1,0,0, 0,0);
    joint = dJointCreateHinge (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetHingeAnchor (joint,0,0,1);
    dJointSetHingeAxis (joint,0,0,1);
    dJointSetHingeParam (joint,dParamFMax,1);
    if (n==231) {
      dJointSetHingeParam (joint,dParamLoStop,-0.5);
      dJointSetHingeParam (joint,dParamHiStop,0.5);
    }
    return 1;

  case 250:			// limit bounce test (gravity down)
  case 251: {			// ...gravity up
    constructWorldForTest ((n==251) ? 0.1 : -0.1, 2,
			   0.5*SIDE,0,1+0.5*SIDE, -0.5*SIDE,0,1-0.5*SIDE,
			   1,0,0, 1,0,0, 0,0);
    joint = dJointCreateHinge (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetHingeAnchor (joint,0,0,1);
    dJointSetHingeAxis (joint,0,1,0);
    dJointSetHingeParam (joint,dParamLoStop,-0.9);
    dJointSetHingeParam (joint,dParamHiStop,0.7854);
    dJointSetHingeParam (joint,dParamBounce,0.5);
    // anchor 2nd body with a fixed joint
    dJointID j = dJointCreateFixed (world,0);
    dJointAttach (j,body[1],0);
    dJointSetFixed (j);
    return 1;
  }

  // ********** slider

  case 300:			// 2 body
    constructWorldForTest (0,2,
			   0,0,1, 0.2,0.2,1.2,
			   0,0,1, -1,1,0, 0,0.25*M_PI);
    joint = dJointCreateSlider (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetSliderAxis (joint,1,1,1);
    return 1;

  case 320:			// slider angle polarity test
  case 321:			// slider angle rate test
    constructWorldForTest (0,2,
			   0,0,1, 0,0,1.2,
			   1,0,0, 1,0,0, 0,0);
    joint = dJointCreateSlider (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetSliderAxis (joint,0,0,1);
    max_iterations = 50;
    return 1;

  case 330:			// slider motor rate (and polarity) test
  case 331:			// ...with stops
    constructWorldForTest (0, 2,
			   0,0,1, 0,0,1.2,
			   1,0,0, 1,0,0, 0,0);
    joint = dJointCreateSlider (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetSliderAxis (joint,0,0,1);
    dJointSetSliderParam (joint,dParamFMax,100);
    if (n==331) {
      dJointSetSliderParam (joint,dParamLoStop,-0.4);
      dJointSetSliderParam (joint,dParamHiStop,0.4);
    }
    return 1;

  case 350:			// limit bounce tests
  case 351: {
    constructWorldForTest ((n==351) ? 0.1 : -0.1, 2,
			   0,0,1, 0,0,1.2,
			   1,0,0, 1,0,0, 0,0);
    joint = dJointCreateSlider (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetSliderAxis (joint,0,0,1);
    dJointSetSliderParam (joint,dParamLoStop,-0.5);
    dJointSetSliderParam (joint,dParamHiStop,0.5);
    dJointSetSliderParam (joint,dParamBounce,0.5);
    // anchor 2nd body with a fixed joint
    dJointID j = dJointCreateFixed (world,0);
    dJointAttach (j,body[1],0);
    dJointSetFixed (j);
    return 1;
  }

  // ********** hinge-2 joint

  case 420:			// hinge-2 steering angle polarity test
  case 421:			// hinge-2 steering angle rate test
    constructWorldForTest (0,2,
			   0.5*SIDE,0,1, -0.5*SIDE,0,1,
			   1,0,0, 1,0,0, 0,0);
    joint = dJointCreateHinge2 (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetHinge2Anchor (joint,-0.5*SIDE,0,1);
    dJointSetHinge2Axis1 (joint,0,0,1);
    dJointSetHinge2Axis2 (joint,1,0,0);
    max_iterations = 50;
    return 1;

  case 430:			// hinge 2 steering motor rate (+polarity) test
  case 431:			// ...with stops
  case 432:			// hinge 2 wheel motor rate (+polarity) test
    constructWorldForTest (0,2,
			   0.5*SIDE,0,1, -0.5*SIDE,0,1,
			   1,0,0, 1,0,0, 0,0);
    joint = dJointCreateHinge2 (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetHinge2Anchor (joint,-0.5*SIDE,0,1);
    dJointSetHinge2Axis1 (joint,0,0,1);
    dJointSetHinge2Axis2 (joint,1,0,0);
    dJointSetHinge2Param (joint,dParamFMax,1);
    dJointSetHinge2Param (joint,dParamFMax2,1);
    if (n==431) {
      dJointSetHinge2Param (joint,dParamLoStop,-0.5);
      dJointSetHinge2Param (joint,dParamHiStop,0.5);
    }
    return 1;

  // ********** angular motor joint

  case 600:			// test euler angle calculations
    constructWorldForTest (0,2,
			   -SIDE*0.5,0,1, SIDE*0.5,0,1,
			   0,0,1, 0,0,1, 0,0);
    joint = dJointCreateAMotor (world,0);
    dJointAttach (joint,body[0],body[1]);

    dJointSetAMotorNumAxes (joint,3);
    dJointSetAMotorAxis (joint,0,1, 0,0,1);
    dJointSetAMotorAxis (joint,2,2, 1,0,0);
    dJointSetAMotorMode (joint,dAMotorEuler);
    max_iterations = 200;
    return 1;

    // ********** universal joint

  case 700:			// 2 body
  case 701:
  case 702:
    constructWorldForTest (0,2,
 			   0.5*SIDE,0.5*SIDE,1, -0.5*SIDE,-0.5*SIDE,1,
 			   1,1,0, 1,1,0, 0.25*M_PI,0.25*M_PI);
    joint = dJointCreateUniversal (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetUniversalAnchor (joint,0,0,1);
    dJointSetUniversalAxis1 (joint, 1, -1, 1.41421356);
    dJointSetUniversalAxis2 (joint, 1, -1, -1.41421356);
    return 1;

  case 720:		// universal transmit torque test
  case 721:
  case 722:
  case 730:		// universal torque about axis 1
  case 731:
  case 732:
  case 740:		// universal torque about axis 2
  case 741:
  case 742:
    constructWorldForTest (0,2,
 			   0.5*SIDE,0.5*SIDE,1, -0.5*SIDE,-0.5*SIDE,1,
 			   1,0,0, 1,0,0, 0,0);
    joint = dJointCreateUniversal (world,0);
    dJointAttach (joint,body[0],body[1]);
    dJointSetUniversalAnchor (joint,0,0,1);
    dJointSetUniversalAxis1 (joint,0,0,1);
    dJointSetUniversalAxis2 (joint, 1, -1,0);
    max_iterations = 100;
    return 1;
  }
  return 0;
}


// do stuff specific to this test each iteration. you can check some
// invariants for the test -- the return value is some scaled error measurement
// that must be less than 1.
// return a dInfinity if error is not measured for this n.

dReal doStuffAndGetError (int n)
{
  switch (n) {

  // ********** fixed joint

  case 0: {			// 2 body
    addOscillatingTorque (0.1);
    dampRotationalMotion (0.1);
    // check the orientations are the same
    const dReal *R1 = dBodyGetRotation (body[0]);
    const dReal *R2 = dBodyGetRotation (body[1]);
    dReal err1 = dMaxDifference (R1,R2,3,3);
    // check the body offset is correct
    dVector3 p,pp;
    const dReal *p1 = dBodyGetPosition (body[0]);
    const dReal *p2 = dBodyGetPosition (body[1]);
    for (int i=0; i<3; i++) p[i] = p2[i] - p1[i];
    dMULTIPLY1_331 (pp,R1,p);
    pp[0] += 0.5;
    pp[1] += 0.5;
    return (err1 + length (pp)) * 300;
  }

  case 1: {			// 1 body to static env
    addOscillatingTorque (0.1);

    // check the orientation is the identity
    dReal err1 = cmpIdentity (dBodyGetRotation (body[0]));

    // check the body offset is correct
    dVector3 p;
    const dReal *p1 = dBodyGetPosition (body[0]);
    for (int i=0; i<3; i++) p[i] = p1[i];
    p[0] -= 0.25;
    p[1] -= 0.25;
    p[2] -= 1;
    return (err1 + length (p)) * 1e6;
  }

  case 2: {			// 2 body
    addOscillatingTorque (0.1);
    dampRotationalMotion (0.1);
    // check the body offset is correct
    // Should really check body rotation too.  Oh well.
    const dReal *R1 = dBodyGetRotation (body[0]);
    dVector3 p,pp;
    const dReal *p1 = dBodyGetPosition (body[0]);
    const dReal *p2 = dBodyGetPosition (body[1]);
    for (int i=0; i<3; i++) p[i] = p2[i] - p1[i];
    dMULTIPLY1_331 (pp,R1,p);
    pp[0] += 0.5;
    pp[1] += 0.5;
    return length(pp) * 300;
  }

  case 3: {			// 1 body to static env with relative rotation
    addOscillatingTorque (0.1);

    // check the body offset is correct
    dVector3 p;
    const dReal *p1 = dBodyGetPosition (body[0]);
    for (int i=0; i<3; i++) p[i] = p1[i];
    p[0] -= 0.25;
    p[1] -= 0.25;
    p[2] -= 1;
    return  length (p) * 1e6;
  }


  // ********** hinge joint

  case 200:			// 2 body
    addOscillatingTorque (0.1);
    dampRotationalMotion (0.1);
    return dInfinity;

  case 220:			// hinge angle polarity test
    dBodyAddTorque (body[0],0,0,0.01);
    dBodyAddTorque (body[1],0,0,-0.01);
    if (iteration == 40) {
      dReal a = dJointGetHingeAngle (joint);
      if (a > 0.5 && a < 1) return 0; else return 10;
    }
    return 0;

  case 221: {			// hinge angle rate test
    static dReal last_angle = 0;
    dBodyAddTorque (body[0],0,0,0.01);
    dBodyAddTorque (body[1],0,0,-0.01);
    dReal a = dJointGetHingeAngle (joint);
    dReal r = dJointGetHingeAngleRate (joint);
    dReal er = (a-last_angle)/STEPSIZE;		// estimated rate
    last_angle = a;
    return fabs(r-er) * 4e4;
  }

  case 230:			// hinge motor rate (and polarity) test
  case 231: {			// ...with stops
    static dReal a = 0;
    dReal r = dJointGetHingeAngleRate (joint);
    dReal err = fabs (cos(a) - r);
    if (a==0) err = 0;
    a += 0.03;
    dJointSetHingeParam (joint,dParamVel,cos(a));
    if (n==231) return dInfinity;
    return err * 1e6;
  }

  // ********** slider joint

  case 300:			// 2 body
    addOscillatingTorque (0.05);
    dampRotationalMotion (0.1);
    addSpringForce (0.5);
    return dInfinity;

  case 320:			// slider angle polarity test
    dBodyAddForce (body[0],0,0,0.1);
    dBodyAddForce (body[1],0,0,-0.1);
    if (iteration == 40) {
      dReal a = dJointGetSliderPosition (joint);
      if (a > 0.2 && a < 0.5) return 0; else return 10;
      return a;
    }
    return 0;

  case 321: {			// slider angle rate test
    static dReal last_pos = 0;
    dBodyAddForce (body[0],0,0,0.1);
    dBodyAddForce (body[1],0,0,-0.1);
    dReal p = dJointGetSliderPosition (joint);
    dReal r = dJointGetSliderPositionRate (joint);
    dReal er = (p-last_pos)/STEPSIZE;	// estimated rate (almost exact)
    last_pos = p;
    return fabs(r-er) * 1e9;
  }

  case 330:			// slider motor rate (and polarity) test
  case 331: {			// ...with stops
    static dReal a = 0;
    dReal r = dJointGetSliderPositionRate (joint);
    dReal err = fabs (0.7*cos(a) - r);
    if (a < 0.04) err = 0;
    a += 0.03;
    dJointSetSliderParam (joint,dParamVel,0.7*cos(a));
    if (n==331) return dInfinity;
    return err * 1e6;
  }

  // ********** hinge-2 joint

  case 420:			// hinge-2 steering angle polarity test
    dBodyAddTorque (body[0],0,0,0.01);
    dBodyAddTorque (body[1],0,0,-0.01);
    if (iteration == 40) {
      dReal a = dJointGetHinge2Angle1 (joint);
      if (a > 0.5 && a < 0.6) return 0; else return 10;
    }
    return 0;

  case 421: {			// hinge-2 steering angle rate test
    static dReal last_angle = 0;
    dBodyAddTorque (body[0],0,0,0.01);
    dBodyAddTorque (body[1],0,0,-0.01);
    dReal a = dJointGetHinge2Angle1 (joint);
    dReal r = dJointGetHinge2Angle1Rate (joint);
    dReal er = (a-last_angle)/STEPSIZE;		// estimated rate
    last_angle = a;
    return fabs(r-er)*2e4;
  }

  case 430:			// hinge 2 steering motor rate (+polarity) test
  case 431: {			// ...with stops
    static dReal a = 0;
    dReal r = dJointGetHinge2Angle1Rate (joint);
    dReal err = fabs (cos(a) - r);
    if (a==0) err = 0;
    a += 0.03;
    dJointSetHinge2Param (joint,dParamVel,cos(a));
    if (n==431) return dInfinity;
    return err * 1e6;
  }

  case 432: {			// hinge 2 wheel motor rate (+polarity) test
    static dReal a = 0;
    dReal r = dJointGetHinge2Angle2Rate (joint);
    dReal err = fabs (cos(a) - r);
    if (a==0) err = 0;
    a += 0.03;
    dJointSetHinge2Param (joint,dParamVel2,cos(a));
    return err * 1e6;
  }

  // ********** angular motor joint

  case 600: {			// test euler angle calculations
    // desired euler angles from last iteration
    static dReal a1,a2,a3;

    // find actual euler angles
    dReal aa1 = dJointGetAMotorAngle (joint,0);
    dReal aa2 = dJointGetAMotorAngle (joint,1);
    dReal aa3 = dJointGetAMotorAngle (joint,2);
    // printf ("actual  = %.4f %.4f %.4f\n\n",aa1,aa2,aa3);

    dReal err = dInfinity;
    if (iteration > 0) {
      err = dFabs(aa1-a1) + dFabs(aa2-a2) + dFabs(aa3-a3);
      err *= 1e10;
    }

    // get random base rotation for both bodies
    dMatrix3 Rbase;
    dRFromAxisAndAngle (Rbase, 3*(dRandReal()-0.5), 3*(dRandReal()-0.5),
			3*(dRandReal()-0.5), 3*(dRandReal()-0.5));
    dBodySetRotation (body[0],Rbase);

    // rotate body 2 by random euler angles w.r.t. body 1
    a1 = 3.14 * 2 * (dRandReal()-0.5);
    a2 = 1.57 * 2 * (dRandReal()-0.5);
    a3 = 3.14 * 2 * (dRandReal()-0.5);
    dMatrix3 R1,R2,R3,Rtmp1,Rtmp2;
    dRFromAxisAndAngle (R1,0,0,1,-a1);
    dRFromAxisAndAngle (R2,0,1,0,a2);
    dRFromAxisAndAngle (R3,1,0,0,-a3);
    dMultiply0 (Rtmp1,R2,R3,3,3,3);
    dMultiply0 (Rtmp2,R1,Rtmp1,3,3,3);
    dMultiply0 (Rtmp1,Rbase,Rtmp2,3,3,3);
    dBodySetRotation (body[1],Rtmp1);
    // printf ("desired = %.4f %.4f %.4f\n",a1,a2,a3);

    return err;
  }

  // ********** universal joint

  case 700: {		// 2 body: joint constraint
    dVector3 ax1, ax2;

    addOscillatingTorque (0.1);
    dampRotationalMotion (0.1);
    dJointGetUniversalAxis1(joint, ax1);
    dJointGetUniversalAxis2(joint, ax2);
    return fabs(10*dDOT(ax1, ax2));
  }

  case 701: {		// 2 body: angle 1 rate
    static dReal last_angle = 0;
    addOscillatingTorque (0.1);
    dampRotationalMotion (0.1);
    dReal a = dJointGetUniversalAngle1(joint);
    dReal r = dJointGetUniversalAngle1Rate(joint);
    dReal diff = a - last_angle;
    if (diff > M_PI) diff -= 2*M_PI;
    if (diff < -M_PI) diff += 2*M_PI;
    dReal er = diff / STEPSIZE;    // estimated rate
    last_angle = a;
    // I'm not sure why the error is so large here.
    return fabs(r - er) * 1e1;
  }

  case 702: {		// 2 body: angle 2 rate
    static dReal last_angle = 0;
    addOscillatingTorque (0.1);
    dampRotationalMotion (0.1);
    dReal a = dJointGetUniversalAngle2(joint);
    dReal r = dJointGetUniversalAngle2Rate(joint);
    dReal diff = a - last_angle;
    if (diff > M_PI) diff -= 2*M_PI;
    if (diff < -M_PI) diff += 2*M_PI;
    dReal er = diff / STEPSIZE;    // estimated rate
    last_angle = a;
    // I'm not sure why the error is so large here.
    return fabs(r - er) * 1e1;
  }

  case 720: {		// universal transmit torque test: constraint error
    dVector3 ax1, ax2;
    addOscillatingTorqueAbout (0.1, 1, 1, 0);
    dampRotationalMotion (0.1);
    dJointGetUniversalAxis1(joint, ax1);
    dJointGetUniversalAxis2(joint, ax2);
    return fabs(10*dDOT(ax1, ax2));
  }

  case 721: {		// universal transmit torque test: angle1 rate
    static dReal last_angle = 0;
    addOscillatingTorqueAbout (0.1, 1, 1, 0);
    dampRotationalMotion (0.1);
    dReal a = dJointGetUniversalAngle1(joint);
    dReal r = dJointGetUniversalAngle1Rate(joint);
    dReal diff = a - last_angle;
    if (diff > M_PI) diff -= 2*M_PI;
    if (diff < -M_PI) diff += 2*M_PI;
    dReal er = diff / STEPSIZE;    // estimated rate
    last_angle = a;
    return fabs(r - er) * 1e10;
  }

  case 722: {		// universal transmit torque test: angle2 rate
    static dReal last_angle = 0;
    addOscillatingTorqueAbout (0.1, 1, 1, 0);
    dampRotationalMotion (0.1);
    dReal a = dJointGetUniversalAngle2(joint);
    dReal r = dJointGetUniversalAngle2Rate(joint);
    dReal diff = a - last_angle;
    if (diff > M_PI) diff -= 2*M_PI;
    if (diff < -M_PI) diff += 2*M_PI;
    dReal er = diff / STEPSIZE;    // estimated rate
    last_angle = a;
    return fabs(r - er) * 1e10;
  }

  case 730:{
    dVector3 ax1, ax2;
    dJointGetUniversalAxis1(joint, ax1);
    dJointGetUniversalAxis2(joint, ax2);
    addOscillatingTorqueAbout (0.1, ax1[0], ax1[1], ax1[2]);
    dampRotationalMotion (0.1);
    return fabs(10*dDOT(ax1, ax2));
  }

  case 731:{
    dVector3 ax1;
    static dReal last_angle = 0;
    dJointGetUniversalAxis1(joint, ax1);
    addOscillatingTorqueAbout (0.1, ax1[0], ax1[1], ax1[2]);
    dampRotationalMotion (0.1);
    dReal a = dJointGetUniversalAngle1(joint);
    dReal r = dJointGetUniversalAngle1Rate(joint);
    dReal diff = a - last_angle;
    if (diff > M_PI) diff -= 2*M_PI;
    if (diff < -M_PI) diff += 2*M_PI;
    dReal er = diff / STEPSIZE;    // estimated rate
    last_angle = a;
    return fabs(r - er) * 2e3;
  }

  case 732:{
    dVector3 ax1;
    static dReal last_angle = 0;
    dJointGetUniversalAxis1(joint, ax1);
    addOscillatingTorqueAbout (0.1, ax1[0], ax1[1], ax1[2]);
    dampRotationalMotion (0.1);
    dReal a = dJointGetUniversalAngle2(joint);
    dReal r = dJointGetUniversalAngle2Rate(joint);
    dReal diff = a - last_angle;
    if (diff > M_PI) diff -= 2*M_PI;
    if (diff < -M_PI) diff += 2*M_PI;
    dReal er = diff / STEPSIZE;    // estimated rate
    last_angle = a;
    return fabs(r - er) * 1e10;
  }

  case 740:{
    dVector3 ax1, ax2;
    dJointGetUniversalAxis1(joint, ax1);
    dJointGetUniversalAxis2(joint, ax2);
    addOscillatingTorqueAbout (0.1, ax2[0], ax2[1], ax2[2]);
    dampRotationalMotion (0.1);
    return fabs(10*dDOT(ax1, ax2));
  }

  case 741:{
    dVector3 ax2;
    static dReal last_angle = 0;
    dJointGetUniversalAxis2(joint, ax2);
    addOscillatingTorqueAbout (0.1, ax2[0], ax2[1], ax2[2]);
    dampRotationalMotion (0.1);
    dReal a = dJointGetUniversalAngle1(joint);
    dReal r = dJointGetUniversalAngle1Rate(joint);
    dReal diff = a - last_angle;
    if (diff > M_PI) diff -= 2*M_PI;
    if (diff < -M_PI) diff += 2*M_PI;
    dReal er = diff / STEPSIZE;    // estimated rate
    last_angle = a;
    return fabs(r - er) * 1e10;
  }

  case 742:{
    dVector3 ax2;
    static dReal last_angle = 0;
    dJointGetUniversalAxis2(joint, ax2);
    addOscillatingTorqueAbout (0.1, ax2[0], ax2[1], ax2[2]);
    dampRotationalMotion (0.1);
    dReal a = dJointGetUniversalAngle2(joint);
    dReal r = dJointGetUniversalAngle2Rate(joint);
    dReal diff = a - last_angle;
    if (diff > M_PI) diff -= 2*M_PI;
    if (diff < -M_PI) diff += 2*M_PI;
    dReal er = diff / STEPSIZE;    // estimated rate
    last_angle = a;
    return fabs(r - er) * 1e4;
  }
  }

  return dInfinity;
}

//****************************************************************************
// simulation stuff common to all the tests

// start simulation - set viewpoint

static void start()
{
  static float xyz[3] = {1.0382f * 2,-1.0811f * 2,1.4700f * 2};
  static float hpr[3] = {135.0000f,-19.5000f,0.0000f};
  dsSetViewpoint (xyz,hpr);
}


// simulation loop

static void simLoop (int pause)
{
  // stop after a given number of iterations, as long as we are not in
  // interactive mode
  if (cmd_graphics && !cmd_interactive &&
      (iteration >= max_iterations)) {
    dsStop();
    return;
  }
  iteration++;

  if (!pause) {
    // do stuff for this test and check to see if the joint is behaving well
    dReal error = doStuffAndGetError (test_num);
    if (error > max_error) max_error = error;
    if (cmd_interactive && error < dInfinity) {
      printf ("scaled error = %.4e\n",error);
    }

    // take a step
    dWorldStep (world,STEPSIZE);

    // occasionally re-orient the first body to create a deliberate error.
    if (cmd_occasional_error) {
      static int count = 0;
      if ((count % 20)==0) {
	// randomly adjust orientation of body[0]
	const dReal *R1;
	dMatrix3 R2,R3;
	R1 = dBodyGetRotation (body[0]);
	dRFromAxisAndAngle (R2,dRandReal()-0.5,dRandReal()-0.5,
			    dRandReal()-0.5,dRandReal()-0.5);
	dMultiply0 (R3,R1,R2,3,3,3);
	dBodySetRotation (body[0],R3);

	// randomly adjust position of body[0]
	const dReal *pos = dBodyGetPosition (body[0]);
	dBodySetPosition (body[0],
			  pos[0]+0.2*(dRandReal()-0.5),
			  pos[1]+0.2*(dRandReal()-0.5),
			  pos[2]+0.2*(dRandReal()-0.5));
      }
      count++;
    }
  }

  if (cmd_graphics) {
    dReal sides1[3] = {SIDE,SIDE,SIDE};
    dReal sides2[3] = {SIDE*0.99f,SIDE*0.99f,SIDE*0.99f};
    dsSetTexture (DS_WOOD);
    dsSetColor (1,1,0);
    dsDrawBox (dBodyGetPosition(body[0]),dBodyGetRotation(body[0]),sides1);
    if (body[1]) {
      dsSetColor (0,1,1);
      dsDrawBox (dBodyGetPosition(body[1]),dBodyGetRotation(body[1]),sides2);
    }
  }
}

//****************************************************************************
// conduct a specific test, and report the results

void doTest (int argc, char **argv, int n, int fatal_if_bad_n)
{
  test_num = n;
  iteration = 0;
  max_iterations = 300;
  max_error = 0;

  if (! setupTest (n)) {
    if (fatal_if_bad_n) dError (0,"bad test number");
    return;
  }

  // setup pointers to drawstuff callback functions
  dsFunctions fn;
  fn.version = DS_VERSION;
  fn.start = &start;
  fn.step = &simLoop;
  fn.command = 0;
  fn.stop = 0;
  fn.path_to_textures = "../../drawstuff/textures";

  // run simulation
  if (cmd_graphics) {
    dsSimulationLoop (argc,argv,352,288,&fn);
  }
  else {
    for (int i=0; i < max_iterations; i++) simLoop (0);
  }
  dWorldDestroy (world);
  body[0] = 0;
  body[1] = 0;
  joint = 0;

  // print results
  printf ("test %d: ",n);
  if (max_error == dInfinity) printf ("error not computed\n");
  else {
    printf ("max scaled error = %.4e",max_error);
    if (max_error < 1) printf (" - passed\n");
    else printf (" - FAILED\n");
  }
}

//****************************************************************************
// main

int main (int argc, char **argv)
{
  int i;

  // process the command line args. anything that starts with `-' is assumed
  // to be a drawstuff argument.
  for (i=1; i<argc; i++) {
    if (loCase (argv[i][0])=='i' && argv[i][1]==0) cmd_interactive = 1;
    if (loCase (argv[i][0])=='g' && argv[i][1]==0) cmd_graphics = 0;
    if (loCase (argv[i][0])=='e' && argv[i][1]==0) cmd_occasional_error = 1;
    char *endptr;
    long int n = strtol (argv[i],&endptr,10);
    if (*endptr == 0) cmd_test_num = n;
  }

  // do the tests
  if (cmd_test_num == -1) {
    for (i=0; i<NUM_JOINTS*100; i++) doTest (argc,argv,i,0);
  }
  else {
    doTest (argc,argv,cmd_test_num,1);
  }

  return 0;
}
