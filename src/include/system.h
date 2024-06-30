#include <sifrpc.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <loadfile.h>
#include <libcdvd.h>

#define SCECdESRDVD_0 0x15  // ESR-patched DVD, as seen without ESR driver active
#define SCECdESRDVD_1 0x16  // ESR-patched DVD, as seen with ESR driver active

typedef struct
{
	int type;
	char name[16];
	int value;
} DiscType;
