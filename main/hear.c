// LISTEN + TRANSCRIBE phase. The STT model is opened at listen start so each
// 360 ms chunk is pre-encoded while the user is still talking; the heavy
// encoder runs once end-of-speech is detected.
#include "hear.h"
#include "phase.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "hear";

volatile int g_hear_listen_count;
volatile bool g_hear_abort;          // Stop button: end listening now, no transcript

#define STT_MODEL_PATH BOARD_SD_MOUNT "/story/stt/model.bin"
#define RING_SLOTS 24

hear_params_t hear_default_params(void)
{
    hear_params_t p = {
        .vad_min_abs = 120,
        .vad_noise_mult = 3.0f,
        .eos_silent_chunks = 2,     // 720 ms
        .max_wait_ms = 8000,
        .min_speech_chunks = 2,
    };
    return p;
}

// ---------------------------------------------------------------- capture task
typedef struct {
    int16_t *ring;                  // RING_SLOTS * STT_CHUNK_SAMPLES
    volatile uint32_t samples;      // total captured
    volatile bool stop;
    SemaphoreHandle_t done;
} cap_t;

static void capture_task(void *arg)
{
    cap_t *c = (cap_t *)arg;
    const size_t ring_n = (size_t)RING_SLOTS * STT_CHUNK_SAMPLES;
    while (!c->stop) {
        size_t pos = c->samples % ring_n;
        size_t n = 512;
        if (pos + n > ring_n) n = ring_n - pos;
        size_t got = board_mic_read(c->ring + pos, n, 200);
        c->samples += got;
    }
    xSemaphoreGive(c->done);
    vTaskDelete(NULL);
}

static int chunk_energy(const int16_t *x)
{
    uint32_t sum = 0;
    for (int i = 0; i < STT_CHUNK_SAMPLES; i++) sum += (uint32_t)abs(x[i]);
    return (int)(sum / STT_CHUNK_SAMPLES);
}

static bool open_stt(void)
{
    if (!stt_open(STT_MODEL_PATH, &g_fast, &g_bulk)) {
        ESP_LOGE(TAG, "STT model unavailable (%s)", STT_MODEL_PATH);
        return false;
    }
    return true;
}

static hear_result_t finish(char *text, size_t cap, hear_stats_t *st)
{
    stt_stats_t ss = {0};
    bool ok = stt_transcribe(text, cap, &ss);
    if (st) st->stt = ss;
    ESP_LOGI(TAG, "STT open %lld ms, pre-encode %lld ms total, infer %lld ms (%d chunks), SD %llu B / %lld ms, resident %u B, heap peak sram %u psram %u",
             ss.open_us / 1000, ss.preenc_us / 1000, ss.infer_us / 1000, ss.chunks,
             (unsigned long long)ss.sd_bytes, ss.sd_us / 1000, (unsigned)ss.resident_bytes,
             (unsigned)ss.sram_peak, (unsigned)ss.psram_peak);
    return ok ? HEAR_OK : HEAR_ERROR;
}

