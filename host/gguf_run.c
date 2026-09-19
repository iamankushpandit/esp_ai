// Host check of the GGUF engine against tools/gguf/ref_llama.py:
//   gguf_run model.gguf "Once upon a time" 30
// Prints the prompt ids, greedy generated ids and text (same format as the
// reference), plus timing.
#include "gguf_llm.h"
#include "check.h"
#include "story_arena.h"
#include "story_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(*n + 64);   // malloc is 16-byte aligned; GGUF data is 32-aligned in-file only
    if (fread(b, 1, *n, f) != *n) { fclose(f); return NULL; }
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: gguf_run model.gguf prompt n\n"); return 2; }
    size_t n;
    uint8_t *file = slurp(argv[1], &n);
    CHECK(file);
    static story_arena_t fast, bulk;
    story_arena_init(&fast, malloc(512 * 1024), 512 * 1024);
    story_arena_init(&bulk, malloc(64 * 1024 * 1024), 64 * 1024 * 1024);
    story_arena_begin(&fast, "gguf");
    story_arena_begin(&bulk, "gguf");
    static gguf_llm_t m;
    char err[128];
    if (!gguf_llm_load(&m, file, n, 256, &fast, &bulk, err, sizeof err)) {
        fprintf(stderr, "load failed: %s\n", err);
        return 1;
    }
    printf("state bytes %zu (file %zu)\n", gguf_llm_state_bytes(file, n, 256), n);
    int ids[256];
    int k = gguf_llm_encode(&m, argv[2], true, ids, 256);
    printf("prompt ids: [");
    for (int i = 0; i < k; i++) printf("%s%d", i ? ", " : "", ids[i]);
    printf("]\n");

    int gen = atoi(argv[3]), tok = ids[0], out[512], no = 0;
    char text[4096] = "", tmp[64];
    int64_t t0 = story_time_us();
    for (int pos = 0; pos < k + gen - 1; pos++) {
        float *lg = gguf_llm_forward(&m, tok, pos);
        if (pos < k - 1) { tok = ids[pos + 1]; continue; }
        int best = 0;
        for (int v = 1; v < m.vocab; v++) if (lg[v] > lg[best]) best = v;
        if (best == m.eos) break;
        int pl;
        const char *pc = gguf_llm_piece(&m, best, tok, &pl, tmp);
        strncat(text, pc, (size_t)pl);
        out[no++] = best;
        tok = best;
    }
    double s = (story_time_us() - t0) / 1e6;
    printf("gen ids: [");
    for (int i = 0; i < no; i++) printf("%s%d", i ? ", " : "", out[i]);
    printf("]\ntext: '%s%s'\n", argv[2], text);
    printf("%d tokens in %.2f s (%.1f tok/s on this PC)\n", k + no, s, (k + no) / s);

    // Chat-style answer path (template + sampler at temperature 0).
    llm_params_t p = llm_default_params();
    p.temperature = 0;
    char ans[512];
    llm_stats_t st;
    CHECK(gguf_llm_answer(&m, argv[2], &p, ans, sizeof ans, NULL, NULL, &st));
    printf("answer (%d in, %d out, %s): %s\n", st.prompt_tokens, st.gen_tokens, st.stop_reason, ans);
    return 0;
}
