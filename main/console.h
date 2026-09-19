#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

void console_init(void);
// Reads one line (without newline). timeout_ms = 0 waits forever.
bool console_readline(char *buf, size_t cap, uint32_t timeout_ms);
