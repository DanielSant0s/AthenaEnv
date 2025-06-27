/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001-2003 Russell L. Smith.       *
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
#define dsDrawSphere dsDrawSphereD
#define dsDrawCylinder dsDrawCylinderD
#define dsDrawCappedCylinder dsDrawCappedCylinderD
#endif


// some constants

#define NUM 10000			// max number of objects
#define DENSITY (5.0)		// density of all objects
#define GPB 3			// maximum number of geometries per body
#define MAX_CONTACTS 4		// maximum number of contact points per body
#define WORLD_SIZE 100


// dynamics and collision objects

struct MyObject {
  dBodyID body;			// the body
  dGeomID geom[GPB];		// geometries representing this body
};

static int num=0;		// number of objects in simulation
static int nextobj=0;		// next object to recycle if num==NUM
static dWorldID world;
static dSpaceID space;
static MyObject obj[NUM];
static dJointGroupID contactgroup;
static int selected = -1;	// selected object
static int show_aabb = 0;	// show geom AABBs?
static int show_contacts = 0;	// show contact points?
static int random_pos = 1;	// drop objects from random position?
static int draw_geom = 1;


// this is called by dSpaceCollide when two objects in space are
// potentially colliding.

static void nearCallback (void *data, dGeomID o1, dGeomID o2)
{
  int i;
  // if (o1->body && o2->body) return;

  // exit without doing anything if the two bodies are connected by a joint
  dBodyID b1 = dGeomGetBody(o1);
  dBodyID b2 = dGeomGetBody(o2);
  if (b1 && b2 && dAreConnectedExcluding (b1,b2,dJointTypeContact)) return;

  dContact contact[MAX_CONTACTS];   // up to MAX_CONTACTS contacts per box-box
  for (i=0; i<MAX_CONTACTS; i++) {
    contact[i].surface.mode = dContactBounce | dContactSoftCFM;
    contact[i].surface.mu = dInfinity;
    contact[i].surface.mu2 = 0;
    contact[i].surface.bounce = 0.1;
    contact[i].surface.bounce_vel = 0.1;
    contact[i].surface.soft_cfm = 0.01;
  }
  if (int numc = dCollide (o1,o2,MAX_CONTACTS,&contact[0].geom,
			   sizeof(dContact))) {
    dMatrix3 RI;
    dRSetIdentity (RI);
    const dReal ss[3] = {0.02,0.02,0.02};
    for (i=0; i<numc; i++) {
      dJointID c = dJointCreateContact (world,contactgroup,contact+i);
      dJointAttach (c,b1,b2);
      if (show_contacts) dsDrawBox (contact[i].geom.pos,RI,ss);
    }
  }
}


// start simulation - set viewpoint

static void start()
{
  static float xyz[3] = {2.1640f,-1.3079f,1.7600f};
  static float hpr[3] = {125.5000f,-17.0000f,0.0000f};
  dsSetViewpoint (xyz,hpr);
  printf ("To drop another object, press:\n");
  printf ("   o to disable rendering.\n");
  printf ("   b for box.\n");
  printf ("   s for sphere.\n");
  printf ("   c for cylinder.\n");
  printf ("   x for a composite object.\n");
  printf ("To select an object, press space.\n");
  printf ("To disable the selected object, press d.\n");
  printf ("To enable the selected object, press e.\n");
  printf ("To toggle showing the geom AABBs, press a.\n");
  printf ("To toggle showing the contact points, press t.\n");
  printf ("To toggle dropping from random position/orientation, press r.\n");
}


char locase (char c)
{
  if (c >= 'A' && c <= 'Z') return c - ('a'-'A');
  else return c;
}


// called when a key pressed

