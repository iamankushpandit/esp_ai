// SPEAKING phase: PicoTTS lingware SD -> PSRAM, synthesize straight into I2S.
#include "speak.h"
#include "phase.h"
#include "board.h"
#include "story_tts.h"
#include "prefs.h"
#include <math.h>
#include "esp_log.h"

static const char *TAG = "speak";

#define TTS_TA_PATH BOARD_SD_MOUNT "/story/tts/en-US_ta.bin"
#define TTS_SG_PATH BOARD_SD_MOUNT "/story/tts/en-US_lh0_sg.bin"
// Second voice (demo): a different PicoTTS lingware, not a pitch trick.
#define TTS_TA_PATH_GB BOARD_SD_MOUNT "/story/tts/en-GB_ta.bin"
#define TTS_SG_PATH_GB BOARD_SD_MOUNT "/story/tts/en-GB_kh0_sg.bin"

typedef struct {
    int64_t first_us;
    long samples;
} spk_ctx_t;

// Voice gain (Settings): digital boost on the TTS samples. Above KNEE the
// signal is compressed smoothly toward full scale instead of hard clipping,
// so +12 dB stays clean on loud syllables.
#define KNEE 24000.0f

static inline int16_t gain_sample(int16_t s, float g)
{
    float y = s * g, a = y < 0 ? -y : y;
    if (a > KNEE) a = KNEE + (a - KNEE) / (1.0f + (a - KNEE) / (32767.0f - KNEE));
    return (int16_t)(y < 0 ? -a : a);
}

static bool to_speaker(void *u, const int16_t *pcm, size_t n)
{
    spk_ctx_t *c = (spk_ctx_t *)u;
    if (!c->first_us) c->first_us = story_time_us();
    const int db = prefs()->voice_gain_db;
    if (db <= 0) {
        board_spk_write(pcm, n, 1000);
    } else {
        const float g = powf(10.0f, db / 20.0f);
        int16_t buf[256];
        for (size_t off = 0; off < n; off += 256) {
            size_t k = n - off < 256 ? n - off : 256;
            for (size_t i = 0; i < k; i++) buf[i] = gain_sample(pcm[off + i], g);
            board_spk_write(buf, k, 1000);
        }
    }
    c->samples += n;
    return true;
}

bool speak_text(const char *text, volatile bool *stop, speak_stats_t *st)
{
    return speak_text_voice(text, VOICE_IVY, stop, st);
}

bool speak_text_voice(const char *text, speak_voice_t v, volatile bool *stop, speak_stats_t *st)
{
    phase_t ph;
    if (!phase_begin(&ph, "SPEAK")) return false;
    int64_t t0 = story_time_us();
    bool ok = false;
    tts_blobs_t b;
    const char *ta = v == VOICE_ASKER ? TTS_TA_PATH_GB : TTS_TA_PATH;
    const char *sg = v == VOICE_ASKER ? TTS_SG_PATH_GB : TTS_SG_PATH;
    b.ta = asset_load(&g_bulk, ta, NULL);
    b.sg = asset_load(&g_bulk, sg, NULL);
    if ((!b.ta || !b.sg) && v == VOICE_ASKER) {
        // The second voice is optional: fall back rather than fail the demo.
        ESP_LOGW(TAG, "en-GB lingware missing; using the default voice");
        phase_end(&ph);
        return speak_text_voice(text, VOICE_IVY, stop, st);
    }
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
