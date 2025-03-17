#include <owl_packet.h>
#include <debug.h>
#include <stdlib.h>

owl_controller controller = { 0 };
owl_packet internal_packet = { 0 };

void owl_init(void *ptr, size_t size) {
    controller.channel = CHANNEL_SIZE;

    SyncDCache(ptr, &((owl_qword *)(ptr))[size-1]);

    controller.base = (uint32_t)(ptr) | 0x30000000;
    internal_packet.ptr = controller.base;

    controller.size = size/2;
    controller.alloc = 0;

    controller.context = false;
}

void owl_flush_packet() {
    if (controller.alloc == 0) {
        return;
    }

    owl_add_end_tag(&internal_packet, 0);

    dmaKit_wait(controller.channel, 0);

	dmaKit_send_chain_ucab(controller.channel, (void *)((uint32_t)(controller.base + (controller.context? controller.size : 0))));

    controller.context = (!controller.context); 

    internal_packet.ptr = controller.base + (controller.context? controller.size : 0);
    controller.alloc = 0;
}

owl_packet *owl_query_packet(owl_channel channel, size_t size) {
    if (channel != controller.channel) {
        if (controller.channel != CHANNEL_SIZE) {
            owl_flush_packet();
            
        }

        controller.channel = channel;
    } else if ((controller.alloc + size) + 1 >= controller.size) { // + 1 for end tag
        owl_flush_packet();
    }

    controller.alloc += size;

    return &internal_packet;
}

owl_packet *owl_create_packet(owl_channel channel, size_t size, void* buf) {
    owl_packet *packet = (owl_packet *)buf;

    packet->channel = channel;
    packet->size = size-(sizeof(owl_packet)/sizeof(owl_qword));
    packet->base = (owl_qword *)(((uint32_t)(buf) + sizeof(owl_packet)) | 0x30000000);
    packet->ptr = packet->base;

    SyncDCache(packet->base, packet->base+packet->size);

    return packet;
}

void owl_send_packet(owl_packet *packet) {
    packet->ptr = packet->base;

	//FlushCache(0);
	dmaKit_send_chain_ucab(packet->channel, (uint32_t)(packet->base)/*, (((uint32_t)(packet->ptr)-(uint32_t)(packet->base))/sizeof(owl_qword))+1*/);
}

owl_controller *owl_get_controller() {
    return &controller;
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

    owl_packet *internal_packet = owl_query_packet(CHANNEL_VIF1, packet_size);

	// get the size of the code as we can only send 256 instructions in each MPGtag
	uint32_t dest = 0;
    uint32_t count = (end - start) / 2;
    if (count & 1)
        count++;

    uint32_t *l_start = start;

    while (count > 0)
    {
        uint16_t curr_count = count > 256 ? 256 : count;

        owl_add_tag(internal_packet, (VIF_CODE(0, 0, VIF_NOP, 0) | (uint64_t)VIF_CODE(dest, curr_count & 0xFF, VIF_MPG, 0) << 32),
                            DMA_TAG(curr_count / 2, 0, DMA_REF, 0, (const u128 *)l_start, 0)
                    );

        l_start += curr_count * 2;
        count -= curr_count;
        dest += curr_count;
    }

    //owl_add_tag(internal_packet, (VIF_CODE(0, 0, VIF_NOP, 0) | (uint64_t)VIF_CODE(0, 0, VIF_NOP, 0) << 32),
    //                    DMA_TAG(0, 0, DMA_END, 0, 0 , 0));
}

void vu1_set_double_buffer_settings(uint32_t base, uint32_t offset)
{
	owl_packet *internal_packet = owl_query_packet(CHANNEL_VIF1, 2);

    owl_add_tag(internal_packet, (VIF_CODE(base, 0, VIF_BASE, 0) | (uint64_t)VIF_CODE(offset, 0, VIF_OFFSET, 0) << 32), 
                        DMA_TAG(0, 0, DMA_CNT, 0, 0 , 0));

    //owl_add_tag(internal_packet, (VIF_CODE(0, 0, VIF_NOP, 0) | (uint64_t)VIF_CODE(0, 0, VIF_NOP, 0) << 32),
    //                DMA_TAG(0, 0, DMA_END, 0, 0 , 0));

}