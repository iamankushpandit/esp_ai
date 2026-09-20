// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

#include "timer.h"
#include "story_mem.h"
#include <stdio.h>

static volatile int64_t s_end_us;     // 0 = no timer
static volatile bool s_fired;

void timer_start(int seconds)
{
    s_fired = false;
    s_end_us = story_time_us() + (int64_t)seconds * 1000000;
}

void timer_cancel(void) { s_end_us = 0; }

bool timer_active(void) { return s_end_us != 0; }

int timer_remaining_s(void)
{
    if (!s_end_us) return 0;
    int64_t left = s_end_us - story_time_us();
    return left > 0 ? (int)((left + 999999) / 1000000) : 0;
}

bool timer_take_fired(void)
{
    if (s_end_us && story_time_us() >= s_end_us) {
        s_end_us = 0;
        s_fired = true;
    }
    bool f = s_fired;
    s_fired = false;
    return f;
}

void timer_say_duration(int s, char *out, size_t cap)
{
    int h = s / 3600, m = s % 3600 / 60, sec = s % 60;
    char parts[3][24];
    int n = 0;
    if (h) snprintf(parts[n++], sizeof parts[0], "%d hour%s", h, h == 1 ? "" : "s");
    if (m) snprintf(parts[n++], sizeof parts[0], "%d minute%s", m, m == 1 ? "" : "s");
    if (sec || !n) snprintf(parts[n++], sizeof parts[0], "%d second%s", sec, sec == 1 ? "" : "s");
    if (n == 1) snprintf(out, cap, "%s", parts[0]);
    else if (n == 2) snprintf(out, cap, "%s and %s", parts[0], parts[1]);
    else snprintf(out, cap, "%s, %s and %s", parts[0], parts[1], parts[2]);
}
