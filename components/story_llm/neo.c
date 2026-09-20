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

// GPT-Neo Q4 engine. Ported from therezor/cardputer-ai main/llm.cpp (MIT).
// Changes from upstream: C, GPT-Neo/CRDP v3 only, no global state except the
// matmul worker, arena allocation, O(1) token decode via an id->offset index,
// explicit worker start/stop per phase.
#include "neo.h"
#include "story_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "neo";

#define BLOCK 32
#define TOK_MAGIC 0x324B5443u  // "CTK2"

static inline size_t row_stride(int n)
{
    size_t sb = 2 * (size_t)(n >> 5);
    return sb + ((0 - sb) & 15) + 16 * (size_t)(n >> 5);
}
static inline size_t nib_off(int n)
{
    size_t sb = 2 * (size_t)(n >> 5);
    return sb + ((0 - sb) & 15);
}
static inline float bf16_f32(uint16_t b)
{
    union { uint32_t u; float f; } v = {(uint32_t)b << 16};
    return v.f;
}
static inline uint16_t f32_bf16(float f)
{
    union { float f; uint32_t u; } v = {f};
    return (uint16_t)((v.u + 0x7FFF + ((v.u >> 16) & 1)) >> 16);
}

// ---------------------------------------------------------------- kernels
static void quantize_q8(const float *x, int8_t *xq, float *xs, int n)
{
    for (int b = 0; b < n / BLOCK; b++) {
        float mx = 0;
        for (int i = 0; i < BLOCK; i++) { float a = fabsf(x[i]); if (a > mx) mx = a; }
        float s = mx / 127.0f, inv = s > 0 ? 1.0f / s : 0;
        xs[b] = s;
        for (int i = 0; i < BLOCK; i++) xq[i] = (int8_t)lrintf(x[i] * inv);
        x += BLOCK;
        xq += BLOCK;
    }
}

static inline float dot_q4q8(const uint8_t *wrow, const int8_t *xq, const float *xs, int n)
{
    int nb = n >> 5;
    const uint8_t *sc = wrow, *nib = wrow + nib_off(n);
    float acc = 0;
    for (int b = 0; b < nb; b++) {
        int isum = 0;
        for (int k = 0; k < 16; k++) {
            uint8_t bk = nib[k];
            isum += ((int)(bk & 0x0F) - 8) * (int)xq[k];
            isum += ((int)(bk >> 4) - 8) * (int)xq[k + 16];
        }
        uint16_t s;
        memcpy(&s, sc + 2 * b, 2);
        acc += bf16_f32(s) * xs[b] * (float)isum;
        nib += 16;
        xq += BLOCK;
    }
    return acc;
}

static void dequant_row(float *out, const uint8_t *wrow, int n)
{
    const uint8_t *sc = wrow, *nib = wrow + nib_off(n);
    for (int b = 0; b < n / BLOCK; b++) {
        uint16_t s;
        memcpy(&s, sc + 2 * b, 2);
        float scale = bf16_f32(s);
        for (int k = 0; k < 16; k++) {
            out[k] = scale * (float)((int)(nib[k] & 0x0F) - 8);
            out[k + 16] = scale * (float)((int)(nib[k] >> 4) - 8);
        }
        out += BLOCK;
        nib += 16;
    }
}

#if defined(__XTENSA__) && !defined(LLM_FORCE_SCALAR)
#define USE_PIE 1
float dot_q4q8_pie(const uint8_t *nib, const int8_t *xq, const uint8_t *scales, const float *xs, int nb);
#endif

typedef struct {
    float *xout;
    const int8_t *xq;
    const float *xs;
    const uint8_t *w;
    int n, r0, r1;
} mm_args_t;

static void mm_rows(const mm_args_t *a)
{
    size_t rb = row_stride(a->n);
#ifdef USE_PIE
    size_t no = nib_off(a->n);
    int nb = a->n >> 5;
    for (int r = a->r0; r < a->r1; r++) {
        const uint8_t *w = a->w + r * rb;
        a->xout[r] = dot_q4q8_pie(w + no, a->xq, w, a->xs, nb);
    }
#else
    for (int r = a->r0; r < a->r1; r++) a->xout[r] = dot_q4q8(a->w + r * rb, a->xq, a->xs, a->n);
#endif
}

// ---------------------------------------------------------------- worker
#ifndef STORY_HOST
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

