// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

#include "story_intent.h"
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void lower_copy(char *dst, const char *src, size_t cap)
{
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = 0;
}

static int has(const char *s, const char *w) { return strstr(s, w) != NULL; }

story_intent_t story_intent(const char *text, char *topic, size_t topic_cap)
{
    char s[256];
    lower_copy(s, text ? text : "", sizeof s);
    if (topic && topic_cap) topic[0] = 0;

    // Goodbye: only for short utterances, so "a story about a bus stop" or
    // "why do birds say goodbye" style questions still get answered.
    int words = 0;
    for (const char *c = s; *c; c++)
        if (isalpha((unsigned char)*c) && (c == s || !isalpha((unsigned char)c[-1]))) words++;
    if (words > 0 && words <= 5) {
        static const char *bye[] = {"bye", "goodbye", "good bye", "see you", "that's all", "thats all",
                                    "that is all", "i'm done", "im done", "stop", "go to sleep", "never mind"};
        for (size_t i = 0; i < sizeof bye / sizeof bye[0]; i++) {
            const char *m = strstr(s, bye[i]);
            size_t n = strlen(bye[i]);
            // whole-word match
            if (m && (m == s || !isalpha((unsigned char)m[-1])) && !isalpha((unsigned char)m[n]))
                return INTENT_BYE;
        }
    }

    if (has(s, "your name") || has(s, "who are you") || has(s, "what are you") || has(s, "call you"))
        return INTENT_IDENTITY;

    // Timers (before the clock rules: "timer" contains "time").
    if (has(s, "timer")) {
        if (has(s, "cancel") || has(s, "stop") || has(s, "delete") || has(s, "remove") || has(s, "turn off"))
            return INTENT_TIMER_CANCEL;
        if (has(s, "how much") || has(s, "how long") || has(s, "left") || has(s, "remaining"))
            return INTENT_TIMER_QUERY;
        if (story_duration_seconds(s) > 0) return INTENT_TIMER_SET;
    }
    if (has(s, "time is left") || has(s, "time left")) return INTENT_TIMER_QUERY;

    // Clock questions are answered by the device clock, never the model.
    static const char *time_q[] = {"time is it", "what's the time", "whats the time", "what is the time",
                                   "tell me the time", "current time", "the time now", "time right now"};
    for (size_t i = 0; i < sizeof time_q / sizeof time_q[0]; i++)
        if (has(s, time_q[i])) return INTENT_TIME;
    static const char *date_q[] = {"what day is it", "what day is today", "which day is it", "which day is today",
                                   "what's the date", "whats the date", "what is the date", "today's date",
                                   "todays date", "what is today", "what's today", "whats today", "date today",
                                   "what month is it", "which month is it", "what year is it",
                                   "day of the week is it", "day of the week today"};
    for (size_t i = 0; i < sizeof date_q / sizeof date_q[0]; i++)
        if (has(s, date_q[i])) return INTENT_DATE;

    if (has(s, "story") || has(s, "fairy tale") || has(s, "bedtime")) {
        const char *about = strstr(s, "about ");
        const char *t = about ? about + 6 : NULL;
        if (!t || !*t) t = "a happy day";   // no topic given
        if (topic && topic_cap) {
            snprintf(topic, topic_cap, "%s", t);
            size_t n = strlen(topic);
            while (n > 0 && (ispunct((unsigned char)topic[n - 1]) || topic[n - 1] == ' ')) topic[--n] = 0;
        }
        return INTENT_STORY;
    }
    return INTENT_CHAT;
}

// ---------------------------------------------------------------- durations
static int word_number(const char *w, int n)
{
    static const char *ones[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
                                 "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
                                 "seventeen", "eighteen", "nineteen"};
    static const char *tens[] = {"twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
    for (int i = 0; i < 20; i++)
        if ((int)strlen(ones[i]) == n && !strncmp(w, ones[i], (size_t)n)) return i;
    for (int i = 0; i < 8; i++)
        if ((int)strlen(tens[i]) == n && !strncmp(w, tens[i], (size_t)n)) return 20 + 10 * i;
    if ((n == 1 && w[0] == 'a') || (n == 2 && !strncmp(w, "an", 2))) return 1;
    bool digits = n > 0;
    for (int i = 0; i < n; i++) digits = digits && isdigit((unsigned char)w[i]);
    return digits ? atoi(w) : -1;
}

static int unit_seconds(const char *w, int n)
{
    if (n >= 3 && (!strncmp(w, "sec", 3))) return 1;
    if (n >= 3 && (!strncmp(w, "min", 3))) return 60;
    if (n >= 4 && (!strncmp(w, "hour", 4))) return 3600;
    if (n == 2 && !strncmp(w, "hr", 2)) return 3600;
    return 0;
}

int story_duration_seconds(const char *text)
{
    char s[256];
    lower_copy(s, text ? text : "", sizeof s);
    int total = 0, cur = -1;
    bool any = false, half = false;
    for (const char *p = s; *p;) {
        while (*p && !isalnum((unsigned char)*p)) p++;
        const char *w = p;
        while (isalnum((unsigned char)*p)) p++;
        int n = (int)(p - w);
        if (!n) break;
        // "5min" / "10s" glued forms
        int k = 0;
        while (k < n && isdigit((unsigned char)w[k])) k++;
        if (k > 0 && k < n && unit_seconds(w + k, n - k)) {
            total += atoi(w) * unit_seconds(w + k, n - k);
            any = true;
            cur = -1;
            continue;
        }
        if (n == 1 && w[0] == 's' && cur > 0) { total += cur; any = true; cur = -1; continue; }
        if (n == 4 && !strncmp(w, "half", 4)) { half = true; continue; }
        int u = unit_seconds(w, n);
        if (u) {
            if (cur < 0 && half) cur = 0;               // "half an hour" -> (0 + 1/2)
            if (cur >= 0) {
                total += cur * u + (half ? u / 2 : 0);
                any = true;
            }
            cur = -1;
            half = false;
            continue;
        }
        int v = word_number(w, n);
        if (v >= 0) {
            if (half && v == 1 && cur < 0) continue;     // the "an" in "half an hour"
            cur = (cur >= 20 && cur % 10 == 0 && v < 10) ? cur + v : v;   // "twenty five"
        } else if (!(n == 3 && !strncmp(w, "and", 3))) {
            if (!half) cur = -1;
        }
    }
    return any && total > 0 ? total : -1;
}
