#include <owl_packet.h>
#include <profiler.h>
#include <debug.h>
#include <stdlib.h>

owl_controller controller = { 0 };
owl_packet internal_packet = { 0 };

// Index of the chain currently being filled. See owl_flush_generation().
static uint32_t flush_generation = 0;
// Chains with a lower index than this are known to have been read in full.
static uint32_t consumed_generation = 0;
// Last chain that DMA_REFs memory the EE may rewrite with no way to wait first
// (JS-owned ArrayBuffers). See owl_mark_volatile_refs().
static uint32_t volatile_generation = UINT32_MAX;
// Runs before a chain is handed to the DMAC. See owl_set_pre_kick_hook().
static void (*pre_kick_hook)(void) = NULL;

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

    if (pre_kick_hook)
        pre_kick_hook();

    prof_push(PROF_GPU_WAIT);
    dmaKit_wait(controller.channel, 0);
    prof_pop();

    // The wait above drained every chain sent so far, i.e. everything below
    // flush_generation.
    consumed_generation = flush_generation;

	dmaKit_send_chain_ucab(controller.channel, (void *)((uint32_t)(controller.base + (controller.context? controller.size : 0))));

    controller.context = (!controller.context); 

    internal_packet.ptr = controller.base + (controller.context? controller.size : 0);
    controller.alloc = 0;

    flush_generation++;
}

uint32_t owl_flush_generation() {
    return flush_generation;
}

int owl_generation_read(uint32_t generation) {
    return generation < consumed_generation;
}

void owl_wait_generation(uint32_t generation) {
    if (owl_generation_read(generation)) {
        return;
    }

    // Still filling that chain: it has to be sent before it can be waited on.
    if (generation == flush_generation) {
        owl_flush_packet();
    }

    prof_push(PROF_GPU_WAIT);
    dmaKit_wait(controller.channel, 0);
    prof_pop();

    consumed_generation = flush_generation;
}

void owl_wait_inflight() {
    prof_push(PROF_GPU_WAIT);

    if (controller.channel != CHANNEL_SIZE)
        dmaKit_wait(controller.channel, 0);

    // Texture uploads ride PATH3, queued from the VIF1 interrupt handler, and
    // can still be streaming after the VIF1 chain itself has drained.
    dmaKit_wait(DMA_CHANNEL_GIF, 0);

    prof_pop();

    consumed_generation = flush_generation;
}

void owl_mark_volatile_refs() {
    volatile_generation = flush_generation;
}

int owl_last_sent_has_volatile_refs() {
    return flush_generation > 0 && volatile_generation == flush_generation - 1;
}

void owl_set_pre_kick_hook(void (*hook)(void)) {
    pre_kick_hook = hook;
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

void vu1_set_double_buffer_settings(uint32_t base, uint32_t offset)
{
	owl_packet *internal_packet = owl_query_packet(CHANNEL_VIF1, 1);

    owl_add_tag(internal_packet, (VIF_CODE(base, 0, VIF_BASE, 0) | (uint64_t)VIF_CODE(offset, 0, VIF_OFFSET, 0) << 32), 
                        DMA_TAG(0, 0, DMA_CNT, 0, 0 , 0));

    //owl_add_tag(internal_packet, (VIF_CODE(0, 0, VIF_NOP, 0) | (uint64_t)VIF_CODE(0, 0, VIF_NOP, 0) << 32),
    //                DMA_TAG(0, 0, DMA_END, 0, 0 , 0));

}
