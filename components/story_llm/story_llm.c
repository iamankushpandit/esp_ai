// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// TinyTalk chat on the GPT-Neo engine. Prompt template from the fine-tune:
//   User: <u1>\nBot: <b1><eos>\nUser: <u2>\nBot:
#include "story_llm.h"
#include "story_log.h"
#include "story_mem.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "llm";

llm_params_t llm_default_params(void)
{
    llm_params_t p = {
        .temperature = 0.3f,   // tuned on host battery (host/llm_battery.txt)
        .top_p = 0.9f,
        .soft_max_tokens = 48,
        .hard_max_tokens = 80,
        .kv_len = 128,
        .seed = 0,
    };
    return p;
}

void llm_history_clear(llm_history_t *h) { memset(h, 0, sizeof *h); }

static void copy_trim(char *dst, const char *src, size_t cap)
{
    while (*src == ' ') src++;
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

void llm_history_push(llm_history_t *h, const char *user, const char *bot)
{
    if (h->n == LLM_MAX_TURNS) {  // drop oldest
        memmove(h->user[0], h->user[1], sizeof(h->user[0]) * (LLM_MAX_TURNS - 1));
        memmove(h->bot[0], h->bot[1], sizeof(h->bot[0]) * (LLM_MAX_TURNS - 1));
        h->n--;
    }
    copy_trim(h->user[h->n], user, LLM_TURN_CHARS);
    copy_trim(h->bot[h->n], bot, LLM_TURN_CHARS);
    h->n++;
}

bool llm_load(llm_t *l, const llm_blobs_t *b, const llm_params_t *p, story_arena_t *fast,
              story_arena_t *bulk)
{
    memset(l, 0, sizeof *l);
    l->params = *p;
    int64_t t0 = story_time_us();
    if (!neo_tok_init(&l->tok, b->tok, b->tok_len, bulk)) return false;
    if (!neo_init(&l->net, b->model, b->model_len, p->kv_len, fast, bulk)) return false;
    if (l->tok.vocab_size != l->net.c.vocab_size) {
        SLOGE(TAG, "tokenizer vocab %d != model vocab %d", l->tok.vocab_size, l->net.c.vocab_size);
        neo_deinit(&l->net);
        return false;
    }
    l->prompt = story_arena_alloc(bulk, sizeof(int) * l->net.kv_len, 4, "llm.prompt");
    if (!l->prompt) { neo_deinit(&l->net); return false; }
    l->loaded = true;
    SLOGI(TAG, "loaded in %lld ms", (story_time_us() - t0) / 1000);
    return true;
}

void llm_unload(llm_t *l)
{
    if (l->loaded) neo_deinit(&l->net);
    l->loaded = false;
}

// Encode "User: <u>\nBot:<b>" segments newest-first within budget.
static int build_prompt(llm_t *l, const llm_history_t *h, const char *q, int budget)
{
    char seg[LLM_TURN_CHARS * 2 + 32];
    int tmp[256];
    int *out = l->prompt;
    int n = 0;

    // Current question first (it must fit; truncate if not).
    snprintf(seg, sizeof seg, "User: %s\nBot:", q);
    int qn = neo_tok_encode(&l->tok, seg, tmp, 256);
    if (qn < 0 || qn > budget) {
        char qq[LLM_TURN_CHARS];
        copy_trim(qq, q, sizeof qq);
        for (int cut = (int)strlen(qq); cut > 0; cut -= 8) {
            qq[cut] = 0;
            snprintf(seg, sizeof seg, "User: %s\nBot:", qq);
            qn = neo_tok_encode(&l->tok, seg, tmp, 256);
            if (qn > 0 && qn <= budget) break;
        }
        if (qn <= 0 || qn > budget) return -1;
    }
    int used = qn;

    // Pick how many history turns fit, newest first.
    int first = h ? h->n : 0;
    while (first > 0) {
        snprintf(seg, sizeof seg, "User: %s\nBot: %s", h->user[first - 1], h->bot[first - 1]);
        int hn = neo_tok_encode(&l->tok, seg, tmp, 256);
        if (hn < 0 || used + hn + 2 > budget) break;
        used += hn + 2;  // + eos + "\n"
        first--;
    }
    int nl_tok[4];
    int nl_n = neo_tok_encode(&l->tok, "\n", nl_tok, 4);
    for (int i = first; h && i < h->n; i++) {
        snprintf(seg, sizeof seg, "User: %s\nBot: %s", h->user[i], h->bot[i]);
        n += neo_tok_encode(&l->tok, seg, out + n, budget - n);
        out[n++] = l->tok.eos_id;
        for (int k = 0; k < nl_n; k++) out[n++] = nl_tok[k];
    }
    snprintf(seg, sizeof seg, "User: %s\nBot:", q);
    int m = neo_tok_encode(&l->tok, seg, out + n, budget - n);
    if (m < 0) {  // truncated case: re-use tmp
        memcpy(out + n, tmp, qn * sizeof(int));
        m = qn;
    }
    return n + m;
}

// Shared decode loop. `chat` stops when the model starts a new "User" turn.
// A repetition guard stops (and trims) when a REP_N-token run repeats, which is
// how small models fall into loops ("who is the one who is the one ...").
#define REP_N 6
#define REP_HIST 96
static bool generate(llm_t *l, int n_prompt, bool chat, char *ans, size_t cap, llm_piece_cb cb,
                     void *user, llm_stats_t *st)
{
    const llm_params_t *p = &l->params;
    int kvL = l->net.kv_len;
    st->prompt_tokens = n_prompt;
    neo_sampler_t smp;
    neo_sampler_init(&smp, p->temperature, p->top_p, p->seed ? p->seed : (uint64_t)story_time_us());

    int gen_ids[REP_HIST];
    size_t gen_alen[REP_HIST];   // answer length before each generated token
    int64_t t0 = story_time_us();
    int pos = 0, abspos = 0, token = l->prompt[0];
    size_t alen = 0;
    bool pending_nl = false;
    st->stop_reason = "hard";
    int V = l->net.c.vocab_size;
    while (1) {
        if (l->stop) { st->stop_reason = "stopped"; break; }
        if (abspos >= l->net.c.seq_len - 1) { st->stop_reason = "ctx"; break; }
        if (pos >= kvL - 1) {
            int keep = kvL / 4 < n_prompt ? kvL / 4 : n_prompt;
            int ev = neo_kv_slide(&l->net, keep, kvL / 4);
            if (ev < 1) { st->stop_reason = "ctx"; break; }
            pos -= ev;
        }
        float *logits = neo_forward(&l->net, token, pos, abspos);
        int next;
        if (abspos < n_prompt - 1) {
            next = l->prompt[abspos + 1];
            if (abspos + 1 == n_prompt - 1) st->prefill_us = story_time_us() - t0;
        } else {
            next = neo_sample(&smp, logits, V);
            if (next == l->tok.eos_id) { st->stop_reason = "eos"; break; }
            int g = st->gen_tokens;
            if (g < REP_HIST) { gen_ids[g] = next; gen_alen[g] = alen; }
            st->gen_tokens++;
            // Repetition guard: last REP_N tokens seen before in this answer?
            if (g + 1 >= 2 * REP_N && g < REP_HIST) {
                int s = g + 1 - REP_N;
                for (int j = 0; j + REP_N <= s; j++) {
                    if (!memcmp(gen_ids + j, gen_ids + s, REP_N * sizeof(int))) {
                        alen = gen_alen[s];   // drop the repeated run
                        ans[alen] = 0;
                        st->stop_reason = "repeat";
                        goto out;
                    }
                }
            }
            int plen;
            const char *piece = neo_tok_piece(&l->tok, next, &plen);
            if (pending_nl) {
                // In chat mode a newline followed by "User" is the next turn.
                if (chat && plen >= 4 && !strncmp(piece, "User", 4)) { st->stop_reason = "turn"; break; }
                if (alen > 0 && alen + 1 < cap && !(plen > 0 && piece[0] == ' ')) ans[alen++] = ' ';
                pending_nl = false;
            }
            if (plen == 1 && piece[0] == '\n') {
                pending_nl = true;
            } else if (plen > 0) {
                const char *pc = piece;
                int pl = plen;
                if (alen == 0) while (pl > 0 && *pc == ' ') { pc++; pl--; }
                if (alen + pl >= cap || alen + pl >= LLM_ANSWER_CHARS) { st->stop_reason = "hard"; break; }
                memcpy(ans + alen, pc, pl);
                alen += pl;
                ans[alen] = 0;
                if (cb && pl > 0 && !cb(user, pc, pl)) { st->stop_reason = "stopped"; break; }
            }
            if (st->gen_tokens >= p->hard_max_tokens) { st->stop_reason = "hard"; break; }
            if (st->gen_tokens >= p->soft_max_tokens && alen > 0 &&
                strchr(".!?", ans[alen - 1])) { st->stop_reason = "soft"; break; }
        }
        token = next;
        pos++;
        abspos++;
    }
out:
    st->gen_us = story_time_us() - t0 - st->prefill_us;
    // If we were cut off mid-sentence (hard/repeat), end at the last sentence.
    if (strcmp(st->stop_reason, "eos") && strcmp(st->stop_reason, "soft")) {
        size_t k = alen;
        while (k > 0 && !strchr(".!?", ans[k - 1])) k--;
        if (k > 0) alen = k;
    }
    while (alen > 0 && isspace((unsigned char)ans[alen - 1])) alen--;
    ans[alen] = 0;
    return true;
}

bool llm_answer(llm_t *l, const llm_history_t *h, const char *q, char *ans, size_t cap,
                llm_piece_cb cb, void *user, llm_stats_t *st)
{
    llm_stats_t dummy;
    if (!st) st = &dummy;
    memset(st, 0, sizeof *st);
    ans[0] = 0;
    if (!l->loaded) { SLOGE(TAG, "answer: model not loaded"); return false; }
    l->stop = false;
    int n_prompt = build_prompt(l, h, q, l->net.kv_len - 16);
    if (n_prompt <= 0) { SLOGE(TAG, "prompt did not fit"); return false; }
    return generate(l, n_prompt, true, ans, cap, cb, user, st);
}

bool llm_story(llm_t *l, const char *topic, char *ans, size_t cap, llm_piece_cb cb, void *user,
               llm_stats_t *st)
{
    llm_stats_t dummy;
    if (!st) st = &dummy;
    memset(st, 0, sizeof *st);
    ans[0] = 0;
    if (!l->loaded) { SLOGE(TAG, "story: model not loaded"); return false; }
    l->stop = false;
    // TinyStories-Instruct's native format.
    char seg[LLM_TURN_CHARS + 32];
    char t[LLM_TURN_CHARS];
    copy_trim(t, topic, sizeof t);
    snprintf(seg, sizeof seg, "Summary: %s\nStory:", t);
    int n = neo_tok_encode(&l->tok, seg, l->prompt, l->net.kv_len - 16);
    if (n <= 0) { SLOGE(TAG, "story prompt did not fit"); return false; }
    // Stories get a longer budget than chat answers.
    llm_params_t saved = l->params;
    l->params.soft_max_tokens = saved.soft_max_tokens + 24;
    l->params.hard_max_tokens = saved.hard_max_tokens + 32;
    bool ok = generate(l, n, false, ans, cap, cb, user, st);
    l->params = saved;
    return ok;
}
