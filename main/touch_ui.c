#include "touch_ui.h"
#include "app_ui.h"
#include "battery.h"
#include "board.h"
#include "models.h"
#include "net.h"
#include "prefs.h"
#include "pipeline.h"
#include "story_mem.h"
#include "ui.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "touch";

#define POLL_MS 20
#define DRAG_SLOP_PX 8          // movement before a touch counts as a scroll

static SemaphoreHandle_t s_ask;
static volatile bool s_screen = true;
static volatile int64_t s_activity;

// Asleep the screen is not blank: the logo drifts through colours at low
// brightness for SLEEP_SHOW_US, then the backlight goes off for good (a
// locked device on battery must not burn current on a screensaver).
#define SLEEP_BRIGHT 20
#define SLEEP_SHOW_US 60000000
static int64_t s_sleep_since;
static bool s_sleep_dark;

void screen_on(void)
{
    app_ui_sleep_exit();
    board_backlight(prefs()->brightness);
    s_screen = true;
    s_activity = story_time_us();
}

void screen_off(void)
{
    app_ui_sleep_enter();
    int b = prefs()->brightness < SLEEP_BRIGHT ? prefs()->brightness : SLEEP_BRIGHT;
    board_backlight((uint8_t)b);
    s_sleep_since = story_time_us();
    s_sleep_dark = false;
    s_screen = false;
}

bool screen_is_on(void) { return s_screen; }
int64_t screen_last_activity_us(void) { return s_activity; }
void screen_note_activity(void) { s_activity = story_time_us(); }
void touch_ui_set_busy(bool busy) { app_ui_busy(busy); }

void touch_ui_post_ask(void)
{
    if (s_ask) xSemaphoreGive(s_ask);
}

