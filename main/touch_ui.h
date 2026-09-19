// Touch handling task + screen power state.
//
// A small task polls the FT6336U every 20 ms (also while a session runs):
//   - touch on a dark screen: wakes it (that touch does nothing else)
//   - press + release inside the Ask pill: posts an "ask" event
//   - drag in the conversation area: scrolls the transcript
#pragma once
#include <stdbool.h>
#include <stdint.h>

void touch_ui_start(void);

// Waits up to timeout_ms for an Ask tap. Returns true if one happened.
bool touch_ui_wait_ask(uint32_t timeout_ms);

// While busy, Ask taps are ignored (scrolling still works).
void touch_ui_set_busy(bool busy);

// Screen power (backlight). Keeps state consistent between tasks.
void screen_on(void);
void screen_off(void);
bool screen_is_on(void);
int64_t screen_last_activity_us(void);
void screen_note_activity(void);
