// Phase arenas + instrumentation shared by the whole app.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "story_arena.h"
#include "story_mem.h"

extern story_arena_t g_fast;   // internal SRAM, DMA-capable, reserved at boot
extern story_arena_t g_bulk;   // PSRAM, reserved at boot

bool phase_arenas_init(size_t fast_bytes, size_t bulk_bytes);

typedef struct {
    const char *name;
    int64_t t0;
    story_mem_snapshot_t mem0;
} phase_t;

// Takes both arenas for `name` and logs memory; returns false if refused.
bool phase_begin(phase_t *p, const char *name);
// Releases both arenas and logs duration + memory delta + arena peaks.
void phase_end(phase_t *p);

// Reads a whole SD file into a 16-byte aligned block of `arena`, streaming
// through an internal DMA bounce buffer (FATFS->PSRAM direct reads are ~2x
// slower). Logs size, time and MB/s. Returns NULL (and logs) on failure.
void *asset_load(story_arena_t *arena, const char *path, size_t *out_len);
