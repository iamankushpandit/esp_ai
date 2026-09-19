// The single assistant screen (portrait 240x320), styled like a CLI session:
//
//   ╭────────────────────────────╮
//   │ ✻ Story                    │
//   │   offline voice assistant  │
//   ╰────────────────────────────╯
//
//   > why is the sky blue            <- transcript (you)
//
//   ● The sky is blue because ...    <- answer (Story), hanging indent
//     sunlight bounces off the air.
//   ⎿  24 tok · 6.2 tok/s            <- dim detail line
//   ✻ Thinking…                      <- status / spinner
//     BOOT to ask · offline          <- footer hint
//
// Every region is a dirty-row box: only rows whose text changed are redrawn.
#pragma once
#include <stdint.h>

void app_ui_init(void);
// Status line. Busy states (any color other than UI_GREEN/UI_GREY) get an
// advancing spinner glyph; "..." is shown as an ellipsis.
void app_ui_status(const char *s, uint16_t color);
void app_ui_you(const char *text);
void app_ui_story(const char *text);
void app_ui_detail(const char *text);
void app_ui_footer(const char *text);
void app_ui_clear_turn(void);
// Streaming answer + live generation speed on the detail line.
void app_ui_llm_progress(const char *text, int tokens, float tok_per_s);
// Last generation speed shown (for keeping it visible while speaking).
float app_ui_last_tok_rate(void);
