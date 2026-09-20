// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Logging shim: ESP_LOGx on target, printf on host builds.
#pragma once
#ifdef STORY_HOST
#include <stdio.h>
#define SLOGE(tag, fmt, ...) fprintf(stderr, "E (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define SLOGW(tag, fmt, ...) fprintf(stderr, "W (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define SLOGI(tag, fmt, ...) fprintf(stderr, "I (%s) " fmt "\n", tag, ##__VA_ARGS__)
#else
#include "esp_log.h"
#define SLOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define SLOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define SLOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#endif