typedef struct {
    SemaphoreHandle_t go, done;
    TaskHandle_t task;
    volatile bool quit;
    mm_args_t args;
} worker_t;

static void worker_fn(void *p)
{
    worker_t *w = (worker_t *)p;
    for (;;) {
        xSemaphoreTake(w->go, portMAX_DELAY);
        if (w->quit) break;
        mm_rows(&w->args);
        xSemaphoreGive(w->done);
    }
    xSemaphoreGive(w->done);
    vTaskDelete(NULL);
}

static void *worker_start(story_arena_t *a)
{
    worker_t *w = story_arena_calloc(a, sizeof(worker_t), 4, "neo.worker");
    if (!w) return NULL;
    w->go = xSemaphoreCreateBinary();
    w->done = xSemaphoreCreateBinary();
    int other = xPortGetCoreID() == 0 ? 1 : 0;
    if (xTaskCreatePinnedToCore(worker_fn, "neo_mm", 3072, w, uxTaskPriorityGet(NULL), &w->task, other) != pdPASS) {
        SLOGE(TAG, "worker task create failed");
        return NULL;
    }
    return w;
}

static void worker_stop(void *p)
{
    worker_t *w = (worker_t *)p;
    if (!w) return;
    w->quit = true;
    xSemaphoreGive(w->go);
    xSemaphoreTake(w->done, portMAX_DELAY);
    vSemaphoreDelete(w->go);
    vSemaphoreDelete(w->done);
}

static void matmul(neo_t *m, float *out, const float *x, const uint8_t *w, int n, int d)
{
    quantize_q8(x, m->xq, m->xs, n);
    worker_t *wk = (worker_t *)m->worker;
    int split = d / 2;
    wk->args = (mm_args_t){out, m->xq, m->xs, w, n, 0, split};
    xSemaphoreGive(wk->go);
    mm_args_t self = {out, m->xq, m->xs, w, n, split, d};
    mm_rows(&self);
    xSemaphoreTake(wk->done, portMAX_DELAY);
}
#else
static void *worker_start(story_arena_t *a) { (void)a; return (void *)1; }
static void worker_stop(void *p) { (void)p; }
static void matmul(neo_t *m, float *out, const float *x, const uint8_t *w, int n, int d)
{
    quantize_q8(x, m->xq, m->xs, n);
    mm_args_t a = {out, m->xq, m->xs, w, n, 0, d};
    mm_rows(&a);
}
#endif

// ---------------------------------------------------------------- math
static void layernorm(float *o, const float *x, const float *g, const float *b, int n)
{
    float mean = 0, var = 0;
    for (int i = 0; i < n; i++) mean += x[i];
    mean /= n;
    for (int i = 0; i < n; i++) { float d = x[i] - mean; var += d * d; }
    float inv = 1.0f / sqrtf(var / n + 1e-5f);
    for (int i = 0; i < n; i++) o[i] = g[i] * ((x[i] - mean) * inv) + b[i];
}
static inline float gelu_new(float x)
{
    return 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x)));
}
static void softmax(float *x, int n)
{
    float mx = x[0], sum = 0;
    for (int i = 1; i < n; i++) if (x[i] > mx) mx = x[i];
    for (int i = 0; i < n; i++) { x[i] = expf(x[i] - mx); sum += x[i]; }
    for (int i = 0; i < n; i++) x[i] /= sum;
}

