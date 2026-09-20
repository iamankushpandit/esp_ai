// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Host test: text -> TinyTalk -> text, same engine code as the device.
// Usage: llm_chat <model.bin> <tok.bin> [question ...]
// With no questions, runs a fixed battery including a 4-turn session.
#include "story_llm.h"
#include "story_mem.h"
#include "check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END);
    *n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = _aligned_malloc(*n, 16);
    if (fread(b, 1, *n, f) != *n) exit(3);
    fclose(f);
    return b;
}

static bool on_piece(void *u, const char *p, int n)
{
    (void)u;
    fwrite(p, 1, n, stdout);
    fflush(stdout);
    return true;
}

static void ask(llm_t *l, llm_history_t *h, const char *q)
{
    char ans[LLM_ANSWER_CHARS + 1];
    llm_stats_t st;
    printf("You:   %s\nStory: ", q);
    CHECK(llm_answer(l, h, q, ans, sizeof ans, on_piece, NULL, &st));
    printf("\n       [prompt %d tok, gen %d tok, stop=%s, %.1f tok/s]\n", st.prompt_tokens,
           st.gen_tokens, st.stop_reason, st.gen_tokens / (st.gen_us / 1e6 + 1e-9));
    CHECK(strlen(ans) <= LLM_ANSWER_CHARS);
    llm_history_push(h, q, ans);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: llm_chat model.bin tok.bin [q...]\n"); return 2; }
    size_t mn, tn;
    uint8_t *model = slurp(argv[1], &mn), *tok = slurp(argv[2], &tn);

    // Same arena split as the device: 264 KB "internal" + 3 MB "PSRAM".
    static uint8_t fast_mem[264 * 1024] __attribute__((aligned(16)));
    uint8_t *bulk_mem = _aligned_malloc(7 << 20, 16);
    story_arena_t fast, bulk;
    story_arena_init(&fast, fast_mem, sizeof fast_mem);
    story_arena_init(&bulk, bulk_mem, 7 << 20);
    CHECK(story_arena_begin(&fast, "think"));
    CHECK(story_arena_begin(&bulk, "think"));

    llm_t l;
    llm_params_t p = llm_default_params();
    p.seed = 1234;
    llm_blobs_t b = {model, mn, tok, tn};
    CHECK(llm_load(&l, &b, &p, &fast, &bulk));
    printf("arena use: fast %u B, bulk %u B\n", (unsigned)fast.used, (unsigned)bulk.used);

    llm_history_t h;
    llm_history_clear(&h);
    if (argc > 3) {
        for (int i = 3; i < argc; i++) ask(&l, &h, argv[i]);
    } else {
        const char *single[] = {"Why is the sky blue?", "Tell me a short story about a red robot.",
                                "What sound does a cow make?", "What is your name?"};
        for (int i = 0; i < 4; i++) { llm_history_clear(&h); ask(&l, &h, single[i]); }
        printf("--- 4-turn session ---\n");
        llm_history_clear(&h);
        ask(&l, &h, "Hi! Do you like dogs?");
        ask(&l, &h, "What color is a banana?");
        ask(&l, &h, "What did I just ask you?");
        ask(&l, &h, "Thank you, goodbye.");
    }
    printf("peak arena: fast %u B, bulk %u B\n", (unsigned)fast.peak, (unsigned)bulk.peak);
    llm_unload(&l);
    printf("llm_chat OK\n");
    return 0;
}
