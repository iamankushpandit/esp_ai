// SPEAKING phase: PicoTTS lingware SD -> PSRAM, synthesize straight into I2S.
#include "speak.h"
#include "phase.h"
#include "board.h"
#include "story_tts.h"
#include "esp_log.h"

static const char *TAG = "speak";

#define TTS_TA_PATH BOARD_SD_MOUNT "/story/tts/en-US_ta.bin"
#define TTS_SG_PATH BOARD_SD_MOUNT "/story/tts/en-US_lh0_sg.bin"

typedef struct {
    int64_t first_us;
    long samples;
} spk_ctx_t;

static bool to_speaker(void *u, const int16_t *pcm, size_t n)
{
    spk_ctx_t *c = (spk_ctx_t *)u;
    if (!c->first_us) c->first_us = story_time_us();
    board_spk_write(pcm, n, 1000);
    c->samples += n;
    return true;
}

bool speak_text(const char *text, volatile bool *stop, speak_stats_t *st)
{
    phase_t ph;
    if (!phase_begin(&ph, "SPEAK")) return false;
    int64_t t0 = story_time_us();
    bool ok = false;
    tts_blobs_t b;
    b.ta = asset_load(&g_bulk, TTS_TA_PATH, NULL);
    b.sg = asset_load(&g_bulk, TTS_SG_PATH, NULL);
    if (!b.ta || !b.sg) {
        ESP_LOGE(TAG, "TTS lingware unavailable on SD");
    } else if (tts_load(&b, &g_bulk)) {
        int64_t t_init = story_time_us();
        spk_ctx_t c = {0};
        board_spk_start();
        long n = tts_speak(text, to_speaker, &c, stop);
        board_spk_stop();
        tts_unload();
        int64_t t_end = story_time_us();
        ok = n >= 0;
        if (st) {
            st->init_us = t_init - t0;
            st->first_audio_us = c.first_us ? c.first_us - t_init : 0;
            st->total_us = t_end - t_init;
            st->audio_ms = c.samples * 1000 / TTS_SAMPLE_RATE;
        }
        ESP_LOGI(TAG, "TTS init %lld ms, first audio %lld ms, speak %lld ms for %ld ms of audio",
                 (t_init - t0) / 1000, c.first_us ? (c.first_us - t_init) / 1000 : -1,
                 (t_end - t_init) / 1000, c.samples * 1000 / TTS_SAMPLE_RATE);
    }
    phase_end(&ph);
    return ok;
}
