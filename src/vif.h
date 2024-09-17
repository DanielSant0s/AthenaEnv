#ifndef VIF_H
#define VIF_H

#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <malloc.h>
#include <stdint.h>
#include <tamtypes.h>

#define GIFTAG(NLOOP,EOP,PRE,PRIM,FLG,NREG) \
                ((u64)(NLOOP)	<< 0)	| \
                ((u64)(EOP)	<< 15)	| \
                ((u64)(PRE)	<< 46)	| \
                ((u64)(PRIM)	<< 47)	| \
                ((u64)(FLG)	<< 58)	| \
                ((u64)(NREG)	<< 60)

typedef struct {
    uint32_t vu_addr;
    bool top;
} unpack_list;

typedef struct {
    uint64_t *base;
    uint64_t *ptr;
    bool unpack_opened; // true when under a unpack list
    bool internal_buf; // true when using malloc instead of a static mem buffer
    unpack_list list;
} dma_packet;

#define register_vu_program(name)               \
	extern u32 name##_CodeStart __attribute__((section(".vudata"))); \
	extern u32 name##_CodeEnd __attribute__((section(".vudata")))

#define dma_add_end_tag(packet) \
	do { \
		*packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0); \
		*packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32); \
	} while (0)

#define vu_start_program(packet, init) \
	do { \
		*packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0); \
		*packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, (init? VIF_MSCALF : VIF_MSCNT), 0) << 32)); \
	} while (0)

typedef struct {
    uint32_t VPS : 2;   // VIF command status
    uint32_t VEW : 1;   // VU is executing microprogram
    uint32_t VGW : 1;   // Stalled waiting for GIF (VIF1 only)
    uint32_t reserved1 : 2;
    uint32_t MRK : 1;   // MARK detected
    uint32_t DBF : 1;   // Double buffer flag (VIF1 only)
    uint32_t VSS : 1;   // Stalled after STOP was sent to FBRST
    uint32_t VFS : 1;   // Stalled after force break was sent to FBRST
    uint32_t VIS : 1;   // Stalled on interrupt ibt
    uint32_t INT : 1;   // Interrupt bit detected
    uint32_t ER0 : 1;   // DMAtag mismatch error
    uint32_t ER1 : 1;   // Invalid VIF command was sent
    uint32_t reserved2 : 9;
    uint32_t FDR : 1;   // FIFO direction (VIF1 only)
    uint32_t FQC : 5;   // Amount of quadwords in FIFO
} VIFn_STAT;

typedef enum {
    IDLE,
    WAIT_FOR_DATA_FOLLOWING_CMD,
    DECODING_CMD,
    DECOMPRESSING_OR_TRANSFERRING_DATA,
} VIF_CMD_STATUS;

#define VIF1_STAT ((VIFn_STAT*)(0x10003C00))

#define UNPACK_S_32 0x00
#define UNPACK_S_16 0x01
#define UNPACK_S_8 0x02
#define UNPACK_V2_32 0x04
#define UNPACK_V2_16 0x05
#define UNPACK_V2_8 0x06
#define UNPACK_V3_32 0x08
#define UNPACK_V3_16 0x09
#define UNPACK_V3_8 0x0A
#define UNPACK_V4_32 0x0C
#define UNPACK_V4_16 0x0D
#define UNPACK_V4_8 0x0E
#define UNPACK_V4_5 0x0F

#define VIF_NOP 0
#define VIF_STCYCL 1
#define VIF_OFFSET 2
#define VIF_BASE 3
#define VIF_ITOP 4
#define VIF_STMOD 5
#define VIF_MSKPATH3 6
#define VIF_MARK 7
#define VIF_FLUSHE 16
#define VIF_FLUSH 17
#define VIF_FLUSHA 19
#define VIF_MSCAL 20
#define VIF_MSCNT 23
#define VIF_MSCALF 21
#define VIF_STMASK 32
#define VIF_STROW 48
#define VIF_STCOL 49
#define VIF_MPG 74
#define VIF_DIRECT 80
#define VIF_DIRECTHL 81

#define VIF_CODE(_immediate, _num, _cmd, _irq) ((u32)(_immediate) | ((u32)(_num) << 16) | ((u32)(_cmd) << 24) | ((u32)(_irq) << 31))

#define DRAW_STQ2_REGLIST \
	((u64)GS_ST)     << 0 | \
	((u64)GS_RGBAQ)  << 4 | \
	((u64)GS_XYZ2)   << 8

#define DRAW_NOTEX_REGLIST \
	((u64)GS_RGBAQ)  << 0 | \
	((u64)GS_XYZ2)   << 4

/** Texture Alpha Expansion */
#define ALPHA_EXPAND_NORMAL			0
#define ALPHA_EXPAND_TRANSPARENT	1

