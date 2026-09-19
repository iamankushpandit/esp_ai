#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int64_t init_us, first_audio_us, total_us;
    long audio_ms;
} speak_stats_t;

// Runs the whole SPEAKING phase (load voice, speak, unload). Arenas must be free.
bool speak_text(const char *text, volatile bool *stop, speak_stats_t *st);
