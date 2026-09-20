// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Free-form English speech-to-text (conformer CTC, 1024 BPE, greedy decode).
// Engine: lspr98/conformer-stt-s3 (Apache-2.0), weights CC-BY-4.0 (NVIDIA
// stt_en_conformer_ctc_small, distilled by lspr98). Model streamed from SD.
//
// Lifecycle (one TRANSCRIBE phase):
//   stt_open()  -> stt_push_chunk() x N (during capture) -> stt_transcribe() -> stt_close()
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "story_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STT_CHUNK_SAMPLES 5760   // 360 ms @ 16 kHz
#define STT_MAX_CHUNKS 23        // 8.28 s (attention must fit 256 KB SRAM)
#define STT_SRAM_BYTES (256 * 1024)
#define STT_PSRAM_WORK_BYTES (4 * 1024 * 1024)

typedef struct {
    int chunks;
    int64_t open_us;         // header table + resident tensors
    int64_t preenc_us;       // total pre-encode time (overlaps capture)
    int64_t infer_us;        // encoder + decoder after end of speech
    uint64_t sd_bytes;
    int64_t sd_us;
    size_t sram_peak, psram_peak, resident_bytes;
} stt_stats_t;

// Allocates the 256 KB SRAM heap from `fast`, the PSRAM work heap + model
// header table + resident tensors from `bulk`, opens the model and starts the
// second-core matmul worker. Logs and returns false on any failure.
bool stt_open(const char *model_path, story_arena_t *fast, story_arena_t *bulk);

// Pre-encodes one STT_CHUNK_SAMPLES chunk (16 kHz s16 mono). ~<360 ms.
bool stt_push_chunk(const int16_t *pcm);
int stt_chunk_count(void);
void stt_drop_last(int n);   // e.g. trailing silence

// Runs the encoder + CTC decode on the pushed chunks. Output lower-case text.
bool stt_transcribe(char *out, size_t cap, stt_stats_t *st);

// Stops the worker, frees everything (arena memory is released by the phase).
void stt_close(void);

#ifdef __cplusplus
}
#endif
