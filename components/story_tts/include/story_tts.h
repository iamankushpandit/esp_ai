// Offline English TTS (SVOX Pico, en-US), synchronous and arena-backed.
// Output: 16 kHz mono s16 PCM delivered in small chunks to a callback, so the
// full waveform is never held in memory.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "story_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TTS_SAMPLE_RATE 16000
#define TTS_WORKSPACE_BYTES 1100000   // Pico's fixed working memory

// Return false to stop synthesis.
typedef bool (*tts_pcm_cb)(void *user, const int16_t *pcm, size_t n);

typedef struct {
    const void *ta;    // text-analysis lingware (4-byte aligned, stays valid while loaded)
    const void *sg;    // signal-generation lingware
} tts_blobs_t;

// Loads the voice using TTS_WORKSPACE_BYTES from `arena`.
bool tts_load(const tts_blobs_t *b, story_arena_t *arena);
// Speak `text`; blocks until done (or stopped). Returns samples produced, -1 on error.
long tts_speak(const char *text, tts_pcm_cb cb, void *user, volatile bool *stop);
void tts_unload(void);

#ifdef __cplusplus
}
#endif
