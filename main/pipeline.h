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
#include "hear.h"
#include "story_llm.h"

typedef enum {
    PIPE_OK = 0,
    PIPE_NO_SPEECH,
    PIPE_NOT_UNDERSTOOD,
    PIPE_ERROR,
    PIPE_BYE,
    PIPE_CANCELLED,     // Stop button
} pipeline_result_t;

#define SESSION_MAX_TURNS 4   // first question + three follow-ups

// One question/answer turn (turn is 1-based). `hist` may be NULL. The last
// turn's answer gets " Bye bye." appended.
pipeline_result_t pipeline_turn(const hear_params_t *hp, llm_history_t *hist, int turn, int max_turns);

// A question typed on the T9 keypad: answered (built-in or AI), shown and
// spoken, like a voice turn but without listening.
pipeline_result_t pipeline_typed(const char *question);

// Stop button: cancels whatever the running turn is doing (listening,
// generating, speaking) and ends the session. Safe from any task.
void pipeline_cancel(void);

// Full conversation session; returns when it ends (screen off afterwards).
void pipeline_session(const hear_params_t *hp, int max_turns);
