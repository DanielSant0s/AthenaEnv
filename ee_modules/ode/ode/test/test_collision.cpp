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

collision tests. if this program is run without any arguments it will
perform all the tests multiple times, with different random data for each
test. if this program is given a test number it will run that test
graphically/interactively, in which case the space bar can be used to
change the random test conditions.

*/


#include <ode/ode.h>
#include <drawstuff/drawstuff.h>

#ifdef _MSC_VER
#pragma warning(disable:4244 4305)  // for VC++, no precision loss complaints
#endif

// select correct drawing functions
#ifdef dDOUBLE
#define dsDrawSphere dsDrawSphereD
#define dsDrawBox dsDrawBoxD
#define dsDrawLine dsDrawLineD
#define dsDrawCappedCylinder dsDrawCappedCylinderD
#endif

//****************************************************************************
// test infrastructure, including constants and macros

#define TEST_REPS1 1000		// run each test this many times (first batch)
#define TEST_REPS2 10000	// run each test this many times (second batch)
const dReal tol = 1e-8;		// tolerance used for numerical checks
#define MAX_TESTS 1000		// maximum number of test slots
#define Z_OFFSET 2		// z offset for drawing (to get above ground)


// test function. returns 1 if the test passed or 0 if it failed
typedef int test_function_t();

struct TestSlot {
  int number;			// number of test
  char *name;			// name of test
  int failcount;
  test_function_t *test_fn;
  int last_failed_line;
};
TestSlot testslot[MAX_TESTS];


// globals used by the test functions
int graphical_test=0;		// show graphical results of this test, 0=none
int current_test;		// currently execiting test
int draw_all_objects_called;


#define MAKE_TEST(number,function) \
  if (testslot[number].name) dDebug (0,"test number already used"); \
  if (number <= 0 || number >= MAX_TESTS) dDebug (0,"bad test number"); \
  testslot[number].name = # function; \
  testslot[number].test_fn = function;

#define FAILED() { if (graphical_test==0) { \
  testslot[current_test].last_failed_line=__LINE__; return 0; } }
#define PASSED() { return 1; }

//****************************************************************************
// globals

int dBoxBox (const dVector3 p1, const dMatrix3 R1,
	     const dVector3 side1, const dVector3 p2,
	     const dMatrix3 R2, const dVector3 side2,
	     dVector3 normal, dReal *depth, int *code,
	     int maxc, dContactGeom *contact, int skip);

void dLineClosestApproach (const dVector3 pa, const dVector3 ua,
			   const dVector3 pb, const dVector3 ub,
			   dReal *alpha, dReal *beta);

//****************************************************************************
// draw all objects in a space, and draw all the collision contact points

void nearCallback (void *data, dGeomID o1, dGeomID o2)
{
  int i,j,n;
  const int N = 100;
  dContactGeom contact[N];

  if (dGeomGetClass (o2) == dRayClass) {
    n = dCollide (o2,o1,N,&contact[0],sizeof(dContactGeom));
  }
  else {
    n = dCollide (o1,o2,N,&contact[0],sizeof(dContactGeom));
  }
  if (n > 0) {
    dMatrix3 RI;
    dRSetIdentity (RI);
    const dReal ss[3] = {0.01,0.01,0.01};
    for (i=0; i<n; i++) {
      contact[i].pos[2] += Z_OFFSET;
      dsDrawBox (contact[i].pos,RI,ss);
      dVector3 n;
      for (j=0; j<3; j++) n[j] = contact[i].pos[j] + 0.1*contact[i].normal[j];
      dsDrawLine (contact[i].pos,n);
    }
  }
}


void draw_all_objects (dSpaceID space)
{
  int i, j;

  draw_all_objects_called = 1;
  if (!graphical_test) return;
  int n = dSpaceGetNumGeoms (space);

  // draw all contact points
  dsSetColor (0,1,1);
  dSpaceCollide (space,0,&nearCallback);

  // draw all rays
  for (i=0; i<n; i++) {
    dGeomID g = dSpaceGetGeom (space,i);
    if (dGeomGetClass (g) == dRayClass) {
      dsSetColor (1,1,1);
      dVector3 origin,dir;
      dGeomRayGet (g,origin,dir);
      origin[2] += Z_OFFSET;
      dReal length = dGeomRayGetLength (g);
      for (j=0; j<3; j++) dir[j] = dir[j]*length + origin[j];
      dsDrawLine (origin,dir);
      dsSetColor (0,0,1);
      dsDrawSphere (origin,dGeomGetRotation(g),0.01);
    }
  }

  // draw all other objects
  for (i=0; i<n; i++) {
    dGeomID g = dSpaceGetGeom (space,i);
    dVector3 pos;
    if (dGeomGetClass (g) != dPlaneClass) {
      memcpy (pos,dGeomGetPosition(g),sizeof(pos));
      pos[2] += Z_OFFSET;
    }

    switch (dGeomGetClass (g)) {

    case dSphereClass: {
      dsSetColorAlpha (1,0,0,0.8);
      dReal radius = dGeomSphereGetRadius (g);
      dsDrawSphere (pos,dGeomGetRotation(g),radius);
      break;
    }

    case dBoxClass: {
      dsSetColorAlpha (1,1,0,0.8);
      dVector3 sides;
      dGeomBoxGetLengths (g,sides);
      dsDrawBox (pos,dGeomGetRotation(g),sides);
      break;
    }

    case dCCylinderClass: {
      dsSetColorAlpha (0,1,0,0.8);
      dReal radius,length;
      dGeomCCylinderGetParams (g,&radius,&length);
      dsDrawCappedCylinder (pos,dGeomGetRotation(g),length,radius);
      break;
    }

    case dPlaneClass: {
      dVector4 n;
      dMatrix3 R,sides;
      dVector3 pos2;
      dGeomPlaneGetParams (g,n);
      dRFromZAxis (R,n[0],n[1],n[2]);
      for (j=0; j<3; j++) pos[j] = n[j]*n[3];
      pos[2] += Z_OFFSET;
      sides[0] = 2;
      sides[1] = 2;
      sides[2] = 0.001;
      dsSetColor (1,0,1);
      for (j=0; j<3; j++) pos2[j] = pos[j] + 0.1*n[j];
      dsDrawLine (pos,pos2);
      dsSetColorAlpha (1,0,1,0.8);
      dsDrawBox (pos,R,sides);
      break;
    }

    }
  }
}

//****************************************************************************
// point depth tests

