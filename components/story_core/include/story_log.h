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
