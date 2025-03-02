#include <owl_packet.h>
#include <debug.h>

owl_controller controller = { 0 };
owl_packet packet = { 0 };

void owl_init(void *ptr, size_t size) {
    controller.channel = CHANNEL_SIZE;

    controller.base = ptr;
    packet.ptr = ptr;

    controller.size = size/2;
    controller.alloc = 0;

    controller.context = false;
}

void owl_flush_packet() {
    if (controller.alloc == 0) {
        return;
    }

    owl_add_end_tag(&packet);

    dmaKit_wait(controller.channel, 0);

	FlushCache(0);
	dmaKit_send_chain(controller.channel, (void *)((uint32_t)(controller.base + (controller.context? controller.size : 0)) & 0x0FFFFFFF), 0);
    
    controller.context = (!controller.context); 

    packet.ptr = controller.base + (controller.context? controller.size : 0);
    controller.alloc = 0;
}

owl_packet *owl_open_packet(owl_channel channel, size_t size) {
    if (channel != controller.channel) {
        if (controller.channel != CHANNEL_SIZE) {
            owl_flush_packet();
        }
            

        controller.channel = channel;
    } else if ((controller.alloc + size) + 1 >= controller.size) { // + 1 for end tag
        owl_flush_packet();
    }

    controller.alloc += size;

    return &packet;
}


static inline uint32_t get_packet_size_for_program(uint32_t *start, uint32_t *end)
{
    // Count instructions
    uint32_t count = (end - start) / 2;
    if (count & 1)
        count++;
    return (count >> 8) + 1;
}

void vu1_upload_micro_program(uint32_t* start, uint32_t* end)
{
	uint32_t packet_size = get_packet_size_for_program(start, end) + 1; // + 1 for end tag

    owl_packet *packet = owl_open_packet(CHANNEL_VIF1, packet_size);

	// get the size of the code as we can only send 256 instructions in each MPGtag
	uint32_t dest = 0;
    uint32_t count = (end - start) / 2;
    if (count & 1)
        count++;

    uint32_t *l_start = start;

    while (count > 0)
    {
        uint16_t curr_count = count > 256 ? 256 : count;

        owl_add_tag(packet, (VIF_CODE(0, 0, VIF_NOP, 0) | (uint64_t)VIF_CODE(dest, curr_count & 0xFF, VIF_MPG, 0) << 32),
                            DMA_TAG(curr_count / 2, 0, DMA_REF, 0, (const u128 *)l_start, 0)
                    );

        l_start += curr_count * 2;
        count -= curr_count;
        dest += curr_count;
    }

    //owl_add_tag(packet, (VIF_CODE(0, 0, VIF_NOP, 0) | (uint64_t)VIF_CODE(0, 0, VIF_NOP, 0) << 32),
    //                    DMA_TAG(0, 0, DMA_END, 0, 0 , 0));
}

void vu1_set_double_buffer_settings(uint32_t base, uint32_t offset)
{
	owl_packet *packet = owl_open_packet(CHANNEL_VIF1, 2);

    owl_add_tag(packet, (VIF_CODE(base, 0, VIF_BASE, 0) | (uint64_t)VIF_CODE(offset, 0, VIF_OFFSET, 0) << 32), 
                        DMA_TAG(0, 0, DMA_CNT, 0, 0 , 0));

    //owl_add_tag(packet, (VIF_CODE(0, 0, VIF_NOP, 0) | (uint64_t)VIF_CODE(0, 0, VIF_NOP, 0) << 32),
    //                DMA_TAG(0, 0, DMA_END, 0, 0 , 0));

}