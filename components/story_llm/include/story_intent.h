// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Tiny rule-based router in front of the LLM. The model can't reliably know
// who it is or when to switch into its story format, so we decide that here.
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INTENT_CHAT = 0,   // normal question/answer
    INTENT_STORY,      // "tell me a story about X" -> story format, topic filled
    INTENT_IDENTITY,   // "what is your name / who are you" -> fixed answer
    INTENT_BYE,        // short "bye / goodbye / that's all" -> end the session
    INTENT_TIME,       // "what time is it" -> device clock (no model)
    INTENT_DATE,       // "what day / date / month / year is it" -> device clock (no model)
    INTENT_TIMER_SET,  // "set a timer for five minutes" (see story_duration_seconds)
    INTENT_TIMER_CANCEL,
    INTENT_TIMER_QUERY, // "how much time is left on the timer"
} story_intent_t;

#define INTENT_IDENTITY_ANSWER "My name is Ivy AI. I am a little talking robot. I like to chat and tell stories."

// Duration in a spoken request, in seconds ("five minutes", "1 minute and
// 30 seconds", "a minute", "half an hour", "ninety seconds"); -1 if none.
int story_duration_seconds(const char *text);

// Classify a (lower- or mixed-case) transcript. For INTENT_STORY, writes the
// story topic (e.g. "a red robot") to `topic`.
story_intent_t story_intent(const char *text, char *topic, size_t topic_cap);

#ifdef __cplusplus
}
#endif
