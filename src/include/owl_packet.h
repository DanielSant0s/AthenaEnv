#ifndef OWL_PACKET_H
#define OWL_PACKET_H

#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <malloc.h>
#include <stdint.h>
#include <tamtypes.h>

#include <vif.h>

typedef enum {
    CHANNEL_VIF0,
    CHANNEL_VIF1,
    CHANNEL_GIF,
    CHANNEL_FROMIPU,
    CHANNEL_TOIPU,
    CHANNEL_SIF0,
    CHANNEL_SIF1,
    CHANNEL_SIF2,
    CHANNEL_FROMSPR,
    CHANNEL_TOSPR,

    CHANNEL_SIZE
} owl_channel;

typedef union {
    __uint128_t qword;
    uint64_t dword[2];
    uint32_t sword[4];
    uint16_t hword[8];
    uint8_t  byte[16];

    float f[4];
} owl_qword;

typedef struct {
    owl_channel channel;

    owl_qword *base;

    size_t size;
    size_t alloc;

    bool context;
} owl_controller;

typedef struct {
    uint32_t vu_addr;
    bool top;
} unpack_list;

typedef struct {
    // used only with a custom packet
    owl_qword *base; 
    owl_channel channel; 
    size_t size;

    // used for main packet stream too
    owl_qword *ptr;
    bool unpack_opened; // true when under a unpack list
    unpack_list list;
} owl_packet __attribute__((aligned(16)));

void owl_init(void *ptr, size_t size);

#define owl_packet_size(size) (sizeof(owl_packet) + size)

owl_packet *owl_create_packet(owl_channel channel, size_t size, void* buf);

void owl_send_packet(owl_packet *packet);

// do a packet request from main packet buffer
owl_packet *owl_query_packet(owl_channel channel, size_t size);

// flush a requested packet
void owl_flush_packet();

owl_controller *owl_get_controller();

void vu1_upload_micro_program(uint32_t* start, uint32_t* end);

void vu1_set_double_buffer_settings(uint32_t base, uint32_t offset);

inline void owl_add_uint(owl_packet *packet, uint32_t a1) {
    uint32_t* tmp = (uint32_t*)packet->ptr;
	*tmp++ = a1;
    packet->ptr = (owl_qword *)tmp;
}

inline void owl_add_float(owl_packet *packet, float a1) {
    float* tmp = (float*)packet->ptr;
	*tmp++ = a1;
    packet->ptr = (owl_qword *)tmp;
}

inline void owl_add_ulong(owl_packet *packet, uint64_t a1) {
    uint64_t* tmp = (uint64_t*)packet->ptr;
	*tmp++ = a1;
    packet->ptr = (owl_qword *)tmp;
}

inline void owl_add_tag(owl_packet *packet, uint64_t a1, uint64_t a2) {
	asm volatile ( 	
		"pcpyld $7, %1, %2 \n"
		"sq    $7,0x0(%0) \n"
		 : : "r" (packet->ptr), "r" (a1), "r" (a2):"$7","memory");

	packet->ptr++;
}

inline void owl_add_uquad_ptr(owl_packet *packet, __uint128_t *a1) {
	asm volatile ( 	
        "lq    $7,0x0(%1)\n" 
		"sq    $7,0x0(%0) \n"
		 : : "r" (packet->ptr), "r" (a1):"$7","memory");
	packet->ptr++;
}


inline void owl_add_uquad(owl_packet *packet, __uint128_t a1) {
	asm volatile ( 	
        "lq    $7,0x0(%1)\n" 
		"sq    $7,0x0(%0) \n"
		 : : "r" (packet->ptr), "r" (&a1):"$7","memory");
	packet->ptr++;
}

inline void owl_add_unpack_data(owl_packet *packet, uint32_t t_dest_address, void *t_data, uint32_t t_size, uint8_t t_use_top) {
    owl_add_tag(packet, (VIF_CODE(0x0101 | (0 << 8), 0, VIF_STCYCL, 0) | (uint64_t)
	                    VIF_CODE(t_dest_address | ((uint32_t)1 << 14) | ((uint32_t)t_use_top << 15), ((t_size == 256) ? 0 : t_size), UNPACK_V4_32 | ((uint32_t)0 << 4) | 0x60, 0) << 32 ),
                        DMA_TAG(t_size, 0, DMA_REF, 0, t_data, 0)
                );
}

#define owl_add_cnt_tag_fill(packet, count) \
    owl_add_tag(packet, 0, DMA_TAG(count, 0, DMA_CNT, 0, 0, 0))

#define owl_add_cnt_tag(packet, count, upper) \
    owl_add_tag(packet, upper, DMA_TAG(count, 0, DMA_CNT, 0, 0, 0))

#define owl_add_end_tag(packet, count) \
    owl_add_tag(packet, 0, DMA_TAG(count, 0, DMA_END, 0, 0 , 0))

#define owl_add_start_program(packet, init) \
    owl_add_tag(packet, ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (uint64_t)VIF_CODE(0, 0, (init? VIF_MSCALF : VIF_MSCNT), 0) << 32)), DMA_TAG(0, 0, DMA_CNT, 0, 0, 0))