hear_result_t hear_listen(const hear_params_t *p, char *text, size_t cap, hear_status_fn status,
                          hear_stats_t *st)
{
    text[0] = 0;
    hear_stats_t local = {0};
    if (!st) st = &local;
    memset(st, 0, sizeof *st);
    phase_t ph;
    if (!phase_begin(&ph, "HEAR")) return HEAR_ERROR;
    hear_result_t res = HEAR_ERROR;

    cap_t *c = story_arena_calloc(&g_bulk, sizeof(cap_t), 4, "hear.cap");
    int16_t *ring = story_arena_alloc(&g_bulk, (size_t)RING_SLOTS * STT_CHUNK_SAMPLES * 2, 16, "hear.ring");
    int16_t *chunk = story_arena_alloc(&g_bulk, STT_CHUNK_SAMPLES * 2, 16, "hear.chunk");
    if (!c || !ring || !chunk || !open_stt()) {
        phase_end(&ph);
        return HEAR_ERROR;
    }
    c->ring = ring;
    c->done = xSemaphoreCreateBinary();

    board_mic_start();
    g_hear_listen_count++;   // lets test harnesses time a spoken question
    int64_t t0 = story_time_us();
    xTaskCreatePinnedToCore(capture_task, "mic_cap", 3072, c, configMAX_PRIORITIES - 2, NULL,
                            xPortGetCoreID() == 0 ? 1 : 0);
    if (status) status("Listening...");

    uint32_t next = 0;             // next chunk index to process
    int noise = -1, speech = 0, silent_run = 0, peak = 0;
    bool started = false;
    const uint32_t ring_n = RING_SLOTS * STT_CHUNK_SAMPLES;
    while (1) {
        if (g_hear_abort) { res = HEAR_NO_SPEECH; break; }
        if (c->samples < (next + 1) * STT_CHUNK_SAMPLES) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (c->samples - next * STT_CHUNK_SAMPLES > ring_n) {
            ESP_LOGE(TAG, "capture overran pre-encode (behind by %u samples)",
                     (unsigned)(c->samples - next * STT_CHUNK_SAMPLES));
            res = HEAR_ERROR;
            break;
        }
        uint32_t off = (next * STT_CHUNK_SAMPLES) % ring_n;
        memcpy(chunk, ring + off, STT_CHUNK_SAMPLES * 2);
        next++;
        int e = chunk_energy(chunk);
        if (e > peak) peak = e;
        int thr = noise < 0 ? p->vad_min_abs : (int)(noise * p->vad_noise_mult);
        if (thr < p->vad_min_abs) thr = p->vad_min_abs;
        bool voiced = e > thr;
        if (!started && !voiced) noise = noise < 0 ? e : (noise * 3 + e) / 4;   // track floor

        if (!started) {
            // Keep only the most recent silent chunk as lead-in context.
            if (stt_chunk_count() > 0) stt_drop_last(1);
            stt_push_chunk(chunk);
            if (voiced) {
                started = true;
                speech = 1;
                ESP_LOGI(TAG, "speech start (energy %d > thr %d, noise %d)", e, thr, noise);
                if (status) status("Listening... (hearing you)");
            } else if (story_time_us() - t0 > (int64_t)p->max_wait_ms * 1000) {
                res = HEAR_NO_SPEECH;
                break;
            }
            continue;
        }
        stt_push_chunk(chunk);
        if (voiced) { speech++; silent_run = 0; } else silent_run++;
        if (silent_run >= p->eos_silent_chunks || stt_chunk_count() >= STT_MAX_CHUNKS) {
            res = HEAR_OK;
            break;
        }
    }
    c->stop = true;
    xSemaphoreTake(c->done, portMAX_DELAY);
    vSemaphoreDelete(c->done);
    board_mic_stop();
    st->record_us = story_time_us() - t0;
    st->speech_chunks = speech;
    st->noise_floor = noise;
    st->peak_energy = peak;
    ESP_LOGI(TAG, "RECORD %.2f s, %d speech chunks, noise %d, peak energy %d",
             st->record_us / 1e6, speech, noise, peak);

    if (res == HEAR_OK && speech < p->min_speech_chunks) res = HEAR_TOO_SHORT;
    if (res == HEAR_OK) {
        // Keep one trailing silent chunk as context, drop the rest.
        if (silent_run > 1) stt_drop_last(silent_run - 1);
        if (status) status("Transcribing...");
        res = finish(text, cap, st);
    }
    stt_close();
    phase_end(&ph);
    return res;
}

// ---------------------------------------------------------------- WAV test path
hear_result_t hear_wav(const char *path, char *text, size_t cap, hear_stats_t *st)
{
    text[0] = 0;
    phase_t ph;
    if (!phase_begin(&ph, "HEAR-WAV")) return HEAR_ERROR;
    hear_result_t res = HEAR_ERROR;
    FILE *f = fopen(path, "rb");
    int16_t *chunk = story_arena_calloc(&g_bulk, STT_CHUNK_SAMPLES * 2, 16, "wav.chunk");
    uint8_t hdr[44];
    if (!f || !chunk || fread(hdr, 1, 44, f) != 44 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
        ESP_LOGE(TAG, "bad or missing WAV %s", path);
    } else if (open_stt()) {
        uint32_t rate = hdr[24] | hdr[25] << 8 | hdr[26] << 16 | (uint32_t)hdr[27] << 24;
        if (rate != 16000 || hdr[22] != 1 || hdr[34] != 16) {
            ESP_LOGE(TAG, "WAV must be 16 kHz mono 16-bit (got %u Hz, %d ch, %d bit)", (unsigned)rate, hdr[22], hdr[34]);
        } else {
            int64_t t0 = story_time_us();
            size_t n;
            while (stt_chunk_count() < STT_MAX_CHUNKS &&
                   (n = fread(chunk, 2, STT_CHUNK_SAMPLES, f)) > 0) {
                if (n < STT_CHUNK_SAMPLES) memset(chunk + n, 0, (STT_CHUNK_SAMPLES - n) * 2);
                stt_push_chunk(chunk);
                if (n < STT_CHUNK_SAMPLES) break;
            }
            if (st) st->record_us = story_time_us() - t0;
            res = finish(text, cap, st);
        }
        stt_close();
    }
    if (f) fclose(f);
    phase_end(&ph);
    return res;
}
