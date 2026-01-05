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

#include <ode/config.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <drawstuff/drawstuff.h>
#include <drawstuff/version.h>
#include "internal.h"

#define DGL_PS2
#include <GL/gl.h>
#include <GL/dgl.h>
#include <libpad.h>

static int width=640,height=480;	// window size
static int run=1;			// 1 if simulation running
static int singlestep=0;		// 1 if single step key pressed
static int pause=0;			// 1 if in `pause' mode

//***************************************************************************
// error handling for unix

static void printMessage (char *msg1, char *msg2, va_list ap)
{
  fflush (stderr);
  fflush (stdout);
  fprintf (stderr,"\n%s: ",msg1);
  vfprintf (stderr,msg2,ap);
  fprintf (stderr,"\n");
  fflush (stderr);
}


extern "C" void dsError (char *msg, ...)
{
  va_list ap;
  va_start (ap,msg);
  printMessage ("Error",msg,ap);
  exit (1);
}


extern "C" void dsDebug (char *msg, ...)
{
  va_list ap;
  va_start (ap,msg);
  printMessage ("INTERNAL ERROR",msg,ap);
  // *((char *)0) = 0;	 ... commit SEGVicide ?
  abort();
}


extern "C" void dsPrint (char *msg, ...)
{
  va_list ap;
  va_start (ap,msg);
  vprintf (msg,ap);
}

DGLcontext	ctx __attribute__((aligned(16)));

int dreamgl_init(void)
{
//	SifInitRpc(0);

	memset(&ctx, 0, sizeof(DGLcontext));

	ctx.ScreenWidth		= 512;
	ctx.ScreenHeight	= 256;
	ctx.ScreenDepth		= 32;
	ctx.ScreenBuffers	= 2;
	ctx.ZDepth			= 32;
	ctx.VSync			= GL_TRUE;

	if(*((char *)0x1FC80000 - 0xAE) == 'E')
		ctx.ps2VidType	= PS2_PAL;
	else
		ctx.ps2VidType	= PS2_NTSC;

	ctx.ps2VidInterlace	= PS2_NONINTERLACED;
	ctx.ps2VidFrame		= PS2_FRAME;

	return(dglInit(&ctx));
}

extern int ode_init_pad(int port, int slot);
extern int read_pad(struct padButtonStatus *buttons, void (*cmd)(int), int port, int slot);

void dsPlatformSimLoop (int window_width, int window_height, dsFunctions *fn,
			int initial_pause)
{
	int port = 0, slot = 0;
	int frame = 0;
	struct padButtonStatus buttons;

  	dreamgl_init();
	ode_init_pad(port, slot);

/*
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glShadeModel(GL_FLAT);

	glMatrixMode(GL_PROJECTION);
	glOrtho(-2.0f, 2.0f, -1.5f, 1.5f, -2.0f, 2.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(30.0f, 0.5f, 1.0f, 0.0f);
*/

  	if (fn->start) fn->start();

  	while (run) {
    		dsDrawFrame (width,height,fn,pause && !singlestep);
    		dglSwapBuffers();
      		read_pad (&buttons, fn->command, port, slot);
    		singlestep = 0;

	};

  	dsStopGraphics();

}

extern "C" void dsStop()
{
  run = 0;
}
