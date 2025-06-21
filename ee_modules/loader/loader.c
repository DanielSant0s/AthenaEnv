/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2020 Francisco Javier Trujillo Mata <fjtrujy@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Modified to not reset IOP for use with NHDDL
*/

#include <kernel.h>
#include <loadfile.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <string.h>
#include <ctype.h>

//--------------------------------------------------------------
// Redefinition of init/deinit libc:
//--------------------------------------------------------------
// DON'T REMOVE is for reducing binary size.
// These functions are defined as weak in /libc/src/init.c
//--------------------------------------------------------------
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}

DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

//--------------------------------------------------------------
// Start of function code:
//--------------------------------------------------------------
// Clear user memory
// PS2Link (C) 2003 Tord Lindstrom (pukko@home.se)
//         (C) 2003 adresd (adresd_ps2dev@yahoo.com)
//--------------------------------------------------------------
static void wipeUserMem(void) {
  int i;
  for (i = 0x100000; i < GetMemorySize(); i += 64) {
    asm volatile("\tsq $0, 0(%0) \n"
                 "\tsq $0, 16(%0) \n"
                 "\tsq $0, 32(%0) \n"
                 "\tsq $0, 48(%0) \n" ::"r"(i));
  }
}

int main(int argc, char *argv[]) {
    static t_ExecData elfdata;
    int ret;
    char *path = argv[0];

    elfdata.epc = 0;

    // argv[0] is path to ELF
    if (argc < 1) {
        return -EINVAL;
    }

// Check if argv[0] starts with "hdd"
if (strncmp(argv[0], "hdd", 3) == 0) {
    char *second_colon = strchr(argv[0], ':');
    if (second_colon != NULL) {
        second_colon = strchr(second_colon + 1, ':');
        if (second_colon != NULL) {
            // Copy to a separate buffer to avoid modifying argv[0]
            static char modified_path[256];  // Adjust size as needed
            strncpy(modified_path, second_colon + 1, sizeof(modified_path) - 1);
            modified_path[sizeof(modified_path) - 1] = '\0'; // Ensure null termination

            path = modified_path; // Use the modified copy
            if (isdigit((unsigned char)*(second_colon + 4)))
                memmove(second_colon + 4, second_colon + 5, strlen(second_colon + 5) + 1);
        }
    }
}

    // Initialize
    SifInitRpc(0);
    wipeUserMem();

    // Writeback data cache before loading ELF.
    FlushCache(0);
    SifLoadFileInit();
    ret = SifLoadElf(path, &elfdata);
    SifLoadFileExit();
    if (ret == 0 && elfdata.epc != 0) {
        FlushCache(0);
        FlushCache(2);
        return ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, argc, argv);
    } else {
        SifExitRpc();
        return -ENOENT;
    }
}
