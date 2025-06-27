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

/*

testing procedure:
  * create a bunch of random boxes
  * test for intersections directly, put results in n^2 array
  * get space to report collisions:
    - all correct collisions reported
    - no pair reported more than once
    - no incorrect collisions reported

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

#define NUM 20			// number of boxes to test


// collision objects and globals

static dSpaceID space;
static dGeomID geom[NUM];
static dReal bounds[NUM][6];
static int good_matrix[NUM][NUM];	// correct collision matrix
static int test_matrix[NUM][NUM];	// testing collision matrix
static int hits[NUM];			// number of collisions a box has
static unsigned long seed=37;


static void init_test()
{
  int i,j;
  const dReal scale = 0.5;

  // set random boxes
  dRandSetSeed (seed);
  for (i=0; i < NUM; i++) {
    bounds[i][0] = dRandReal()*2-1;
    bounds[i][1] = bounds[i][0] + dRandReal()*scale;
    bounds[i][2] = dRandReal()*2-1;
    bounds[i][3] = bounds[i][2] + dRandReal()*scale;
    bounds[i][4] = dRandReal()*2;
    bounds[i][5] = bounds[i][4] + dRandReal()*scale;

    if (geom[i]) dGeomDestroy (geom[i]);
    geom[i] = dCreateBox (space,
			  bounds[i][1] - bounds[i][0],
			  bounds[i][3] - bounds[i][2],
			  bounds[i][5] - bounds[i][4]);
    dGeomSetPosition (geom[i],
		      (bounds[i][0] + bounds[i][1])*0.5,
		      (bounds[i][2] + bounds[i][3])*0.5,
		      (bounds[i][4] + bounds[i][5])*0.5);
    dGeomSetData (geom[i],(void*) (i));
  }

  // compute all intersections and put the results in "good_matrix"
  for (i=0; i < NUM; i++) {
    for (j=0; j < NUM; j++) good_matrix[i][j] = 0;
  }
  for (i=0; i < NUM; i++) hits[i] = 0;

  for (i=0; i < NUM; i++) {
    for (j=i+1; j < NUM; j++) {
      dReal *bounds1 = &bounds[i][0];
      dReal *bounds2 = &bounds[j][0];
      if (bounds1[0] > bounds2[1] ||
	  bounds1[1] < bounds2[0] ||
	  bounds1[2] > bounds2[3] ||
	  bounds1[3] < bounds2[2] ||
	  bounds1[4] > bounds2[5] ||
	  bounds1[5] < bounds2[4]) continue;
      good_matrix[i][j] = 1;
      good_matrix[j][i] = 1;
      hits[i]++;
      hits[j]++;
    }
  }
}


// this is called by dSpaceCollide when two objects in space are
// potentially colliding.

static void nearCallback (void *data, dGeomID o1, dGeomID o2)
{
  int i,j;
  i = (int) dGeomGetData (o1);
  j = (int) dGeomGetData (o2);
  if (i==j)
    printf ("collision (%d,%d) is between the same object\n",i,j);
  if (!good_matrix[i][j] || !good_matrix[j][i])
    printf ("collision (%d,%d) is incorrect\n",i,j);
  if (test_matrix[i][j] || test_matrix[j][i])
    printf ("collision (%d,%d) reported more than once\n",i,j);
  test_matrix[i][j] = 1;
  test_matrix[j][i] = 1;
}


// start simulation - set viewpoint

static void start()
{
  static float xyz[3] = {2.1640f,-1.3079f,1.7600f};
  static float hpr[3] = {125.5000f,-17.0000f,0.0000f};
  dsSetViewpoint (xyz,hpr);
}


static void command (int cmd)
{
  if (cmd == ' ') {
    seed++;
    init_test();
  }
}


// simulation loop

static void simLoop (int pause)
{
  int i,j;

  for (i=0; i < NUM; i++) {
    for (j=0; j < NUM; j++) test_matrix[i][j] = 0;
  }
  dSpaceCollide (space,0,&nearCallback);
  for (i=0; i < NUM; i++) {
    for (j=i+1; j < NUM; j++) {
      if (good_matrix[i][j] && !test_matrix[i][j]) {
	printf ("failed to report collision (%d,%d) (seed=%ld)\n",i,j,seed);
      }
    }
  }

  seed++;
  init_test();

  for (i=0; i<NUM; i++) {
    dVector3 pos,side;
    dMatrix3 R;
    dRSetIdentity (R);
    for (j=0; j<3; j++) pos[j] = (bounds[i][j*2+1] + bounds[i][j*2]) * 0.5;
    for (j=0; j<3; j++) side[j] = bounds[i][j*2+1] - bounds[i][j*2];
    if (hits[i] > 0) dsSetColor (1,0,0);
    else dsSetColor (1,1,0);
    dsDrawBox (pos,R,side);
  }
}


int main (int argc, char **argv)
{
  int i;

  // setup pointers to drawstuff callback functions
  dsFunctions fn;
  fn.version = DS_VERSION;
  fn.start = &start;
  fn.step = &simLoop;
  fn.command = &command;
  fn.stop = 0;
  fn.path_to_textures = "../../drawstuff/textures";

  // test the simple space:
  // space = dSimpleSpaceCreate();

  // test the hash space:
  // space = dHashSpaceCreate (0);
  // dHashSpaceSetLevels (space,-10,10);

  // test the quadtree space
  dVector3 Center = {0, 0, 0, 0};
  dVector3 Extents = {10, 0, 10, 0};
  space = dQuadTreeSpaceCreate(0, Center, Extents, 7);

  for (i=0; i < NUM; i++) geom[i] = 0;
  init_test();

  // run simulation
  dsSimulationLoop (argc,argv,352,288,&fn);

  dSpaceDestroy (space);
  return 0;
}
