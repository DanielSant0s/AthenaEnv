/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

/**
 * @file
 * The relocatable elf loader/linker.
 *
 * AthenaEnv vendorized version with fixes:
 *  - Distinguish defined vs undefined symbols via st_shndx == SHN_UNDEF (0)
 *    instead of st_info STT_NOTYPE, fixing assembly functions without .type
 *    (such as libmpeg_op.s) and external FUNC/OBJECT symbols.
 *  - Respect SHF_ALLOC when sizing and loading PROGBITS/NOBITS sections so
 *    non-alloc metadata/debug sections never waste EE RAM.
 *  - Fix next_sym_n typo in R_MIPS_HI16/LO16 section relocation lookahead.
 *  - Include "js_init_module" in local_names so multiple QuickJS .erl modules
 *    can coexist without colliding on their entry point symbol.
 *  - Allocate ERL image with 64-byte alignment (memalign(64, ...)) for R5900
 *    MMI 128-bit lq/sq and DMA cacheline alignment.
 */

#define DEBUG 1
#define STANDALONE 1
#define FORCE_ALIGN 1

#undef DEBUG
#undef STANDALONE

#ifdef DEBUG
#ifdef FULLDEBUG
#define dprintf(fmt, args...) printf("(%s:%s:%i): " fmt, __FILE__, __FUNCTION__, __LINE__, ## args)
#else
#define dprintf(fmt, args...) printf(fmt, ## args)
#endif
#define rprintf(fmt, args...) printf(fmt, ## args)
#else
#define dprintf(a...) do { } while(0)
#define rprintf(a...) do { } while(0)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef _WIN32
#include <io.h>
#endif

#include "include/erl.h"

#include "hashtab.h"
#include "recycle.h"
#undef align


#ifdef _EE
#include <tamtypes.h>
#include <kernel.h>
#else
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
#endif

#define REL_TYPE 1
#define PROGBITS 1
#define NOBITS 8
#define REL 9
#define RELA 4
#define SHF_ALLOC 2
#define SHN_UNDEF 0
#define GLOBAL 1
#define WEAK 2
#define NOTYPE 0
#define OBJECT 1
#define FUNC 2
#define SECTION 3
#define R_MIPS_32 2
#define R_MIPS_26 4
#define R_MIPS_HI16 5
#define R_MIPS_LO16 6


/* These global names will not be 'exported' to the global space. */

static const char * local_names[] = {
    "_init",
    "_fini",
    "erl_id",
    "erl_dependancies",
    "erl_copyright",
    "erl_version",
    "_start",
    "js_init_module",
    0
};


/* Structures mapped onto the loaded erl file. */

struct elf_header_t {
    union {
	u8 raw[16];
	struct e_ident_t {
	    u8 ei_magic[4];
	    u8 ei_class;
	    u8 ei_data;
	    u8 ei_version;
	} cook;
    } e_ident;
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u32 e_entry;
    u32 e_phoff;
    u32 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
};

struct elf_section_t {
    u32 sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size;
    u32 sh_link, sh_info, sh_addralign, sh_entsize;
};

struct elf_symbol_t {
    u32 st_name, st_value, st_size;
    u8 st_info, st_other;
    u16 st_shndx;
};

struct elf_reloc_t {
    u32 r_offset, r_info;
};

struct elf_rela_t {
    u32 r_offset, r_info, r_addend;
};

/* Our internal structures. */

struct loosy_t {
    u8 * reloc;
    u8 * lo_reloc;
    int type;
    struct loosy_t * next;
    struct erl_record_t * erl;
};

struct dependancy_t {
    struct erl_record_t * depender, * provider;
    struct dependancy_t * next, * prev;
};


/* And our global variables. */
static struct erl_record_t * erl_record_root = 0;

static htab * global_symbols = 0;
static htab * loosy_relocs = 0;

char _init_erl_prefix[256] = "";

static struct dependancy_t * dependancy_root = 0;


static u32 align(u32 x, int align) {
#ifdef FORCE_ALIGN
    if (align < 16)
	align = 16;
#endif

    align--;

    if (x & align) {
	x |= align;
	x++;
    }

    return x;
}

static struct loosy_t * create_loosy(struct erl_record_t * erl, u8 * reloc, u8 * lo_reloc, int type) {
    struct loosy_t * r;

