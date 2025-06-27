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
#define SIDE (0.5f)	// side length of a box
#define MASS (1.0)	// mass of a box


// dynamics and collision objects
static dWorldID world;
static dBodyID body[2];
static dJointID slider;


// state set by keyboard commands
static int occasional_error = 0;


// start simulation - set viewpoint

static void start()
{
  static float xyz[3] = {1.0382f * 2,-1.0811f * 2,1.4700f * 2};
  static float hpr[3] = {135.0000f,-19.5000f,0.0000f};
  dsSetViewpoint (xyz,hpr);
  printf ("Press 'e' to start/stop occasional error.\n");
}


// called when a key pressed

static void command (int cmd)
{
  if (cmd == 'e' || cmd == 'E') {
    occasional_error ^= 1;
  }
}


// simulation loop

static void simLoop (int pause)
{
  const dReal kd = -0.3;	// angular damping constant
  const dReal ks = 0.5;	// spring constant
  if (!pause) {
    // add an oscillating torque to body 0, and also damp its rotational motion
    static dReal a=0;
    const dReal *w = dBodyGetAngularVel (body[0]);
    dBodyAddTorque (body[0],kd*w[0],kd*w[1]+0.1*cos(a),kd*w[2]+0.1*sin(a));
    a += 0.01;

    // add a spring force to keep the bodies together, otherwise they will
    // fly apart along the slider axis.
    const dReal *p1 = dBodyGetPosition (body[0]);
    const dReal *p2 = dBodyGetPosition (body[1]);
    dBodyAddForce (body[0],ks*(p2[0]-p1[0]),ks*(p2[1]-p1[1]),
		   ks*(p2[2]-p1[2]));
    dBodyAddForce (body[1],ks*(p1[0]-p2[0]),ks*(p1[1]-p2[1]),
		   ks*(p1[2]-p2[2]));

    // occasionally re-orient one of the bodies to create a deliberate error.
    if (occasional_error) {
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

    dWorldStep (world,0.05);
  }

  dReal sides1[3] = {SIDE,SIDE,SIDE};
  dReal sides2[3] = {SIDE*0.8f,SIDE*0.8f,SIDE*2.0f};
  dsSetTexture (DS_WOOD);
  dsSetColor (1,1,0);
  dsDrawBox (dBodyGetPosition(body[0]),dBodyGetRotation(body[0]),sides1);
  dsSetColor (0,1,1);
  dsDrawBox (dBodyGetPosition(body[1]),dBodyGetRotation(body[1]),sides2);
}


int main (int argc, char **argv)
{
  // setup pointers to drawstuff callback functions
  dsFunctions fn;
  fn.version = DS_VERSION;
  fn.start = &start;
  fn.step = &simLoop;
  fn.command = &command;
  fn.stop = 0;
  fn.path_to_textures = "../../drawstuff/textures";

  // create world
  world = dWorldCreate();
  dMass m;
  dMassSetBox (&m,1,SIDE,SIDE,SIDE);
  dMassAdjust (&m,MASS);

  body[0] = dBodyCreate (world);
  dBodySetMass (body[0],&m);
  dBodySetPosition (body[0],0,0,1);
  body[1] = dBodyCreate (world);
  dBodySetMass (body[1],&m);
  dQuaternion q;
  dQFromAxisAndAngle (q,-1,1,0,0.25*M_PI);
  dBodySetPosition (body[1],0.2,0.2,1.2);
  dBodySetQuaternion (body[1],q);

  slider = dJointCreateSlider (world,0);
  dJointAttach (slider,body[0],body[1]);
  dJointSetSliderAxis (slider,1,1,1);

  // run simulation
  dsSimulationLoop (argc,argv,352,288,&fn);

  dWorldDestroy (world);
  return 0;
}
