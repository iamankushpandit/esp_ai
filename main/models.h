// Language-model registry: every /sd/story/llm*/ folder that holds model.bin
// + tok.bin is a selectable model. An optional name.txt gives a display name.
// The active choice is saved in NVS and survives reboots.
#pragma once
#include <stdbool.h>
#include <stddef.h>

#define MODELS_MAX 8

typedef struct {
    char dir[64];        // e.g. /sd/story/llm8m_kid
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