// ---------------------------------------------------------------- forward
float *neo_forward(neo_t *m, int token, int pos, int abspos)
{
    const neo_config_t *p = &m->c;
    int dim = p->dim, hd = p->hidden_dim, hs = dim / p->n_heads, kvL = m->kv_len;
    int ng = dim / 32;
    if (pos >= kvL) pos = kvL - 1;
    if (abspos >= p->seq_len) abspos = p->seq_len - 1;
    if (abspos < 0) abspos = 0;

    dequant_row(m->x, m->wte + (size_t)token * row_stride(dim), dim);
    const float *pe = m->wpe + (size_t)abspos * dim;
    for (int i = 0; i < dim; i++) m->x[i] += pe[i];

    for (int l = 0; l < p->n_layers; l++) {
        layernorm(m->xb, m->x, m->ln1_g + (size_t)l * dim, m->ln1_b + (size_t)l * dim, dim);
        matmul(m, m->q, m->xb, m->wq + l * m->stride_attn, dim, dim);
        matmul(m, m->k, m->xb, m->wk + l * m->stride_attn, dim, dim);
        matmul(m, m->v, m->xb, m->wv + l * m->stride_attn, dim, dim);

        // quantize this position's K/V to int4 with bf16 per-32 group scales
        uint8_t *krow = m->kc4 + ((size_t)l * kvL + pos) * (dim / 2);
        uint8_t *vrow = m->vc4 + ((size_t)l * kvL + pos) * (dim / 2);
        uint16_t *kg = m->kgs + ((size_t)l * kvL + pos) * ng;
        uint16_t *vg = m->vgs + ((size_t)l * kvL + pos) * ng;
        for (int g = 0; g < ng; g++) {
            const float *kk = m->k + g * 32, *vv = m->v + g * 32;
            float kmax = 0, kd = 0, vmax = 0, vd = 0;
            for (int i = 0; i < 32; i++) {
                float ka = fabsf(kk[i]); if (ka > kmax) { kmax = ka; kd = kk[i]; }
                float va = fabsf(vv[i]); if (va > vmax) { vmax = va; vd = vv[i]; }
            }
            kg[g] = f32_bf16(kd / -8.0f);
            vg[g] = f32_bf16(vd / -8.0f);
            float ks = bf16_f32(kg[g]), vs = bf16_f32(vg[g]);
            float ki = ks != 0 ? 1.0f / ks : 0, vi = vs != 0 ? 1.0f / vs : 0;
            for (int i = 0; i < 32; i += 2) {
                int k0 = (int)lrintf(kk[i] * ki), k1 = (int)lrintf(kk[i + 1] * ki);
                int v0 = (int)lrintf(vv[i] * vi), v1 = (int)lrintf(vv[i + 1] * vi);
#define CL(a) ((a) > 7 ? 7 : (a) < -8 ? -8 : (a))
                krow[g * 16 + i / 2] = (uint8_t)((CL(k0) + 8) | ((CL(k1) + 8) << 4));
                vrow[g * 16 + i / 2] = (uint8_t)((CL(v0) + 8) | ((CL(v1) + 8) << 4));
#undef CL
            }
        }

        for (int h = 0; h < p->n_heads; h++) {
            float *q = m->q + h * hs, *att = m->att + h * kvL;
            int g = (h * hs) / 32, nb = (h * hs) / 2;
            for (int t = 0; t <= pos; t++) {
                const uint8_t *kk = m->kc4 + ((size_t)l * kvL + t) * (dim / 2) + nb;
                float s = 0;
                for (int i = 0; i < hs; i += 2) {
                    s += q[i] * (float)((int)(kk[i / 2] & 0x0F) - 8);
                    s += q[i + 1] * (float)((int)(kk[i / 2] >> 4) - 8);
                }
                att[t] = s * bf16_f32(m->kgs[((size_t)l * kvL + t) * ng + g]);  // GPT-Neo: no 1/sqrt(d)
            }
            softmax(att, pos + 1);
            float *xb = m->xb + h * hs;
            memset(xb, 0, hs * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                const uint8_t *vv = m->vc4 + ((size_t)l * kvL + t) * (dim / 2) + nb;
                float a = att[t] * bf16_f32(m->vgs[((size_t)l * kvL + t) * ng + g]);
                for (int i = 0; i < hs; i += 2) {
                    xb[i] += a * (float)((int)(vv[i / 2] & 0x0F) - 8);
                    xb[i + 1] += a * (float)((int)(vv[i / 2] >> 4) - 8);
                }
            }
        }

        matmul(m, m->xb2, m->xb, m->wo + l * m->stride_attn, dim, dim);
        const float *bo = m->bo + (size_t)l * dim;
        for (int i = 0; i < dim; i++) m->x[i] += m->xb2[i] + bo[i];

        layernorm(m->xb, m->x, m->ln2_g + (size_t)l * dim, m->ln2_b + (size_t)l * dim, dim);
        matmul(m, m->hb, m->xb, m->w1 + l * m->stride_w1, dim, hd);
        const float *bf = m->b_fc + (size_t)l * hd;
        for (int i = 0; i < hd; i++) m->hb[i] = gelu_new(m->hb[i] + bf[i]);
        matmul(m, m->xb, m->hb, m->w2 + l * m->stride_w2, hd, dim);
        const float *bp = m->b_proj + (size_t)l * dim;
        for (int i = 0; i < dim; i++) m->x[i] += m->xb[i] + bp[i];
    }
    layernorm(m->x, m->x, m->lnf_g, m->lnf_b, dim);
    matmul(m, m->logits, m->x, m->wte, dim, p->vocab_size);  // tied classifier
    return m->logits;
}

