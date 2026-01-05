/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
# Pad demo app
# Quick and dirty, little or no error checks etc.. 
# Distributed as is
#
# -- patched for drawstuff by rinco
*/

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include "libpad.h"
#include <ode/config.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <drawstuff/drawstuff.h>
#include <drawstuff/version.h>
#include "internal.h"

#define ROM_PADMAN

/*
 * Global var's
 */
// pad_dma_buf is provided by the user, one buf for each pad
// contains the pad's current state
static char padBuf[256] __attribute__((aligned(64)));

static char actAlign[6];
static int actuators;

/*
 * loadModules()
 */
int
loadModules(void)
{
    int ret;

    
#ifdef ROM_PADMAN
    	ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
#else
    	ret = SifLoadModule("rom0:XSIO2MAN", 0, NULL);
#endif
    	if (ret < 0) {
//        	printf("sifLoadModule sio failed: %d\n", ret);
		return 1;
	}    

#ifdef ROM_PADMAN
    	ret = SifLoadModule("rom0:PADMAN", 0, NULL);
#else
    	ret = SifLoadModule("rom0:XPADMAN", 0, NULL);
#endif 
	if (ret < 0) {
//	        printf("sifLoadModule pad failed: %d\n", ret);
		return 1;
	}
	return 0;
}

/*
 * waitPadReady()
 */
int waitPadReady(int port, int slot)
{
    int state;
    int lastState;
    char stateString[16];

    state = padGetState(port, slot);
    lastState = -1;
    while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1)) {
        if (state != lastState) {
            padStateInt2String(state, stateString);
//            printf("Please wait, pad(%d,%d) is in state %s\n", 
//                       port, slot, stateString);
        }
        lastState = state;
        state=padGetState(port, slot);
    }
    // Were the pad ever 'out of sync'?
    if (lastState != -1) {
        printf("Pad OK!\n");
    }
    return 0;
}

/*
 * initializePad()
 */
int
initializePad(int port, int slot)
{

    int ret;
    int modes;
    int curid;
    int i;

    waitPadReady(port, slot);

    // How many different modes can this device operate in?
    // i.e. get # entrys in the modetable
    modes = padInfoMode(port, slot, PAD_MODETABLE, -1);
//    printf("The device has %d modes\n", modes);

    curid = padInfoMode(port, slot, PAD_MODECURID, 0);

//    printf("It is currently using mode %d\n", curid);

    // If modes == 0, this is not a Dual shock controller 
    // (it has no actuator engines)
    if (modes == 0) {
//        printf("error: modes (%d) == 0\n", modes);
        return 1;
    }

    // Verify that the controller has a DUAL SHOCK mode
    i = 0;
    do {
        if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
        i++;
    } while (i < modes);
    if (i >= modes) {
//        printf("error: i (%d) >= modes (%d)\n", i, modes);
        return 1;
    }

    // If ExId != 0x0 => This controller has actuator engines
    // This check should always pass if the Dual Shock test above passed
    ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0) {
//        printf("This is no Dual Shock controller??\n");
        return 1;
    }

    printf("Enabling dual shock functions\n");

    // When using MMODE_LOCK, user cant change mode with Select button
    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(port, slot);
//    printf("infoPressMode: %d\n", padInfoPressMode(port, slot));

    waitPadReady(port, slot);        
//    printf("enterPressMode: %d\n", padEnterPressMode(port, slot));

    waitPadReady(port, slot);
    actuators = padInfoAct(port, slot, -1, 0);
//    printf("# of actuators: %d\n",actuators);

    if (actuators != 0) {
        actAlign[0] = 0;   // Enable small engine
        actAlign[1] = 1;   // Enable big engine
        actAlign[2] = 0xff;
        actAlign[3] = 0xff;
        actAlign[4] = 0xff;
        actAlign[5] = 0xff;

        waitPadReady(port, slot);
        printf("padSetActAlign: %d\n", 
                   padSetActAlign(port, slot, actAlign));
    }
//    else {
//        printf("Did not find any actuators.\n");
//    }

    waitPadReady(port, slot);

    return 1;
}

int ode_init_pad(int port, int slot)
{
	int ret;

    	if((ret = loadModules()) != 0) {
		printf("loadModules failed: %d\n", ret);
		return 1;
	}

	padInit(0);

	if((ret = padPortOpen(port, slot, padBuf)) == 0) {
		printf("padOpenPort failed: %d\n", ret);
		return 1;
	}

	if(!initializePad(port, slot)) {
		printf("pad initalization failed!\n");
		return 1;
	}

	return 0;
}

int read_pad(struct padButtonStatus *buttons, void (*cmd)(int), int port, int slot)
{
	int ret;
	u32 paddata;
	static u32 old_pad = 0;
	static int mx=127, my=127, nx=127, ny=127; 
	u32 new_pad;

	ret=padGetState(port, slot);
	while((ret != PAD_STATE_STABLE) && (ret != PAD_STATE_FINDCTP1)) {
		if(ret==PAD_STATE_DISCONN) {
			printf("Pad(%d, %d) is disconnected\n", port, slot);
		}
		ret=padGetState(port, slot);
	}

	ret = padRead(port, slot, buttons); // port, slot, buttons

	if (!cmd) return 1;

// following is a shortcut (kludge) to get the ode examples working
	if (ret != 0) {

		paddata = 0xffff ^ buttons->btns;
		    
		new_pad = paddata & ~old_pad;
		old_pad = paddata;

		dsMotion (2, -1 * (buttons->rjoy_h - mx), -1 * (buttons->rjoy_v - my));
		mx = buttons->rjoy_h;
		my = buttons->rjoy_v;

		dsMotion (1, -1 * (buttons->ljoy_h - nx), -1 * (buttons->ljoy_v - ny));
		nx = buttons->ljoy_h;
		ny = buttons->ljoy_v;
		    
		// Directions
		if(new_pad & PAD_LEFT) {
      			cmd (',');
		}
		if(new_pad & PAD_DOWN) {
      			cmd ('z');
		}
		if(new_pad & PAD_RIGHT) {
      			cmd ('.');
		}
		if(new_pad & PAD_UP) {
      			cmd ('a');
		}
		if(new_pad & PAD_START) {
      			cmd (' ');
		}
		if(new_pad & PAD_R3) {
      			cmd (' ');
		}
		if(new_pad & PAD_L3) {
      			cmd ('w');
		}
		if(new_pad & PAD_SELECT) {
      			cmd ('r');
		}
		if(new_pad & PAD_SQUARE) {
      			cmd ('s');
		}
		if(new_pad & PAD_CROSS) {
      			cmd ('x');
		}
		if(new_pad & PAD_CIRCLE) {
      			cmd ('c');
		}
		if(new_pad & PAD_TRIANGLE) {
      			cmd ('b');
		}
		if(new_pad & PAD_R1) {
      			cmd ('1');
		}
		if(new_pad & PAD_L1) {
      			cmd ('1');
		}
		if(new_pad & PAD_R2) {
      			cmd ('p');
		}
		if(new_pad & PAD_L2) {
      			cmd ('o');
		}
	} 
	return 0;
}
