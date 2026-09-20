// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Always-on wake word (Espressif esp-sr WakeNet), independent of the LLM.
//
// The WakeNet model lives in the flash "model" partition (packed at build
// time from the Kconfig choice). While idle, a task feeds 16 kHz mic audio to
// WakeNet; on detection it calls the callback. It is stopped (engine
// destroyed, memory freed, mic released) for the duration of a conversation.
#pragma once
#include <stdbool.h>

#define WAKE_PHRASE "Hey Ivy"   // stock esp-sr model wn9_heyivy_tts2 (matches the name, Ivy AI)

typedef void (*wake_cb_t)(void);

// Maps the model partition and resolves the WakeNet model. Call once.
bool wake_init(void);
// Starts listening; `cb` runs in the wake task when the phrase is heard.
bool wake_start(wake_cb_t cb);
// Stops listening and frees the engine (blocks until the task exits).
void wake_stop(void);
bool wake_running(void);
const char *wake_model_name(void);
