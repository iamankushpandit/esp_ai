// Host stub
#pragma once
#include <stdio.h>
#define ESP_LOGE(t, ...) (fprintf(stderr, __VA_ARGS__), fputc(10, stderr))
#define ESP_LOGI(t, ...) ((void)0)
