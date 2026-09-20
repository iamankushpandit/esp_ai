#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int64_t init_us, first_audio_us, total_us;
    long audio_ms;
} speak_stats_t;

// Which PicoTTS lingware to speak with. The demo uses two so the question and
// the answer are plainly different speakers; VOICE_ASKER falls back to
// VOICE_IVY if the en-GB lingware is not on the SD card.
typedef enum { VOICE_IVY = 0, VOICE_ASKER } speak_voice_t;

// Runs the whole SPEAKING phase (load voice, speak, unload). Arenas must be free.
bool speak_text(const char *text, volatile bool *stop, speak_stats_t *st);
bool speak_text_voice(const char *text, speak_voice_t v, volatile bool *stop, speak_stats_t *st);
