// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// GGUF (llama.cpp) model loader and llama-architecture inference. See
// gguf_llm.h for the supported subset. Reference: tools/gguf/ref_llama.py
// (NumPy, same maths) — the host test checks this file against it.
#include "gguf_llm.h"
#include "neo.h"
#include "story_log.h"
#include "story_mem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "gguf";

enum { T_F32 = 0, T_F16 = 1, T_Q4_0 = 2, T_Q8_0 = 8, T_Q4P = 100 };
enum { V_U8, V_I8, V_U16, V_I16, V_U32, V_I32, V_F32, V_BOOL, V_STR, V_ARR, V_U64, V_I64, V_F64 };
#define QK 32

// ------------------------------------------------------------------ reader
typedef struct {
    const uint8_t *p, *end;
    bool bad;
} rd_t;

static bool need(rd_t *r, size_t n)
{
    if (r->bad || (size_t)(r->end - r->p) < n) { r->bad = true; return false; }
    return true;
}
static uint32_t u32(rd_t *r) { uint32_t v = 0; if (need(r, 4)) { memcpy(&v, r->p, 4); r->p += 4; } return v; }
static uint64_t u64(rd_t *r) { uint64_t v = 0; if (need(r, 8)) { memcpy(&v, r->p, 8); r->p += 8; } return v; }
static const uint8_t *gstr(rd_t *r, uint64_t *len)
{
    *len = u64(r);
    if (!need(r, *len)) return NULL;
    const uint8_t *s = r->p;
    r->p += *len;
    return s;
}
static const size_t VSIZE[] = {1, 1, 2, 2, 4, 4, 4, 1, 0, 0, 8, 8, 8};

static void skip_value(rd_t *r, uint32_t t)
{
    if (t == V_STR) { uint64_t n; gstr(r, &n); return; }
    if (t == V_ARR) {
        uint32_t et = u32(r);
        uint64_t n = u64(r);
        if (et == V_STR) for (uint64_t i = 0; i < n && !r->bad; i++) skip_value(r, V_STR);
        else if (et < 13 && need(r, n * VSIZE[et])) r->p += n * VSIZE[et];
        else r->bad = true;
        return;
    }
    if (t < 13 && need(r, VSIZE[t])) r->p += VSIZE[t];
    else r->bad = true;
}

static double num(rd_t *r, uint32_t t)
{
    uint8_t b[8] = {0};
    if (t >= 13 || t == V_STR || t == V_ARR || !need(r, VSIZE[t])) { r->bad = true; return 0; }
    memcpy(b, r->p, VSIZE[t]);
    r->p += VSIZE[t];
    switch (t) {
    case V_U8: case V_BOOL: return b[0];
    case V_I8: return (int8_t)b[0];
    case V_U16: { uint16_t v; memcpy(&v, b, 2); return v; }
    case V_I16: { int16_t v; memcpy(&v, b, 2); return v; }
    case V_U32: { uint32_t v; memcpy(&v, b, 4); return v; }
    case V_I32: { int32_t v; memcpy(&v, b, 4); return v; }
    case V_F32: { float v; memcpy(&v, b, 4); return v; }
    case V_U64: { uint64_t v; memcpy(&v, b, 8); return (double)v; }
    case V_I64: { int64_t v; memcpy(&v, b, 8); return (double)v; }
    case V_F64: { double v; memcpy(&v, b, 8); return v; }
    }
    return 0;
}

static bool key_is(const uint8_t *k, uint64_t kl, const char *arch, const char *suffix)
{
    char want[96];
    snprintf(want, sizeof want, "%s%s", arch ? arch : "", suffix);
    return kl == strlen(want) && !memcmp(k, want, kl);
}

// Metadata we use, gathered in one pass.
typedef struct {
    char arch[24], tok_model[16], name[48];
    char ivy_prompt[192];
    bool has_ivy_prompt;
    int chat_kind;                    // 0 none, 1 chatml, 2 llama2, 3 zephyr
    double dim, hidden, layers, heads, kv_heads, ctx, eps, rope_base, alignment;
    double bos, eos, add_bos, add_space;
    const uint8_t *tokens, *scores, *types;
    uint64_t n_tokens, n_scores, n_types;
    uint32_t scores_t, types_t;
} meta_t;

static void copy_str(char *dst, size_t cap, const uint8_t *s, uint64_t n)
{
    size_t k = n < cap - 1 ? (size_t)n : cap - 1;
    memcpy(dst, s, k);
    dst[k] = 0;
}

static bool contains(const uint8_t *s, uint64_t n, const char *needle)
{
    size_t k = strlen(needle);
    for (uint64_t i = 0; i + k <= n; i++)
        if (!memcmp(s + i, needle, k)) return true;
    return false;
}

