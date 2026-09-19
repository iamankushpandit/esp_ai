// Memory snapshots and phase timing, logged at every phase transition.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t int_free;       // internal 8-bit capable heap
    size_t int_largest;
    size_t int_min_ever;
    size_t dma_free;       // internal DMA-capable
    size_t psram_free;
    size_t psram_largest;
    size_t psram_min_ever;
} story_mem_snapshot_t;

void story_mem_snapshot(story_mem_snapshot_t *s);

// Logs "MEM <label>: int free/largest/min, psram free/largest/min".
void story_mem_log(const char *label);

// Logs the delta between two snapshots.
void story_mem_log_delta(const char *label, const story_mem_snapshot_t *before,
                         const story_mem_snapshot_t *after);

// Monotonic microseconds.
int64_t story_time_us(void);

#ifdef __cplusplus
}
#endif
