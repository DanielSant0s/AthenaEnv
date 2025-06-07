#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <owl_packet.h>
#include <mpg_manager.h>

#include <macros.h>

struct vu_mpg_cache {
    vu_mpg *entries[MPG_CACHE_SIZE];
    uint32_t dests[MPG_CACHE_SIZE];
}; 

uint32_t vu_code_qwc[VECTOR_UNIT_SIZE] = { VU0_QWC, VU1_QWC };
uint32_t vu_code_mem_size[VECTOR_UNIT_SIZE] = { VU0_SIZE, VU1_SIZE };
uint32_t vu_code_mem_map[VECTOR_UNIT_SIZE] = { VU0_CODE, VU1_CODE };

uint32_t vu_code_qwc_used[VECTOR_UNIT_SIZE] = { 0, 0 };

struct vu_mpg_cache mpg_cache[VECTOR_UNIT_SIZE] = { 
    { .entries={ NULL }, .dests={ 0 } },
    { .entries={ NULL }, .dests={ 0 } }
};


static inline uint32_t mpg_count_instr(uint32_t size) {
    uint32_t count = size / 2;
    if (count & 1)
        count++;

    return count;
}

static inline uint32_t mpg_packet_size(uint32_t size) {
    return (size >> 8);
}

void vu_mpg_upload(void* src, uint32_t dst, uint32_t size, uint32_t qwc, int vu) {
	uint32_t dest = dst;
    uint32_t count = qwc;

    owl_packet *internal_packet = owl_query_packet(vu, mpg_packet_size(count)+1);

    uint32_t *l_start = (uint32_t *)src;

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
}

vu_mpg *vu_mpg_load_buffer(void* ptr, uint32_t size, int dst, bool autofree) {
    vu_mpg *mpg = (vu_mpg *)malloc(sizeof(vu_mpg));

    mpg->code = ptr;
    mpg->size = size;
    mpg->free = autofree;
    mpg->qwc = mpg_count_instr(size);

    mpg->dst = dst;

    return mpg;
}

vu_mpg *vu_mpg_load_file(char *path, int dst) {
    FILE *f = fopen(path, "rb");

    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) < 0)
        goto vu_mpg_load_file_fail;

    uint32_t size = ftell(f);

    if (fseek(f, 0, SEEK_SET) < 0)
        goto vu_mpg_load_file_fail;

    void *ptr = malloc(size);

    if (!ptr) 
        goto vu_mpg_load_file_fail;

    if (fread(ptr, 1, size, f) != size)
        free(ptr);
        goto vu_mpg_load_file_fail;

    fclose(f);

    return vu_mpg_load_buffer(ptr, size, dst, true);

vu_mpg_load_file_fail:
    fclose(f);
    return NULL;
}

void vu_mpg_unload(vu_mpg *mpg) {
    if (mpg->free)
        free(mpg->code);

    free(mpg);
}

int vu_mpg_cycle_cache(int idx, int dst) {
    if (!idx) return 0;

    vu_mpg *tmp = mpg_cache[dst].entries[idx-1];
    mpg_cache[dst].entries[idx-1] = mpg_cache[dst].entries[idx];
    mpg_cache[dst].entries[idx] = tmp;

    return idx-1;
}

int vu_mpg_preload(vu_mpg *mpg, bool dma_transfer) {
    int i = 0;
    for (; i < MPG_CACHE_SIZE; i++) {
        if (mpg == mpg_cache[mpg->dst].entries[i]) {
            return mpg_cache[mpg->dst].dests[vu_mpg_cycle_cache(i, mpg->dst)];
        }
        
        if (!mpg_cache[mpg->dst].entries[i]) {
            break;
        }
    }

    i -= (i < MPG_CACHE_SIZE? 0 : 1);

    for (vu_mpg *tmp = NULL; (i > 0 && (vu_code_qwc_used[mpg->dst] + mpg->qwc) > vu_code_qwc[mpg->dst]); i--, tmp = mpg_cache[mpg->dst].entries[i]) {
        if (tmp) {
            vu_code_qwc_used[mpg->dst] -= tmp->qwc;
            mpg_cache[mpg->dst].entries[i] = NULL;
        }
    }

    mpg_cache[mpg->dst].entries[i] = mpg;
    mpg_cache[mpg->dst].dests[i] = vu_code_qwc_used[mpg->dst];

    vu_code_qwc_used[mpg->dst] += mpg->qwc;

    if (dma_transfer) {
        vu_mpg_upload(mpg->code, mpg_cache[mpg->dst].dests[i], mpg->size, mpg->qwc, mpg->dst);
    } else {
        memcpy((void *)(vu_code_mem_map[mpg->dst]+(mpg_cache[mpg->dst].dests[i] << 4)), mpg->code, mpg->qwc << 4);
    }

    return mpg_cache[mpg->dst].dests[i];
}