static void command (int cmd)
{
  int i,j,k;
  dReal sides[3];
  dMass m;

  cmd = locase (cmd);
  if (cmd == 'b' || cmd == 's' || cmd == 'c' || cmd == 'x'
      /* || cmd == 'l' */) {
    if (num < NUM) {
      i = num;
      num++;
    }
    else {
      i = nextobj;
      nextobj++;
      if (nextobj >= num) nextobj = 0;

      // destroy the body and geoms for slot i
      dBodyDestroy (obj[i].body);
      for (k=0; k < GPB; k++) {
	if (obj[i].geom[k]) dGeomDestroy (obj[i].geom[k]);
      }
      memset (&obj[i],0,sizeof(obj[i]));
    }

    obj[i].body = dBodyCreate (world);
    for (k=0; k<3; k++) sides[k] = dRandReal()*0.5+0.1;

    dMatrix3 R;
    if (random_pos) {
      dBodySetPosition (obj[i].body,
			dRandReal()*WORLD_SIZE-(WORLD_SIZE/2),dRandReal()*WORLD_SIZE-(WORLD_SIZE/2),dRandReal()+1);
      dRFromAxisAndAngle (R,dRandReal()*2.0-1.0,dRandReal()*2.0-1.0,
			  dRandReal()*2.0-1.0,dRandReal()*10.0-5.0);
    }
    else {
      dReal maxheight = 0;
      for (k=0; k<num; k++) {
	const dReal *pos = dBodyGetPosition (obj[k].body);
	if (pos[2] > maxheight) maxheight = pos[2];
      }
      dBodySetPosition (obj[i].body, 0,0,maxheight+1);
      dRFromAxisAndAngle (R,0,0,1,dRandReal()*10.0-5.0);
    }
    dBodySetRotation (obj[i].body,R);
    dBodySetData (obj[i].body,(void*) i);

    if (cmd == 'b') {
      dMassSetBox (&m,DENSITY,sides[0],sides[1],sides[2]);
      obj[i].geom[0] = dCreateBox (space,sides[0],sides[1],sides[2]);
    }
    else if (cmd == 'c') {
      sides[0] *= 0.5;
      dMassSetCappedCylinder (&m,DENSITY,3,sides[0],sides[1]);
      obj[i].geom[0] = dCreateCCylinder (space,sides[0],sides[1]);
    }
/*
    // cylinder option not yet implemented
    else if (cmd == 'l') {
      sides[1] *= 0.5;
      dMassSetCappedCylinder (&m,DENSITY,3,sides[0],sides[1]);
      obj[i].geom[0] = dCreateCylinder (space,sides[0],sides[1]);
    }
*/
    else if (cmd == 's') {
      sides[0] *= 0.5;
      dMassSetSphere (&m,DENSITY,sides[0]);
      obj[i].geom[0] = dCreateSphere (space,sides[0]);
    }
    else if (cmd == 'x') {
      dGeomID g2[GPB];		// encapsulated geometries
      dReal dpos[GPB][3];	// delta-positions for encapsulated geometries

      // start accumulating masses for the encapsulated geometries
      dMass m2;
      dMassSetZero (&m);

      // set random delta positions
      for (j=0; j<GPB; j++) {
	for (k=0; k<3; k++) dpos[j][k] = dRandReal()*0.3-0.15;
      }

      for (k=0; k<GPB; k++) {
	obj[i].geom[k] = dCreateGeomTransform (space);
	dGeomTransformSetCleanup (obj[i].geom[k],1);
	if (k==0) {
	  dReal radius = dRandReal()*0.25+0.05;
	  g2[k] = dCreateSphere (0,radius);
	  dMassSetSphere (&m2,DENSITY,radius);
	}
	else if (k==1) {
	  g2[k] = dCreateBox (0,sides[0],sides[1],sides[2]);
	  dMassSetBox (&m2,DENSITY,sides[0],sides[1],sides[2]);
	}
	else {
	  dReal radius = dRandReal()*0.1+0.05;
	  dReal length = dRandReal()*1.0+0.1;
	  g2[k] = dCreateCCylinder (0,radius,length);
	  dMassSetCappedCylinder (&m2,DENSITY,3,radius,length);
	}
	dGeomTransformSetGeom (obj[i].geom[k],g2[k]);

	// set the transformation (adjust the mass too)
	dGeomSetPosition (g2[k],dpos[k][0],dpos[k][1],dpos[k][2]);
	dMassTranslate (&m2,dpos[k][0],dpos[k][1],dpos[k][2]);
	dMatrix3 Rtx;
	dRFromAxisAndAngle (Rtx,dRandReal()*2.0-1.0,dRandReal()*2.0-1.0,
			    dRandReal()*2.0-1.0,dRandReal()*10.0-5.0);
	dGeomSetRotation (g2[k],Rtx);
	dMassRotate (&m2,Rtx);

	// add to the total mass
	dMassAdd (&m,&m2);
      }

      // move all encapsulated objects so that the center of mass is (0,0,0)
      for (k=0; k<2; k++) {
	dGeomSetPosition (g2[k],
			  dpos[k][0]-m.c[0],
			  dpos[k][1]-m.c[1],
			  dpos[k][2]-m.c[2]);
      }
      dMassTranslate (&m,-m.c[0],-m.c[1],-m.c[2]);
    }

    for (k=0; k < GPB; k++) {
      if (obj[i].geom[k]) dGeomSetBody (obj[i].geom[k],obj[i].body);
    }

    dBodySetMass (obj[i].body,&m);
  }

  if (cmd == ' ') {
    selected++;
    if (selected >= num) selected = 0;
    if (selected < 0) selected = 0;
  }
  else if (cmd == 'd' && selected >= 0 && selected < num) {
    dBodyDisable (obj[selected].body);
  }
  else if (cmd == 'e' && selected >= 0 && selected < num) {
    dBodyEnable (obj[selected].body);
  }
  else if (cmd == 'a') {
    show_aabb ^= 1;
  }
  else if (cmd == 't') {
    show_contacts ^= 1;
  }
  else if (cmd == 'r') {
    random_pos ^= 1;
  }
  else if (cmd == 'o') {
    draw_geom ^= 1;
  }
}


