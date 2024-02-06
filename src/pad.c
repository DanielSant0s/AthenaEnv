#include <kernel.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "include/pad.h"
#include "include/dbgprintf.h"

static char pad0Buf[256] __attribute__((aligned(64)));
static char pad1Buf[256] __attribute__((aligned(64)));

static char actAlign[6];
static int actuators;

int port, slot;

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
            dbgprintf("Please wait, pad(%d,%d) is in state %s\n",
                       port, slot, stateString);
        }
        lastState = state;
        state=padGetState(port, slot);
    }
    // Were the pad ever 'out of sync'?
    if (lastState != -1) {
        dbgprintf("Pad OK!\n");
    }
    return 0;
}


/*
 * initializePad()
 */
int initializePad(int port, int slot)
{

    int ret;
    int modes;
    int i;
    int status;

    waitPadReady(port, slot);

    // How many different modes can this device operate in?
    // i.e. get # entrys in the modetable
    modes = padInfoMode(port, slot, PAD_MODETABLE, -1);
    dbgprintf("The device has %d modes\n", modes);

    if (modes > 0) {
        dbgprintf("( ");
        for (i = 0; i < modes; i++) {
            status = padInfoMode(port, slot, PAD_MODETABLE, i);
            dbgprintf("%d ", status);
        }
        dbgprintf(")");
    }

    status = padInfoMode(port, slot, PAD_MODECURID, 0);
    dbgprintf("It is currently using mode %d\n",
               status);

    // If modes == 0, this is not a Dual shock controller
    // (it has no actuator engines)
    if (modes == 0) {
        dbgprintf("This is a digital controller?\n");
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
        dbgprintf("This is no Dual Shock controller\n");
        return 1;
    }

    // If ExId != 0x0 => This controller has actuator engines
    // This check should always pass if the Dual Shock test above passed
    ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0) {
        dbgprintf("This is no Dual Shock controller??\n");
        return 1;
    }

    dbgprintf("Enabling dual shock functions\n");

    // When using MMODE_LOCK, user cant change mode with Select button
    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(port, slot);
    status = padInfoPressMode(port, slot);
    dbgprintf("infoPressMode: %d\n", status);

    waitPadReady(port, slot);
    status = padEnterPressMode(port, slot);
    dbgprintf("enterPressMode: %d\n", status);

    waitPadReady(port, slot);
    actuators = padInfoAct(port, slot, -1, 0);
    dbgprintf("# of actuators: %d\n", actuators);

    if (actuators != 0) {
        actAlign[0] = 0;   // Enable small engine
        actAlign[1] = 1;   // Enable big engine
        actAlign[2] = 0xff;
        actAlign[3] = 0xff;
        actAlign[4] = 0xff;
        actAlign[5] = 0xff;

        waitPadReady(port, slot);
        status = padSetActAlign(port, slot, actAlign);
        dbgprintf("padSetActAlign: %d\n", status);
    }
    else {
        dbgprintf("Did not find any actuators.\n");
    }

    waitPadReady(port, slot);

    return 1;
}

struct padButtonStatus readPad(int port, int slot)
{
    struct padButtonStatus buttons;
    int ret;    

    do {
    	ret = padGetState(port, slot);
    } while((ret != PAD_STATE_STABLE) && (ret != PAD_STATE_FINDCTP1));  

    ret = padRead(port, slot, &buttons);      

    return buttons;

}

int isButtonPressed(u32 button)
{
   int ret;
   u32 paddata;
   
   struct padButtonStatus padbuttons;
   
   while (((ret=padGetState(0, 0)) != PAD_STATE_STABLE)&&(ret!=PAD_STATE_FINDCTP1)&&(ret != PAD_STATE_DISCONN)); // more error check ?
   if (padRead(0, 0, &padbuttons) != 0)
   {
    	paddata = 0xffff ^ padbuttons.btns;
     	if(paddata & button)
            return 1;
   }
   return 0;

}

void pad_init()
{
    int ret;

    padInit(0);

    port = 0; // 0 -> Connector 1, 1 -> Connector 2
    slot = 0; // Always zero if not using multitap

    dbgprintf("PortMax: %d\n", padGetPortMax());
    dbgprintf("SlotMax: %d\n", padGetSlotMax(port));

    if((ret = padPortOpen(0, 0, pad0Buf)) == 0) {
        dbgprintf("padOpenPort failed: %d\n", ret);
        SleepThread();
    }

    if(!initializePad(port, slot)) {
        dbgprintf("pad initalization failed!\n");
        SleepThread();
    }

    if((ret = padPortOpen(1, 0, pad1Buf)) == 0) {
        dbgprintf("padOpenPort failed: %d\n", ret);
        SleepThread();
    }
}

