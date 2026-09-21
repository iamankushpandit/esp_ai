// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.
//
// Ported from therezor/cardputer-ai (MIT) -- see THIRD_PARTY.md.

// GPT-Neo Q4 inference engine (CRDP v3 model blobs, CTK2 GPT-2 BPE tokenizer).
// C port of therezor/cardputer-ai main/llm.cpp (MIT, see LICENSE.cardputer-ai),
// GPT-Neo path only, with all memory taken from caller-provided phase arenas.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "story_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int dim, hidden_dim, n_layers, n_heads, vocab_size, seq_len;
} neo_config_t;

typedef struct {
    neo_config_t c;
    int kv_len;
    // weights (point into the model blob: PSRAM copy or mmap'd flash)
    const float *ln1_g, *ln1_b, *ln2_g, *ln2_b, *lnf_g, *lnf_b;
    const float *bo, *b_fc, *b_proj, *wpe;
    const uint8_t *wte, *wq, *wk, *wv, *wo, *w1, *w2;
    size_t stride_attn, stride_w1, stride_w2;
    // run state (arena memory)
    float *x, *xb, *xb2, *hb, *q, *k, *v, *att, *logits;
    int8_t *xq;
    float *xs;
    uint8_t *kc4, *vc4;       // int4 KV [L, kv_len, dim/2]
    uint16_t *kgs, *vgs;      // bf16 group scales [L, kv_len, dim/32]
    void *worker;             // dual-core matmul worker (target only)
} neo_t;

typedef struct {
    const uint8_t *base;
    size_t size;
    int vocab_size, max_len, eos_id, n_merges;
    const uint16_t *byte_ids;
    const uint16_t *merges;    // (a,b,c) sorted by (a,b)
    const uint8_t *pieces;
    uint32_t *piece_off;       // arena-built id -> offset index (O(1) decode)
} neo_tok_t;

typedef struct {
    float temperature;   // 0 = greedy
    float top_p;         // >= 1 disables nucleus
    uint64_t rng;
} neo_sampler_t;

// Parse the header only (no allocation). Returns false on a bad blob.
bool neo_parse_config(const uint8_t *model, size_t size, neo_config_t *out);

// Bind weights and allocate run state. Hot, small buffers go to `fast`
// (internal SRAM) when they fit, otherwise to `bulk` (PSRAM); each placement
// is logged. Returns false (after logging why) on any failure.
bool neo_init(neo_t *m, const uint8_t *model, size_t size, int kv_len,
              story_arena_t *fast, story_arena_t *bulk);
void neo_deinit(neo_t *m);   // stops the worker task; arena memory is released by the phase

float *neo_forward(neo_t *m, int token, int slot, int abspos);
int neo_kv_slide(neo_t *m, int keep_head, int evict);

bool neo_tok_init(neo_tok_t *t, const uint8_t *data, size_t size, story_arena_t *arena);
// Encodes into tokens[max]; returns count, or -1 if it did not fit.
int neo_tok_encode(const neo_tok_t *t, const char *text, int *tokens, int max);
// Returns the byte string of a token (not NUL-terminated) and its length.
const char *neo_tok_piece(const neo_tok_t *t, int id, int *len);

void neo_sampler_init(neo_sampler_t *s, float temperature, float top_p, uint64_t seed);
int neo_sample(neo_sampler_t *s, float *logits, int vocab);

#ifdef __cplusplus
}
#endif
