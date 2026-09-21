// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Countdown timer ("set a timer for five minutes"), one at a time. Plain C,
// no model; it counts on the monotonic clock, so it works whether or not the
// wall clock has been set.
#pragma once
#include <stdbool.h>
#include <stddef.h>

void timer_start(int seconds);
void timer_cancel(void);
bool timer_active(void);
int timer_remaining_s(void);        // 0 when none / done
// True once when the running timer reaches zero (the main task rings).
bool timer_take_fired(void);

// "5 minutes", "1 minute and 30 seconds", "2 hours and 5 minutes".
void timer_say_duration(int seconds, char *out, size_t cap);