static bool parse_meta(rd_t *r, uint64_t n_kv, meta_t *m)
{
    memset(m, 0, sizeof *m);
    m->heads = m->kv_heads = -1;
    m->eps = 1e-5;
    m->rope_base = 10000;
    m->alignment = 32;
    m->bos = 1;
    m->eos = 2;
    m->add_bos = 1;
    m->add_space = 1;
    // Pass 1 needs the arch before arch-prefixed keys, so remember where
    // metadata starts and read general.architecture first.
    const uint8_t *start = r->p;
    for (int pass = 0; pass < 2; pass++) {
        r->p = start;
        for (uint64_t i = 0; i < n_kv && !r->bad; i++) {
            uint64_t kl;
            const uint8_t *k = gstr(r, &kl);
            uint32_t t = u32(r);
            if (r->bad) return false;
            const char *a = m->arch;
            if (pass == 0) {
                if (key_is(k, kl, NULL, "general.architecture") && t == V_STR) {
                    uint64_t n; const uint8_t *s = gstr(r, &n); copy_str(m->arch, sizeof m->arch, s, n);
                } else skip_value(r, t);
                continue;
            }
            if (t == V_STR) {
                uint64_t n;
                const uint8_t *s = gstr(r, &n);
                if (key_is(k, kl, NULL, "tokenizer.ggml.model")) copy_str(m->tok_model, sizeof m->tok_model, s, n);
                else if (key_is(k, kl, NULL, "general.name")) copy_str(m->name, sizeof m->name, s, n);
                else if (key_is(k, kl, NULL, "ivy.prompt")) {
                    copy_str(m->ivy_prompt, sizeof m->ivy_prompt, s, n);
                    m->has_ivy_prompt = true;
                } else if (key_is(k, kl, NULL, "tokenizer.chat_template")) {
                    m->chat_kind = contains(s, n, "<|im_start|>") ? 1 : contains(s, n, "[INST]") ? 2
                                 : contains(s, n, "<|user|>") ? 3 : 0;
                }
            } else if (t == V_ARR) {
                const uint8_t *save = r->p;
                uint32_t et = u32(r);
                uint64_t n = u64(r);
                const uint8_t *body = r->p;
                r->p = save;
                skip_value(r, V_ARR);
                if (key_is(k, kl, NULL, "tokenizer.ggml.tokens") && et == V_STR) { m->tokens = body; m->n_tokens = n; }
                else if (key_is(k, kl, NULL, "tokenizer.ggml.scores")) { m->scores = body; m->n_scores = n; m->scores_t = et; }
                else if (key_is(k, kl, NULL, "tokenizer.ggml.token_type")) { m->types = body; m->n_types = n; m->types_t = et; }
            } else {
                double v = num(r, t);
                if (key_is(k, kl, a, ".embedding_length")) m->dim = v;
                else if (key_is(k, kl, a, ".feed_forward_length")) m->hidden = v;
                else if (key_is(k, kl, a, ".block_count")) m->layers = v;
                else if (key_is(k, kl, a, ".attention.head_count")) m->heads = v;
                else if (key_is(k, kl, a, ".attention.head_count_kv")) m->kv_heads = v;
                else if (key_is(k, kl, a, ".context_length")) m->ctx = v;
                else if (key_is(k, kl, a, ".attention.layer_norm_rms_epsilon")) m->eps = v;
                else if (key_is(k, kl, a, ".rope.freq_base")) m->rope_base = v;
                else if (key_is(k, kl, NULL, "general.alignment")) m->alignment = v;
                else if (key_is(k, kl, NULL, "tokenizer.ggml.bos_token_id")) m->bos = v;
                else if (key_is(k, kl, NULL, "tokenizer.ggml.eos_token_id")) m->eos = v;
                else if (key_is(k, kl, NULL, "tokenizer.ggml.add_bos_token")) m->add_bos = v;
                else if (key_is(k, kl, NULL, "tokenizer.ggml.add_space_prefix")) m->add_space = v;
            }
        }
    }
    if (m->kv_heads < 0) m->kv_heads = m->heads;
    return !r->bad;
}

static size_t row_bytes(int type, int ne0)
{
    switch (type) {
    case T_F32: return (size_t)ne0 * 4;
    case T_F16: return (size_t)ne0 * 2;
    case T_Q8_0: return (size_t)ne0 / QK * 34;
    case T_Q4_0: return (size_t)ne0 / QK * 18;
    }
    return 0;
}

// Walks the tensor directory; with m != NULL binds the tensors we need.
static bool parse_tensors(rd_t *r, uint64_t n_t, const meta_t *mt, const uint8_t *file, size_t len,
                          gguf_llm_t *m, char *err, size_t errcap)
{
    typedef struct { const uint8_t *name; uint64_t nl; int type, ne0, ne1; uint64_t off; } ti_t;
    const uint8_t *dir = r->p;
    for (uint64_t i = 0; i < n_t && !r->bad; i++) {           // pass 1: find the data start
        uint64_t nl;
        gstr(r, &nl);
        uint32_t nd = u32(r);
        for (uint32_t d = 0; d < nd; d++) u64(r);
        u32(r);
        u64(r);
    }
    if (r->bad) { snprintf(err, errcap, "corrupt tensor table"); return false; }
    size_t al = (size_t)mt->alignment ? (size_t)mt->alignment : 32;
    size_t data_off = ((size_t)(r->p - file) + al - 1) / al * al;
    r->p = dir;
    for (uint64_t i = 0; i < n_t; i++) {
        ti_t t = {0};
        t.name = gstr(r, &t.nl);
        uint32_t nd = u32(r);
        uint64_t dims[4] = {1, 1, 1, 1};
        for (uint32_t d = 0; d < nd; d++) { uint64_t v = u64(r); if (d < 4) dims[d] = v; }
        t.type = (int)u32(r);
        t.off = u64(r);
        t.ne0 = (int)dims[0];
        t.ne1 = (int)dims[1];
        if (!m) continue;
        gg_tensor_t *dst = NULL;
        char nm[64];
        copy_str(nm, sizeof nm, t.name, t.nl);
        int l = -1;
        char part[40];
        if (!strcmp(nm, "token_embd.weight")) dst = &m->tok_embd;
        else if (!strcmp(nm, "output.weight")) dst = &m->output;
        else if (!strcmp(nm, "output_norm.weight")) dst = &m->output_norm;
        else if (sscanf(nm, "blk.%d.%39s", &l, part) == 2 && l >= 0 && l < m->layers) {
            gg_layer_t *L = &m->layer[l];
            if (!strcmp(part, "attn_norm.weight")) dst = &L->attn_norm;
            else if (!strcmp(part, "attn_q.weight")) dst = &L->wq;
            else if (!strcmp(part, "attn_k.weight")) dst = &L->wk;
            else if (!strcmp(part, "attn_v.weight")) dst = &L->wv;
            else if (!strcmp(part, "attn_output.weight")) dst = &L->wo;
            else if (!strcmp(part, "ffn_norm.weight")) dst = &L->ffn_norm;
            else if (!strcmp(part, "ffn_gate.weight")) dst = &L->w_gate;
            else if (!strcmp(part, "ffn_up.weight")) dst = &L->w_up;
            else if (!strcmp(part, "ffn_down.weight")) dst = &L->w_down;
        }
        if (!dst) continue;
        if (t.type != T_F32 && t.type != T_F16 && t.type != T_Q4_0 && t.type != T_Q8_0) {
            snprintf(err, errcap, "%s: tensor type %d unsupported (use Q4_0/Q8_0)", nm, t.type);
            return false;
        }
        if ((t.type == T_Q4_0 || t.type == T_Q8_0) && t.ne0 % QK) {
            snprintf(err, errcap, "%s: row %d not a multiple of 32", nm, t.ne0);
            return false;
        }
        size_t need = row_bytes(t.type, t.ne0) * (size_t)t.ne1;
        if (data_off + t.off + need > len) { snprintf(err, errcap, "%s: data past end of file", nm); return false; }
        dst->type = t.type;
        dst->ne0 = t.ne0;
        dst->ne1 = t.ne1;
        dst->data = file + data_off + t.off;
    }
    return true;
}