// draw a geom

void drawGeom (dGeomID g, const dReal *pos, const dReal *R, int show_aabb)
{
	if (!draw_geom){
		return;
	}

  if (!g) return;
  if (!pos) pos = dGeomGetPosition (g);
  if (!R) R = dGeomGetRotation (g);

  int type = dGeomGetClass (g);
  if (type == dBoxClass) {
    dVector3 sides;
    dGeomBoxGetLengths (g,sides);
    dsDrawBox (pos,R,sides);
  }
  else if (type == dSphereClass) {
    dsDrawSphere (pos,R,dGeomSphereGetRadius (g));
  }
  else if (type == dCCylinderClass) {
    dReal radius,length;
    dGeomCCylinderGetParams (g,&radius,&length);
    dsDrawCappedCylinder (pos,R,length,radius);
  }
/*
  // cylinder option not yet implemented
  else if (type == dCylinderClass) {
    dReal radius,length;
    dGeomCylinderGetParams (g,&radius,&length);
    dsDrawCylinder (pos,R,length,radius);
  }
*/
  else if (type == dGeomTransformClass) {
    dGeomID g2 = dGeomTransformGetGeom (g);
    const dReal *pos2 = dGeomGetPosition (g2);
    const dReal *R2 = dGeomGetRotation (g2);
    dVector3 actual_pos;
    dMatrix3 actual_R;
    dMULTIPLY0_331 (actual_pos,R,pos2);
    actual_pos[0] += pos[0];
    actual_pos[1] += pos[1];
    actual_pos[2] += pos[2];
    dMULTIPLY0_333 (actual_R,R,R2);
    drawGeom (g2,actual_pos,actual_R,0);
  }

  if (show_aabb) {
    // draw the bounding box for this geom
    dReal aabb[6];
    dGeomGetAABB (g,aabb);
    dVector3 bbpos;
    for (int i=0; i<3; i++) bbpos[i] = 0.5*(aabb[i*2] + aabb[i*2+1]);
    dVector3 bbsides;
    for (int j=0; j<3; j++) bbsides[j] = aabb[j*2+1] - aabb[j*2];
    dMatrix3 RI;
    dRSetIdentity (RI);
    dsSetColorAlpha (1,0,0,0.5);
    dsDrawBox (bbpos,RI,bbsides);
  }
}


// simulation loop

static void simLoop (int pause)
{
  dsSetColor (0,0,2);
  dSpaceCollide (space,0,&nearCallback);
  //if (!pause) dWorldStep (world,0.05);
  //if (!pause) dWorldStepFast (world,0.05, 1);

  // remove all contact joints
  dJointGroupEmpty (contactgroup);

  dsSetColor (1,1,0);
  dsSetTexture (DS_WOOD);
  for (int i=0; i<num; i++) {
    for (int j=0; j < GPB; j++) {
      if (i==selected) {
	dsSetColor (0,0.7,1);
      }
      else if (! dBodyIsEnabled (obj[i].body)) {
	dsSetColor (1,0,0);
      }
      else {
	dsSetColor (1,1,0);
      }
      drawGeom (obj[i].geom[j],0,0,show_aabb);
    }
  }
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


  dVector3 Center = {0, 0, 0, 0};
  dVector3 Extents = {WORLD_SIZE * 0.55, WORLD_SIZE * 0.55, WORLD_SIZE * 0.55, 0};

  //space = dSimpleSpaceCreate(0);
  //space = dHashSpaceCreate (0);
  space = dQuadTreeSpaceCreate (0, Center, Extents, 6);
  
  contactgroup = dJointGroupCreate (0);
  dWorldSetGravity (world,0,0,-0.5);
  dWorldSetCFM (world,1e-5);
  dCreatePlane (space,0,0,1,0);
  memset (obj,0,sizeof(obj));

  for (int i = 0; i < NUM; i++){
	command('s');
  }

  // run simulation
  dsSimulationLoop (argc,argv,352,288,&fn);

  dJointGroupDestroy (contactgroup);
  dSpaceDestroy (space);
  dWorldDestroy (world);

  return 0;
}
