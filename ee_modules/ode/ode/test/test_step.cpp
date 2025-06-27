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

// test the step function by comparing the output of the fast and the slow
// version, for various systems. currently you have to define COMPARE_METHODS
// in step.cpp for this to work properly.
//
// @@@ report MAX error

#include <time.h>
#include <ode/ode.h>
#include <drawstuff/drawstuff.h>

#ifdef _MSC_VER
#pragma warning(disable:4244 4305)  // for VC++, no precision loss complaints
#endif

// select correct drawing functions

#ifdef dDOUBLE
#define dsDrawBox dsDrawBoxD
#define dsDrawSphere dsDrawSphereD
#define dsDrawCylinder dsDrawCylinderD
#define dsDrawCappedCylinder dsDrawCappedCylinderD
#endif


// some constants

#define NUM 10			// number of bodies
#define NUMJ 9			// number of joints
#define SIDE (0.2)		// side length of a box
#define MASS (1.0)		// mass of a box
#define RADIUS (0.1732f)	// sphere radius



// dynamics and collision objects

static dWorldID world=0;
static dBodyID body[NUM];
static dJointID joint[NUMJ];


// create the test system

void createTest()
{
  int i,j;
  if (world) dWorldDestroy (world);

  world = dWorldCreate();

  // create random bodies
  for (i=0; i<NUM; i++) {
    // create bodies at random position and orientation
    body[i] = dBodyCreate (world);
    dBodySetPosition (body[i],dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2+RADIUS);
    dReal q[4];
    for (j=0; j<4; j++) q[j] = dRandReal()*2-1;
    dBodySetQuaternion (body[i],q);

    // set random velocity
    dBodySetLinearVel (body[i], dRandReal()*2-1,dRandReal()*2-1,
		       dRandReal()*2-1);
    dBodySetAngularVel (body[i], dRandReal()*2-1,dRandReal()*2-1,
			dRandReal()*2-1);

    // set random mass (random diagonal mass rotated by a random amount)
    dMass m;
    dMatrix3 R;
    dMassSetBox (&m,1,dRandReal()+0.1,dRandReal()+0.1,dRandReal()+0.1);
    dMassAdjust (&m,dRandReal()+1);
    for (j=0; j<4; j++) q[j] = dRandReal()*2-1;
    dQtoR (q,R);
    dMassRotate (&m,R);
    dBodySetMass (body[i],&m);
  }

  // create ball-n-socket joints at random positions, linking random bodies
  // (but make sure not to link the same pair of bodies twice)
  char linked[NUM*NUM];
  for (i=0; i<NUM*NUM; i++) linked[i] = 0;
  for (i=0; i<NUMJ; i++) {
    int b1,b2;
    do {
      b1 = dRandInt (NUM);
      b2 = dRandInt (NUM);
    } while (linked[b1*NUM + b2] || b1==b2);
    linked[b1*NUM + b2] = 1;
    linked[b2*NUM + b1] = 1;
    joint[i] = dJointCreateBall (world,0);
    dJointAttach (joint[i],body[b1],body[b2]);
    dJointSetBallAnchor (joint[i],dRandReal()*2-1,
			 dRandReal()*2-1,dRandReal()*2+RADIUS);
  }

  for (i=0; i<NUM; i++) {
    // move bodies a bit to get some joint error
    const dReal *pos = dBodyGetPosition (body[i]); 
    dBodySetPosition (body[i],pos[0]+dRandReal()*0.2-0.1,
		      pos[1]+dRandReal()*0.2-0.1,pos[2]+dRandReal()*0.2-0.1);
  }
}


// start simulation - set viewpoint

static void start()
{
  static float xyz[3] = {2.6117f,-1.4433f,2.3700f};
  static float hpr[3] = {151.5000f,-30.5000f,0.0000f};
  dsSetViewpoint (xyz,hpr);
}


// simulation loop

static void simLoop (int pause)
{
  if (!pause) {
    // add random forces and torques to all bodies
    int i;
    const dReal scale1 = 5;
    const dReal scale2 = 5;
    for (i=0; i<NUM; i++) {
      dBodyAddForce (body[i],
		     scale1*(dRandReal()*2-1),
		     scale1*(dRandReal()*2-1),
		     scale1*(dRandReal()*2-1));
      dBodyAddTorque (body[i],
		     scale2*(dRandReal()*2-1),
		     scale2*(dRandReal()*2-1),
		     scale2*(dRandReal()*2-1));
    }

    dWorldStep (world,0.05);
    createTest();
  }

  // float sides[3] = {SIDE,SIDE,SIDE};
  dsSetColor (1,1,0);
  for (int i=0; i<NUM; i++)
    dsDrawSphere (dBodyGetPosition(body[i]), dBodyGetRotation(body[i]),RADIUS);
}


int main (int argc, char **argv)
{
  // setup pointers to drawstuff callback functions
  dsFunctions fn;
  fn.version = DS_VERSION;
  fn.start = &start;
  fn.step = &simLoop;
  fn.command = 0;
  fn.stop = 0;
  fn.path_to_textures = "../../drawstuff/textures";

  dRandSetSeed (time(0));
  createTest();

  // run simulation
  dsSimulationLoop (argc,argv,352,288,&fn);

  dWorldDestroy (world);
  return 0;
}
