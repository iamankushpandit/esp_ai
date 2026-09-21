// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

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

// Why the last think_answer() failed ("" if it didn't): e.g. a GGUF model
// that is too big or uses an unsupported architecture.
const char *think_last_error(void);

// Asks a running generation to stop (from another task, e.g. the Stop button).
void think_stop(void);