int test_sphere_point_depth()
{
  int j;
  dVector3 p,q;
  dMatrix3 R;
  dReal r,d;

  dSimpleSpace space(0);
  dGeomID sphere = dCreateSphere (0,1);
  dSpaceAdd (space,sphere);

  // ********** make a random sphere of radius r at position p

  r = dRandReal()+0.1;
  dGeomSphereSetRadius (sphere,r);
  dMakeRandomVector (p,3,1.0);
  dGeomSetPosition (sphere,p[0],p[1],p[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (sphere,R);

  // ********** test center point has depth r

  if (dFabs(dGeomSpherePointDepth (sphere,p[0],p[1],p[2]) - r) > tol) FAILED();

  // ********** test point on surface has depth 0

  for (j=0; j<3; j++) q[j] = dRandReal()-0.5;
  dNormalize3 (q);
  for (j=0; j<3; j++) q[j] = q[j]*r + p[j];
  if (dFabs(dGeomSpherePointDepth (sphere,q[0],q[1],q[2])) > tol) FAILED();

  // ********** test point at random depth

  d = (dRandReal()*2-1) * r;
  for (j=0; j<3; j++) q[j] = dRandReal()-0.5;
  dNormalize3 (q);
  for (j=0; j<3; j++) q[j] = q[j]*(r-d) + p[j];
  if (dFabs(dGeomSpherePointDepth (sphere,q[0],q[1],q[2])-d) > tol) FAILED();

  PASSED();
}


int test_box_point_depth()
{
  int i,j;
  dVector3 s,p,q,q2;	// s = box sides
  dMatrix3 R;
  dReal ss,d;		// ss = smallest side

  dSimpleSpace space(0);
  dGeomID box = dCreateBox (0,1,1,1);
  dSpaceAdd (space,box);

  // ********** make a random box

  for (j=0; j<3; j++) s[j] = dRandReal() + 0.1;
  dGeomBoxSetLengths (box,s[0],s[1],s[2]);
  dMakeRandomVector (p,3,1.0);
  dGeomSetPosition (box,p[0],p[1],p[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (box,R);

  // ********** test center point has depth of smallest side

  ss = 1e9;
  for (j=0; j<3; j++) if (s[j] < ss) ss = s[j];
  if (dFabs(dGeomBoxPointDepth (box,p[0],p[1],p[2]) - 0.5*ss) > tol)
    FAILED();

  // ********** test point on surface has depth 0

  for (j=0; j<3; j++) q[j] = (dRandReal()-0.5)*s[j];
  i = dRandInt (3);
  if (dRandReal() > 0.5) q[i] = 0.5*s[i]; else q[i] = -0.5*s[i];
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q2[j] += p[j];
  if (dFabs(dGeomBoxPointDepth (box,q2[0],q2[1],q2[2])) > tol) FAILED();

  // ********** test points outside box have -ve depth

  for (j=0; j<3; j++) {
    q[j] = 0.5*s[j] + dRandReal() + 0.01;
    if (dRandReal() > 0.5) q[j] = -q[j];
  }
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q2[j] += p[j];
  if (dGeomBoxPointDepth (box,q2[0],q2[1],q2[2]) >= 0) FAILED();

  // ********** test points inside box have +ve depth

  for (j=0; j<3; j++) q[j] = s[j] * 0.99 * (dRandReal()-0.5);
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q2[j] += p[j];
  if (dGeomBoxPointDepth (box,q2[0],q2[1],q2[2]) <= 0) FAILED();

  // ********** test random depth of point aligned along axis (up to ss deep)

  i = dRandInt (3);
  for (j=0; j<3; j++) q[j] = 0;
  d = (dRandReal()*(ss*0.5+1)-1);
  q[i] = s[i]*0.5 - d;
  if (dRandReal() > 0.5) q[i] = -q[i];
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q2[j] += p[j];
  if (dFabs(dGeomBoxPointDepth (box,q2[0],q2[1],q2[2]) - d) >= tol) FAILED();

  PASSED();
}


int test_ccylinder_point_depth()
{
  int j;
  dVector3 p,a;
  dMatrix3 R;
  dReal r,l,beta,x,y,d;

  dSimpleSpace space(0);
  dGeomID ccyl = dCreateCCylinder (0,1,1);
  dSpaceAdd (space,ccyl);

  // ********** make a random ccyl

  r = dRandReal()*0.5 + 0.01;
  l = dRandReal()*1 + 0.01;
  dGeomCCylinderSetParams (ccyl,r,l);
  dMakeRandomVector (p,3,1.0);
  dGeomSetPosition (ccyl,p[0],p[1],p[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (ccyl,R);

  // ********** test point on axis has depth of 'radius'

  beta = dRandReal()-0.5;
  for (j=0; j<3; j++) a[j] = p[j] + l*beta*R[j*4+2];
  if (dFabs(dGeomCCylinderPointDepth (ccyl,a[0],a[1],a[2]) - r) >= tol)
    FAILED();

  // ********** test point on surface (excluding caps) has depth 0

  beta = dRandReal()*2*M_PI;
  x = r*sin(beta);
  y = r*cos(beta);
  beta = dRandReal()-0.5;
  for (j=0; j<3; j++) a[j] = p[j] + x*R[j*4+0] + y*R[j*4+1] + l*beta*R[j*4+2];
  if (dFabs(dGeomCCylinderPointDepth (ccyl,a[0],a[1],a[2])) >= tol) FAILED();

  // ********** test point on surface of caps has depth 0

  for (j=0; j<3; j++) a[j] = dRandReal()-0.5;
  dNormalize3 (a);
  if (dDOT14(a,R+2) > 0) {
    for (j=0; j<3; j++) a[j] = p[j] + a[j]*r + l*0.5*R[j*4+2];
  }
  else {
    for (j=0; j<3; j++) a[j] = p[j] + a[j]*r - l*0.5*R[j*4+2];
  }
  if (dFabs(dGeomCCylinderPointDepth (ccyl,a[0],a[1],a[2])) >= tol) FAILED();

  // ********** test point inside ccyl has positive depth

  for (j=0; j<3; j++) a[j] = dRandReal()-0.5;
  dNormalize3 (a);
  beta = dRandReal()-0.5;
  for (j=0; j<3; j++) a[j] = p[j] + a[j]*r*0.99 + l*beta*R[j*4+2];
  if (dGeomCCylinderPointDepth (ccyl,a[0],a[1],a[2]) < 0) FAILED();

  // ********** test point depth (1)

  d = (dRandReal()*2-1) * r;
  beta = dRandReal()*2*M_PI;
  x = (r-d)*sin(beta);
  y = (r-d)*cos(beta);
  beta = dRandReal()-0.5;
  for (j=0; j<3; j++) a[j] = p[j] + x*R[j*4+0] + y*R[j*4+1] + l*beta*R[j*4+2];
  if (dFabs(dGeomCCylinderPointDepth (ccyl,a[0],a[1],a[2]) - d) >= tol)
    FAILED();

  // ********** test point depth (2)

  d = (dRandReal()*2-1) * r;
  for (j=0; j<3; j++) a[j] = dRandReal()-0.5;
  dNormalize3 (a);
  if (dDOT14(a,R+2) > 0) {
    for (j=0; j<3; j++) a[j] = p[j] + a[j]*(r-d) + l*0.5*R[j*4+2];
  }
  else {
    for (j=0; j<3; j++) a[j] = p[j] + a[j]*(r-d) - l*0.5*R[j*4+2];
  }
  if (dFabs(dGeomCCylinderPointDepth (ccyl,a[0],a[1],a[2]) - d) >= tol)
    FAILED();

  PASSED();
}


int test_plane_point_depth()
{
  int j;
  dVector3 n,p,q,a,b;	// n = plane normal
  dReal d;

  dSimpleSpace space(0);
  dGeomID plane = dCreatePlane (0,0,0,1,0);
  dSpaceAdd (space,plane);

  // ********** make a random plane

  for (j=0; j<3; j++) n[j] = dRandReal() - 0.5;
  dNormalize3 (n);
  d = dRandReal() - 0.5;
  dGeomPlaneSetParams (plane,n[0],n[1],n[2],d);
  dPlaneSpace (n,p,q);

  // ********** test point on plane has depth 0

  a[0] = dRandReal() - 0.5;
  a[1] = dRandReal() - 0.5;
  a[2] = 0;
  for (j=0; j<3; j++) b[j] = a[0]*p[j] + a[1]*q[j] + (a[2]+d)*n[j];
  if (dFabs(dGeomPlanePointDepth (plane,b[0],b[1],b[2])) >= tol) FAILED();

  // ********** test arbitrary depth point

  a[0] = dRandReal() - 0.5;
  a[1] = dRandReal() - 0.5;
  a[2] = dRandReal() - 0.5;
  for (j=0; j<3; j++) b[j] = a[0]*p[j] + a[1]*q[j] + (a[2]+d)*n[j];
  if (dFabs(dGeomPlanePointDepth (plane,b[0],b[1],b[2]) + a[2]) >= tol)
    FAILED();

  // ********** test depth-1 point

  a[0] = dRandReal() - 0.5;
  a[1] = dRandReal() - 0.5;
  a[2] = -1;
  for (j=0; j<3; j++) b[j] = a[0]*p[j] + a[1]*q[j] + (a[2]+d)*n[j];
  if (dFabs(dGeomPlanePointDepth (plane,b[0],b[1],b[2]) - 1) >= tol) FAILED();

  PASSED();
}

//****************************************************************************
// ray tests

int test_ray_and_sphere()
{
  int j;
  dContactGeom contact;
  dVector3 p,q,q2,n,v1;
  dMatrix3 R;
  dReal r,k;

  dSimpleSpace space(0);
  dGeomID ray = dCreateRay (0,0);
  dGeomID sphere = dCreateSphere (0,1);
  dSpaceAdd (space,ray);
  dSpaceAdd (space,sphere);

  // ********** make a random sphere of radius r at position p

  r = dRandReal()+0.1;
  dGeomSphereSetRadius (sphere,r);
  dMakeRandomVector (p,3,1.0);
  dGeomSetPosition (sphere,p[0],p[1],p[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (sphere,R);

  // ********** test zero length ray just inside sphere

  dGeomRaySetLength (ray,0);
  dMakeRandomVector (q,3,1.0);
  dNormalize3 (q);
  for (j=0; j<3; j++) q[j] = 0.99*r * q[j] + p[j];
  dGeomSetPosition (ray,q[0],q[1],q[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (ray,R);
  if (dCollide (ray,sphere,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test zero length ray just outside that sphere

  dGeomRaySetLength (ray,0);
  dMakeRandomVector (q,3,1.0);
  dNormalize3 (q);
  for (j=0; j<3; j++) q[j] = 1.01*r * q[j] + p[j];
  dGeomSetPosition (ray,q[0],q[1],q[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (ray,R);
  if (dCollide (ray,sphere,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test finite length ray totally contained inside the sphere

  dMakeRandomVector (q,3,1.0);
  dNormalize3 (q);
  k = dRandReal();
  for (j=0; j<3; j++) q[j] = k*r*0.99 * q[j] + p[j];
  dMakeRandomVector (q2,3,1.0);
  dNormalize3 (q2);
  k = dRandReal();
  for (j=0; j<3; j++) q2[j] = k*r*0.99 * q2[j] + p[j];
  for (j=0; j<3; j++) n[j] = q2[j] - q[j];
  dNormalize3 (n);
  dGeomRaySet (ray,q[0],q[1],q[2],n[0],n[1],n[2]);
  dGeomRaySetLength (ray,dDISTANCE (q,q2));
  if (dCollide (ray,sphere,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test finite length ray totally outside the sphere

  dMakeRandomVector (q,3,1.0);
  dNormalize3 (q);
  do {
    dMakeRandomVector (n,3,1.0);
    dNormalize3 (n);
  }
  while (dDOT(n,q) < 0);	// make sure normal goes away from sphere
  for (j=0; j<3; j++) q[j] = 1.01*r * q[j] + p[j];
  dGeomRaySet (ray,q[0],q[1],q[2],n[0],n[1],n[2]);
  dGeomRaySetLength (ray,100);
  if (dCollide (ray,sphere,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test ray from outside to just above surface

  dMakeRandomVector (q,3,1.0);
  dNormalize3 (q);
  for (j=0; j<3; j++) n[j] = -q[j];
  for (j=0; j<3; j++) q2[j] = 2*r * q[j] + p[j];
  dGeomRaySet (ray,q2[0],q2[1],q2[2],n[0],n[1],n[2]);
  dGeomRaySetLength (ray,0.99*r);
  if (dCollide (ray,sphere,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test ray from outside to just below surface

  dGeomRaySetLength (ray,1.01*r);
  if (dCollide (ray,sphere,0,&contact,sizeof(dContactGeom)) != 1) FAILED();
  for (j=0; j<3; j++) q2[j] = r * q[j] + p[j];
  if (dDISTANCE (contact.pos,q2) > tol) FAILED();

  // ********** test contact point distance for random rays

  dMakeRandomVector (q,3,1.0);
  dNormalize3 (q);
  k = dRandReal()+0.5;
  for (j=0; j<3; j++) q[j] = k*r * q[j] + p[j];
  dMakeRandomVector (n,3,1.0);
  dNormalize3 (n);
  dGeomRaySet (ray,q[0],q[1],q[2],n[0],n[1],n[2]);
  dGeomRaySetLength (ray,100);
  if (dCollide (ray,sphere,0,&contact,sizeof(dContactGeom))) {
    k = dDISTANCE (contact.pos,dGeomGetPosition(sphere));
    if (dFabs(k - r) > tol) FAILED();
    // also check normal signs
    if (dDOT (n,contact.normal) > 0) FAILED();
    // also check depth of contact point
    if (dFabs (dGeomSpherePointDepth
	       (sphere,contact.pos[0],contact.pos[1],contact.pos[2])) > tol)
      FAILED();

    draw_all_objects (space);
  }

  // ********** test tangential grazing - miss

  dMakeRandomVector (q,3,1.0);
  dNormalize3 (q);
  dPlaneSpace (q,n,v1);
  for (j=0; j<3; j++) q[j] = 1.01*r * q[j] + p[j];
  for (j=0; j<3; j++) q[j] -= n[j];
  dGeomRaySet (ray,q[0],q[1],q[2],n[0],n[1],n[2]);
  dGeomRaySetLength (ray,2);
  if (dCollide (ray,sphere,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test tangential grazing - hit

  dMakeRandomVector (q,3,1.0);
  dNormalize3 (q);
  dPlaneSpace (q,n,v1);
  for (j=0; j<3; j++) q[j] = 0.99*r * q[j] + p[j];
  for (j=0; j<3; j++) q[j] -= n[j];
  dGeomRaySet (ray,q[0],q[1],q[2],n[0],n[1],n[2]);
  dGeomRaySetLength (ray,2);
  if (dCollide (ray,sphere,0,&contact,sizeof(dContactGeom)) != 1) FAILED();

  PASSED();
}


int test_ray_and_box()
{
  int i,j;
  dContactGeom contact;
  dVector3 s,p,q,n,q2,q3,q4;		// s = box sides
  dMatrix3 R;
  dReal k;

  dSimpleSpace space(0);
  dGeomID ray = dCreateRay (0,0);
  dGeomID box = dCreateBox (0,1,1,1);
  dSpaceAdd (space,ray);
  dSpaceAdd (space,box);

  // ********** make a random box

  for (j=0; j<3; j++) s[j] = dRandReal() + 0.1;
  dGeomBoxSetLengths (box,s[0],s[1],s[2]);
  dMakeRandomVector (p,3,1.0);
  dGeomSetPosition (box,p[0],p[1],p[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (box,R);

  // ********** test zero length ray just inside box

  dGeomRaySetLength (ray,0);
  for (j=0; j<3; j++) q[j] = (dRandReal()-0.5)*s[j];
  i = dRandInt (3);
  if (dRandReal() > 0.5) q[i] = 0.99*0.5*s[i]; else q[i] = -0.99*0.5*s[i];
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q2[j] += p[j];
  dGeomSetPosition (ray,q2[0],q2[1],q2[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (ray,R);
  if (dCollide (ray,box,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test zero length ray just outside box

  dGeomRaySetLength (ray,0);
  for (j=0; j<3; j++) q[j] = (dRandReal()-0.5)*s[j];
  i = dRandInt (3);
  if (dRandReal() > 0.5) q[i] = 1.01*0.5*s[i]; else q[i] = -1.01*0.5*s[i];
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q2[j] += p[j];
  dGeomSetPosition (ray,q2[0],q2[1],q2[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (ray,R);
  if (dCollide (ray,box,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test finite length ray totally contained inside the box

  for (j=0; j<3; j++) q[j] = (dRandReal()-0.5)*0.99*s[j];
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q2[j] += p[j];
  for (j=0; j<3; j++) q3[j] = (dRandReal()-0.5)*0.99*s[j];
  dMultiply0 (q4,dGeomGetRotation(box),q3,3,3,1);
  for (j=0; j<3; j++) q4[j] += p[j];
  for (j=0; j<3; j++) n[j] = q4[j] - q2[j];
  dNormalize3 (n);
  dGeomRaySet (ray,q2[0],q2[1],q2[2],n[0],n[1],n[2]);
  dGeomRaySetLength (ray,dDISTANCE(q2,q4));
  if (dCollide (ray,box,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test finite length ray totally outside the box

  for (j=0; j<3; j++) q[j] = (dRandReal()-0.5)*s[j];
  i = dRandInt (3);
  if (dRandReal() > 0.5) q[i] = 1.01*0.5*s[i]; else q[i] = -1.01*0.5*s[i];
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q3[j] = q2[j] + p[j];
  dNormalize3 (q2);
  dGeomRaySet (ray,q3[0],q3[1],q3[2],q2[0],q2[1],q2[2]);
  dGeomRaySetLength (ray,10);
  if (dCollide (ray,box,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test ray from outside to just above surface

  for (j=0; j<3; j++) q[j] = (dRandReal()-0.5)*s[j];
  i = dRandInt (3);
  if (dRandReal() > 0.5) q[i] = 1.01*0.5*s[i]; else q[i] = -1.01*0.5*s[i];
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q3[j] = 2*q2[j] + p[j];
  k = dSqrt(q2[0]*q2[0] + q2[1]*q2[1] + q2[2]*q2[2]);
  for (j=0; j<3; j++) q2[j] = -q2[j];
  dGeomRaySet (ray,q3[0],q3[1],q3[2],q2[0],q2[1],q2[2]);
  dGeomRaySetLength (ray,k*0.99);
  if (dCollide (ray,box,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test ray from outside to just below surface

  dGeomRaySetLength (ray,k*1.01);
  if (dCollide (ray,box,0,&contact,sizeof(dContactGeom)) != 1) FAILED();

  // ********** test contact point position for random rays

  for (j=0; j<3; j++) q[j] = dRandReal()*s[j];
  dMultiply0 (q2,dGeomGetRotation(box),q,3,3,1);
  for (j=0; j<3; j++) q2[j] += p[j];
  for (j=0; j<3; j++) q3[j] = dRandReal()-0.5;
  dNormalize3 (q3);
  dGeomRaySet (ray,q2[0],q2[1],q2[2],q3[0],q3[1],q3[2]);
  dGeomRaySetLength (ray,10);
  if (dCollide (ray,box,0,&contact,sizeof(dContactGeom))) {
    // check depth of contact point
    if (dFabs (dGeomBoxPointDepth
	       (box,contact.pos[0],contact.pos[1],contact.pos[2])) > tol)
      FAILED();
    // check position of contact point
    for (j=0; j<3; j++) contact.pos[j] -= p[j];
    dMultiply1 (q,dGeomGetRotation(box),contact.pos,3,3,1);
    if ( dFabs(dFabs (q[0]) - 0.5*s[0]) > tol &&
	 dFabs(dFabs (q[1]) - 0.5*s[1]) > tol &&
	 dFabs(dFabs (q[2]) - 0.5*s[2]) > tol) {
      FAILED();
    }
    // also check normal signs
    if (dDOT (q3,contact.normal) > 0) FAILED();

    draw_all_objects (space);
  }

  PASSED();
}


int test_ray_and_ccylinder()
{
  int j;
  dContactGeom contact;
  dVector3 p,a,b,n;
  dMatrix3 R;
  dReal r,l,k,x,y;

  dSimpleSpace space(0);
  dGeomID ray = dCreateRay (0,0);
  dGeomID ccyl = dCreateCCylinder (0,1,1);
  dSpaceAdd (space,ray);
  dSpaceAdd (space,ccyl);

  // ********** make a random capped cylinder

  r = dRandReal()*0.5 + 0.01;
  l = dRandReal()*1 + 0.01;
  dGeomCCylinderSetParams (ccyl,r,l);
  dMakeRandomVector (p,3,1.0);
  dGeomSetPosition (ccyl,p[0],p[1],p[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (ccyl,R);

  // ********** test ray completely within ccyl

  for (j=0; j<3; j++) a[j] = dRandReal()-0.5;
  dNormalize3 (a);
  k = (dRandReal()-0.5)*l;
  for (j=0; j<3; j++) a[j] = p[j] + r*0.99*a[j] + k*0.99*R[j*4+2];
  for (j=0; j<3; j++) b[j] = dRandReal()-0.5;
  dNormalize3 (b);
  k = (dRandReal()-0.5)*l;
  for (j=0; j<3; j++) b[j] = p[j] + r*0.99*b[j] + k*0.99*R[j*4+2];
  dGeomRaySetLength (ray,dDISTANCE(a,b));
  for (j=0; j<3; j++) b[j] -= a[j];
  dNormalize3 (b);
  dGeomRaySet (ray,a[0],a[1],a[2],b[0],b[1],b[2]);
  if (dCollide (ray,ccyl,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test ray outside ccyl that just misses (between caps)

  k = dRandReal()*2*M_PI;
  x = sin(k);
  y = cos(k);
  for (j=0; j<3; j++) a[j] = x*R[j*4+0] + y*R[j*4+1];
  k = (dRandReal()-0.5)*l;
  for (j=0; j<3; j++) b[j] = -a[j]*r*2 + k*R[j*4+2] + p[j];
  dGeomRaySet (ray,b[0],b[1],b[2],a[0],a[1],a[2]);
  dGeomRaySetLength (ray,r*0.99);
  if (dCollide (ray,ccyl,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test ray outside ccyl that just hits (between caps)

  dGeomRaySetLength (ray,r*1.01);
  if (dCollide (ray,ccyl,0,&contact,sizeof(dContactGeom)) != 1) FAILED();
  // check depth of contact point
  if (dFabs (dGeomCCylinderPointDepth
	     (ccyl,contact.pos[0],contact.pos[1],contact.pos[2])) > tol)
    FAILED();

  // ********** test ray outside ccyl that just misses (caps)

  for (j=0; j<3; j++) a[j] = dRandReal()-0.5;
  dNormalize3 (a);
  if (dDOT14(a,R+2) < 0) {
    for (j=0; j<3; j++) b[j] = p[j] - a[j]*2*r + l*0.5*R[j*4+2];
  }
  else {
    for (j=0; j<3; j++) b[j] = p[j] - a[j]*2*r - l*0.5*R[j*4+2];
  }
  dGeomRaySet (ray,b[0],b[1],b[2],a[0],a[1],a[2]);
  dGeomRaySetLength (ray,r*0.99);
  if (dCollide (ray,ccyl,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test ray outside ccyl that just hits (caps)

  dGeomRaySetLength (ray,r*1.01);
  if (dCollide (ray,ccyl,0,&contact,sizeof(dContactGeom)) != 1) FAILED();
  // check depth of contact point
  if (dFabs (dGeomCCylinderPointDepth
	     (ccyl,contact.pos[0],contact.pos[1],contact.pos[2])) > tol)
    FAILED();

  // ********** test random rays

  for (j=0; j<3; j++) a[j] = dRandReal()-0.5;
  for (j=0; j<3; j++) n[j] = dRandReal()-0.5;
  dNormalize3 (n);
  dGeomRaySet (ray,a[0],a[1],a[2],n[0],n[1],n[2]);
  dGeomRaySetLength (ray,10);

  if (dCollide (ray,ccyl,0,&contact,sizeof(dContactGeom))) {
    // check depth of contact point
    if (dFabs (dGeomCCylinderPointDepth
	       (ccyl,contact.pos[0],contact.pos[1],contact.pos[2])) > tol)
      FAILED();

    // check normal signs
    if (dDOT (n,contact.normal) > 0) FAILED();

    draw_all_objects (space);
  }

  PASSED();
}


int test_ray_and_plane()
{
  int j;
  dContactGeom contact;
  dVector3 n,p,q,a,b,g,h;		// n,d = plane parameters
  dMatrix3 R;
  dReal d;

  dSimpleSpace space(0);
  dGeomID ray = dCreateRay (0,0);
  dGeomID plane = dCreatePlane (0,0,0,1,0);
  dSpaceAdd (space,ray);
  dSpaceAdd (space,plane);

  // ********** make a random plane

  for (j=0; j<3; j++) n[j] = dRandReal() - 0.5;
  dNormalize3 (n);
  d = dRandReal() - 0.5;
  dGeomPlaneSetParams (plane,n[0],n[1],n[2],d);
  dPlaneSpace (n,p,q);

  // ********** test finite length ray below plane

  dGeomRaySetLength (ray,0.09);
  a[0] = dRandReal()-0.5;
  a[1] = dRandReal()-0.5;
  a[2] = -dRandReal()*0.5 - 0.1;
  for (j=0; j<3; j++) b[j] = a[0]*p[j] + a[1]*q[j] + (a[2]+d)*n[j];
  dGeomSetPosition (ray,b[0],b[1],b[2]);
  dRFromAxisAndAngle (R,dRandReal()*2-1,dRandReal()*2-1,
		      dRandReal()*2-1,dRandReal()*10-5);
  dGeomSetRotation (ray,R);
  if (dCollide (ray,plane,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test finite length ray above plane

  a[0] = dRandReal()-0.5;
  a[1] = dRandReal()-0.5;
  a[2] = dRandReal()*0.5 + 0.01;
  for (j=0; j<3; j++) b[j] = a[0]*p[j] + a[1]*q[j] + (a[2]+d)*n[j];
  g[0] = dRandReal()-0.5;
  g[1] = dRandReal()-0.5;
  g[2] = dRandReal() + 0.01;
  for (j=0; j<3; j++) h[j] = g[0]*p[j] + g[1]*q[j] + g[2]*n[j];
  dNormalize3 (h);
  dGeomRaySet (ray,b[0],b[1],b[2],h[0],h[1],h[2]);
  dGeomRaySetLength (ray,10);
  if (dCollide (ray,plane,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test finite length ray that intersects plane

  a[0] = dRandReal()-0.5;
  a[1] = dRandReal()-0.5;
  a[2] = dRandReal()-0.5;
  for (j=0; j<3; j++) b[j] = a[0]*p[j] + a[1]*q[j] + (a[2]+d)*n[j];
  g[0] = dRandReal()-0.5;
  g[1] = dRandReal()-0.5;
  g[2] = dRandReal()-0.5;
  for (j=0; j<3; j++) h[j] = g[0]*p[j] + g[1]*q[j] + g[2]*n[j];
  dNormalize3 (h);
  dGeomRaySet (ray,b[0],b[1],b[2],h[0],h[1],h[2]);
  dGeomRaySetLength (ray,10);
  if (dCollide (ray,plane,0,&contact,sizeof(dContactGeom))) {
    // test that contact is on plane surface
    if (dFabs (dDOT(contact.pos,n) - d) > tol) FAILED();
    // also check normal signs
    if (dDOT (h,contact.normal) > 0) FAILED();
    // also check contact point depth
    if (dFabs (dGeomPlanePointDepth
	       (plane,contact.pos[0],contact.pos[1],contact.pos[2])) > tol)
      FAILED();

    draw_all_objects (space);
  }

  // ********** test ray that just misses

  for (j=0; j<3; j++) b[j] = (1+d)*n[j];
  for (j=0; j<3; j++) h[j] = -n[j];
  dGeomRaySet (ray,b[0],b[1],b[2],h[0],h[1],h[2]);
  dGeomRaySetLength (ray,0.99);
  if (dCollide (ray,plane,0,&contact,sizeof(dContactGeom)) != 0) FAILED();

  // ********** test ray that just hits

  dGeomRaySetLength (ray,1.01);
  if (dCollide (ray,plane,0,&contact,sizeof(dContactGeom)) != 1) FAILED();

  // ********** test polarity with typical ground plane

  dGeomPlaneSetParams (plane,0,0,1,0);
  for (j=0; j<3; j++) a[j] = 0.1;
  for (j=0; j<3; j++) b[j] = 0;
  a[2] = 1;
  b[2] = -1;
  dGeomRaySet (ray,a[0],a[1],a[2],b[0],b[1],b[2]);
  dGeomRaySetLength (ray,2);
  if (dCollide (ray,plane,0,&contact,sizeof(dContactGeom)) != 1) FAILED();
  if (dFabs (contact.depth - 1) > tol) FAILED();
  a[2] = -1;
  b[2] = 1;
  dGeomRaySet (ray,a[0],a[1],a[2],b[0],b[1],b[2]);
  if (dCollide (ray,plane,0,&contact,sizeof(dContactGeom)) != 1) FAILED();
  if (dFabs (contact.depth - 1) > tol) FAILED();

  PASSED();
}

//****************************************************************************
// a really inefficient, but hopefully correct implementation of
// dBoxTouchesBox(), that does 144 edge-face tests.

// return 1 if edge v1 -> v2 hits the rectangle described by p1,p2,p3

static int edgeIntersectsRect (dVector3 v1, dVector3 v2,
			       dVector3 p1, dVector3 p2, dVector3 p3)
{
  int k;
  dVector3 u1,u2,n,tmp;
  for (k=0; k<3; k++) u1[k] = p3[k]-p1[k];
  for (k=0; k<3; k++) u2[k] = p2[k]-p1[k];
  dReal d1 = dSqrt(dDOT(u1,u1));
  dReal d2 = dSqrt(dDOT(u2,u2));
  dNormalize3 (u1);
  dNormalize3 (u2);
  if (dFabs(dDOT(u1,u2)) > 1e-8) dDebug (0,"bad");
  dCROSS (n,=,u1,u2);
  for (k=0; k<3; k++) tmp[k] = v2[k]-v1[k];
  dReal d = -dDOT(n,p1);
  if (dFabs(dDOT(n,p1)+d) > 1e-8) dDebug (0,"bad");
  if (dFabs(dDOT(n,p2)+d) > 1e-8) dDebug (0,"bad");
  if (dFabs(dDOT(n,p3)+d) > 1e-8) dDebug (0,"bad");
  dReal alpha = -(d+dDOT(n,v1))/dDOT(n,tmp);
  for (k=0; k<3; k++) tmp[k] = v1[k]+alpha*(v2[k]-v1[k]);
  if (dFabs(dDOT(n,tmp)+d) > 1e-8) dDebug (0,"bad");
  if (alpha < 0) return 0;
  if (alpha > 1) return 0;
  for (k=0; k<3; k++) tmp[k] -= p1[k];
  dReal a1 = dDOT(u1,tmp);
  dReal a2 = dDOT(u2,tmp);
  if (a1<0 || a2<0 || a1>d1 || a2>d2) return 0;
  return 1;
}


// return 1 if box 1 is completely inside box 2

static int box1inside2 (const dVector3 p1, const dMatrix3 R1,
			const dVector3 side1, const dVector3 p2,
			const dMatrix3 R2, const dVector3 side2)
{
  for (int i=-1; i<=1; i+=2) {
    for (int j=-1; j<=1; j+=2) {
      for (int k=-1; k<=1; k+=2) {
	dVector3 v,vv;
	v[0] = i*0.5*side1[0];
	v[1] = j*0.5*side1[1];
	v[2] = k*0.5*side1[2];
	dMULTIPLY0_331 (vv,R1,v);
	vv[0] += p1[0] - p2[0];
	vv[1] += p1[1] - p2[1];
	vv[2] += p1[2] - p2[2];
	for (int axis=0; axis < 3; axis++) {
	  dReal z = dDOT14(vv,R2+axis);
	  if (z < (-side2[axis]*0.5) || z > (side2[axis]*0.5)) return 0;
	}
      }
    }
  }
  return 1;
}


// test if any edge from box 1 hits a face from box 2

static int testBoxesTouch2 (const dVector3 p1, const dMatrix3 R1,
			    const dVector3 side1, const dVector3 p2,
			    const dMatrix3 R2, const dVector3 side2)
{
  int j,k,j1,j2;

  // for 6 faces from box 2
  for (int fd=0; fd<3; fd++) {		// direction for face

    for (int fo=0; fo<2; fo++) {	// offset of face
      // get four points on the face. first get 2 indexes that are not fd
      int k1=0,k2=0;
      if (fd==0) { k1 = 1; k2 = 2; }
      if (fd==1) { k1 = 0; k2 = 2; }
      if (fd==2) { k1 = 0; k2 = 1; }
      dVector3 fp[4],tmp;
      k=0;
      for (j1=-1; j1<=1; j1+=2) {
	for (j2=-1; j2<=1; j2+=2) {
	  fp[k][k1] = j1;
	  fp[k][k2] = j2;
	  fp[k][fd] = fo*2-1;
	  k++;
	}
      }
      for (j=0; j<4; j++) {
	for (k=0; k<3; k++) fp[j][k] *= 0.5*side2[k];
	dMULTIPLY0_331 (tmp,R2,fp[j]);
	for (k=0; k<3; k++) fp[j][k] = tmp[k] + p2[k];
      }

      // for 8 vertices
      dReal v1[3];
      for (v1[0]=-1; v1[0] <= 1; v1[0] += 2) {
	for (v1[1]=-1; v1[1] <= 1; v1[1] += 2) {
	  for (v1[2]=-1; v1[2] <= 1; v1[2] += 2) {
	    // for all possible +ve leading edges from those vertices
	    for (int ei=0; ei < 3; ei ++) {
	      if (v1[ei] < 0) {
		// get vertex1 -> vertex2 = an edge from box 1
		dVector3 vv1,vv2;
		for (k=0; k<3; k++) vv1[k] = v1[k] * 0.5*side1[k];
		for (k=0; k<3; k++) vv2[k] = (v1[k] + (k==ei)*2)*0.5*side1[k];
		dVector3 vertex1,vertex2;
		dMULTIPLY0_331 (vertex1,R1,vv1);
		dMULTIPLY0_331 (vertex2,R1,vv2);
		for (k=0; k<3; k++) vertex1[k] += p1[k];
		for (k=0; k<3; k++) vertex2[k] += p1[k];

		// see if vertex1 -> vertex2 interesects face
		if (edgeIntersectsRect (vertex1,vertex2,fp[0],fp[1],fp[2]))
		  return 1;
	      }
	    }
	  }
	}
      }
    }
  }

  if (box1inside2 (p1,R1,side1,p2,R2,side2)) return 1;
  if (box1inside2 (p2,R2,side2,p1,R1,side1)) return 1;

  return 0;
}

//****************************************************************************
// dBoxTouchesBox() test

int test_dBoxTouchesBox()
{
  int k,bt1,bt2;
  dVector3 p1,p2,side1,side2;
  dMatrix3 R1,R2;

  dSimpleSpace space(0);
  dGeomID box1 = dCreateBox (0,1,1,1);
  dSpaceAdd (space,box1);
  dGeomID box2 = dCreateBox (0,1,1,1);
  dSpaceAdd (space,box2);

  dMakeRandomVector (p1,3,0.5);
  dMakeRandomVector (p2,3,0.5);
  for (k=0; k<3; k++) side1[k] = dRandReal() + 0.01;
  for (k=0; k<3; k++) side2[k] = dRandReal() + 0.01;
  dRFromAxisAndAngle (R1,dRandReal()*2.0-1.0,dRandReal()*2.0-1.0,
		      dRandReal()*2.0-1.0,dRandReal()*10.0-5.0);
  dRFromAxisAndAngle (R2,dRandReal()*2.0-1.0,dRandReal()*2.0-1.0,
		      dRandReal()*2.0-1.0,dRandReal()*10.0-5.0);

  dGeomBoxSetLengths (box1,side1[0],side1[1],side1[2]);
  dGeomBoxSetLengths (box2,side2[0],side2[1],side2[2]);
  dGeomSetPosition (box1,p1[0],p1[1],p1[2]);
  dGeomSetRotation (box1,R1);
  dGeomSetPosition (box2,p2[0],p2[1],p2[2]);
  dGeomSetRotation (box2,R2);
  draw_all_objects (space);

  int t1 = testBoxesTouch2 (p1,R1,side1,p2,R2,side2);
  int t2 = testBoxesTouch2 (p2,R2,side2,p1,R1,side1);
  bt1 = t1 || t2;
  bt2 = dBoxTouchesBox (p1,R1,side1,p2,R2,side2);

  if (bt1 != bt2) FAILED();

  /*
    // some more debugging info if necessary
    if (bt1 && bt2) printf ("agree - boxes touch\n");
    if (!bt1 && !bt2) printf ("agree - boxes don't touch\n");
    if (bt1 && !bt2) printf ("disagree - boxes touch but dBoxTouchesBox "
			     "says no\n");
    if (!bt1 && bt2) printf ("disagree - boxes don't touch but dBoxTouchesBox "
			     "says yes\n");
  */

  PASSED();
}

//****************************************************************************
// test box-box collision

int test_dBoxBox()
{
  int k,bt;
  dVector3 p1,p2,side1,side2,normal,normal2;
  dMatrix3 R1,R2;
  dReal depth,depth2;
  int code;
  dContactGeom contact[48];

  dSimpleSpace space(0);
  dGeomID box1 = dCreateBox (0,1,1,1);
  dSpaceAdd (space,box1);
  dGeomID box2 = dCreateBox (0,1,1,1);
  dSpaceAdd (space,box2);

  dMakeRandomVector (p1,3,0.5);
  dMakeRandomVector (p2,3,0.5);
  for (k=0; k<3; k++) side1[k] = dRandReal() + 0.01;
  for (k=0; k<3; k++) side2[k] = dRandReal() + 0.01;

  dRFromAxisAndAngle (R1,dRandReal()*2.0-1.0,dRandReal()*2.0-1.0,
		      dRandReal()*2.0-1.0,dRandReal()*10.0-5.0);
  dRFromAxisAndAngle (R2,dRandReal()*2.0-1.0,dRandReal()*2.0-1.0,
		      dRandReal()*2.0-1.0,dRandReal()*10.0-5.0);

  // dRSetIdentity (R1);	// we can also try this
  // dRSetIdentity (R2);

  dGeomBoxSetLengths (box1,side1[0],side1[1],side1[2]);
  dGeomBoxSetLengths (box2,side2[0],side2[1],side2[2]);
  dGeomSetPosition (box1,p1[0],p1[1],p1[2]);
  dGeomSetRotation (box1,R1);
  dGeomSetPosition (box2,p2[0],p2[1],p2[2]);
  dGeomSetRotation (box2,R2);

  code = 0;
  depth = 0;
  bt = dBoxBox (p1,R1,side1,p2,R2,side2,normal,&depth,&code,8,contact,
		sizeof(dContactGeom));
  if (bt==1) {
    p2[0] += normal[0] * 0.96 * depth;
    p2[1] += normal[1] * 0.96 * depth;
    p2[2] += normal[2] * 0.96 * depth;
    bt = dBoxBox (p1,R1,side1,p2,R2,side2,normal2,&depth2,&code,8,contact,
		  sizeof(dContactGeom));

    /*
    dGeomSetPosition (box2,p2[0],p2[1],p2[2]);
    draw_all_objects (space);
    */

    if (bt != 1) {
      FAILED();
      dGeomSetPosition (box2,p2[0],p2[1],p2[2]);
      draw_all_objects (space);
    }

    p2[0] += normal[0] * 0.08 * depth;
    p2[1] += normal[1] * 0.08 * depth;
    p2[2] += normal[2] * 0.08 * depth;
    bt = dBoxBox (p1,R1,side1,p2,R2,side2,normal2,&depth2,&code,8,contact,
		  sizeof(dContactGeom));
    if (bt != 0) FAILED();

    // dGeomSetPosition (box2,p2[0],p2[1],p2[2]);
    // draw_all_objects (space);
  }

  // printf ("code=%2d  depth=%.4f  ",code,depth);

  PASSED();
}

//****************************************************************************
// graphics

int space_pressed = 0;


// start simulation - set viewpoint

static void start()
{
  static float xyz[3] = {2.4807,-1.8023,2.7600};
  static float hpr[3] = {141.5000,-18.5000,0.0000};
  dsSetViewpoint (xyz,hpr);
}


// called when a key pressed

static void command (int cmd)
{
  if (cmd == ' ') space_pressed = 1;
}


// simulation loop

static void simLoop (int pause)
{
  do {
    draw_all_objects_called = 0;
    unsigned long seed = dRandGetSeed();
    testslot[graphical_test].test_fn();
    if (draw_all_objects_called) {
      if (space_pressed) space_pressed = 0; else dRandSetSeed (seed);
    }
  }
  while (!draw_all_objects_called);
}

//****************************************************************************
// do all the tests

void do_tests (int argc, char **argv)
{
  int i,j;

  // process command line arguments
  if (argc >= 2) {
    graphical_test = atoi (argv[1]);
  }

  if (graphical_test) {
    // do one test gaphically and interactively

    if (graphical_test < 1 || graphical_test >= MAX_TESTS ||
	!testslot[graphical_test].name) {
      dError (0,"invalid test number");
    }

    printf ("performing test: %s\n",testslot[graphical_test].name);

    // setup pointers to drawstuff callback functions
    dsFunctions fn;
    fn.version = DS_VERSION;
    fn.start = &start;
    fn.step = &simLoop;
    fn.command = &command;
    fn.stop = 0;
    fn.path_to_textures = "../../drawstuff/textures";

    dsSetSphereQuality (3);
    dsSetCappedCylinderQuality (8);
    dsSimulationLoop (argc,argv,1280,900,&fn);
  }
  else {
    // do all tests noninteractively

    for (i=0; i<MAX_TESTS; i++) testslot[i].number = i;

    // first put the active tests into a separate array
    int n=0;
    for (i=0; i<MAX_TESTS; i++) if (testslot[i].name) n++;
    TestSlot **ts = (TestSlot**) alloca (n * sizeof(TestSlot*));
    j = 0;
    for (i=0; i<MAX_TESTS; i++) if (testslot[i].name) ts[j++] = testslot+i;
    if (j != n) dDebug (0,"internal");

    // do two test batches. the first test batch has far fewer reps and will
    // catch problems quickly. if all tests in the first batch passes, the
    // second batch is run.

    for (i=0; i<n; i++) ts[i]->failcount = 0;
    int total_reps=0;
    for (int batch=0; batch<2; batch++) {
      int reps = (batch==0) ? TEST_REPS1 : TEST_REPS2;
      total_reps += reps;
      printf ("testing batch %d (%d reps)...\n",batch+1,reps);

      // run tests
      for (j=0; j<reps; j++) {
	for (i=0; i<n; i++) {
	  current_test = ts[i]->number;
	  if (ts[i]->test_fn() != 1) ts[i]->failcount++;
	}
      }

      // check for failures
      int total_fail_count=0;
      for (i=0; i<n; i++) total_fail_count += ts[i]->failcount;
      if (total_fail_count) break;
    }

    // print results
    for (i=0; i<n; i++) {
      printf ("%3d: %-30s: ",ts[i]->number,ts[i]->name);
      if (ts[i]->failcount) {
	printf ("FAILED (%.2f%%) at line %d\n",
		double(ts[i]->failcount)/double(total_reps)*100.0,
		ts[i]->last_failed_line);
      }
      else {
	printf ("ok\n");
      }
    }
  }
}

//****************************************************************************

int main (int argc, char **argv)
{
  // setup all tests

  memset (testslot,0,sizeof(testslot));

  MAKE_TEST(1,test_sphere_point_depth);
  MAKE_TEST(2,test_box_point_depth);
  MAKE_TEST(3,test_ccylinder_point_depth);
  MAKE_TEST(4,test_plane_point_depth);

  MAKE_TEST(10,test_ray_and_sphere);
  MAKE_TEST(11,test_ray_and_box);
  MAKE_TEST(12,test_ray_and_ccylinder);
  MAKE_TEST(13,test_ray_and_plane);

  MAKE_TEST(100,test_dBoxTouchesBox);
  MAKE_TEST(101,test_dBoxBox);

  do_tests (argc,argv);
  return 0;
}