    if (!(r = (struct loosy_t *) malloc(sizeof(struct loosy_t))))
	return 0;

    r->reloc = reloc;
    r->lo_reloc = lo_reloc;
    r->type = type;
    r->next = 0;
    r->erl = erl;

    return r;
}

static void destroy_loosy(struct loosy_t * l) {
    free(l);
}

static void r_destroy_loosy(struct loosy_t * l) {
    while (l) {
	struct loosy_t * next = l->next;
	destroy_loosy(l);
	l = next;
    }
}

static reroot * symbol_recycle = 0;

static struct symbol_t * create_symbol(struct erl_record_t * provider, u32 address) {
    struct symbol_t * r;

    if (!symbol_recycle)
	symbol_recycle = remkroot(sizeof(struct symbol_t));

    r = (struct symbol_t *) renew(symbol_recycle);

    r->provider = provider;
    r->address = address;

    return r;
}

static void destroy_symbol(struct symbol_t * s) {
    redel(symbol_recycle, s);
}

static struct erl_record_t * allocate_erl_record(void) {
    struct erl_record_t * r;

    if (!(r = (struct erl_record_t *) malloc(sizeof(struct erl_record_t))))
	return 0;

    r->bytes = NULL;
    r->symbols = hcreate(6);

    if ((r->next = erl_record_root))
	r->next->prev = r;
    r->prev = 0;
    erl_record_root = r;

    r->flags = 0;

    return r;
}

static void destroy_erl_record(struct erl_record_t * erl) {
    if (!erl)
	return;

    if (erl->bytes) {
        if (erl->flags & ERL_FLAG_CLEAR)
            memset(erl->bytes, 0, erl->fullsize);
        if (!(erl->flags & ERL_FLAG_STATIC))
            free(erl->bytes);
    }

    erl_flush_symbols(erl);

    if (erl->prev)
	erl->prev->next = erl->next;
    else
	erl_record_root = erl->next;

    if (erl->next)
	erl->next->prev = erl->prev;

    free(erl);
}

static int apply_mips_hi16_lo16_pair(u8 * hi_reloc, u8 * lo_reloc, u32 addr) {
    u32 hi_word, lo_word, value, new_hi;

    memcpy(&hi_word, hi_reloc, 4);
    memcpy(&lo_word, lo_reloc, 4);

    value = ((hi_word & 0xffff) << 16) + (u32)(s16)(lo_word & 0xffff) + addr;
    new_hi = ((value + 0x8000) >> 16) & 0xffff;

    hi_word = (hi_word & 0xffff0000) | new_hi;
    memcpy(hi_reloc, &hi_word, 4);

    return 0;
}

static int apply_reloc(u8 * reloc, int type, u32 addr) {
    u32 u_current_data;
    s32 s_current_data;
    u32 newstate;

    if (((u32)reloc)&0x3) {
	printf("Unaligned reloc (%p) type=%d!\n", reloc, type);
    }
    memcpy(&u_current_data, reloc, 4);
    s_current_data = (s32)u_current_data;

    switch (type) {
    case R_MIPS_32:
	newstate = s_current_data + addr;
	break;
    case R_MIPS_26:
	newstate = (u_current_data & 0xfc000000) | (((u_current_data & 0x03ffffff) + (addr >> 2)) & 0x3ffffff);
	break;
    case R_MIPS_HI16:
	newstate = (u_current_data & 0xffff0000) | ((((s_current_data << 16) >> 16) + (addr >> 16) + ((addr & 0xffff) >= 0x8000 ? 1 : 0)) & 0xffff);
	break;
    case R_MIPS_LO16:
	newstate = (u_current_data & 0xffff0000) | ((((s_current_data << 16) >> 16) + (addr & 0xffff)) & 0xffff);
	break;
    default:
	return -1;
    }

    memcpy(reloc, &newstate, 4);

    dprintf("Changed data at %08X from %08X to %08X.\n", reloc, u_current_data, newstate);
    return 0;
}

static int apply_reloc_ex(u8 * reloc, int type, u32 addr, u8 * lo_reloc) {
    if (type == R_MIPS_HI16 && lo_reloc)
	return apply_mips_hi16_lo16_pair(reloc, lo_reloc, addr);
    return apply_reloc(reloc, type, addr);
}