int neo_kv_slide(neo_t *m, int keep, int evict)
{
    int kvL = m->kv_len, dim = m->c.dim, ng = dim / 32;
    if (keep < 0) keep = 0;
    if (keep >= kvL) return 0;
    int from = keep + evict;
    if (evict < 1 || from >= kvL) return 0;
    int nmove = kvL - from;
    size_t row = (size_t)dim / 2;
    for (int l = 0; l < m->c.n_layers; l++) {
        uint8_t *kb = m->kc4 + (size_t)l * kvL * row, *vb = m->vc4 + (size_t)l * kvL * row;
        uint16_t *kg = m->kgs + (size_t)l * kvL * ng, *vg = m->vgs + (size_t)l * kvL * ng;
        memmove(kb + keep * row, kb + from * row, nmove * row);
        memmove(vb + keep * row, vb + from * row, nmove * row);
        memmove(kg + keep * ng, kg + from * ng, nmove * ng * 2);
        memmove(vg + keep * ng, vg + from * ng, nmove * ng * 2);
    }
    return evict;
}

// ---------------------------------------------------------------- init
bool neo_parse_config(const uint8_t *b, size_t size, neo_config_t *c)
{
    if (size < 64 || memcmp(b, "CRDP", 4) != 0) { SLOGE(TAG, "bad model magic"); return false; }
    uint32_t ver;
    memcpy(&ver, b + 4, 4);
    if (ver != 3 || b[37] != 4 || b[38] != 2) {
        SLOGE(TAG, "unsupported model: version %u quant %u arch %u (need v3 Q4 GPT-Neo)",
              (unsigned)ver, b[37], b[38]);
        return false;
    }
    int h[7];
    memcpy(h, b + 8, sizeof h);
    c->dim = h[0]; c->hidden_dim = h[1]; c->n_layers = h[2]; c->n_heads = h[3];
    c->vocab_size = h[5]; c->seq_len = h[6];
    return true;
}

// Prefer fast (internal) memory; fall back to bulk (PSRAM) and say so.
static void *place(story_arena_t *fast, story_arena_t *bulk, size_t n, const char *tag)
{
    void *p = NULL;
    if (fast && story_arena_free_bytes(fast) >= n + 16) p = story_arena_alloc(fast, n, 16, tag);
    if (p) return p;
    p = story_arena_alloc(bulk, n, 16, tag);
    if (p && fast) SLOGI(TAG, "placed %s (%u B) in bulk arena", tag, (unsigned)n);
    return p;
}

