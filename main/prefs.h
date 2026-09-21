// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// User preferences, saved in NVS ("braino" namespace) and shown on /settings.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t volume;         // speaker volume 10..100 %
    uint8_t brightness;     // backlight 10..100 %
    uint16_t screen_s;      // screen off after this many idle seconds; 0 = never
    bool wake;              // listen for "Hey Ivy"
    int8_t voice_gain_db;   // digital gain on the spoken voice, 0..+12 dB
} prefs_t;

void prefs_load(void);                  // defaults if nothing saved
void prefs_save(void);
prefs_t *prefs(void);

// Step helpers used by the settings page (clamp + save + return new value).
int prefs_step_volume(int dir);          // dir -1 / +1, 10 % steps
int prefs_step_brightness(int dir);
uint16_t prefs_cycle_screen(void);       // 15 s -> 30 s -> 1 min -> 2 min -> 5 min -> never
bool prefs_toggle_wake(void);
int prefs_step_voice_gain(int dir);      // 3 dB steps, 0..+12 dB
