// THINKING phase: SD -> PSRAM model load, TinyTalk answer, streamed to UI.
#include "think.h"
#include "story_intent.h"
#include "gguf_llm.h"
#include <sys/stat.h>
#include "phase.h"
#include "board.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "think";

// Model directory holds model.bin + tok.bin; swap models without rebuilding.
static char s_dir[64] = BOARD_SD_MOUNT "/story/llm8m_v4";    // v4 kid + Gume fine-tune (Mac)

void think_set_model_dir(const char *dir) { snprintf(s_dir, sizeof s_dir, "%s", dir); }
const char *think_model_dir(void) { return s_dir; }

typedef struct {
    think_stream_fn fn;
    const llm_stats_t *st;     // prompt_tokens is set before the first piece
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
        s->fn(s->user, s->buf, s->st->prompt_tokens, s->tokens, rate(s, now));
        s->flushed = s->len;
        s->last_us = now;
    }
    return true;
}

// ---- GGUF models (a .gguf file instead of a model.bin/tok.bin folder)
static char s_err[96];
static volatile bool *s_stop_flag;      // set while a model is generating

const char *think_last_error(void) { return s_err; }

void think_stop(void)
{
    if (s_stop_flag) *s_stop_flag = true;
}

#define GGUF_CTX 128

static bool think_gguf(const char *question, char *answer, size_t cap, think_stream_fn fn, void *user,
                       llm_stats_t *st, int64_t t0)
{
    struct stat sb;
    if (stat(s_dir, &sb) != 0) {
        snprintf(s_err, sizeof s_err, "Model file missing");
        return false;
    }
    // The file is copied to PSRAM whole; the KV cache and tables come after.
    const size_t free = story_arena_free_bytes(&g_bulk), reserve = 1200 * 1024;
    if ((size_t)sb.st_size + reserve > free) {
        snprintf(s_err, sizeof s_err, "Model too big: %.1f MB, this device fits %.1f MB",
                 sb.st_size / 1048576.0, (free - reserve) / 1048576.0);
        ESP_LOGE(TAG, "%s (%s)", s_err, s_dir);
        return false;
    }
    size_t n = 0;
    const uint8_t *file = asset_load(&g_bulk, s_dir, &n);
    if (!file) { snprintf(s_err, sizeof s_err, "Can't read model file"); return false; }
    static gguf_llm_t m;
    char err[96];
    if (!gguf_llm_load(&m, file, n, GGUF_CTX, &g_fast, &g_bulk, err, sizeof err)) {
        snprintf(s_err, sizeof s_err, "Unsupported model: %.70s", err);
        ESP_LOGE(TAG, "%s (%s)", s_err, s_dir);
        return false;
    }
    int64_t load_us = story_time_us() - t0;
    llm_params_t p = llm_default_params();
    stream_t s = {.fn = fn, .user = user, .st = st};
    s_stop_flag = &m.stop;
    bool ok = gguf_llm_answer(&m, question, &p, answer, cap, on_piece, &s, st);
    s_stop_flag = NULL;
    if (fn) fn(user, answer, st->prompt_tokens, st->gen_tokens,
               st->gen_us ? st->gen_tokens / (st->gen_us / 1e6f) : rate(&s, story_time_us()));
    st->load_us = load_us;
    if (ok)
        ESP_LOGI(TAG, "GGUF %s: load %lld ms, prompt %d tok (prefill %lld ms), gen %d tok in %lld ms = %.1f tok/s, stop=%s",
                 m.name, load_us / 1000, st->prompt_tokens, st->prefill_us / 1000, st->gen_tokens,
                 st->gen_us / 1000, st->gen_tokens / (st->gen_us / 1e6 + 1e-9), st->stop_reason);
    return ok;
}

bool think_answer(const llm_history_t *hist, const char *question, char *answer, size_t cap,
                  think_stream_fn fn, void *user, llm_stats_t *st)
{
    llm_stats_t local;
    if (!st) st = &local;   // the stream callback reads the prompt size from here
    char topic[LLM_TURN_CHARS];
    story_intent_t intent = story_intent(question, topic, sizeof topic);
    phase_t ph;
    if (!phase_begin(&ph, "THINK")) return false;
    bool ok = false;
    int64_t t0 = story_time_us();
    s_err[0] = 0;
    size_t pl = strlen(s_dir);
    if (pl > 5 && !strcmp(s_dir + pl - 5, ".gguf")) {
        ok = think_gguf(question, answer, cap, fn, user, st, t0);
        phase_end(&ph);
        return ok;
    }
    llm_blobs_t b = {0};
    char mp[80], tp[80];
    snprintf(mp, sizeof mp, "%s/model.bin", s_dir);
    snprintf(tp, sizeof tp, "%s/tok.bin", s_dir);
    b.model = asset_load(&g_bulk, mp, &b.model_len);
    b.tok = asset_load(&g_bulk, tp, &b.tok_len);
    static llm_t llm;   // ~ small struct; buffers live in the arenas
    if (!b.model || !b.tok) {
        ESP_LOGE(TAG, "LLM assets unavailable on SD");
    } else {
        llm_params_t p = llm_default_params();
        if (llm_load(&llm, &b, &p, &g_fast, &g_bulk)) {
            int64_t load_us = story_time_us() - t0;
            stream_t s = {.fn = fn, .user = user, .st = st};
            s_stop_flag = &llm.stop;
            if (intent == INTENT_STORY) {
                ESP_LOGI(TAG, "intent: story about \"%s\"", topic);
                ok = llm_story(&llm, topic, answer, cap, on_piece, &s, st);
            } else {
                ok = llm_answer(&llm, hist, question, answer, cap, on_piece, &s, st);
            }
            if (fn) fn(user, answer, st->prompt_tokens, st->gen_tokens,
                       st->gen_us ? st->gen_tokens / (st->gen_us / 1e6f) : rate(&s, story_time_us()));
            s_stop_flag = NULL;
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
