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

test that the rotational physics is correct.

an "anchor body" has a number of other randomly positioned bodies
("particles") attached to it by ball-and-socket joints, giving it some
random effective inertia tensor. the effective inertia matrix is calculated,
and then this inertia is assigned to another "test" body. a random torque is
applied to both bodies and the difference in angular velocity and orientation
is observed after a number of iterations.

typical errors for each test cycle are about 1e-5 ... 1e-4.

*/


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

#define NUM 10			// number of particles
#define SIDE 0.1		// visual size of the particles


// dynamics objects an globals

static dWorldID world=0;
static dBodyID anchor_body,particle[NUM],test_body;
static dJointID particle_joint[NUM];
static dReal torque[3];
static int iteration;


// start simulation - set viewpoint

static void start()
{
  static float xyz[3] = {1.5572f,-1.8886f,1.5700f};
  static float hpr[3] = {118.5000f,-17.0000f,0.0000f};
  dsSetViewpoint (xyz,hpr);
}


// compute the mass parameters of a particle set. q = particle positions,
// pm = particle masses

#define _I(i,j) I[(i)*4+(j)]

void computeMassParams (dMass *m, dReal q[NUM][3], dReal pm[NUM])
{
  int i,j;
  dMassSetZero (m);
  for (i=0; i<NUM; i++) {
    m->mass += pm[i];
    for (j=0; j<3; j++) m->c[j] += pm[i]*q[i][j];
    m->_I(0,0) += pm[i]*(q[i][1]*q[i][1] + q[i][2]*q[i][2]);
    m->_I(1,1) += pm[i]*(q[i][0]*q[i][0] + q[i][2]*q[i][2]);
    m->_I(2,2) += pm[i]*(q[i][0]*q[i][0] + q[i][1]*q[i][1]);
    m->_I(0,1) -= pm[i]*(q[i][0]*q[i][1]);
    m->_I(0,2) -= pm[i]*(q[i][0]*q[i][2]);
    m->_I(1,2) -= pm[i]*(q[i][1]*q[i][2]);
  }
  for (j=0; j<3; j++) m->c[j] /= m->mass;
  m->_I(1,0) = m->_I(0,1);
  m->_I(2,0) = m->_I(0,2);
  m->_I(2,1) = m->_I(1,2);
}


void reset_test()
{
  int i;
  dMass m,anchor_m;
  dReal q[NUM][3], pm[NUM];	// particle positions and masses
  dReal pos1[3] = {1,0,1};	// point of reference (POR)
  dReal pos2[3] = {-1,0,1};	// point of reference (POR)

  // make random particle positions (relative to POR) and masses
  for (i=0; i<NUM; i++) {
    pm[i] = dRandReal()+0.1;
    q[i][0] = dRandReal()-0.5;
    q[i][1] = dRandReal()-0.5;
    q[i][2] = dRandReal()-0.5;
  }

  // adjust particle positions so centor of mass = POR
  computeMassParams (&m,q,pm);
  for (i=0; i<NUM; i++) {
    q[i][0] -= m.c[0];
    q[i][1] -= m.c[1];
    q[i][2] -= m.c[2];
  }

  if (world) dWorldDestroy (world);
  world = dWorldCreate();

  anchor_body = dBodyCreate (world);
  dBodySetPosition (anchor_body,pos1[0],pos1[1],pos1[2]);
  dMassSetBox (&anchor_m,1,SIDE,SIDE,SIDE);
  dMassAdjust (&anchor_m,0.1);
  dBodySetMass (anchor_body,&anchor_m);

  for (i=0; i<NUM; i++) {
    particle[i] = dBodyCreate (world);
    dBodySetPosition (particle[i],
		      pos1[0]+q[i][0],pos1[1]+q[i][1],pos1[2]+q[i][2]);
    dMassSetBox (&m,1,SIDE,SIDE,SIDE);
    dMassAdjust (&m,pm[i]);
    dBodySetMass (particle[i],&m);
  }

  for (i=0; i < NUM; i++) {
    particle_joint[i] = dJointCreateBall (world,0);
    dJointAttach (particle_joint[i],anchor_body,particle[i]);
    const dReal *p = dBodyGetPosition (particle[i]);
    dJointSetBallAnchor (particle_joint[i],p[0],p[1],p[2]);
  }

  // make test_body with the same mass and inertia of the anchor_body plus
  // all the particles

  test_body = dBodyCreate (world);
  dBodySetPosition (test_body,pos2[0],pos2[1],pos2[2]);
  computeMassParams (&m,q,pm);
  m.mass += anchor_m.mass;
  for (i=0; i<12; i++) m.I[i] = m.I[i] + anchor_m.I[i];
  dBodySetMass (test_body,&m);

  // rotate the test and anchor bodies by a random amount
  dQuaternion qrot;
  for (i=0; i<4; i++) qrot[i] = dRandReal()-0.5;
  dNormalize4 (qrot);
  dBodySetQuaternion (anchor_body,qrot);
  dBodySetQuaternion (test_body,qrot);
  dMatrix3 R;
  dQtoR (qrot,R);
  for (i=0; i<NUM; i++) {
    dVector3 v;
    dMultiply0 (v,R,&q[i][0],3,3,1);
    dBodySetPosition (particle[i],pos1[0]+v[0],pos1[1]+v[1],pos1[2]+v[2]);
  }

  // set random torque
  for (i=0; i<3; i++) torque[i] = (dRandReal()-0.5) * 0.1;


  iteration=0;
}


// simulation loop

static void simLoop (int pause)
{
  if (!pause) {
    dBodyAddTorque (anchor_body,torque[0],torque[1],torque[2]);
    dBodyAddTorque (test_body,torque[0],torque[1],torque[2]);
    dWorldStep (world,0.03);

    iteration++;
    if (iteration >= 100) {
      // measure the difference between the anchor and test bodies
      const dReal *w1 = dBodyGetAngularVel (anchor_body);
      const dReal *w2 = dBodyGetAngularVel (test_body);
      const dReal *q1 = dBodyGetQuaternion (anchor_body);
      const dReal *q2 = dBodyGetQuaternion (test_body);
      dReal maxdiff = dMaxDifference (w1,w2,1,3);
      printf ("w-error = %.4e  (%.2f,%.2f,%.2f) and (%.2f,%.2f,%.2f)\n",
	      maxdiff,w1[0],w1[1],w1[2],w2[0],w2[1],w2[2]);
      maxdiff = dMaxDifference (q1,q2,1,4);
      printf ("q-error = %.4e\n",maxdiff);
      reset_test();
    }
  }

  dReal sides[3] = {SIDE,SIDE,SIDE};
  dReal sides2[3] = {6*SIDE,6*SIDE,6*SIDE};
  dReal sides3[3] = {3*SIDE,3*SIDE,3*SIDE};
  dsSetColor (1,1,1);
  dsDrawBox (dBodyGetPosition(anchor_body), dBodyGetRotation(anchor_body),
	     sides3);
  dsSetColor (1,0,0);
  dsDrawBox (dBodyGetPosition(test_body), dBodyGetRotation(test_body), sides2);
  dsSetColor (1,1,0);
  for (int i=0; i<NUM; i++)
    dsDrawBox (dBodyGetPosition (particle[i]),
	       dBodyGetRotation (particle[i]), sides);
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
  reset_test();

  // run simulation
  dsSimulationLoop (argc,argv,352,288,&fn);

  dWorldDestroy (world);
  return 0;
}