struct symbol_t * erl_find_local_symbol(const char * symbol, struct erl_record_t * erl) {
    if (!erl)
	return 0;
    if (hfind(erl->symbols, (const ub1 *)symbol, strlen(symbol)))
	return hstuff(erl->symbols);
    return 0;
}

static struct symbol_t * r_find_symbol(const char * symbol, struct erl_record_t * erl) {
    for (; erl; erl = erl->next) {
	if (hfind(erl->symbols, (const ub1 *)symbol, strlen(symbol)))
	    return hstuff(erl->symbols);
    }
    return 0;
}

struct symbol_t * erl_find_symbol(const char * symbol) {
    if (global_symbols)
	if (hfind(global_symbols, (const ub1 *)symbol, strlen(symbol)))
	    return hstuff(global_symbols);
    return r_find_symbol(symbol, erl_record_root);
}

static struct dependancy_t * add_dependancy(struct erl_record_t * depender, struct erl_record_t * provider) {
    struct dependancy_t * d;

    if (depender == provider)
	return 0;

    if (!(d = (struct dependancy_t *) malloc(sizeof(struct dependancy_t))))
	return 0;

    d->depender = depender;
    d->provider = provider;

    if ((d->next = dependancy_root))
	d->next->prev = d;

    d->prev = 0;
    dependancy_root = d;

    return d;
}

static void destroy_dependancy(struct dependancy_t * d) {
    if (d->prev)
	d->prev->next = d->next;
    else
	dependancy_root = d->next;

    if (d->next)
	d->next->prev = d->prev;

    free(d);
}

static void r_destroy_dependancy_r(struct erl_record_t * erl, struct dependancy_t * d) {
    struct dependancy_t * next;

    while (d) {
	next = d->next;
	if (erl == d->depender)
	    destroy_dependancy(d);
	d = next;
    }
}

static void destroy_dependancy_r(struct erl_record_t * erl) {
    r_destroy_dependancy_r(erl, dependancy_root);
}

static void add_loosy(struct erl_record_t * erl, u8 * reloc, u8 * lo_reloc, int type, const char * symbol) {
    struct loosy_t * l;

    l = create_loosy(erl, reloc, lo_reloc, type);

    if (!loosy_relocs)
	loosy_relocs = hcreate(6);

    if (!hadd(loosy_relocs, (ub1 *)symbol, strlen(symbol), l)) {
	l->next = hstuff(loosy_relocs);
	hstuff(loosy_relocs) = l;
    } else {
	hkey(loosy_relocs) = (ub1 *)strdup(symbol);
    }
}

static int fix_loosy(struct erl_record_t * provider, const char * symbol, u32 address) {
    struct loosy_t * l;
    int count = 0;

    if (!loosy_relocs)
	return count;

    if (hfind(loosy_relocs, (const ub1 *)symbol, strlen(symbol))) {
	for (l = hstuff(loosy_relocs); l; l = l->next) {
	    apply_reloc_ex(l->reloc, l->type, address, l->lo_reloc);
	    add_dependancy(l->erl, provider);
	    count++;
	}
	r_destroy_loosy(hstuff(loosy_relocs));
	free(hkey(loosy_relocs));
	hdel(loosy_relocs);
    }

    return count;
}

static int is_local(const char * symbol) {
    const char ** p;

    for (p = local_names; *p; p++)
	if (!strcmp(*p, symbol))
	    return 1;

    return 0;
}

static int add_symbol(struct erl_record_t * erl, const char * symbol, u32 address) {
    htab * symbols;

    if (erl) {
	symbols = erl->symbols;
    } else {
	if (!global_symbols)
	    global_symbols = hcreate(6);
	symbols = global_symbols;
    }

    if (!is_local(symbol) && erl_find_symbol(symbol))
	return -1;

    dprintf("Adding symbol %s at address %08X\n", symbol, address);

    if (fix_loosy(erl, symbol, address)) {
#ifdef _EE
        FlushCache(2);
	FlushCache(0);
#endif
    }

    hadd(symbols, (ub1 *)strdup(symbol), strlen(symbol), create_symbol(erl, address));

    return 0;
}

int erl_add_global_symbol(const char * symbol, u32 address) {
    return add_symbol(0, symbol, address);
}

