#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "story_llm.h"

// Called (batched) with the answer-so-far while tokens stream.
typedef void (*think_stream_fn)(void *user, const char *text_so_far);

// Runs the whole THINKING phase (load, generate, unload). Arenas must be free.
bool think_answer(const llm_history_t *hist, const char *question, char *answer, size_t cap,
                  think_stream_fn fn, void *user, llm_stats_t *st);
