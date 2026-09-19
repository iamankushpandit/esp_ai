// The single assistant screen (portrait 240x320), styled like a CLI session:
//
//   ╭────────────────────────────╮
//   │ ✻ Braino AI                │
//   │   (c) iamankushpandit      │
//   ╰────────────────────────────╯
//   > what color is a banana         <- session transcript: every turn of
//   ● The banana is yellow.             the current session, newest at the
//                                       bottom, scrolling like a terminal
//   > how many legs does a dog have
//   ● A dog has four legs.
//   ⎿  8 tok · 6.2 tok/s              <- dim detail line
//   ✻ Thinking…                      <- status / spinner
//              ( ✻ Ask )           <- ask button (tap to talk)
//
// Every region is a dirty-row box: only rows whose text changed are redrawn,
// in place (no clears). Scrolling rewrites the rows that shifted.
#pragma once
#include <stdbool.h>
#include <stdint.h>

void app_ui_init(void);
// Status line. Busy states (any color other than UI_GREEN/UI_GREY) get an
// advancing spinner glyph; "..." is shown as an ellipsis.
void app_ui_status(const char *s, uint16_t color);
// Starts a new turn in the transcript with the recognized question.
void app_ui_you(const char *text);
// Sets (or streams) the answer of the latest turn.
void app_ui_story(const char *text);
void app_ui_detail(const char *text);

// Clears the whole transcript (new session).
void app_ui_clear_turn(void);
// Streaming answer + live generation speed on the detail line.
void app_ui_llm_progress(const char *text, int tokens, float tok_per_s);
// Last generation speed shown (for keeping it visible while speaking).
float app_ui_last_tok_rate(void);

// Ask button: slim outlined pill with a small spark and "Ask", bottom center.
typedef enum { BTN_IDLE = 0, BTN_PRESSED, BTN_BUSY } app_btn_state_t;
void app_ui_button(app_btn_state_t st);     // redraws only the button tile, only on change
bool app_ui_button_hit(int x, int y);       // touch coordinates -> inside the button?