// ------------------------------------------------------------------ math
static inline float f16_to_f32(uint16_t h)
{
    uint32_t s = (uint32_t)(h & 0x8000) << 16, e = (h >> 10) & 0x1F, f = h & 0x3FF, u;
    if (e == 0) {
        if (!f) u = s;
        else {                                   // subnormal
            e = 127 - 15 + 1;
            while (!(f & 0x400)) { f <<= 1; e--; }
            u = s | (e << 23) | ((f & 0x3FF) << 13);
        }
    } else if (e == 31) u = s | 0x7F800000 | (f << 13);
    else u = s | ((e + 127 - 15) << 23) | (f << 13);
    float out;
    memcpy(&out, &u, 4);
    return out;
}

static inline uint16_t f32_to_f16(float v)
{
    uint32_t u;
    memcpy(&u, &v, 4);
    uint32_t s = (u >> 16) & 0x8000;
    int e = (int)((u >> 23) & 0xFF) - 127 + 15;
    uint32_t f = u & 0x7FFFFF;
    if (e <= 0) return (uint16_t)s;                        // flush tiny values to 0
    if (e >= 31) return (uint16_t)(s | 0x7C00);
    uint32_t r = s | ((uint32_t)e << 10) | (f >> 13);
    if (f & 0x1000) r++;                                   // round to nearest
    return (uint16_t)r;
}

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

// Activations quantized once per matvec input (Q8_0-style blocks of 32).
static void quantize_x(const float *x, int n, int8_t *q, float *s)
{
    for (int b = 0; b < n / QK; b++) {
        float amax = 0;
        for (int i = 0; i < QK; i++) { float a = fabsf(x[b * QK + i]); if (a > amax) amax = a; }
        float d = amax / 127.0f, id = d ? 1.0f / d : 0;
        s[b] = d;
        for (int i = 0; i < QK; i++) q[b * QK + i] = (int8_t)lrintf(x[b * QK + i] * id);
    }
}

#ifndef STORY_HOST
// ESP32-S3 PIE kernel (dot_q4_pie.S): 16-aligned nibble plane + bf16 scales.
extern float dot_q4q8_pie(const uint8_t *nib, const int8_t *xq, const uint8_t *scales, const float *xs, int nb);
#endif

