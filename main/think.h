#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "story_llm.h"

// Called (batched) with the answer-so-far while tokens stream, plus the
// prompt ("in") and generated ("out") token counts and the live speed.
typedef void (*think_stream_fn)(void *user, const char *text_so_far, int in_tokens, int out_tokens,
                                float tok_per_s);

// Runs the whole THINKING phase (load, generate, unload). Arenas must be free.
bool think_answer(const llm_history_t *hist, const char *question, char *answer, size_t cap,
                  think_stream_fn fn, void *user, llm_stats_t *st);

// SD directory containing model.bin + tok.bin (default /sd/story/llm8m).
void think_set_model_dir(const char *dir);
const char *think_model_dir(void);