#define owl_vif_code_double(a, b) ((b | (uint64_t)a << 32))

#define owl_add_vif_codes(packet, a, b, c, d) \
    packet->ptr->sword[3] = a; \
    packet->ptr->sword[2] = b; \
    packet->ptr->sword[1] = c; \
    packet->ptr->sword[0] = d; \

inline void unpack_list_open(owl_packet *packet, uint32_t vu_base, bool top) {
    packet->unpack_opened = true;
    packet->list.vu_addr = vu_base;
    packet->list.top = top;
}

inline void *unpack_list_append(owl_packet *packet, void *t_data, uint32_t t_size) {
    owl_add_tag(packet, (VIF_CODE(0x0101 | (0 << 8), 0, VIF_STCYCL, 0) | (uint64_t)
	                    VIF_CODE(packet->list.vu_addr | ((uint32_t)1 << 14) | ((uint32_t)packet->list.top << 15), ((t_size == 256) ? 0 : t_size), UNPACK_V4_32 | ((uint32_t)0 << 4) | 0x60, 0) << 32 ),
                        DMA_TAG(t_size, 0, DMA_REF, 0, t_data, 0)
                );

    packet->list.vu_addr += t_size;
}

inline void unpack_list_close(owl_packet *packet) {
    packet->unpack_opened = false;
    packet->list.vu_addr = 0;
    packet->list.top = false;
}

static const int16_t OWL_XYOFFSET[8] __attribute__((aligned(16))) = { 2048, 2048, 0, 0, 2048, 2048, 0, 0 };
static const uint16_t OWL_XYMAX[8] __attribute__((aligned(16))) =   { 4096, 4096, 0, 0, 4096, 4096, 0, 0 };

// Too much specific stuff
inline void owl_add_xy_uv_2x(owl_packet *packet, int x1, int y1, int u1, int v1, int x2, int y2, int u2, int v2) {
	asm volatile ( 	
        "lq      $8, %[xyoffset] \n"
        "lq      $9, %[xymax]    \n"
        
        "pcpyld $7, %[xy2], %[xy1]    \n" // upper: x1, y1 - lower: x2, y2
        "paddsh $7, $7, $8    \n" // Add XYOFFSET
        
        "pmaxh  $7, $7, $0    \n" // Clamp XY >= 0
        "pminh  $7, $7, $9     \n"  // Clamp MAX_XY > XY
        
        "psllh $7, $7, 4      \n" // ftoi4 - convert XY to 12:4

        "pcpyld  $9, %[uv2], %[uv1]   \n" // upper: u1, v1 - lower: u2, v2

        "psllh  $9, $9, 4     \n"  // ftoi4 - convert UVs to 12:4

        "pcpyld  $8, $7, $9   \n" // upper: u1, v1 - lower: x1, y1
        "pcpyud  $7, $9, $7   \n" // upper: u2, u2 - lower: x2, y2

        "sq    $8, 0x00(%[ptr])    \n"
		"sq    $7, 0x10(%[ptr])    \n"

        "daddiu   %0, %0, 0x20    \n" // packet->ptr += 2;
		 : [ptr] "+r" (packet->ptr) : 
            [xy1] "r"  (((union { int16_t coors[2]; uint32_t w; }){ .coors = {x1, y1} }).w),
            [xy2] "r"  (((union { int16_t coors[2]; uint32_t w; }){ .coors = {x2, y2} }).w),
            [uv1] "r"  (((union { int16_t coors[2]; uint32_t w; }){ .coors = {u1, v1} }).w),
            [uv2] "r"  (((union { int16_t coors[2]; uint32_t w; }){ .coors = {u2, v2} }).w),
            [xyoffset] "m" (OWL_XYOFFSET),
            [xymax]    "m" (OWL_XYMAX)
         : "$7", "$8", "$9", "memory");
}

inline void owl_add_xy(owl_packet *packet, int x, int y) {
	asm volatile ( 	
        "li      $8, 2048     \n" // Y_OFFSET
        "dsll32  $8, 0        \n"
        "ori     $8, 2048     \n" // X_OFFSET

        "paddsw $7, %1, $8    \n"
        "pmaxw  $7, $7, $0    \n"

        "psllw $7, $7, 4      \n"
        
        "li      $9, 0xFFFE   \n" // MAX_Y_COORD
        "dsll32  $9, 0        \n"
        "ori     $9, 0xFFFE   \n" // MAX_X_COORD
         
        "pminw $7, $7, $9     \n"

		"sq    $7,0x00(%0)    \n"
		 : : "r" (packet->ptr++), "r" (((union { int32_t coors[2]; uint64_t dw; }){ .coors = {x, y} }).dw): "$7", "$8", "$9", "memory");
}

inline void owl_add_color(owl_packet *packet, uint64_t color) {
    asm volatile ( 	
        "pextlb $7, $0, %1      \n" //extend to 16bit wide channel
        "pextlh $7, $0, $7      \n" //extend to 32bit wide channel
	    "sq    $7,0x00(%0)      \n"
	     : : "r" (packet->ptr), "r" (color):"$7", "memory");

    packet->ptr++;
}

#endif