static int read_erl(int elf_handle, u8 * elf_mem, u32 addr, struct erl_record_t ** p_erl_record) {
    struct elf_header_t head;
    struct elf_section_t * sec = 0;
    struct elf_symbol_t * sym = 0;
    struct elf_reloc_t reloc;
    int i, j;
    char * names = 0, * strtab_names = 0, * reloc_section = 0;
    int symtab = 0, strtab = 0, linked_strtab = 0;
    u8 * magic;
    u32 fullsize = 0;
    struct erl_record_t * erl_record = 0;
    struct symbol_t * s;
    u32 addend = 0;

    *p_erl_record = 0;

#define free_and_return(code) if (!elf_mem) { \
    if (names) free(names); \
    if (strtab_names) free(strtab_names); \
    if (sec) free(sec); \
    if (sym) free(sym); \
    if ((code < 0) && erl_record) destroy_erl_record(erl_record); \
} \
return code

    if (!(erl_record = allocate_erl_record())) {
	dprintf("Memory allocation error.\n");
	free_and_return(-1);
    }

   // Reading the main ELF header.
    if (elf_mem) {
	memcpy(&head, elf_mem, sizeof(head));
    } else {
        lseek(elf_handle, 0, SEEK_SET);
        read(elf_handle, &head, sizeof(head));
    }

    magic = head.e_ident.cook.ei_magic;

    if ((magic[0] != 0x7f) || (magic[1] != 'E') || (magic[2] != 'L') || ((magic[3] != 'F') && (magic[3] != 'X'))) {
	dprintf("Not an ELF file.\n");
	free_and_return(-1);
    }

    if (head.e_type != REL_TYPE) {
	dprintf("File isn't a relocatable ELF file.\n");
	free_and_return(-1);
    }

   // Reading the section table.
    if (sizeof(struct elf_section_t) != head.e_shentsize) {
	dprintf("Inconsistancy in section table entries.\n");
	free_and_return(-1);
    }

    if (elf_mem) {
	sec = (struct elf_section_t *) (elf_mem + head.e_shoff);
    } else {
        if (!(sec = (struct elf_section_t *) malloc(sizeof(struct elf_section_t) * head.e_shnum))) {
    	    dprintf("Not enough memory.\n");
	    free_and_return(-1);
	}
	lseek(elf_handle, head.e_shoff, SEEK_SET);
        read(elf_handle, sec, sizeof(struct elf_section_t) * head.e_shnum);
    }

   // Reading the section names's table.
    if (elf_mem) {
	names = (char *) (elf_mem + sec[head.e_shstrndx].sh_offset);
    } else {
	if (!(names = (char *) malloc(sec[head.e_shstrndx].sh_size))) {
	    dprintf("Not enough memory.\n");
	    free_and_return(-1);
	}
        lseek(elf_handle, sec[head.e_shstrndx].sh_offset, SEEK_SET);
	read(elf_handle, names, sec[head.e_shstrndx].sh_size);
    }

   // Parsing the sections.
    for (i = 1; i < head.e_shnum; i++) {
	if (!strcmp(names + sec[i].sh_name, ".symtab")) {
	    symtab = i;
	    linked_strtab = sec[i].sh_link;
    	} else if (!strcmp(names + sec[i].sh_name, ".strtab")) {
	    strtab = i;
	}

	if (((sec[i].sh_type == PROGBITS) || (sec[i].sh_type == NOBITS)) && (sec[i].sh_flags & SHF_ALLOC)) {
	    fullsize = align(fullsize, sec[i].sh_addralign);
	    sec[i].sh_addr = fullsize;
	    fullsize += sec[i].sh_size;
	    dprintf("Section to load at %08X:\n", sec[i].sh_addr);
	}
    }

    if (!symtab || !strtab || (strtab != linked_strtab) || (sizeof(struct elf_symbol_t) != sec[symtab].sh_entsize)) {
	free_and_return(-1);
    }

   // Loading progbits/nobits sections.
    if (addr == ERL_DYN_ADDR) {
        erl_record->bytes = (u8 *) memalign(64, fullsize);
        if (!erl_record->bytes) {
            dprintf("Cannot allocate ERL bytes.\n");
            free_and_return(-1);
        }
    } else {
        erl_record->bytes = (u8 *) addr;
        erl_record->flags |= ERL_FLAG_STATIC;
    }

    erl_record->fullsize = fullsize;
    for (i = 1; i < head.e_shnum; i++) {
	if (!(sec[i].sh_flags & SHF_ALLOC))
	    continue;
	switch (sec[i].sh_type) {
	case PROGBITS:
	    if (elf_mem) {
		memcpy(erl_record->bytes + sec[i].sh_addr, elf_mem + sec[i].sh_offset, sec[i].sh_size);
	    } else {
		lseek(elf_handle, sec[i].sh_offset, SEEK_SET);
		read(elf_handle, erl_record->bytes + sec[i].sh_addr, sec[i].sh_size);
	    }
	    break;
	case NOBITS:
	    memset(erl_record->bytes + sec[i].sh_addr, 0, sec[i].sh_size);
	    break;
	}
    }

   // Loading strtab.
    if (elf_mem) {
	strtab_names = (char *) (elf_mem + sec[strtab].sh_offset);
    } else {
        if (!(strtab_names = (char *) malloc(sec[strtab].sh_size))) {
	    free_and_return(-1);
	}
	lseek(elf_handle, sec[strtab].sh_offset, SEEK_SET);
	read(elf_handle, strtab_names, sec[strtab].sh_size);
    }

   // Loading symtab.
    if (elf_mem) {
	sym = (struct elf_symbol_t *) (elf_mem + sec[symtab].sh_offset);
    } else {
	if (!(sym = (struct elf_symbol_t *) malloc(sec[symtab].sh_size))) {
	    free_and_return(-1);
	}
        lseek(elf_handle, sec[symtab].sh_offset, SEEK_SET);
	read(elf_handle, sym, sec[symtab].sh_size);
    }

   // Parsing sections to find relocation sections.
    for (i = 0; i < head.e_shnum; i++) {
	if (sec[i].sh_type != REL && sec[i].sh_type != RELA)
	    continue;
	if (!(sec[sec[i].sh_info].sh_flags & SHF_ALLOC))
	    continue;

	if (sec[i].sh_entsize != sizeof(struct elf_reloc_t) && sec[i].sh_entsize != sizeof(struct elf_rela_t)) {
	    free_and_return(-1);
	}

        if (elf_mem) {
            reloc_section = (char *)(elf_mem + sec[i].sh_offset);
        } else {
	    if (!(reloc_section = (char *) malloc(sec[i].sh_size))) {
	        free_and_return(-1);
  	    }
            lseek(elf_handle, sec[i].sh_offset, SEEK_SET);
	    read(elf_handle, reloc_section, sec[i].sh_size);
        }

	for (j = 0; (u32)j < (sec[i].sh_size / sec[i].sh_entsize); j++) {
	    int sym_n;
	    const char * sym_name;
	    u8 * reloc_ptr;
	    u8 * lo_reloc_ptr = 0;

            addend = (sec[i].sh_type == RELA ? ((struct elf_rela_t *)(reloc_section + j * sec[i].sh_entsize))->r_addend : 0);
	    reloc = *((struct elf_reloc_t *) (reloc_section + j * sec[i].sh_entsize));
	    sym_n = reloc.r_info >> 8;

	    reloc_ptr = erl_record->bytes + sec[sec[i].sh_info].sh_addr + reloc.r_offset;

	    /* A SHT_REL R_MIPS_HI16 relocation needs the in-place addend of its
	     * matching R_MIPS_LO16 to compute the carry bit. The MIPS ABI does not
	     * require the LO16 to be immediately adjacent to the HI16 (e.g. two
	     * consecutive `lui` followed by their `lw`/`addiu`), so scan forward
	     * for the matching LO16, matching GNU ld. */
	    if (sec[i].sh_type == REL && (reloc.r_info & 255) == R_MIPS_HI16) {
		u32 k;
		u32 reloc_count = sec[i].sh_size / sec[i].sh_entsize;
		for (k = j + 1; k < reloc_count; k++) {
		    struct elf_reloc_t lo_candidate =
			*((struct elf_reloc_t *)(reloc_section + k * sec[i].sh_entsize));
		    if ((lo_candidate.r_info & 255) == R_MIPS_LO16 &&
			(int)(lo_candidate.r_info >> 8) == sym_n) {
			lo_reloc_ptr = erl_record->bytes + sec[sec[i].sh_info].sh_addr + lo_candidate.r_offset;
			break;
		    }
		}
		if (!lo_reloc_ptr) {
		    for (k = j + 1; k < reloc_count; k++) {
			struct elf_reloc_t lo_candidate =
			    *((struct elf_reloc_t *)(reloc_section + k * sec[i].sh_entsize));
			if ((lo_candidate.r_info & 255) == R_MIPS_LO16) {
			    lo_reloc_ptr = erl_record->bytes + sec[sec[i].sh_info].sh_addr + lo_candidate.r_offset;
			    break;
			}
		    }
		}
	    }

            sym_name = strtab_names + sym[sym_n].st_name;

            if (sym[sym_n].st_shndx == SHN_UNDEF) {
		if (!(s = erl_find_symbol(sym_name))) {
		    if (strcmp(sym_name, "_Jv_RegisterClasses") != 0) {
			printf("%s: Symbol not found, adding as loosy relocation.\n", sym_name);
			add_loosy(erl_record, reloc_ptr, lo_reloc_ptr, reloc.r_info & 255, sym_name);
		    }
		} else {
		    if (apply_reloc_ex(reloc_ptr, reloc.r_info & 255, s->address + addend, lo_reloc_ptr) < 0) {
			free_and_return(-1);
		    }
		    add_dependancy(erl_record, s->provider);
		}
            } else if ((sym[sym_n].st_info & 15) == SECTION) {
                if (apply_reloc_ex(reloc_ptr, reloc.r_info & 255, (u32) (erl_record->bytes + sec[sym[sym_n].st_shndx].sh_addr) + addend, lo_reloc_ptr) < 0) {
		    free_and_return(-1);
		}
            } else {
		if (!is_local(sym_name) && (s = erl_find_symbol(sym_name))) {
		    if (apply_reloc_ex(reloc_ptr, reloc.r_info & 255, s->address + addend, lo_reloc_ptr) < 0) {
			free_and_return(-1);
		    }
		    add_dependancy(erl_record, s->provider);
		} else {
		    if (apply_reloc_ex(reloc_ptr, reloc.r_info & 255, (u32) (erl_record->bytes + sec[sym[sym_n].st_shndx].sh_addr + sym[sym_n].st_value) + addend, lo_reloc_ptr) < 0) {
			free_and_return(-1);
		    }
		}
            }
	}
	if (!elf_mem)
	    free(reloc_section);
    }

    for (i = 0; (u32)i < sec[symtab].sh_size / sec[symtab].sh_entsize; i++) {
	if (((sym[i].st_info >> 4) == GLOBAL) || ((sym[i].st_info >> 4) == WEAK)) {
	    if ((sym[i].st_shndx != SHN_UNDEF) && ((sym[i].st_info & 15) != SECTION)) {
		add_symbol(erl_record, strtab_names + sym[i].st_name, ((u32)erl_record->bytes) + sec[sym[i].st_shndx].sh_addr + sym[i].st_value);
	    }
	}
    }