static inline float bf16_to_f32(const uint8_t *p)
{
    uint32_t u = (uint32_t)(p[0] | p[1] << 8) << 16;
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static float dot_row(const gg_tensor_t *w, int row, const float *x, const int8_t *xq, const float *xs)
{
    const int n = w->ne0;
    if (w->type == T_Q4P) {
        const int nb = n / QK;
        const uint8_t *nib = w->data + (size_t)row * nb * 16, *sc = w->scales + (size_t)row * nb * 2;
#ifndef STORY_HOST
        return dot_q4q8_pie(nib, xq, sc, xs, nb);
#else
        float sum = 0;
        for (int b = 0; b < nb; b++, nib += 16) {
            int32_t acc = 0;
            for (int i = 0; i < 16; i++)
                acc += ((nib[i] & 0x0F) - 8) * xq[b * QK + i] + ((nib[i] >> 4) - 8) * xq[b * QK + i + 16];
            sum += acc * bf16_to_f32(sc + 2 * b) * xs[b];
        }
        return sum;
#endif
    }
    const uint8_t *p = w->data + row_bytes(w->type, n) * (size_t)row;
    float sum = 0;
    switch (w->type) {
    case T_F32: {
        const float *f = (const float *)p;
        for (int i = 0; i < n; i++) sum += f[i] * x[i];
        break;
    }
    case T_F16:
        for (int i = 0; i < n; i++) sum += f16_to_f32(rd16(p + 2 * i)) * x[i];
        break;
    case T_Q8_0:
        for (int b = 0; b < n / QK; b++, p += 34) {
            const int8_t *qw = (const int8_t *)(p + 2);
            const int8_t *qx = xq + b * QK;
            int32_t acc = 0;
            for (int i = 0; i < QK; i++) acc += qw[i] * qx[i];
            sum += acc * f16_to_f32(rd16(p)) * xs[b];
        }
        break;
    case T_Q4_0:
        for (int b = 0; b < n / QK; b++, p += 18) {
            const uint8_t *qw = p + 2;
            const int8_t *qx = xq + b * QK;
            int32_t acc = 0;
            for (int i = 0; i < QK / 2; i++) {
                acc += ((qw[i] & 0x0F) - 8) * qx[i];
                acc += ((qw[i] >> 4) - 8) * qx[i + QK / 2];
            }
            sum += acc * f16_to_f32(rd16(p)) * xs[b];
        }
        break;
    }
    return sum;
}

typedef struct {
    const gg_tensor_t *w;
    const float *x;
    const int8_t *xq;
    const float *xs;
    float *out;
    int r0, r1;
} mv_args_t;

static void mv_rows(const mv_args_t *a)
{
    for (int r = a->r0; r < a->r1; r++) a->out[r] = dot_row(a->w, r, a->x, a->xq, a->xs);
}

#ifndef STORY_HOST
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
// Second-core worker: each matvec gives it the first half of the rows.
typedef struct {
    SemaphoreHandle_t go, done;
    volatile bool quit;
    mv_args_t args;
} mv_worker_t;

static void mv_worker_fn(void *p)
{
    mv_worker_t *w = (mv_worker_t *)p;
    for (;;) {
        xSemaphoreTake(w->go, portMAX_DELAY);
        if (w->quit) break;
        mv_rows(&w->args);
        xSemaphoreGive(w->done);
    }
    xSemaphoreGive(w->done);
    vTaskDelete(NULL);
}

static void *worker_start(story_arena_t *a)
{
    mv_worker_t *w = story_arena_alloc(a, sizeof *w, 4, "gguf.worker");
    if (!w) return NULL;
    memset(w, 0, sizeof *w);
    w->go = xSemaphoreCreateBinary();
    w->done = xSemaphoreCreateBinary();
    int other = xPortGetCoreID() == 0 ? 1 : 0;
    if (xTaskCreatePinnedToCore(mv_worker_fn, "gguf_mv", 3072, w, uxTaskPriorityGet(NULL), NULL, other) != pdPASS)
        return NULL;
    return w;
}

void gguf_llm_unload(gguf_llm_t *m)
{
    mv_worker_t *w = (mv_worker_t *)m->worker;
    if (!w) return;
    w->quit = true;
    xSemaphoreGive(w->go);
    xSemaphoreTake(w->done, portMAX_DELAY);
    vSemaphoreDelete(w->go);
    vSemaphoreDelete(w->done);
    m->worker = NULL;
}
#else
static void *worker_start(story_arena_t *a) { (void)a; return NULL; }
void gguf_llm_unload(gguf_llm_t *m) { m->worker = NULL; }
#endif

static void matvec(gguf_llm_t *m, const gg_tensor_t *w, const float *x, float *out)
{
    bool q = w->type == T_Q4_0 || w->type == T_Q8_0 || w->type == T_Q4P;
    if (q) quantize_x(x, w->ne0, m->xq, m->xqs);
#ifndef STORY_HOST
    mv_worker_t *wk = (mv_worker_t *)m->worker;
    if (wk && w->ne1 >= 64) {
        int split = w->ne1 / 2;
        wk->args = (mv_args_t){w, x, m->xq, m->xqs, out, 0, split};
        xSemaphoreGive(wk->go);
        mv_args_t self = {w, x, m->xq, m->xqs, out, split, w->ne1};
        mv_rows(&self);
        xSemaphoreTake(wk->done, portMAX_DELAY);
        return;
    }
#endif
    mv_args_t all = {w, x, m->xq, m->xqs, out, 0, w->ne1};
    mv_rows(&all);
}

// Q4_0 -> planar, in place: nibbles compacted to the front of the tensor
// (16 B per block, so rows stay 16-aligned for the PIE kernel), then the
// block scales (as bf16) moved into the freed tail. `tmp` holds one
// tensor's scales meanwhile.
static void repack_q4(gg_tensor_t *t, uint8_t *tmp)
{
    uint8_t *base = (uint8_t *)t->data;
    const size_t nblk = (size_t)t->ne0 / QK * t->ne1;
    for (size_t b = 0; b < nblk; b++) {
        const uint8_t *src = base + b * 18;
        float d = f16_to_f32(rd16(src));
        uint32_t u;
        memcpy(&u, &d, 4);
        u += 0x7FFF + ((u >> 16) & 1);                  // round to nearest even bf16
        tmp[2 * b] = (uint8_t)(u >> 16);
        tmp[2 * b + 1] = (uint8_t)(u >> 24);
        memmove(base + b * 16, src + 2, 16);             // write pos <= read pos: safe
    }
    memcpy(base + nblk * 16, tmp, nblk * 2);
    t->scales = base + nblk * 16;
    t->type = T_Q4P;
}

static void embed_row(const gg_tensor_t *w, int row, float *out)
{
    const int n = w->ne0;
    if (w->type == T_Q4P) {
        const int nb = n / QK;
        const uint8_t *nib = w->data + (size_t)row * nb * 16, *sc = w->scales + (size_t)row * nb * 2;
        for (int b = 0; b < nb; b++, nib += 16) {
            float d = bf16_to_f32(sc + 2 * b);
            for (int i = 0; i < 16; i++) {
                out[b * QK + i] = d * ((nib[i] & 0x0F) - 8);
                out[b * QK + i + 16] = d * ((nib[i] >> 4) - 8);
            }
        }
        return;
    }
    const uint8_t *p = w->data + row_bytes(w->type, n) * (size_t)row;
    switch (w->type) {
    case T_F32: memcpy(out, p, (size_t)n * 4); break;
    case T_F16: for (int i = 0; i < n; i++) out[i] = f16_to_f32(rd16(p + 2 * i)); break;
    case T_Q8_0:
        for (int b = 0; b < n / QK; b++, p += 34) {
            float d = f16_to_f32(rd16(p));
            for (int i = 0; i < QK; i++) out[b * QK + i] = d * (int8_t)p[2 + i];
        }
        break;
    case T_Q4_0:
        for (int b = 0; b < n / QK; b++, p += 18) {
            float d = f16_to_f32(rd16(p));
            for (int i = 0; i < QK / 2; i++) {
                out[b * QK + i] = d * ((p[2 + i] & 0x0F) - 8);
                out[b * QK + i + QK / 2] = d * ((p[2 + i] >> 4) - 8);
            }
        }
        break;
    }
}

static void rmsnorm(float *o, const float *x, const gg_tensor_t *w, int n, float eps)
{
    float ss = 0;
    for (int i = 0; i < n; i++) ss += x[i] * x[i];
    ss = 1.0f / sqrtf(ss / n + eps);
    const float *g = (const float *)w->data;           // norms are F32 in every GGUF we accept
    for (int i = 0; i < n; i++) o[i] = x[i] * ss * g[i];
}

// cos/sin for this position are computed once per token (rope_prepare).
static void rope_prepare(gguf_llm_t *m, int pos)
{
    for (int i = 0; i < m->head_dim / 2; i++) {
        float f = pos * m->rope_freq[i];
        m->rope_cos[i] = cosf(f);
        m->rope_sin[i] = sinf(f);
    }
}

static void rope(const gguf_llm_t *m, float *v, int heads)
{
    const int hd = m->head_dim;
    for (int h = 0; h < heads; h++)
        for (int i = 0; i < hd; i += 2) {
            float c = m->rope_cos[i / 2], s = m->rope_sin[i / 2];
            float *p = v + h * hd + i, a = p[0], b = p[1];
            p[0] = a * c - b * s;
            p[1] = a * s + b * c;
        }
}

float *gguf_llm_forward(gguf_llm_t *m, int token, int pos)
{
    const int d = m->dim, hd = m->head_dim, kvd = m->kv_dim, H = m->heads, grp = m->heads / m->kv_heads;
    embed_row(&m->tok_embd, token, m->x);
    rope_prepare(m, pos);
    for (int l = 0; l < m->layers; l++) {
        gg_layer_t *L = &m->layer[l];
        rmsnorm(m->xb, m->x, &L->attn_norm, d, m->eps);
        matvec(m, &L->wq, m->xb, m->q);
        matvec(m, &L->wk, m->xb, m->hb);                   // k (kv_dim) into scratch
        matvec(m, &L->wv, m->xb, m->hb2);                  // v
        rope(m, m->q, H);
        rope(m, m->hb, m->kv_heads);
        uint16_t *kc = m->kc + ((size_t)l * m->ctx + pos) * kvd, *vc = m->vc + ((size_t)l * m->ctx + pos) * kvd;
        for (int i = 0; i < kvd; i++) { kc[i] = f32_to_f16(m->hb[i]); vc[i] = f32_to_f16(m->hb2[i]); }
        const float scale = 1.0f / sqrtf((float)hd);
        for (int h = 0; h < H; h++) {
            const float *q = m->q + h * hd;
            const int kh = h / grp;
            float mx = -1e30f;
            for (int t = 0; t <= pos; t++) {
                const uint16_t *k = m->kc + ((size_t)l * m->ctx + t) * kvd + kh * hd;
                float s = 0;
                for (int i = 0; i < hd; i++) s += q[i] * f16_to_f32(k[i]);
                s *= scale;
                m->att[t] = s;
                if (s > mx) mx = s;
            }
            float sum = 0;
            for (int t = 0; t <= pos; t++) { m->att[t] = expf(m->att[t] - mx); sum += m->att[t]; }
            float *o = m->xb2 + h * hd;
            memset(o, 0, (size_t)hd * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                const uint16_t *v = m->vc + ((size_t)l * m->ctx + t) * kvd + kh * hd;
                float a = m->att[t] / sum;
                for (int i = 0; i < hd; i++) o[i] += a * f16_to_f32(v[i]);
            }
        }
        matvec(m, &L->wo, m->xb2, m->xb);
        for (int i = 0; i < d; i++) m->x[i] += m->xb[i];
        rmsnorm(m->xb, m->x, &L->ffn_norm, d, m->eps);
        matvec(m, &L->w_gate, m->xb, m->hb);
        matvec(m, &L->w_up, m->xb, m->hb2);
        for (int i = 0; i < m->hidden; i++) {
            float g = m->hb[i];
            m->hb[i] = g / (1.0f + expf(-g)) * m->hb2[i];     // SwiGLU
        }
        matvec(m, &L->w_down, m->hb, m->xb);
        for (int i = 0; i < d; i++) m->x[i] += m->xb[i];
    }
    rmsnorm(m->x, m->x, &m->output_norm, d, m->eps);
    matvec(m, m->output.data ? &m->output : &m->tok_embd, m->x, m->logits);
    return m->logits;
}

// ------------------------------------------------------------------ tokenizer
static const gguf_llm_t *s_sort_m;
static int cmp_tok(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    int lx = s_sort_m->tok_len[x], ly = s_sort_m->tok_len[y];
    int c = memcmp(s_sort_m->tok_str[x], s_sort_m->tok_str[y], (size_t)(lx < ly ? lx : ly));
    return c ? c : lx - ly;
}

static int lookup(const gguf_llm_t *m, const char *s, int n)
{
    int lo = 0, hi = m->vocab - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, id = m->sorted[mid], l = m->tok_len[id];
        int c = memcmp(m->tok_str[id], s, (size_t)(l < n ? l : n));
        if (!c) c = l - n;
        if (!c) return id;
        if (c < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

// Plain-text segment (no special tokens) -> SentencePiece BPE ids.
static int encode_plain(const gguf_llm_t *m, const char *text, int n, bool space_prefix, int *ids, int max)
{
    int k = 0;
    char ch[8];
    // Characters, with ' ' -> U+2581 and an optional leading dummy space.
    for (int i = space_prefix ? -1 : 0; i < n && k < max;) {
        int cl;
        if (i < 0 || text[i] == ' ') { memcpy(ch, "\xE2\x96\x81", 3); cl = 3; i++; }
        else {
            unsigned char c = (unsigned char)text[i];
            cl = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            if (i + cl > n) cl = n - i;
            memcpy(ch, text + i, (size_t)cl);
            i += cl;
        }
        int id = lookup(m, ch, cl);
        if (id >= 0) ids[k++] = id;
        else
            for (int b = 0; b < cl && k < max; b++) {
                int bt = m->byte_tok[(unsigned char)ch[b]];
                if (bt >= 0) ids[k++] = bt;
            }
    }
    // Greedy merges: best-scoring adjacent pair first.
    char buf[128];
    for (;;) {
        float best = -1e30f;
        int bi = -1, bid = -1;
        for (int i = 0; i + 1 < k; i++) {
            int la = m->tok_len[ids[i]], lb = m->tok_len[ids[i + 1]];
            if (la + lb > (int)sizeof buf) continue;
            memcpy(buf, m->tok_str[ids[i]], (size_t)la);
            memcpy(buf + la, m->tok_str[ids[i + 1]], (size_t)lb);
            int id = lookup(m, buf, la + lb);
            if (id >= 0 && m->score[id] > best) { best = m->score[id]; bi = i; bid = id; }
        }
        if (bi < 0) break;
        ids[bi] = bid;
        memmove(ids + bi + 1, ids + bi + 2, (size_t)(k - bi - 2) * sizeof(int));
        k--;
    }
    return k;
}

int gguf_llm_encode(const gguf_llm_t *m, const char *text, bool bos, int *ids, int max)
{
    int k = 0;
    if (bos && m->add_bos && max > 0) ids[k++] = m->bos;
    bool first = true;
    const char *p = text, *seg = text;
    // Special tokens (control / user-defined) are matched literally.
    while (*p) {
        int hit = -1, hl = 0;
        for (int s = 0; s < m->n_special; s++) {
            int id = m->special[s], l = m->tok_len[id];
            if (l > hl && !strncmp(p, (const char *)m->tok_str[id], (size_t)l)) { hit = id; hl = l; }
        }
        if (hit < 0) { p++; continue; }
        if (p > seg) { k += encode_plain(m, seg, (int)(p - seg), first && m->add_space_prefix, ids + k, max - k); first = false; }
        if (k < max) ids[k++] = hit;
        p += hl;
        seg = p;
        first = false;
    }
    if (p > seg) k += encode_plain(m, seg, (int)(p - seg), first && m->add_space_prefix, ids + k, max - k);
    return k;
}

const char *gguf_llm_piece(const gguf_llm_t *m, int id, int prev, int *len, char *tmp)
{
    *len = 0;
    if (id < 0 || id >= m->vocab) return "";
    int t = m->tok_type ? m->tok_type[id] : 1;
    if (t == 3) return "";                                  // control
    if (t == 6) {                                           // <0xXX>
        const char *s = (const char *)m->tok_str[id];
        tmp[0] = (char)strtol(s + 3, NULL, 16);
        *len = 1;
        return tmp;
    }
    int n = 0, l = m->tok_len[id];
    const uint8_t *s = m->tok_str[id];
    for (int i = 0; i < l && n < 60;) {
        if (i + 2 < l && s[i] == 0xE2 && s[i + 1] == 0x96 && s[i + 2] == 0x81) { tmp[n++] = ' '; i += 3; }
        else tmp[n++] = (char)s[i++];
    }
    if (prev == m->bos && n && tmp[0] == ' ') { memmove(tmp, tmp + 1, (size_t)--n); }
    *len = n;
    return tmp;
}

// ------------------------------------------------------------------ load
static bool read_header(const uint8_t *file, size_t len, rd_t *r, uint64_t *n_t, uint64_t *n_kv)
{
    r->p = file;
    r->end = file + len;
    r->bad = false;
    if (len < 24 || memcmp(file, "GGUF", 4)) return false;
    r->p += 4;
    uint32_t ver = u32(r);
    if (ver < 2) return false;
    *n_t = u64(r);
    *n_kv = u64(r);
    return !r->bad;
}

size_t gguf_llm_state_bytes(const uint8_t *file, size_t len, int ctx_req)
{
    rd_t r;
    uint64_t nt, nkv;
    meta_t mt;
    if (!read_header(file, len, &r, &nt, &nkv) || !parse_meta(&r, nkv, &mt) || mt.heads <= 0) return 0;
    int ctx = mt.ctx > 0 && mt.ctx < ctx_req ? (int)mt.ctx : ctx_req;
    size_t kvd = (size_t)mt.dim / (size_t)mt.heads * (size_t)mt.kv_heads;
    return (size_t)mt.layers * ctx * kvd * 2 * 2 + (size_t)mt.n_tokens * 24 + (size_t)mt.n_tokens * 4 + 65536;
}

bool gguf_llm_load(gguf_llm_t *m, const uint8_t *file, size_t len, int ctx_req, story_arena_t *fast,
                   story_arena_t *bulk, char *err, size_t errcap)
{
    memset(m, 0, sizeof *m);
    rd_t r;
    uint64_t nt, nkv;
    meta_t mt;
    if (!read_header(file, len, &r, &nt, &nkv)) { snprintf(err, errcap, "not a GGUF file"); return false; }
    if (!parse_meta(&r, nkv, &mt)) { snprintf(err, errcap, "corrupt GGUF metadata"); return false; }
    if (strcmp(mt.arch, "llama")) { snprintf(err, errcap, "architecture '%s' not supported (llama only)", mt.arch); return false; }
    if (strcmp(mt.tok_model, "llama")) { snprintf(err, errcap, "tokenizer '%s' not supported", mt.tok_model); return false; }
    if (mt.dim <= 0 || mt.heads <= 0 || mt.layers <= 0 || !mt.tokens || !mt.scores) {
        snprintf(err, errcap, "missing llama metadata");
        return false;
    }
    m->dim = (int)mt.dim;
    m->hidden = (int)mt.hidden;
    m->layers = (int)mt.layers;
    m->heads = (int)mt.heads;
    m->kv_heads = (int)mt.kv_heads;
    m->head_dim = m->dim / m->heads;
    m->kv_dim = m->head_dim * m->kv_heads;
    m->vocab = (int)mt.n_tokens;
    m->ctx = mt.ctx > 0 && mt.ctx < ctx_req ? (int)mt.ctx : ctx_req;
    m->eps = (float)mt.eps;
    m->rope_base = (float)mt.rope_base;
    m->bos = (int)mt.bos;
    m->eos = (int)mt.eos;
    m->add_bos = mt.add_bos != 0;
    m->add_space_prefix = mt.add_space != 0;
    snprintf(m->name, sizeof m->name, "%s", mt.name[0] ? mt.name : "GGUF model");

#define ALLOC(ar, n, tag) story_arena_alloc((ar), (n), 16, (tag))
    m->layer = ALLOC(bulk, sizeof(gg_layer_t) * m->layers, "gguf.layers");
    if (!m->layer) { snprintf(err, errcap, "out of memory (layers)"); return false; }
    memset(m->layer, 0, sizeof(gg_layer_t) * m->layers);
    if (!parse_tensors(&r, nt, &mt, file, len, m, err, errcap)) return false;
    if (!m->tok_embd.data || !m->output_norm.data || m->tok_embd.ne1 != m->vocab) {
        snprintf(err, errcap, "missing embedding/norm tensors");
        return false;
    }
    for (int l = 0; l < m->layers; l++) {
        gg_layer_t *L = &m->layer[l];
        if (!L->attn_norm.data || !L->wq.data || !L->wk.data || !L->wv.data || !L->wo.data || !L->ffn_norm.data ||
            !L->w_gate.data || !L->w_up.data || !L->w_down.data) {
            snprintf(err, errcap, "layer %d incomplete", l);
            return false;
        }
    }

    // Tokenizer tables.
    int V = m->vocab;
    m->tok_str = ALLOC(bulk, sizeof(*m->tok_str) * V, "gguf.tok_str");
    m->tok_len = ALLOC(bulk, sizeof(*m->tok_len) * V, "gguf.tok_len");
    m->tok_type = ALLOC(bulk, (size_t)V, "gguf.tok_type");
    m->score = ALLOC(bulk, sizeof(float) * V, "gguf.score");
    m->sorted = ALLOC(bulk, sizeof(int) * V, "gguf.sorted");
    if (!m->tok_str || !m->tok_len || !m->tok_type || !m->score || !m->sorted) {
        snprintf(err, errcap, "out of memory (tokenizer)");
        return false;
    }
    rd_t tr = {mt.tokens, file + len, false};
    for (int i = 0; i < 256; i++) m->byte_tok[i] = -1;
    int n_special = 0;
    for (int i = 0; i < V; i++) {
        uint64_t l;
        m->tok_str[i] = gstr(&tr, &l);
        m->tok_len[i] = (uint16_t)l;
        float sc = 0;
        if (mt.scores && (uint64_t)i < mt.n_scores) memcpy(&sc, mt.scores + 4 * (size_t)i, 4);
        m->score[i] = sc;
        int ty = 1;
        if (mt.types && (uint64_t)i < mt.n_types) {
            int32_t v;
            memcpy(&v, mt.types + 4 * (size_t)i, 4);
            ty = v;
        }
        m->tok_type[i] = (uint8_t)ty;
        if (ty == 6 && l == 6) m->byte_tok[strtol((const char *)m->tok_str[i] + 3, NULL, 16) & 0xFF] = i;
        if ((ty == 3 || ty == 4) && l > 1) n_special++;
        m->sorted[i] = i;
    }
    if (tr.bad) { snprintf(err, errcap, "corrupt token list"); return false; }
    s_sort_m = m;
    qsort(m->sorted, (size_t)V, sizeof(int), cmp_tok);
    m->special = ALLOC(bulk, sizeof(int) * (n_special + 1), "gguf.special");
    for (int i = 0; i < V && m->special; i++)
        if ((m->tok_type[i] == 3 || m->tok_type[i] == 4) && m->tok_len[i] > 1) m->special[m->n_special++] = i;

    // Prompt template.
    const char *tpl = mt.has_ivy_prompt ? mt.ivy_prompt
                    : mt.chat_kind == 1 ? "<|im_start|>user\n{q}<|im_end|>\n<|im_start|>assistant\n"
                    : mt.chat_kind == 2 ? "[INST] {q} [/INST]"
                    : mt.chat_kind == 3 ? "<|user|>\n{q}</s>\n<|assistant|>\n"
                    : "{q}";
    const char *q = strstr(tpl, "{q}");
    if (!q) q = tpl + strlen(tpl);
    snprintf(m->pre, sizeof m->pre, "%.*s", (int)(q - tpl), tpl);
    snprintf(m->post, sizeof m->post, "%s", *q ? q + 3 : "");

    // Repack Q4_0 weights to the planar layout the SIMD kernel reads. The
    // file image is in RAM and ours, so this happens in place.
    {
        size_t max_blk = 0;
        gg_tensor_t *all[4 + 9 * 64];
        int na = 0;
        all[na++] = &m->tok_embd;
        all[na++] = &m->output;
        for (int l = 0; l < m->layers && l < 64; l++) {
            gg_layer_t *L = &m->layer[l];
            gg_tensor_t *lt[9] = {&L->wq, &L->wk, &L->wv, &L->wo, &L->w_gate, &L->w_up, &L->w_down,
                                  &L->attn_norm, &L->ffn_norm};
            for (int i = 0; i < 9; i++) all[na++] = lt[i];
        }
        for (int i = 0; i < na; i++)
            if (all[i]->data && all[i]->type == T_Q4_0) {
                size_t nb = (size_t)all[i]->ne0 / QK * all[i]->ne1;
                if (nb > max_blk) max_blk = nb;
            }
        if (max_blk) {
            size_t mark = story_arena_mark(bulk);
            uint8_t *tmp = ALLOC(bulk, max_blk * 2, "gguf.repack");
            if (!tmp) { snprintf(err, errcap, "out of memory (repack %u KB)", (unsigned)(max_blk * 2 / 1024)); return false; }
            int64_t t0 = story_time_us();
            for (int i = 0; i < na; i++)
                if (all[i]->data && all[i]->type == T_Q4_0) repack_q4(all[i], tmp);
            story_arena_rewind(bulk, mark);
            SLOGI(TAG, "repacked Q4_0 weights to planar in %lld ms", (story_time_us() - t0) / 1000);
        }
    }

    // RoPE frequencies (per pair), cos/sin filled per token.
    m->rope_freq = ALLOC(fast, sizeof(float) * (m->head_dim / 2), "gguf.rope");
    m->rope_cos = ALLOC(fast, sizeof(float) * (m->head_dim / 2), "gguf.cos");
    m->rope_sin = ALLOC(fast, sizeof(float) * (m->head_dim / 2), "gguf.sin");
    if (!m->rope_freq || !m->rope_cos || !m->rope_sin) { snprintf(err, errcap, "out of memory (rope)"); return false; }
    for (int i = 0; i < m->head_dim / 2; i++) m->rope_freq[i] = 1.0f / powf(m->rope_base, 2.0f * i / m->head_dim);

    // Run state: activations in fast (internal) RAM, KV cache in PSRAM.
    int big = m->hidden > m->dim ? m->hidden : m->dim;
    m->x = ALLOC(fast, sizeof(float) * m->dim, "gguf.x");
    m->xb = ALLOC(fast, sizeof(float) * m->dim, "gguf.xb");
    m->xb2 = ALLOC(fast, sizeof(float) * m->dim, "gguf.xb2");
    m->q = ALLOC(fast, sizeof(float) * m->dim, "gguf.q");
    m->hb = ALLOC(fast, sizeof(float) * big, "gguf.hb");
    m->hb2 = ALLOC(fast, sizeof(float) * big, "gguf.hb2");
    m->att = ALLOC(fast, sizeof(float) * m->ctx, "gguf.att");
    m->xq = ALLOC(fast, (size_t)big, "gguf.xq");
    m->xqs = ALLOC(fast, sizeof(float) * (big / QK + 1), "gguf.xqs");
    m->logits = ALLOC(bulk, sizeof(float) * V, "gguf.logits");
    size_t kvn = (size_t)m->layers * m->ctx * m->kv_dim;
    m->kc = ALLOC(bulk, kvn * 2, "gguf.kcache");
    m->vc = ALLOC(bulk, kvn * 2, "gguf.vcache");
    if (!m->x || !m->xb || !m->xb2 || !m->q || !m->hb || !m->hb2 || !m->att || !m->xq || !m->xqs || !m->logits ||
        !m->kc || !m->vc) {
        snprintf(err, errcap, "out of memory (KV cache %u KB)", (unsigned)(kvn * 4 / 1024));
        return false;
    }
    m->worker = worker_start(fast);         // NULL on the host: single-threaded
    SLOGI(TAG, "%s: llama dim %d ffn %d layers %d heads %d/%d vocab %d ctx %d, template \"%s{q}%s\"", m->name,
          m->dim, m->hidden, m->layers, m->heads, m->kv_heads, m->vocab, m->ctx, m->pre, m->post);
    return true;
}

// ------------------------------------------------------------------ generate
bool gguf_llm_answer(gguf_llm_t *m, const char *question, const llm_params_t *p, char *ans, size_t cap,
                     llm_piece_cb cb, void *user, llm_stats_t *st)
{
    llm_stats_t dummy;
    if (!st) st = &dummy;
    memset(st, 0, sizeof *st);
    ans[0] = 0;
    m->stop = false;
    char prompt[LLM_TURN_CHARS + 200];
    snprintf(prompt, sizeof prompt, "%s%s%s", m->pre, question, m->post);
    int ids[256];
    int n = gguf_llm_encode(m, prompt, true, ids, (int)(sizeof ids / sizeof ids[0]));
    if (n <= 0 || n >= m->ctx - 8) { SLOGE(TAG, "prompt too long (%d tokens, ctx %d)", n, m->ctx); return false; }
    st->prompt_tokens = n;
    neo_sampler_t smp;
    neo_sampler_init(&smp, p->temperature, p->top_p, p->seed ? p->seed : (uint64_t)story_time_us());
    int64_t t0 = story_time_us();
    size_t alen = 0;
    int tok = ids[0], prev = -1;
    st->stop_reason = "hard";
    char tmp[64];
    // Repetition guard (as in story_llm.c): stop and trim when the last
    // REP_N generated tokens already occurred in this answer.
    enum { REP_N = 6, REP_HIST = 96 };
    int gen_ids[REP_HIST];
    size_t gen_alen[REP_HIST];
    for (int pos = 0; pos < m->ctx - 1; pos++) {
        if (m->stop) { st->stop_reason = "stopped"; break; }
        float *logits = gguf_llm_forward(m, tok, pos);
        int next;
        if (pos < n - 1) {
            next = ids[pos + 1];
            if (pos + 1 == n - 1) st->prefill_us = story_time_us() - t0;
        } else {
            next = neo_sample(&smp, logits, m->vocab);
            if (next == m->eos || (m->tok_type && m->tok_type[next] == 3)) { st->stop_reason = "eos"; break; }
            int g = st->gen_tokens++;
            if (g < REP_HIST) { gen_ids[g] = next; gen_alen[g] = alen; }
            if (g + 1 >= 2 * REP_N && g < REP_HIST) {
                int s0 = g + 1 - REP_N;
                bool looped = false;
                for (int j = 0; j + REP_N <= s0 && !looped; j++)
                    looped = !memcmp(gen_ids + j, gen_ids + s0, REP_N * sizeof(int));
                if (looped) {
                    alen = gen_alen[s0];
                    ans[alen] = 0;
                    st->stop_reason = "repeat";
                    break;
                }
            }
            int pl;
            const char *pc = gguf_llm_piece(m, next, tok, &pl, tmp);
            // No leading blank; a plain-completion model continues the question,
            // so also drop the "," it usually starts with.
            bool plain = !m->pre[0] && !m->post[0];
            if (alen == 0)
                while (pl > 0 && (*pc == ' ' || *pc == '\n' || (plain && strchr(",;:", *pc)))) { pc++; pl--; }
            if (pl > 0 && alen + (size_t)pl < cap) {
                memcpy(ans + alen, pc, (size_t)pl);
                alen += (size_t)pl;
                ans[alen] = 0;
                if (cb && !cb(user, pc, pl)) { st->stop_reason = "stopped"; break; }
            }
            bool sentence = alen && strchr(".!?", ans[alen - 1]);
            if (strstr(ans, "\n\n")) { st->stop_reason = "turn"; break; }
            if (st->gen_tokens >= p->soft_max_tokens && sentence) { st->stop_reason = "soft"; break; }
            if (st->gen_tokens >= p->hard_max_tokens || alen + 8 >= cap) { st->stop_reason = "hard"; break; }
        }
        prev = tok;
        tok = next;
    }
    (void)prev;
    st->gen_us = story_time_us() - t0 - st->prefill_us;
    // Trim trailing whitespace / partial "\n\n".
    while (alen && (ans[alen - 1] == ' ' || ans[alen - 1] == '\n')) ans[--alen] = 0;
    return alen > 0;
}
