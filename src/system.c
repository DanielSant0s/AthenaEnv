
#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

#include <system.h>

#include <dbgprintf.h>

extern char MountPoint[32+6+1];

//
// All the following code is modified version of elf.c from PS2SDK with unneeded bits removed
//
// Loader ELF variables
extern uint8_t loader_elf[];
extern int size_loader_elf;

typedef struct {
	uint8_t ident[16]; // struct definition for ELF object header
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	uint32_t entry;
	uint32_t phoff;
	uint32_t shoff;
	uint32_t flags;
	uint16_t ehsize;
	uint16_t phentsize;
	uint16_t phnum;
	uint16_t shentsize;
	uint16_t shnum;
	uint16_t shstrndx;
} elf_header_t;

typedef struct {
	uint32_t type; // struct definition for ELF program section header
	uint32_t offset;
	void *vaddr;
	uint32_t paddr;
	uint32_t filesz;
	uint32_t memsz;
	uint32_t flags;
	uint32_t align;
} elf_pheader_t;

// ELF-loading stuff
#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1

int LoadELFFromFileNoReset(const char *path, int argc, char *argv[]) {
	uint8_t *boot_elf;
	elf_header_t *eh;
	elf_pheader_t *eph;
	void *pdata;
	int i;

	char *new_argv[argc + 1];

	// Modify path if it starts with "pfs"
	if (strncmp(path, "pfs", 3) == 0) {
		char modified_path[384];

		if (MountPoint[0] != NULL) {
			snprintf(modified_path, sizeof(modified_path), "%s:%s", MountPoint, path);
			new_argv[0] = modified_path;
		} else {
			new_argv[0] = (char *)path;
		}

		dbgprintf("Modified path: %s\n", new_argv[0]);
	} else {
		new_argv[0] = (char *)path;
	}

	for (i = 0; i < argc; i++) {
		new_argv[i + 1] = argv[i];
	}

	// Wipe memory region where the ELF loader is going to be loaded (see
	// loader/linkfile)
	memset((void *)0x00084000, 0, 0x00100000 - 0x00084000);

	boot_elf = (uint8_t *)loader_elf;
	eh = (elf_header_t *)boot_elf;
	if (_lw((uint32_t)&eh->ident) != ELF_MAGIC)
		__builtin_trap();

	eph = (elf_pheader_t *)(boot_elf + eh->phoff);

	// Scan through the ELF's program headers and copy them into RAM
	for (i = 0; i < eh->phnum; i++) {
		if (eph[i].type != ELF_PT_LOAD)
			continue;

		pdata = (void *)(boot_elf + eph[i].offset);
		memcpy(eph[i].vaddr, pdata, eph[i].filesz);
	}

	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	return ExecPS2((void *)eh->entry, NULL, argc + 1, new_argv);
}
