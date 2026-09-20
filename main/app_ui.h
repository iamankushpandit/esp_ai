// The single assistant screen (portrait 240x320), styled like a CLI session:
//
//   ╭────────────────────────────╮
//   │ [leaf]  Ivy AI             │
//   │   (C) iamankushpandit      │
//   │   TinyTalk 2 8M            │ <- the loaded model
//   ╰────────────────────────────╯
//   > what color is a banana       ▐ <- scrollable page area: the session
//   ● The banana is yellow.        ▐    transcript (chat), the /model list,
//                                  ▐    or /settings. Thin scrollbar on the right.
//   > how many legs does a dog have
//   ● A dog has four legs.
//   ⎿ AI 12 in · 8 out · 6.2/s        <- dim detail line (tokens + speed)
//   ✻ Thinking…                      <- status / spinner
//   ( /model ) ( ✻ Ask ) ( /settings ) <- bottom bar (tap)
//
// Every region is a dirty-row box: only rows whose text changed are redrawn,
// in place (no clears). Scrolling rewrites the rows that shifted.
#pragma once
#include <stdbool.h>
#include <stdint.h>

// Boot welcome screen: the Ivy AI logo + "(C) iamankushpandit".
void app_ui_splash(void);
void app_ui_init(void);
// Repaints the header's model row (call after models_scan/models_select).
void app_ui_model_changed(void);
// Sleep (the lock button, or the idle timeout): the logo alone on black, its
// colour drifting. tick() draws one frame; exit() rebuilds the whole screen.
void app_ui_sleep_enter(void);
void app_ui_sleep_tick(void);
void app_ui_sleep_exit(void);
bool app_ui_sleeping(void);
// Status line. Busy states (any color other than UI_OK/UI_GREY) get an
// advancing spinner glyph; "..." is shown as an ellipsis.
void app_ui_status(const char *s, uint16_t color);
// Starts a new turn in the transcript with the recognized question.
void app_ui_you(const char *text);
// Sets (or streams) the answer of the latest turn.
void app_ui_story(const char *text);
// Answer produced by plain C code (clock, name), not the model: drawn with a
// white diamond instead of the pink bullet, detail line "<source> . no AI".
void app_ui_builtin(const char *text, const char *source);
void app_ui_detail(const char *text);
// Clears the whole transcript (new session).
void app_ui_clear_turn(void);
// Streaming answer + prompt/generated token counts and live speed on the
// detail line.
void app_ui_llm_progress(const char *text, int in_tokens, int out_tokens, float tok_per_s);
// Animation tick (call every ~20 ms): while the AI works the Ask spark pulses
// and the status spinner turns (any UI_BUSY status: listening, transcribing, thinking).
void app_ui_tick(void);
// Last generation speed shown (for keeping it visible while speaking).
float app_ui_last_tok_rate(void);

// Pages shown in the scrollable area.
typedef enum { PAGE_CHAT = 0, PAGE_MODELS, PAGE_SETTINGS, PAGE_ABOUT, PAGE_WIFI, PAGE_KEYBOARD, PAGE_T9 } app_page_t;
// Pages reached from /settings (its pill stays lit on them).
static inline bool app_page_in_settings(app_page_t p)
{
    return p == PAGE_SETTINGS || p == PAGE_ABOUT || p == PAGE_WIFI || p == PAGE_KEYBOARD;
}
void app_ui_page(app_page_t p);            // switch page (re-renders the area)
app_page_t app_ui_page_get(void);
void app_ui_refresh_page(void);            // e.g. after the model list changed

// Row tags on /settings and its sub-pages (network rows use their index 0..).
#define TAG_WIFI_SETUP 100                 // /settings: "Wi-Fi & clock" row
#define TAG_WIFI_SYNC 101
#define TAG_WIFI_FORGET 102
#define TAG_WIFI_RESCAN 103
#define TAG_SET_VOLUME 110                 // [-] / [+] by tap column, see app_ui_tap_col()
#define TAG_SET_BRIGHT 111
#define TAG_SET_SCREEN 112                 // tap cycles the screen-off time
#define TAG_SET_WAKE 113                   // tap toggles the wake word
#define TAG_SET_ABOUT 114
#define TAG_SET_GAIN 115                   // voice gain [-] / [+]
#define TAG_TYPE 120                       // chat: "[ Type a question ]" -> T9 keypad
// Settings rows "Label      [-] 70% [+]": columns 13..18 step down, 20.. step
// up; a tap on the label does nothing.
#define SET_MINUS_COL_MIN 13
#define SET_MINUS_COL_MAX 18
#define SET_PLUS_COL_MIN 20
int app_ui_tap_col(int x);                 // text column under x

// Wi-Fi page: scan state and results (copied).
#include "net.h"
void app_ui_wifi_scanning(void);
void app_ui_wifi_results(const net_ap_t *aps, int n);   // n < 0: scan failed
const net_ap_t *app_ui_wifi_ap(int i);                  // NULL if out of range
bool app_ui_wifi_scanned(void);

// On-screen keyboard (password entry) in the page area. app_ui_kb_tap()
// returns 1 when Join was tapped; the text is then in app_ui_kb_text().
void app_ui_kb_open(const char *ssid);
int app_ui_kb_tap(int x, int y);
const char *app_ui_kb_text(void);
const char *app_ui_kb_ssid(void);

// T9 keypad for typed questions. app_ui_t9_tap() returns 1 when Ask was
// tapped (text in app_ui_t9_text()), 2 for cancel, 0 otherwise.
void app_ui_t9_open(void);
int app_ui_t9_tap(int x, int y);
const char *app_ui_t9_text(void);

// Scrolling (touch drag). While at the bottom the chat follows new text;
// after scrolling up it stays put until dragged back or a new question.
bool app_ui_convo_hit(int x, int y);
void app_ui_scroll_begin(void);
void app_ui_scroll_drag(int dy_px);          // dy since begin; + = finger moved down
// A tap (not a drag) in the page area: returns the tag of the tapped row
// (on the /model page: the model index), or -1.

int app_ui_convo_tap(int x, int y);

// Bottom bar: three pills.
typedef enum { BAR_MODEL = 0, BAR_ASK, BAR_SETTINGS, BAR_COUNT } app_bar_t;
typedef enum { BTN_IDLE = 0, BTN_PRESSED, BTN_BUSY, BTN_ACTIVE } app_btn_state_t;
int app_ui_bar_hit(int x, int y);            // -> app_bar_t or -1
void app_ui_bar_state(app_bar_t b, app_btn_state_t st);   // redraws only that pill, only on change

// Restart button (circular arrow, top-right of the header box).
bool app_ui_restart_hit(int x, int y);
void app_ui_restart_state(app_btn_state_t st);   // IDLE, PRESSED, ACTIVE (armed), BUSY

// Lock button (padlock, right end of the model row): sleeps the screen.
bool app_ui_lock_hit(int x, int y);
void app_ui_lock_state(app_btn_state_t st);

// Battery percentage in the header (0..100; -1 = unknown) with a bolt while
// charging. Redraws only on change.
void app_ui_battery(int pct, bool charging);
// Busy (session running): Ask shows busy, /model and /settings are disabled and
// the chat page is shown.
void app_ui_busy(bool busy);
bool app_ui_is_busy(void);

// Back-compat helpers for the Ask pill.
static inline void app_ui_button(app_btn_state_t st) { app_ui_bar_state(BAR_ASK, st); }