bool neo_init(neo_t *m, const uint8_t *b, size_t size, int kv_len, story_arena_t *fast, story_arena_t *bulk)
{
    memset(m, 0, sizeof *m);
    if (!neo_parse_config(b, size, &m->c)) return false;
    neo_config_t *p = &m->c;
    if ((uintptr_t)b & 15) { SLOGE(TAG, "model blob must be 16-byte aligned"); return false; }
    m->kv_len = (kv_len > 0 && kv_len < p->seq_len) ? kv_len : p->seq_len;
    int dim = p->dim, hd = p->hidden_dim, L = p->n_layers;
    size_t off = 64, Ld = (size_t)L * dim * 4;
    m->ln1_g = (const float *)(b + off); off += Ld;
    m->ln1_b = (const float *)(b + off); off += Ld;
    m->ln2_g = (const float *)(b + off); off += Ld;
    m->ln2_b = (const float *)(b + off); off += Ld;
    m->lnf_g = (const float *)(b + off); off += (size_t)dim * 4;
    m->lnf_b = (const float *)(b + off); off += (size_t)dim * 4;
    m->bo = (const float *)(b + off); off += Ld;
    m->b_fc = (const float *)(b + off); off += (size_t)L * hd * 4;
    m->b_proj = (const float *)(b + off); off += Ld;
    m->wpe = (const float *)(b + off); off += (size_t)p->seq_len * dim * 4;
    off += (0 - off) & 15;
    m->stride_attn = (size_t)dim * row_stride(dim);
    m->stride_w1 = (size_t)hd * row_stride(dim);
    m->stride_w2 = (size_t)dim * row_stride(hd);
    m->wte = b + off; off += (size_t)p->vocab_size * row_stride(dim);
    m->wq = b + off; off += m->stride_attn * L;
    m->wk = b + off; off += m->stride_attn * L;
    m->wv = b + off; off += m->stride_attn * L;
    m->wo = b + off; off += m->stride_attn * L;
    m->w1 = b + off; off += m->stride_w1 * L;
    m->w2 = b + off; off += m->stride_w2 * L;
    if (off > size) {
        SLOGE(TAG, "model truncated: need %u B, have %u", (unsigned)off, (unsigned)size);
        return false;
    }

    int kvL = m->kv_len, mx = dim > hd ? dim : hd;
    size_t nib = (size_t)L * kvL * (dim / 2), gs = (size_t)L * kvL * (dim / 32) * 2;
    m->x = place(fast, bulk, dim * 4, "neo.x");
    m->xb = place(fast, bulk, dim * 4, "neo.xb");
    m->xb2 = place(fast, bulk, dim * 4, "neo.xb2");
    m->hb = place(fast, bulk, hd * 4, "neo.hb");
    m->q = place(fast, bulk, dim * 4, "neo.q");
    m->k = place(fast, bulk, dim * 4, "neo.k");
    m->v = place(fast, bulk, dim * 4, "neo.v");
    m->xq = place(fast, bulk, mx, "neo.xq");
    m->xs = place(fast, bulk, (mx / BLOCK) * 4, "neo.xs");
    m->att = place(fast, bulk, (size_t)p->n_heads * kvL * 4, "neo.att");
    m->kc4 = place(fast, bulk, nib, "neo.kcache");
    m->vc4 = place(fast, bulk, nib, "neo.vcache");
    m->kgs = place(fast, bulk, gs, "neo.kscale");
    m->vgs = place(fast, bulk, gs, "neo.vscale");
    m->logits = place(fast, bulk, (size_t)p->vocab_size * 4, "neo.logits");
    if (!(m->x && m->xb && m->xb2 && m->hb && m->q && m->k && m->v && m->xq && m->xs && m->att &&
          m->kc4 && m->vc4 && m->kgs && m->vgs && m->logits)) {
        SLOGE(TAG, "run-state allocation failed (kv_len=%d)", kvL);
        return false;
    }
    m->worker = worker_start(fast ? fast : bulk);
    if (!m->worker) return false;
#ifdef USE_PIE
    // SIMD/scalar parity on real rows: a broken kernel must fail loudly.
    for (int i = 0; i < dim; i++) m->xq[i] = (int8_t)((i * 37 + 11) & 0xFF);
    for (int bb = 0; bb < dim / BLOCK; bb++) m->xs[bb] = 0.5f + 0.25f * bb;
    for (int r = 0; r < 8; r++) {
        const uint8_t *w = m->wq + r * row_stride(dim);
        float ref = dot_q4q8(w, m->xq, m->xs, dim);
        float got = dot_q4q8_pie(w + nib_off(dim), m->xq, w, m->xs, dim / BLOCK);
        if (fabsf(got - ref) > 1e-3f * (1.0f + fabsf(ref))) {
            SLOGE(TAG, "PIE kernel parity FAILED row %d: %f vs %f", r, got, ref);
            neo_deinit(m);
            return false;
        }
    }
#endif
    SLOGI(TAG, "GPT-Neo dim=%d hidden=%d layers=%d heads=%d vocab=%d seq=%d kv=%d",
          dim, hd, L, p->n_heads, p->vocab_size, p->seq_len, kvL);
    return true;
}

void neo_deinit(neo_t *m)
{
    if (m->worker) worker_stop(m->worker);
    m->worker = NULL;
}

// ---------------------------------------------------------------- tokenizer
bool neo_tok_init(neo_tok_t *t, const uint8_t *d, size_t size, story_arena_t *arena)
{
    memset(t, 0, sizeof *t);
    uint32_t magic;
    if (size < 20 + 512) return false;
    memcpy(&magic, d, 4);
    if (magic != TOK_MAGIC) { SLOGE(TAG, "tokenizer: bad magic"); return false; }
    int32_t v[4];
    memcpy(v, d + 4, 16);
    t->base = d; t->size = size;
    t->vocab_size = v[0]; t->max_len = v[1]; t->eos_id = v[2]; t->n_merges = v[3];
    t->byte_ids = (const uint16_t *)(d + 20);
    t->merges = (const uint16_t *)(d + 20 + 512);
    t->pieces = d + 20 + 512 + (size_t)t->n_merges * 6;
    t->piece_off = story_arena_alloc(arena, (size_t)t->vocab_size * 4, 4, "tok.index");
    if (!t->piece_off) return false;
    const uint8_t *p = t->pieces, *end = d + size;
    for (int i = 0; i < t->vocab_size; i++) {
        int32_t l;
        if (p + 4 > end) { SLOGE(TAG, "tokenizer truncated at %d", i); return false; }
        memcpy(&l, p, 4);
        t->piece_off[i] = (uint32_t)(p - t->pieces);
        p += 4 + l;
    }
    return true;
}

