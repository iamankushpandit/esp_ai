// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

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

// Idle-time memory reuse: give the internal arena back to the system heap
// (e.g. for Wi-Fi during a clock sync), then reserve it again. While lent,
// phase_begin() is refused. Reclaim re-sizes adaptively and logs the size.
bool phase_fast_lend(const char *who);
bool phase_fast_reclaim(void);

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
