// Host eval: run a question battery through the LLM with given decoding
// settings, one answer per line, for side-by-side comparison of configs.
// Usage: llm_eval model.bin tok.bin battery.txt temp top_p seed ["primer user" "primer bot"]
#include "story_llm.h"
#include "story_intent.h"
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
    uint8_t *b = _aligned_malloc(*n + 1, 16);
    if (fread(b, 1, *n, f) != *n) exit(3);
    b[*n] = 0;
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    if (argc < 7) { fprintf(stderr, "usage: llm_eval model tok battery temp top_p seed [pu pb]\n"); return 2; }
    size_t mn, tn, bn;
    uint8_t *model = slurp(argv[1], &mn), *tok = slurp(argv[2], &tn);
    char *battery = (char *)slurp(argv[3], &bn);
    static uint8_t fast_mem[264 * 1024] __attribute__((aligned(16)));
    uint8_t *bulk_mem = _aligned_malloc(7 << 20, 16);
    story_arena_t fast, bulk;
    story_arena_init(&fast, fast_mem, sizeof fast_mem);
    story_arena_init(&bulk, bulk_mem, 7 << 20);
    CHECK(story_arena_begin(&fast, "eval"));
    CHECK(story_arena_begin(&bulk, "eval"));
    llm_t l;
    llm_params_t p = llm_default_params();
    p.temperature = (float)atof(argv[4]);
    p.top_p = (float)atof(argv[5]);
    p.seed = strtoull(argv[6], NULL, 10);
    llm_blobs_t b = {model, mn, tok, tn};
    CHECK(llm_load(&l, &b, &p, &fast, &bulk));

    int total_prompt = 0, n = 0;
    for (char *q = strtok(battery, "\r\n"); q; q = strtok(NULL, "\r\n")) {
        llm_history_t h;
        llm_history_clear(&h);
        if (argc >= 9) llm_history_push(&h, argv[7], argv[8]);
        char ans[LLM_ANSWER_CHARS + 1];
        llm_stats_t st;
        char topic[128];
        story_intent_t it = story_intent(q, topic, sizeof topic);
        memset(&st, 0, sizeof st);
        if (it == INTENT_IDENTITY) snprintf(ans, sizeof ans, "%s", INTENT_IDENTITY_ANSWER);
        else if (it == INTENT_STORY) CHECK(llm_story(&l, topic, ans, sizeof ans, NULL, NULL, &st));
        else CHECK(llm_answer(&l, &h, q, ans, sizeof ans, NULL, NULL, &st));
        printf("%-42s |%c| %s  [%s]\n", q, "CSI"[it], ans, st.stop_reason ? st.stop_reason : "-");
        total_prompt += st.prompt_tokens;
        n++;
    }
    printf("# avg prompt tokens %.1f\n", n ? (double)total_prompt / n : 0.0);
    llm_unload(&l);
    return 0;
}
