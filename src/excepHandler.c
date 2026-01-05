/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <stdio.h>
#include <tamtypes.h>
#include <kernel.h>
#include <debug.h>

#include "excepHandler.h"
//#include "cmdHandler.h"
#include "exceptions.h"
//#include "globals.h"

////////////////////////////////////////////////////////////////////////
typedef union __attribute__((packed))
{
    unsigned int uint128 __attribute__((mode(TI)));
    unsigned long uint64[2];
} eeReg;

////////////////////////////////////////////////////////////////////////
static const unsigned char regName[32][5] =
    {
        "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
        "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "t8", "t9", "s0", "s1", "s2", "s3", "s4", "s5",
        "s6", "s7", "k0", "k1", "gp", "sp", "fp", "ra"};

static char codeTxt[14][24] =
    {
        "Interrupt", "TLB modification", "TLB load/inst fetch", "TLB store",
        "Address load/inst fetch", "Address store", "Bus error (instr)",
        "Bus error (data)", "Syscall", "Breakpoint", "Reserved instruction",
        "Coprocessor unusable", "Arithmetic overflow", "Trap"};

char _exceptionStack[8 * 1024] __attribute__((aligned(16)));
eeReg _savedRegs[32 + 4] __attribute__((aligned(16)));

extern struct export_list_t {
    char * name;
    void * pointer;
} export_list[];

////////////////////////////////////////////////////////////////////////
// The 'ee exception handler', only dumps registers to console or screen atm
void pkoDebug(int cause, int badvaddr, int status, int epc, eeReg *regs)
{
    int i;
    int code;

    FlushCache(0);
    FlushCache(2);

    //if (userThreadID) {
    //    TerminateThread(userThreadID);
    //    DeleteThread(userThreadID);
    //}

    struct export_list_t * p;

    char *cur_ra_function = NULL, *cur_epc_function = NULL;
    unsigned int cur_ra_function_addr = 0, cur_epc_function_addr = 0;

    for (p = export_list; p->name; p++) {
        if (p->pointer <= regs[31].uint64[0] && p->pointer > cur_ra_function_addr) {
            cur_ra_function_addr = p->pointer;
            cur_ra_function = p->name;
        } 
        
        if (p->pointer <= epc && p->pointer > cur_epc_function_addr) {
            cur_epc_function_addr = p->pointer;
            cur_epc_function = p->name;
        }
    }

    code = cause & 0x7c;

    init_scr();

    scr_setbgcolor(0x00400020);
    scr_setcursorcolor(0x00400020);

    scr_printf("\n\n           AthenaEnv EE Exception: %s\n\n", codeTxt[code >> 2]);
    scr_printf("      Cause %08X  BadVAddr %08X  Status %08X  EPC %08X\n\n", cause, badvaddr, status, epc);

    for (i = 0; i < 32 / 2; i++) {
        scr_printf("  %4s: %016lX%016lX %4s: %016lX%016lX\n", 
            regName[i], regs[i].uint64[1], regs[i].uint64[0],
            regName[i + 16], regs[i + 16].uint64[1], regs[i + 16].uint64[0]);
    }

    if (cur_epc_function != NULL) {
        unsigned int epc_offset = epc - cur_epc_function_addr;
        scr_printf("\n    Exception function: %s+0x%x\n", cur_epc_function, epc_offset);
    } 

    if (cur_ra_function != NULL) {
        unsigned int ra_offset = regs[31].uint64[0] - cur_ra_function_addr;
        scr_printf("    Return function: %s+0x%x\n", cur_ra_function, ra_offset);
    } 

    SleepThread();
}

////////////////////////////////////////////////////////////////////////
// The 'iop exception handler', only dumps registers to console or screen atm

void iopException(int cause, int badvaddr, int status, int epc, u32 *regs, int repc, char *name)
{
    int i;
    int code;

    FlushCache(0);
    FlushCache(2);

    //if (userThreadID) {
    //    TerminateThread(userThreadID);
    //    DeleteThread(userThreadID);
    //}

    code = cause & 0x7c;

    init_scr();

    scr_setbgcolor(0x00400020);
    scr_setcursorcolor(0x00400020);

    scr_printf("\n\n         AthenaEnv IOP Exception: %s\n\n",
               codeTxt[code >> 2]);

    scr_printf("               Module Name \"%s\" Relative EPC %08X\n\n",
               name, repc);


    scr_printf("      Cause %08X  BadVAddr %08X  Status %08X  EPC %08X\n\n",
               cause, badvaddr, status, epc);

    for (i = 0; i < 32 / 4; i++) {
        scr_printf("       %4s: %08X %4s: %08X %4s: %08X %4s: %08X\n",
                   regName[i], regs[i], regName[i + 8], regs[i + 8],
                   regName[i + 16], regs[i + 16], regName[i + 24], regs[i + 24]);
    }

    scr_printf("\n");



    SleepThread();
}


////////////////////////////////////////////////////////////////////////
// Installs ee exception handlers for the 'usual' exceptions and iop
// exception callback
void installExceptionHandlers(void)
{
    int i;

    // Skip exception #8 (syscall) & 9 (breakpoint)
    for (i = 1; i < 4; i++) {
        SetVTLBRefillHandler(i, pkoExceptionHandler);
    }
    for (i = 4; i < 8; i++) {
        SetVCommonHandler(i, pkoExceptionHandler);
    }
    for (i = 10; i < 14; i++) {
        SetVCommonHandler(i, pkoExceptionHandler);
    }
}