bool touch_ui_wait_ask(uint32_t timeout_ms)
{
    return xSemaphoreTake(s_ask, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

typedef enum { G_NONE, G_WAKE, G_BAR, G_RESTART, G_LOCK, G_PAGE_MAYBE, G_SCROLL, G_OTHER } gesture_t;

// Restart button: the first tap arms it (pink + prompt), a second tap
// within 3 s restarts; otherwise it disarms on its own.
#define RESTART_CONFIRM_US 3000000
static int64_t s_restart_armed;

static void restart_tap(void)
{
    int64_t now = story_time_us();
    if (s_restart_armed && now - s_restart_armed < RESTART_CONFIRM_US) {
        app_ui_status("Restarting...", UI_ERR);
        ESP_LOGW(TAG, "restart requested from the UI");
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    }
    s_restart_armed = now;
    app_ui_restart_state(BTN_ACTIVE);
    app_ui_status("Tap restart again to confirm", UI_ACCENT);
}

static void restart_expire(void)
{
    if (s_restart_armed && story_time_us() - s_restart_armed >= RESTART_CONFIRM_US) {
        s_restart_armed = 0;
        app_ui_restart_state(app_ui_is_busy() ? BTN_BUSY : BTN_IDLE);
        if (!app_ui_is_busy()) app_ui_status("Ready", UI_OK);
    }
}

// Restore a pill after a press: Ask -> idle, page pills -> active if open.
static void bar_release_look(int b)
{
    if (app_ui_is_busy()) { app_ui_bar_state((app_bar_t)b, BTN_BUSY); return; }
    app_page_t p = app_ui_page_get();
    if (b == BAR_ASK && app_ui_typing()) { app_ui_bar_state(BAR_ASK, BTN_BUSY); return; }
    bool active = (b == BAR_MODEL && p == PAGE_MODELS) || (b == BAR_SETTINGS && app_page_in_settings(p));
    app_ui_bar_state((app_bar_t)b, active ? BTN_ACTIVE : BTN_IDLE);
}

static void bar_action(int b)
{
    switch (b) {
    case BAR_ASK:
        if (app_ui_is_busy()) {
            ESP_LOGW(TAG, "stop requested");
            app_ui_status("Stopping...", UI_ERR);
            pipeline_cancel();
        } else {
            ESP_LOGI(TAG, "ask");
            xSemaphoreGive(s_ask);
        }
        break;
    case BAR_MODEL:
        if (app_ui_page_get() != PAGE_MODELS) models_scan();   // pick up newly copied models
        app_ui_page(app_ui_page_get() == PAGE_MODELS ? PAGE_CHAT : PAGE_MODELS);
        break;
    case BAR_SETTINGS: {
        // /settings closes itself; from a sub-page it goes back one level.
        app_page_t p = app_ui_page_get();
        app_ui_page(p == PAGE_SETTINGS ? PAGE_CHAT : p == PAGE_KEYBOARD ? PAGE_WIFI : PAGE_SETTINGS);
        break;
    }
    }
}

static void join(const char *ssid, const char *pass)
{
    char st[48];
    if (!net_set_credentials(ssid, pass)) {
        app_ui_status("Network name or password too long", UI_ERR);
        return;
    }
    snprintf(st, sizeof st, "Joining %.30s", ssid);
    app_ui_status(st, UI_INFO);
    net_request_sync();                  // the main task connects and sets the clock
    app_ui_page(PAGE_WIFI);
}

static void wifi_tap(int tag)
{
    if (app_ui_is_busy()) return;
    const net_ap_t *ap = app_ui_wifi_ap(tag);
    if (ap) {
        if (ap->open) join(ap->ssid, "");
        else app_ui_kb_open(ap->ssid);
    } else if (tag == TAG_WIFI_SYNC) {
        net_request_sync();
        app_ui_status("Setting clock" UI_G_ELLIPSIS, UI_INFO);
    } else if (tag == TAG_WIFI_FORGET) {
        net_clear_credentials();
        app_ui_status("Wi-Fi forgotten", UI_OK);
        app_ui_refresh_page();
    } else if (tag == TAG_WIFI_RESCAN) {
        net_request_scan();
        app_ui_wifi_scanning();
    }
}

// Volume preview: set on every volume step, consumed by the main loop once
// the taps have stopped for VOLUME_PREVIEW_US (one "Hi there", not one per tap).
#define VOLUME_PREVIEW_US 600000
static volatile int64_t s_volume_changed;

bool touch_ui_take_volume_preview(void)
{
    int64_t t = s_volume_changed;
    if (!t || story_time_us() - t < VOLUME_PREVIEW_US) return false;
    s_volume_changed = 0;
    return true;
}

// Typed question (T9 Ask), consumed by the main task.
static char s_typed[128];
static volatile bool s_typed_ready;

static volatile bool s_demo_ready, s_demo_full;

bool touch_ui_take_demo(bool *full)
{
    if (!s_demo_ready) return false;
    s_demo_ready = false;
    *full = s_demo_full;
    return true;
}

bool touch_ui_take_typed(char *out, size_t cap)
{
    if (!s_typed_ready) return false;
    snprintf(out, cap, "%s", s_typed);
    s_typed_ready = false;
    return true;
}

static void settings_tap(int tag, int col)
{
    int dir = col >= SET_PLUS_COL_MIN ? 1 : (col >= SET_MINUS_COL_MIN && col <= SET_MINUS_COL_MAX) ? -1 : 0;
    ESP_LOGI(TAG, "settings tap: tag %d col %d dir %d", tag, col, dir);
    switch (tag) {
    case TAG_WIFI_SETUP:
        app_ui_page(PAGE_WIFI);
        if (!app_ui_wifi_scanned()) {       // first visit: look for networks
            net_request_scan();
            app_ui_wifi_scanning();
        }
        return;
    case TAG_SET_VOLUME:
        if (!dir) return;
        board_audio_set_volume(prefs_step_volume(dir));
        s_volume_changed = story_time_us();  // main loop says "Hi there" at the new level
        break;
    case TAG_SET_GAIN:
        if (!dir) return;
        prefs_step_voice_gain(dir);
        s_volume_changed = story_time_us();  // hear the new gain too
        break;
    case TAG_SET_BRIGHT:
        if (!dir) return;
        board_backlight(prefs_step_brightness(dir));
        break;
    case TAG_SET_SCREEN:
        prefs_cycle_screen();
        break;
    case TAG_SET_WAKE:
        prefs_toggle_wake();                // the main loop starts/stops WakeNet
        break;
    case TAG_SET_ABOUT:
        app_ui_page(PAGE_ABOUT);
        return;
    case TAG_SET_DEMO:
    case TAG_SET_DEMO_FULL:
        s_demo_full = tag == TAG_SET_DEMO_FULL;
        s_demo_ready = true;                // the main loop owns the arenas
        app_ui_page(PAGE_CHAT);
        return;
    default:
        return;
    }
    app_ui_refresh_page();                  // dirty rows only: just the changed value
}

static void page_tap(int x, int y)
{
    app_page_t page = app_ui_page_get();
    if (page == PAGE_KEYBOARD) {
        if (app_ui_kb_tap(x, y) == 1) join(app_ui_kb_ssid(), app_ui_kb_text());
        return;
    }
    if (page == PAGE_T9) {
        int r = app_ui_t9_tap(x, y);
        if (r == 1) {                        // Ask: hand the text to the main task
            snprintf(s_typed, sizeof s_typed, "%s", app_ui_t9_text());
            s_typed_ready = true;
        } else if (r == 2) {
            app_ui_page(PAGE_CHAT);
        }
        return;
    }
    int tag = app_ui_convo_tap(x, y);
    ESP_LOGI(TAG, "tap (%d,%d) page %d tag %d", x, y, (int)page, tag);
    if (page == PAGE_SETTINGS) {
        settings_tap(tag, app_ui_tap_col(x));
        return;
    }
    if (page == PAGE_WIFI) {
        if (tag >= 0) wifi_tap(tag);
        return;
    }
    if (page == PAGE_CHAT && tag == TAG_TYPE && !app_ui_is_busy()) {
        app_ui_t9_open();
        return;
    }
    if (app_ui_page_get() == PAGE_MODELS && tag >= 0 && !app_ui_is_busy() && tag != models_active()) {
        if (models_select(tag)) {
            char st[48];
            snprintf(st, sizeof st, "Model: %s", models_get(tag)->name);
            app_ui_status(st, UI_OK);
            app_ui_model_changed();
            app_ui_refresh_page();
        }
    }
}

static void touch_task(void *arg)
{
    (void)arg;
    gesture_t g = G_NONE;
    int x0 = 0, y0 = 0, bar = -1;
    bool down = false;
    for (;;) {
        int x = 0, y = 0;
        bool t = board_touch_read(&x, &y);
        if (t && !down) {                                   // touch down
            s_activity = story_time_us();
            x0 = x;
            y0 = y;
            if (!s_screen) {
                screen_on();
                g = G_WAKE;                                 // this touch only wakes
            } else if (app_ui_restart_hit(x, y)) {
                g = app_ui_is_busy() ? G_OTHER : G_RESTART;  // ignored during a session
                if (g == G_RESTART) app_ui_restart_state(BTN_PRESSED);
            } else if (app_ui_lock_hit(x, y)) {
                g = app_ui_is_busy() ? G_OTHER : G_LOCK;     // ignored during a session
                if (g == G_LOCK) app_ui_lock_state(BTN_PRESSED);
            } else if ((bar = app_ui_bar_hit(x, y)) >= 0) {
                // While busy only the Ask pill works: it is the Stop button.
                // While typing it is the one pill that doesn't: the keypad's
                // own Ask key sends the question.
                g = (app_ui_is_busy() && bar != BAR_ASK) || (bar == BAR_ASK && app_ui_typing())
                        ? G_OTHER : G_BAR;
                if (g == G_BAR) app_ui_bar_state((app_bar_t)bar, BTN_PRESSED);
            } else if (app_ui_convo_hit(x, y)) {
                g = G_PAGE_MAYBE;
                app_ui_scroll_begin();
            } else {
                g = G_OTHER;
            }
        } else if (t && down) {                             // move
            s_activity = story_time_us();
            int dy = y - y0;
            app_page_t pg = app_ui_page_get();
            if (g == G_PAGE_MAYBE && abs(dy) > DRAG_SLOP_PX && pg != PAGE_KEYBOARD && pg != PAGE_T9) g = G_SCROLL;
            if (g == G_SCROLL) app_ui_scroll_drag(dy);
            if (g == G_BAR && app_ui_bar_hit(x, y) != bar) {  // slid off: cancel
                bar_release_look(bar);
                g = G_OTHER;
            }
        } else if (!t && down) {                            // release
            if (g == G_BAR) {
                bar_release_look(bar);
                bar_action(bar);
            } else if (g == G_RESTART) {
                restart_tap();
            } else if (g == G_LOCK) {
                app_ui_lock_state(BTN_IDLE);
                screen_off();                               // sleep now, don't wait for the timeout
            } else if (g == G_PAGE_MAYBE) {
                page_tap(x0, y0);                           // a tap, not a drag
            }
            g = G_NONE;
        }
        down = t;
        restart_expire();
        if (!s_screen && !s_sleep_dark) {                   // sleeping: drift the logo, then go dark
            if (story_time_us() - s_sleep_since < SLEEP_SHOW_US) app_ui_sleep_tick();
            else { board_backlight(0); s_sleep_dark = true; }
        }
        app_ui_tick();                                      // spark pulse + spinner while the AI works
        if (battery_poll(app_ui_is_busy())) {               // charger connected: wake up
            if (!s_screen) screen_on();
            else s_activity = story_time_us();
            if (!app_ui_is_busy()) app_ui_status("Charging", UI_OK);
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void touch_ui_start(void)
{
    s_ask = xSemaphoreCreateBinary();
    s_activity = story_time_us();
    // Core 0 (the STT/LLM co-workers use the other core); modest priority.
    xTaskCreatePinnedToCore(touch_task, "touch_ui", 4096, NULL, 4, NULL, 0);
}

// Serial "tap x y": drives the same handlers a finger does, including the
// bottom pills and the header buttons, so a scripted test is not a special case.
void touch_ui_sim_tap(int x, int y)
{
    int bar = app_ui_bar_hit(x, y);
    if (bar >= 0) {
        if (!app_ui_is_busy() || bar == BAR_ASK) bar_action(bar);
    } else if (app_ui_lock_hit(x, y) && !app_ui_is_busy()) {
        screen_off();
    } else {
        page_tap(x, y);
    }
}