#define VU_GS_PRIM(PRIM, IIP, TME, FGE, ABE, AA1, FST, CTXT, FIX) (u128)(((FIX << 10) | (CTXT << 9) | (FST << 8) | (AA1 << 7) | (ABE << 6) | (FGE << 5) | (TME << 4) | (IIP << 3) | (PRIM)))
#define VU_GS_GIFTAG(NLOOP, EOP, PRE, PRIM, FLG, NREG) (((u64)(NREG) << 60) | ((u64)(FLG) << 58) | ((u64)(PRIM) << 47) | ((u64)(PRE) << 46) | (EOP << 15) | (NLOOP << 0))

inline uint64_t *vu_add_unpack_data(uint64_t *p_data, u32 t_dest_address, void *t_data, u32 t_size, u8 t_use_top) {
    *p_data++ = DMA_TAG(t_size, 0, DMA_REF, 0, t_data, 0);
	*p_data++ = (VIF_CODE(0x0101 | (0 << 8), 0, VIF_STCYCL, 0) | (u64)
	VIF_CODE(t_dest_address | ((u32)1 << 14) | ((u32)t_use_top << 15), ((t_size == 256) ? 0 : t_size), UNPACK_V4_32 | ((u32)0 << 4) | 0x60, 0) << 32 );

	return p_data;
}

void vu1_upload_micro_program(u32* start, u32* end);

void vu1_set_double_buffer_settings(u32 base, u32 offset);

void vifSendPacket(void* packet, u32 vif_channel);

void *vifCreatePacket(uint32_t size);

void vifDestroyPacket(void* packet);

inline void dma_packet_create(dma_packet *packet, void* addr, uint32_t size) {
    packet->base = (addr? addr : vifCreatePacket(size));
    packet->internal_buf = !addr;
    packet->ptr = packet->base;
    packet->unpack_opened = false;
}

inline void dma_packet_reset(dma_packet *packet) {
    packet->ptr = packet->base;
}

inline void dma_packet_destroy(dma_packet *packet) {
    if (packet->internal_buf)
        vifDestroyPacket(packet->base);

    packet->base = NULL;
    packet->internal_buf = false;
    packet->ptr = NULL;
    packet->unpack_opened = false;
}

inline void dma_packet_send(dma_packet *packet, uint32_t channel) {
    vifSendPacket(packet->base, channel);
}

inline void dma_packet_add_tag(dma_packet *packet, uint64_t a1, uint64_t a2) {
	asm volatile ( 	
		"pcpyld $7, %1, %2 \n"
		"sq    $7,0x0(%0) \n"
		 : : "r" (packet->ptr), "r" (a1), "r" (a2):"$7","memory");
	packet->ptr+=2;
}

#define dma_packet_add_end_tag(packet) \
    dma_packet_add_tag(packet, (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32), DMA_TAG(0, 0, DMA_END, 0, 0 , 0))

#define dma_packet_start_program(packet, init) \
    dma_packet_add_tag(packet, ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, (init? VIF_MSCALF : VIF_MSCNT), 0) << 32)), DMA_TAG(0, 0, DMA_CNT, 0, 0, 0))


inline void dma_packet_add_uquad(dma_packet *packet, __uint128_t a1) {
	asm volatile ( 	
        "lq    $7,0x0(%1)\n" 
		"sq    $7,0x0(%0) \n"
		 : : "r" (packet->ptr), "r" (&a1):"$7","memory");
	packet->ptr+=2;
}

inline void dma_packet_add_uint(dma_packet *packet, uint32_t a1) {
    uint32_t* tmp = (uint32_t*)packet->ptr;
	*tmp++ = a1;
    packet->ptr = (uint64_t *)tmp;
}

inline void dma_packet_add_float(dma_packet *packet, float a1) {
    float* tmp = (float*)packet->ptr;
	*tmp++ = a1;
    packet->ptr = (uint64_t *)tmp;
}

inline void unpack_list_open(dma_packet *packet, uint32_t vu_base, bool top) {
    packet->unpack_opened = true;
    packet->list.vu_addr = vu_base;
    packet->list.top = top;
}

inline void *unpack_list_append(dma_packet *packet, void *t_data, uint32_t t_size) {
    *packet->ptr++ = DMA_TAG(t_size, 0, DMA_REF, 0, t_data, 0);
	*packet->ptr++ = (VIF_CODE(0x0101 | (0 << 8), 0, VIF_STCYCL, 0) | (u64)
	VIF_CODE(packet->list.vu_addr | ((u32)1 << 14) | ((u32)packet->list.top << 15), ((t_size == 256) ? 0 : t_size), UNPACK_V4_32 | ((u32)0 << 4) | 0x60, 0) << 32 );
    packet->list.vu_addr += t_size;
}

inline void unpack_list_close(dma_packet *packet) {
    packet->unpack_opened = false;
    packet->list.vu_addr = 0;
    packet->list.top = false;
}

#endif
