// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Language-model registry: every /sd/story/llm*/ folder that holds model.bin
// + tok.bin is a selectable model (optional name.txt = display name), and so
// is every llama.cpp GGUF file in /sd/models/ (see gguf_llm.h for what runs).
// The active choice is saved in NVS and survives reboots.
#pragma once
#include <stdbool.h>
#include <stddef.h>

#define MODELS_MAX 24

typedef struct {
    char dir[64];        // folder (/sd/story/llm8m_kid) or GGUF file (/sd/models/x.gguf)
    char name[40];       // display name
    size_t bytes;        // model.bin + tok.bin
} model_info_t;

// Scans the SD card and restores the saved choice (falls back to the first
// model found). Returns the number of models.
int models_scan(void);
int models_count(void);
const model_info_t *models_get(int i);
int models_active(void);
// Selects model i: sets the THINK phase directory and persists the choice.
bool models_select(int i);
