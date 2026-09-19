// The single assistant screen (portrait 240x320), styled like a CLI session:
//
//   ╭────────────────────────────╮
//   │ [leaf]  Ivy AI             │
//   │   (c) iamankushpandit      │
//   ╰────────────────────────────╯
//   > what color is a banana       ▐ <- scrollable page area: the session
//   ● The banana is yellow.        ▐    transcript (chat), the /model list,
//                                  ▐    or /about. Thin scrollbar on the right.
//   > how many legs does a dog have
//   ● A dog has four legs.
//   ⎿  8 tok · 6.2 tok/s              <- dim detail line
//   ✻ Thinking…                      <- status / spinner
//   ( /model ) (  ✻ Ask  ) ( /about ) <- bottom bar (tap)
//
// Every region is a dirty-row box: only rows whose text changed are redrawn,
// in place (no clears). Scrolling rewrites the rows that shifted.
#pragma once
#include <stdbool.h>
#include <stdint.h>

// Boot welcome screen: the Ivy AI logo + "(c) iamankushpandit".
void app_ui_splash(void);
void app_ui_init(void);
// Status line. Busy states (any color other than UI_GREEN/UI_GREY) get an
// advancing spinner glyph; "..." is shown as an ellipsis.
void app_ui_status(const char *s, uint16_t color);
// Starts a new turn in the transcript with the recognized question.
void app_ui_you(const char *text);
// Sets (or streams) the answer of the latest turn.
void app_ui_story(const char *text);
// Answer produced by plain C code (clock, name), not the model: drawn with a
// cyan diamond instead of the pink bullet, detail line "<source> . no AI".
void app_ui_builtin(const char *text, const char *source);
void app_ui_detail(const char *text);
// Clears the whole transcript (new session).
void app_ui_clear_turn(void);
// Streaming answer + live generation speed on the detail line.
void app_ui_llm_progress(const char *text, int tokens, float tok_per_s);
// Last generation speed shown (for keeping it visible while speaking).
float app_ui_last_tok_rate(void);

// Pages shown in the scrollable area.
typedef enum { PAGE_CHAT = 0, PAGE_MODELS, PAGE_ABOUT, PAGE_WIFI, PAGE_KEYBOARD } app_page_t;
void app_ui_page(app_page_t p);            // switch page (re-renders the area)
app_page_t app_ui_page_get(void);
void app_ui_refresh_page(void);            // e.g. after the model list changed

// Row tags on the /about and Wi-Fi pages (network rows use their index 0..).
#define TAG_WIFI_SETUP 100                 // /about: "Wi-Fi & clock" row
#define TAG_WIFI_SYNC 101
#define TAG_WIFI_FORGET 102
#define TAG_WIFI_RESCAN 103

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

// Scrolling (touch drag). While at the bottom the chat follows new text;
// after scrolling up it stays put until dragged back or a new question.
bool app_ui_convo_hit(int x, int y);
void app_ui_scroll_begin(void);
void app_ui_scroll_drag(int dy_px);          // dy since begin; + = finger moved down
// A tap (not a drag) in the page area: returns the tag of the tapped row
// (on the /model page: the model index), or -1.

int app_ui_convo_tap(int x, int y);

// Bottom bar: three pills.
typedef enum { BAR_MODEL = 0, BAR_ASK, BAR_ABOUT, BAR_COUNT } app_bar_t;
typedef enum { BTN_IDLE = 0, BTN_PRESSED, BTN_BUSY, BTN_ACTIVE } app_btn_state_t;
int app_ui_bar_hit(int x, int y);            // -> app_bar_t or -1
void app_ui_bar_state(app_bar_t b, app_btn_state_t st);   // redraws only that pill, only on change

// Restart button (circular arrow, top-right of the header box).
bool app_ui_restart_hit(int x, int y);
void app_ui_restart_state(app_btn_state_t st);   // IDLE, PRESSED, ACTIVE (armed), BUSY

// Battery percentage in the header (0..100; -1 = unknown) with a bolt while
// charging. Redraws only on change.
void app_ui_battery(int pct, bool charging);
// Busy (session running): Ask shows busy, /model and /about are disabled and
// the chat page is shown.
void app_ui_busy(bool busy);
bool app_ui_is_busy(void);

// Back-compat helpers for the Ask pill.
static inline void app_ui_button(app_btn_state_t st) { app_ui_bar_state(BAR_ASK, st); }