static int find_merge(const neo_tok_t *t, int a, int b)
{
    int lo = 0, hi = t->n_merges - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        const uint16_t *m = t->merges + mid * 3;
        if (m[0] < a || (m[0] == a && m[1] < b)) lo = mid + 1;
        else if (m[0] > a || (m[0] == a && m[1] > b)) hi = mid - 1;
        else return m[2];
    }
    return -1;
}

int neo_tok_encode(const neo_tok_t *t, const char *text, int *tok, int max)
{
    int n = 0;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        if (n >= max) return -1;
        tok[n++] = t->byte_ids[*c];
    }
    while (n > 1) {
        int best = -1, bi = -1;
        for (int i = 0; i < n - 1; i++) {
            int c = find_merge(t, tok[i], tok[i + 1]);
            if (c >= 0 && (best < 0 || c < best)) { best = c; bi = i; }
        }
        if (bi < 0) break;
        tok[bi] = best;
        memmove(tok + bi + 1, tok + bi + 2, (n - bi - 2) * sizeof(int));
        n--;
    }
    return n;
}

const char *neo_tok_piece(const neo_tok_t *t, int id, int *len)
{
    *len = 0;
    if (id < 0 || id >= t->vocab_size || id == t->eos_id) return "";
    const uint8_t *p = t->pieces + t->piece_off[id];
    int32_t l;
    memcpy(&l, p, 4);
    *len = l;
    return (const char *)p + 4;
}

// ---------------------------------------------------------------- sampler
void neo_sampler_init(neo_sampler_t *s, float temperature, float top_p, uint64_t seed)
{
    s->temperature = temperature;
    s->top_p = top_p;
    s->rng = seed ? seed : 0xC0FFEEull;
}

static uint32_t rng_u32(uint64_t *st)
{
    *st ^= *st >> 12; *st ^= *st << 25; *st ^= *st >> 27;
    return (uint32_t)((*st * 0x2545F4914F6CDD1Dull) >> 32);
}

int neo_sample(neo_sampler_t *s, float *lg, int V)
{
    if (s->temperature <= 0.0f) {
        int mi = 0;
        for (int i = 1; i < V; i++) if (lg[i] > lg[mi]) mi = i;
        return mi;
    }
    for (int i = 0; i < V; i++) lg[i] /= s->temperature;
    softmax(lg, V);
    float coin = (rng_u32(&s->rng) >> 8) / 16777216.0f;
    if (s->top_p >= 1.0f) {
        float c = 0;
        for (int i = 0; i < V; i++) { c += lg[i]; if (coin < c) return i; }
        return V - 1;
    }
    enum { CAP = 64 };
    float pr[CAP];
    int id[CAP];
    int n = 0, lo = 0;
    for (int i = 0; i < V; i++) {
        float p = lg[i];
        if (n < CAP) {
            pr[n] = p; id[n] = i; n++;
            if (n == CAP) { lo = 0; for (int j = 1; j < CAP; j++) if (pr[j] < pr[lo]) lo = j; }
        } else if (p > pr[lo]) {
            pr[lo] = p; id[lo] = i;
            lo = 0;
            for (int j = 1; j < CAP; j++) if (pr[j] < pr[lo]) lo = j;
        }
    }
    for (int i = 1; i < n; i++) {
        float p = pr[i]; int d = id[i], j = i - 1;
        while (j >= 0 && pr[j] < p) { pr[j + 1] = pr[j]; id[j + 1] = id[j]; j--; }
        pr[j + 1] = p; id[j + 1] = d;
    }
    float cum = 0;
    int cut = n;
    for (int i = 0; i < n; i++) { cum += pr[i]; if (cum >= s->top_p) { cut = i + 1; break; } }
    float target = coin * cum, c = 0;
    for (int i = 0; i < cut; i++) { c += pr[i]; if (target < c) return id[i]; }
    return id[cut - 1];
}
