// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// GGUF model support: load a llama.cpp GGUF file straight from the SD card
// (copied to PSRAM) and run it. Scope, set by this hardware (8 MB PSRAM,
// no FPU-heavy SIMD path yet):
//
//   architecture   "llama" (llama2.c TinyStories family, TinyLlama-style
//                  small models, anything llama.cpp converts to "llama")
//   tensor types   F32, F16, Q8_0, Q4_0   (quantize with llama-quantize
//                  ... Q4_0 or Q8_0; K-quants are not supported yet)
//   tokenizer      "llama" (SentencePiece BPE with byte fallback)
//   prompt         GGUF key "ivy.prompt" ("...{q}..."), else detected from
//                  tokenizer.chat_template (ChatML, Llama-2 [INST], Zephyr),
//                  else plain completion of the question
//
// The whole file must fit in the PSRAM arena together with the KV cache;
// tools/gguf/shrink_vocab.py cuts a model's vocabulary to what a corpus
// uses, which is what makes e.g. stories15M fit.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "story_arena.h"
#include "story_llm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int type;                    // ggml type: 0 F32, 1 F16, 2 Q4_0, 8 Q8_0;
                                 // 100 = Q4_0 repacked planar at load (see below)
    int ne0, ne1;                // row length, rows
    const uint8_t *data;         // planar: nibble plane, rows * ne0/2 bytes, 16-aligned rows
    const uint8_t *scales;       // planar: bf16 block scales, rows * ne0/32 * 2 bytes
} gg_tensor_t;

typedef struct {
    gg_tensor_t attn_norm, wq, wk, wv, wo, ffn_norm, w_gate, w_up, w_down;
} gg_layer_t;

typedef struct {
    // config
    int dim, hidden, layers, heads, kv_heads, head_dim, kv_dim, vocab, ctx;
    float eps, rope_base;
    int bos, eos;
    bool add_bos, add_space_prefix;
    char name[48];
    // weights (pointers into the file image)
    gg_tensor_t tok_embd, output, output_norm;
    gg_layer_t *layer;
    // tokenizer
    const uint8_t **tok_str;
    uint16_t *tok_len;
    uint8_t *tok_type;           // 1 normal, 3 control, 4 user-defined, 6 byte
    float *score;
    int *sorted;                 // ids sorted by string, for lookup
    int byte_tok[256];
    int *special;                // control/user-defined ids matched literally
    int n_special;
    // prompt template: text before and after the question (may contain
    // special tokens), from ivy.prompt or the detected chat template
    char pre[96], post[96];
    // run state
    float *x, *xb, *xb2, *hb, *hb2, *q, *att, *logits;
    int8_t *xq;
    float *xqs;
    uint16_t *kc, *vc;           // [layers][ctx][kv_dim], f16
    float *rope_freq, *rope_cos, *rope_sin;   // [head_dim/2]
    void *worker;                // second-core matvec worker (device only)
    volatile bool stop;
} gguf_llm_t;

// Parses the GGUF image (which must stay alive: tensors point into it) and
// allocates run state: activations from `fast`, KV cache and tables from
// `bulk`. On failure writes a short human-readable reason to err.
bool gguf_llm_load(gguf_llm_t *m, const uint8_t *file, size_t len, int ctx_req, story_arena_t *fast,
                   story_arena_t *bulk, char *err, size_t errcap);

// Stops the second-core worker. Arena memory is released with the phase.
void gguf_llm_unload(gguf_llm_t *m);

// Bytes of PSRAM the run state needs beyond the file itself (for "too big"
// checks before loading).
size_t gguf_llm_state_bytes(const uint8_t *file, size_t len, int ctx_req);

int gguf_llm_encode(const gguf_llm_t *m, const char *text, bool bos, int *ids, int max);
float *gguf_llm_forward(gguf_llm_t *m, int token, int pos);
// Text of a generated token ("" for control tokens). prev = previous token.
const char *gguf_llm_piece(const gguf_llm_t *m, int id, int prev, int *len, char *tmp);

// Answers `question` with the model's prompt template; same streaming and
// stats contract as llm_answer().
bool gguf_llm_answer(gguf_llm_t *m, const char *question, const llm_params_t *p, char *ans, size_t cap,
                     llm_piece_cb cb, void *user, llm_stats_t *st);

#ifdef __cplusplus
}
#endif
