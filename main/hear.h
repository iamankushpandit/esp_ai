#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "story_stt.h"

typedef enum {
    HEAR_OK = 0,        // transcript produced (may be empty)
    HEAR_NO_SPEECH,     // nothing above the VAD threshold before timeout
    HEAR_TOO_SHORT,     // speech shorter than the minimum
    HEAR_ERROR,         // model/memory/SD failure
} hear_result_t;

typedef struct {
    int vad_min_abs;          // absolute mean|x| floor for "speech"
    float vad_noise_mult;     // speech if energy > noise_floor * mult
    int eos_silent_chunks;    // end of speech after this many quiet chunks
    int max_wait_ms;          // give up if no speech starts within this
    int min_speech_chunks;
} hear_params_t;

hear_params_t hear_default_params(void);

typedef void (*hear_status_fn)(const char *status);

typedef struct {
    stt_stats_t stt;
    int64_t record_us;        // from mic start to end-of-speech decision
    int speech_chunks;
    int noise_floor, peak_energy;
} hear_stats_t;

// LISTEN + TRANSCRIBE phase from the microphone.
hear_result_t hear_listen(const hear_params_t *p, char *text, size_t cap, hear_status_fn status,
                          hear_stats_t *st);

// TRANSCRIBE phase from a 16 kHz mono s16 WAV on the SD card (test path).
hear_result_t hear_wav(const char *path, char *text, size_t cap, hear_stats_t *st);

// Incremented each time the mic opens for a question (test harness hook).
extern volatile int g_hear_listen_count;
