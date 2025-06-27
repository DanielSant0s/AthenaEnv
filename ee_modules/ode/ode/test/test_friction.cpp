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

test the Coulomb friction approximation.

a 10x10 array of boxes is made, each of which rests on the ground.
a horizantal force is applied to each box to try and get it to slide.
box[i][j] has a mass (i+1)*MASS and a force (j+1)*FORCE. by the Coloumb
friction model, the box should only slide if the force is greater than MU
times the contact normal force, i.e.

  f > MU * body_mass * GRAVITY
  (j+1)*FORCE > MU * (i+1)*MASS * GRAVITY
  (j+1) > (i+1) * (MU*MASS*GRAVITY/FORCE)
  (j+1) > (i+1) * k

this should be independent of the number of contact points, as N contact
points will each have 1/N'th the normal force but the pushing force will
have to overcome N contacts. the constants are chosen so that k=1.
thus you should see a triangle made of half the bodies in the array start to
slide.

*/


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

#define LENGTH 0.2	// box length & width
#define HEIGHT 0.05	// box height
#define MASS 0.2	// mass of box[i][j] = (i+1) * MASS
#define FORCE 0.05	// force applied to box[i][j] = (j+1) * FORCE
#define MU 0.5		// the global mu to use
#define GRAVITY 0.5	// the global gravity to use
#define N1 10		// number of different forces to try
#define N2 10		// number of different masses to try


// dynamics and collision objects

static dWorldID world;
static dSpaceID space;
static dBodyID body[N1][N2];
static dJointGroupID contactgroup;
static dGeomID ground;
static dGeomID box[N1][N2];



// this is called by dSpaceCollide when two objects in space are
// potentially colliding.

static void nearCallback (void *data, dGeomID o1, dGeomID o2)
{
  int i;

  // only collide things with the ground
  int g1 = (o1 == ground);
  int g2 = (o2 == ground);
  if (!(g1 ^ g2)) return;

  dBodyID b1 = dGeomGetBody(o1);
  dBodyID b2 = dGeomGetBody(o2);

  dContact contact[3];		// up to 3 contacts per box
  for (i=0; i<3; i++) {
    contact[i].surface.mode = dContactSoftCFM | dContactApprox1;
    contact[i].surface.mu = MU;
    contact[i].surface.soft_cfm = 0.01;
  }
  if (int numc = dCollide (o1,o2,3,&contact[0].geom,sizeof(dContact))) {
    for (i=0; i<numc; i++) {
      dJointID c = dJointCreateContact (world,contactgroup,contact+i);
      dJointAttach (c,b1,b2);
    }
  }
}


// start simulation - set viewpoint

static void start()
{
  static float xyz[3] = {1.7772,-0.7924,2.7600};
  static float hpr[3] = {90.0000,-54.0000,0.0000};
  dsSetViewpoint (xyz,hpr);
}


// simulation loop

static void simLoop (int pause)
{
  int i;
  if (!pause) {
    // apply forces to all bodies
    for (i=0; i<N1; i++) {
      for (int j=0; j<N2; j++) {
	dBodyAddForce (body[i][j],FORCE*(i+1),0,0);
      }
    }

    dSpaceCollide (space,0,&nearCallback);
    dWorldStep (world,0.05);

    // remove all contact joints
    dJointGroupEmpty (contactgroup);
  }

  dsSetColor (1,0,1);
  dReal sides[3] = {LENGTH,LENGTH,HEIGHT};
  for (i=0; i<N1; i++) {
    for (int j=0; j<N2; j++) {
      dsDrawBox (dGeomGetPosition(box[i][j]),dGeomGetRotation(box[i][j]),
		 sides);
    }
  }
}


int main (int argc, char **argv)
{
  int i,j;
  dMass m;

  // setup pointers to drawstuff callback functions
  dsFunctions fn;
  fn.version = DS_VERSION;
  fn.start = &start;
  fn.step = &simLoop;
  fn.command = 0;
  fn.stop = 0;
  fn.path_to_textures = "../../drawstuff/textures";

  // create world
  world = dWorldCreate();
  space = dHashSpaceCreate (0);
  contactgroup = dJointGroupCreate (0);
  dWorldSetGravity (world,0,0,-GRAVITY);
  ground = dCreatePlane (space,0,0,1,0);

  // bodies
  for (i=0; i<N1; i++) {
    for (j=0; j<N2; j++) {
      body[i][j] = dBodyCreate (world);
      dMassSetBox (&m,1,LENGTH,LENGTH,HEIGHT);
      dMassAdjust (&m,MASS*(j+1));
      dBodySetMass (body[i][j],&m);
      dBodySetPosition (body[i][j],i*2*LENGTH,j*2*LENGTH,HEIGHT*0.5);

      box[i][j] = dCreateBox (space,LENGTH,LENGTH,HEIGHT);
      dGeomSetBody (box[i][j],body[i][j]);
    }
  }

  // run simulation
  dsSimulationLoop (argc,argv,352,288,&fn);

  dJointGroupDestroy (contactgroup);
  dSpaceDestroy (space);
  dWorldDestroy (world);

  return 0;
}
