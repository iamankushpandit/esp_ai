// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Host test: rule-based intent router.
#include "story_intent.h"
#include "check.h"
#include <string.h>

int main(void)
{
    char t[64];
    CHECK(story_intent("what is your name", t, sizeof t) == INTENT_IDENTITY);
    CHECK(story_intent("Who are you?", t, sizeof t) == INTENT_IDENTITY);
    CHECK(story_intent("what should i call you", t, sizeof t) == INTENT_IDENTITY);
    CHECK(story_intent("whats your name", t, sizeof t) == INTENT_IDENTITY);
    CHECK(story_intent("tell me a short story about a red robot.", t, sizeof t) == INTENT_STORY);
    CHECK(strcmp(t, "a red robot") == 0);
    CHECK(story_intent("can you tell me a bedtime story", t, sizeof t) == INTENT_STORY);
    CHECK(strcmp(t, "a happy day") == 0);
    CHECK(story_intent("why is the sky blue", t, sizeof t) == INTENT_CHAT);
    CHECK(story_intent("", t, sizeof t) == INTENT_CHAT);
    CHECK(story_intent("bye", t, sizeof t) == INTENT_BYE);
    CHECK(story_intent("okay goodbye story", t, sizeof t) == INTENT_BYE);
    CHECK(story_intent("that's all thank you", t, sizeof t) == INTENT_BYE);
    CHECK(story_intent("stop", t, sizeof t) == INTENT_BYE);
    CHECK(story_intent("tell me a story about a bus stop", t, sizeof t) == INTENT_STORY);
    CHECK(story_intent("what is a butterfly", t, sizeof t) == INTENT_CHAT);      // "bye" inside a word
    CHECK(story_intent("why do people say goodbye when they leave the house", t, sizeof t) == INTENT_CHAT);
    CHECK(story_intent("what time is it", t, sizeof t) == INTENT_TIME);
    CHECK(story_intent("Hey, what's the time?", t, sizeof t) == INTENT_TIME);
    CHECK(story_intent("what day is it today", t, sizeof t) == INTENT_DATE);
    CHECK(story_intent("what is the date", t, sizeof t) == INTENT_DATE);
    CHECK(story_intent("what month is it", t, sizeof t) == INTENT_DATE);
    CHECK(story_intent("what comes after tuesday", t, sizeof t) == INTENT_CHAT);   // a fact, not the clock
    CHECK(story_intent("how many days are in a week", t, sizeof t) == INTENT_CHAT);
    CHECK(story_intent("what time do owls wake up", t, sizeof t) == INTENT_CHAT);
    CHECK(story_intent("tell me a story about time travel", t, sizeof t) == INTENT_STORY);
    CHECK(story_intent("set a timer for five minutes", t, sizeof t) == INTENT_TIMER_SET);
    CHECK(story_duration_seconds("set a timer for five minutes") == 300);
    CHECK(story_duration_seconds("ten second timer") == 10);
    CHECK(story_duration_seconds("timer for one minute and thirty seconds") == 90);
    CHECK(story_duration_seconds("set a timer for twenty five minutes") == 1500);
    CHECK(story_duration_seconds("timer for a minute") == 60);
    CHECK(story_duration_seconds("set a timer for half an hour") == 1800);
    CHECK(story_duration_seconds("timer 90 seconds") == 90);
    CHECK(story_duration_seconds("set a 5 min timer") == 300);
    CHECK(story_duration_seconds("set a timer for 2 hours") == 7200);
    CHECK(story_duration_seconds("what is the time") == -1);
    CHECK(story_intent("cancel the timer", t, sizeof t) == INTENT_TIMER_CANCEL);
    CHECK(story_intent("how much time is left on the timer", t, sizeof t) == INTENT_TIMER_QUERY);
    CHECK(story_intent("how much time is left", t, sizeof t) == INTENT_TIMER_QUERY);
    CHECK(story_intent("what time is it", t, sizeof t) == INTENT_TIME);
    printf("test_intent OK\n");
    return 0;
}
