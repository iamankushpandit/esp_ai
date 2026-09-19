// THINKING phase: SD -> PSRAM model load, TinyTalk answer, streamed to UI.
#include "think.h"
#include "phase.h"
#include "board.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "think";

#define LLM_MODEL_PATH BOARD_SD_MOUNT "/story/llm/model.bin"
#define LLM_TOK_PATH BOARD_SD_MOUNT "/story/llm/tok.bin"

typedef struct {
    think_stream_fn fn;
    void *user;
    char buf[LLM_ANSWER_CHARS + 1];
    size_t len, flushed;
    int64_t last_us, first_us;
    int tokens;
} stream_t;

static float rate(const stream_t *s, int64_t now)
{
    // Speed over the tokens after the first one (excludes prefill latency).
    return (s->tokens > 1 && now > s->first_us) ? (s->tokens - 1) / ((now - s->first_us) / 1e6f) : 0.0f;
}

// Batch UI updates: flush at most every 150 ms or on sentence end.
static bool on_piece(void *u, const char *p, int n)
{
    stream_t *s = (stream_t *)u;
    if (s->len + n > LLM_ANSWER_CHARS) n = LLM_ANSWER_CHARS - (int)s->len;
    memcpy(s->buf + s->len, p, n);
    s->len += n;
    s->buf[s->len] = 0;
    int64_t now = story_time_us();
    if (s->tokens++ == 0) s->first_us = now;
    bool sentence = n > 0 && strchr(".!?", p[n - 1]);
    if (s->fn && (now - s->last_us > 150000 || sentence)) {
        s->fn(s->user, s->buf, s->tokens, rate(s, now));
        s->flushed = s->len;
        s->last_us = now;
    }
    return true;
}

bool think_answer(const llm_history_t *hist, const char *question, char *answer, size_t cap,
                  think_stream_fn fn, void *user, llm_stats_t *st)
{
    phase_t ph;
    if (!phase_begin(&ph, "THINK")) return false;
    bool ok = false;
    int64_t t0 = story_time_us();
    llm_blobs_t b = {0};
    b.model = asset_load(&g_bulk, LLM_MODEL_PATH, &b.model_len);
    b.tok = asset_load(&g_bulk, LLM_TOK_PATH, &b.tok_len);
    static llm_t llm;   // ~ small struct; buffers live in the arenas
    if (!b.model || !b.tok) {
        ESP_LOGE(TAG, "LLM assets unavailable on SD");
    } else {
        llm_params_t p = llm_default_params();
        if (llm_load(&llm, &b, &p, &g_fast, &g_bulk)) {
            int64_t load_us = story_time_us() - t0;
            stream_t s = {.fn = fn, .user = user};
            ok = llm_answer(&llm, hist, question, answer, cap, on_piece, &s, st);
            if (fn) fn(user, answer, st ? st->gen_tokens : s.tokens,
                       st && st->gen_us ? st->gen_tokens / (st->gen_us / 1e6f) : rate(&s, story_time_us()));
            if (st) st->load_us = load_us;
            llm_unload(&llm);
        }
    }
    phase_end(&ph);
    if (st && ok) {
        ESP_LOGI(TAG, "LLM load %lld ms, prompt %d tok (prefill %lld ms), gen %d tok in %lld ms = %.1f tok/s, stop=%s",
                 st->load_us / 1000, st->prompt_tokens, st->prefill_us / 1000, st->gen_tokens,
                 st->gen_us / 1000, st->gen_tokens / (st->gen_us / 1e6 + 1e-9), st->stop_reason);
    }
    return ok;
}
