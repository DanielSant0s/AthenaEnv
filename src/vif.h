#ifndef VIF_H
#define VIF_H

#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <malloc.h>
#include <stdint.h>
#include <tamtypes.h>

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

inline u64* vu_add_unpack_data(u64 *p_data, u32 t_dest_address, void *t_data, u32 t_size, u8 t_use_top)
{
    *p_data++ = DMA_TAG(t_size, 0, DMA_REF, 0, t_data, 0);
	*p_data++ = (VIF_CODE(0x0101 | (0 << 8), 0, VIF_STCYCL, 0) | (u64)
	VIF_CODE(t_dest_address | ((u32)1 << 14) | ((u32)t_use_top << 15), ((t_size == 256) ? 0 : t_size), UNPACK_V4_32 | ((u32)0 << 4) | 0x60, 0) << 32 );

	return p_data;
}

void vu1_upload_micro_program(u32* start, u32* end);

void vu1_set_double_buffer_settings();

void vifSendPacket(void* packet, u32 vif_channel);

void *vifCreatePacket(u32 size);

void vifDestroyPacket(void* packet);

#endif
