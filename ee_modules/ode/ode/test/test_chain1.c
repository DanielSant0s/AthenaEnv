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

/* exercise the C interface */

#include <stdio.h>
#include "ode/ode.h"
#include "drawstuff/drawstuff.h"

#ifdef _MSC_VER
#pragma warning(disable:4244 4305)  // for VC++, no precision loss complaints
#endif

/* select correct drawing functions */

#ifdef dDOUBLE
#define dsDrawBox dsDrawBoxD
#define dsDrawSphere dsDrawSphereD
#define dsDrawCylinder dsDrawCylinderD
#define dsDrawCappedCylinder dsDrawCappedCylinderD
#endif


/* some constants */

#define NUM 10			/* number of boxes */
#define SIDE (0.2)		/* side length of a box */
#define MASS (1.0)		/* mass of a box */
#define RADIUS (0.1732f)	/* sphere radius */


/* dynamics and collision objects */

static dWorldID world;
static dSpaceID space;
static dBodyID body[NUM];
static dJointID joint[NUM-1];
static dJointGroupID contactgroup;
static dGeomID sphere[NUM];


/* this is called by dSpaceCollide when two objects in space are
 * potentially colliding.
 */

static void nearCallback (void *data, dGeomID o1, dGeomID o2)
{
  /* exit without doing anything if the two bodies are connected by a joint */
  dBodyID b1,b2;
  dContact contact;

  b1 = dGeomGetBody(o1);
  b2 = dGeomGetBody(o2);
  if (b1 && b2 && dAreConnected (b1,b2)) return;

  contact.surface.mode = 0;
  contact.surface.mu = 0.1;
  contact.surface.mu2 = 0;
  if (dCollide (o1,o2,0,&contact.geom,sizeof(dContactGeom))) {
    dJointID c = dJointCreateContact (world,contactgroup,&contact);
    dJointAttach (c,b1,b2);
  }
}


/* start simulation - set viewpoint */

static void start()
{
  static float xyz[3] = {2.1640f,-1.3079f,1.7600f};
  static float hpr[3] = {125.5000f,-17.0000f,0.0000f};
  dsSetViewpoint (xyz,hpr);
}


/* simulation loop */

static void simLoop (int pause)
{
  int i;
  if (!pause) {
    static double angle = 0;
    angle += 0.05;
    dBodyAddForce (body[NUM-1],0,0,1.5*(sin(angle)+1.0));

    dSpaceCollide (space,0,&nearCallback);
    dWorldStep (world,0.05);

    /* remove all contact joints */
    dJointGroupEmpty (contactgroup);
  }

  dsSetColor (1,1,0);
  dsSetTexture (DS_WOOD);
  for (i=0; i<NUM; i++) dsDrawSphere (dBodyGetPosition(body[i]),
				      dBodyGetRotation(body[i]),RADIUS);
}


int main (int argc, char **argv)
{
  int i;
  dReal k;
  dMass m;

  /* setup pointers to drawstuff callback functions */
  dsFunctions fn;
  fn.version = DS_VERSION;
  fn.start = &start;
  fn.step = &simLoop;
  fn.command = 0;
  fn.stop = 0;
  fn.path_to_textures = "../../drawstuff/textures";

  /* create world */

  world = dWorldCreate();
  space = dHashSpaceCreate (0);
  contactgroup = dJointGroupCreate (1000000);
  dWorldSetGravity (world,0,0,-0.5);
  dCreatePlane (space,0,0,1,0);

  for (i=0; i<NUM; i++) {
    body[i] = dBodyCreate (world);
    k = i*SIDE;
    dBodySetPosition (body[i],k,k,k+0.4);
    dMassSetBox (&m,1,SIDE,SIDE,SIDE);
    dMassAdjust (&m,MASS);
    dBodySetMass (body[i],&m);
    sphere[i] = dCreateSphere (space,RADIUS);
    dGeomSetBody (sphere[i],body[i]);
  }
  for (i=0; i<(NUM-1); i++) {
    joint[i] = dJointCreateBall (world,0);
    dJointAttach (joint[i],body[i],body[i+1]);
    k = (i+0.5)*SIDE;
    dJointSetBallAnchor (joint[i],k,k,k+0.4);
  }

  /* run simulation */
  dsSimulationLoop (argc,argv,352,288,&fn);

  dJointGroupDestroy (contactgroup);
  dSpaceDestroy (space);
  dWorldDestroy (world);

  return 0;
}