#ifdef _EE
    FlushCache(2);
    FlushCache(0);
#endif

    *p_erl_record = erl_record;

    free_and_return(0);
}

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef int (*func_t)(void);
typedef int (*start_t)(int argc, char ** argv);

static struct erl_record_t * _init_load_erl_wrapper_from_file(char * erl_id) {
    char tmpnam[256];
    strcpy(tmpnam, erl_id);
    strcat(tmpnam, ".erl");
    return _init_load_erl_from_file(tmpnam, erl_id);
}

erl_loader_t _init_load_erl = _init_load_erl_wrapper_from_file;

static struct erl_record_t * load_erl(const char * fname, u8 * elf_mem, u32 addr, int argc, char ** argv) {
    struct erl_record_t * r;
    struct symbol_t * s;
    int elf_handle = 0;

    if (fname) {
        if ((elf_handle = open(fname, O_RDONLY | O_BINARY)) < 0) {
	    return 0;
	}
    }

    if (read_erl(elf_handle, elf_mem, addr, &r) < 0) {
	if (fname) {
	    close(elf_handle);
	}
	return 0;
    }

    if (fname) {
	close(elf_handle);
    }

    if ((s = erl_find_local_symbol("erl_id", r))) {
	r->name = *(char **) s->address;
    } else {
	r->name = 0;
    }

