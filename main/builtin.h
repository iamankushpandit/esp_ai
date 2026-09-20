// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Answers that come from plain C code, not the language model: the device's
// name, the time and the date. The UI labels these so it's always clear
// whether the AI model or the device itself answered.
#pragma once
#include <stdbool.h>
#include <stddef.h>

// If `question` is one the device answers itself, writes the answer and a
// short source label ("device clock", "built-in") and returns true.
bool builtin_answer(const char *question, char *out, size_t cap, const char **source);

// True once the clock has been set (by NTP) since power-on.
bool clock_is_set(void);

// Speakable time / date for the current local time zone.
void clock_say_time(char *out, size_t cap);
void clock_say_date(char *out, size_t cap);

// Compact on-screen form, e.g. "Sat 12:51 PM" or "not set".
void clock_short(char *out, size_t cap);
