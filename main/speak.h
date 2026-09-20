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
#include <stdint.h>

typedef struct {
    int64_t init_us, first_audio_us, total_us;
    long audio_ms;
} speak_stats_t;

// Runs the whole SPEAKING phase (load voice, speak, unload). Arenas must be free.
bool speak_text(const char *text, volatile bool *stop, speak_stats_t *st);