    if ((s = erl_find_local_symbol("erl_dependancies", r))) {
	r->dependancies = (char **) s->address;
    } else {
	r->dependancies = 0;
    }

    if (r->dependancies) {
	char ** d;
	for (d = r->dependancies; *d; d++) {
	    _init_load_erl(*d);
	}
    }

    if ((s = erl_find_local_symbol("_init", r))) {
#ifdef _EE
        ((func_t)s->address)();
#endif
    }

    if ((s = erl_find_local_symbol("_start", r))) {
#ifdef _EE
        int _start_ret;
        if ((_start_ret = ((start_t)s->address)(argc, argv))) {
	    if (unload_erl(r))
		return 0;
	}
#endif
    }

    return r;
}

struct erl_record_t * _init_load_erl_from_file(const char * fname, char * erl_id) {
    char tfname[1024];
    char * argv[2];

    if (erl_id) {
        struct erl_record_t * r;
        if ((r = find_erl(erl_id)))
    	    return r;
    }

    argv[0] = erl_id;
    argv[1] = 0;

    strcpy(tfname, _init_erl_prefix);
    strcat(tfname, fname);

    return load_erl_from_file(tfname, 1, argv);
}

struct erl_record_t * load_erl_from_file(const char * fname, int argc, char ** argv) {
    return load_erl(fname, 0, ERL_DYN_ADDR, argc, argv);
}

