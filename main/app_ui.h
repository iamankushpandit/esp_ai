// The single assistant screen (portrait 240x320), dirty-row regions:
//   row 0      title
//   row 1      status line
//   rows 3-7   "You:" + transcript
//   rows 9-21  "Story:" + answer
#pragma once
#include <stdint.h>

void app_ui_init(void);
void app_ui_status(const char *s, uint16_t color);
void app_ui_you(const char *text);
void app_ui_story(const char *text);
void app_ui_clear_turn(void);
