#include "vif.h"

void vifSendPacket(void* packet, u32 vif_channel) {
    dmaKit_wait(DMA_CHANNEL_GIF, 0);
    dmaKit_wait(vif_channel, 0);
	FlushCache(0);
	dmaKit_send_chain(vif_channel, (void *)((u32)packet & 0x0FFFFFFF), 0);
}

void *vifCreatePacket(u32 size) {
    return memalign(128, size*16);
}

void vifDestroyPacket(void* packet) {
    free(packet);
}

static inline u32 get_packet_size_for_program(u32 *start, u32 *end)
{
    // Count instructions
    u32 count = (end - start) / 2;
    if (count & 1)
        count++;
    return (count >> 8) + 1;
}

void vu1_upload_micro_program(u32* start, u32* end)
{
	u32 packet_size = get_packet_size_for_program(start, end) + 1; // + 1 for end tag
	u64* p_store;
	u64* p_data = p_store = vifCreatePacket(packet_size);

	// get the size of the code as we can only send 256 instructions in each MPGtag
	u32 dest = 0;
    u32 count = (end - start) / 2;
    if (count & 1)
        count++;

    u32 *l_start = start;

    while (count > 0)
    {
        u16 curr_count = count > 256 ? 256 : count;

		*p_data++ = DMA_TAG(curr_count / 2, 0, DMA_REF, 0, (const u128 *)l_start, 0);

		*p_data++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(dest, curr_count & 0xFF, VIF_MPG, 0) << 32);

        l_start += curr_count * 2;
        count -= curr_count;
        dest += curr_count;
    }

	*p_data++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*p_data++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

    vifSendPacket(p_store, 1);
	vifDestroyPacket(p_store);
}

void vu1_set_double_buffer_settings()
{
	u64* p_data;
	u64* p_store;
	p_data = p_store = vifCreatePacket(2);

	*p_data++ = DMA_TAG(0, 0, DMA_CNT, 0, 0 , 0);
	*p_data++ = (VIF_CODE(8, 0, VIF_BASE, 0) | (u64)VIF_CODE(496, 0, VIF_OFFSET, 0) << 32);

	*p_data++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*p_data++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

    vifSendPacket(p_store, 1);
	vifDestroyPacket(p_store);
}