struct erl_record_t * load_erl_from_mem(u8 * mem, int argc, char ** argv) {
    return load_erl(0, mem, ERL_DYN_ADDR, argc, argv);
}

struct erl_record_t * load_erl_from_mem_to_addr(u8 * mem, u32 addr, int argc, char ** argv) {
    return load_erl(0, mem, addr, argc, argv);
}

struct erl_record_t * load_erl_from_file_to_addr(const char * fname, u32 addr, int argc, char ** argv) {
    return load_erl(fname, 0, addr, argc, argv);
}

struct erl_record_t * _init_load_erl_from_file_to_addr(const char * fname, u32 addr, char * erl_id) {
    char tfname[1024];
    char * argv[2];

    if (erl_id) {
        struct erl_record_t * r;
        if ((r = find_erl(erl_id)))
    	    return r;
    }

    argv[0] = erl_id;
    argv[1] = 0;

    strcpy(tfname, _init_erl_prefix);
    strcat(tfname, fname);

    return load_erl_from_file_to_addr(tfname, addr, 1, argv);
}

void r_unload_dependancies(char ** d) {
    struct erl_record_t * erl;
    int count = 0;
    int i;

    if (!d || !(*d))
	return;

    while (d[count])
	count++;

    for (i = count - 1; i >= 0; i--) {
	if ((erl = find_erl(d[i])))
	    unload_erl(erl);
    }
}

int unload_erl(struct erl_record_t * erl) {
    struct symbol_t * s;
    struct dependancy_t * p;

    if ((erl->flags) & ERL_FLAG_STICKY) {
	return 0;
    }

    for (p = dependancy_root; p; p = p->next) {
	if (p->provider == erl) {
	    return 0;
	}
    }

    if ((s = erl_find_local_symbol("_fini", erl))) {
#ifdef _EE
        ((func_t)s->address)();
#endif
    }

    if (erl->dependancies)
	r_unload_dependancies(erl->dependancies);

    erl_flush_symbols(erl);

    destroy_dependancy_r(erl);

    destroy_erl_record(erl);

    return 1;
}

struct erl_record_t * erl_resolve(u32 address) {
    struct erl_record_t * r;

    for (r = erl_record_root; r; r = r->next) {
	u32 r_ptr = (u32) r->bytes;
	if ((address >= r_ptr) && (address < (r_ptr + r->fullsize)))
	    return r;
    }

    return 0;
}

struct erl_record_t * find_erl(const char * name) {
    struct erl_record_t * r;

    for (r = erl_record_root; r; r = r->next) {
	if (r->name)
	    if (!strcmp(name, r->name))
		return r;
    }

    return 0;
}

void erl_flush_symbols(struct erl_record_t * erl) {
    if (!erl->symbols)
	return;

    if (hfirst(erl->symbols)) do {
	destroy_symbol((struct symbol_t *) hstuff(erl->symbols));
	free(hkey(erl->symbols));
	hdel(erl->symbols);
    } while (hcount(erl->symbols));

    hdestroy(erl->symbols);

    erl->symbols = 0;
}
