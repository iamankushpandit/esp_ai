#include "touch_ui.h"
#include "app_ui.h"
#include "battery.h"
#include "board.h"
#include "models.h"
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
#define BACKLIGHT_PCT 80

static SemaphoreHandle_t s_ask;
static volatile bool s_screen = true;
static volatile int64_t s_activity;

void screen_on(void)
{
    board_backlight(BACKLIGHT_PCT);
    s_screen = true;
    s_activity = story_time_us();
}

void screen_off(void)
{
    board_backlight(0);
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

typedef enum { G_NONE, G_WAKE, G_BAR, G_RESTART, G_PAGE_MAYBE, G_SCROLL, G_OTHER } gesture_t;

// Restart button: the first tap arms it (orange + prompt), a second tap
// within 3 s restarts; otherwise it disarms on its own.
#define RESTART_CONFIRM_US 3000000
static int64_t s_restart_armed;

static void restart_tap(void)
{
    int64_t now = story_time_us();
    if (s_restart_armed && now - s_restart_armed < RESTART_CONFIRM_US) {
        app_ui_status("Restarting...", UI_RED);
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
        if (!app_ui_is_busy()) app_ui_status("Ready", UI_GREEN);
    }
}

// Restore a pill after a press: Ask -> idle, page pills -> active if open.
static void bar_release_look(int b)
{
    if (app_ui_is_busy()) { app_ui_bar_state((app_bar_t)b, BTN_BUSY); return; }
    app_page_t p = app_ui_page_get();
    bool active = (b == BAR_MODEL && p == PAGE_MODELS) || (b == BAR_ABOUT && p == PAGE_ABOUT);
    app_ui_bar_state((app_bar_t)b, active ? BTN_ACTIVE : BTN_IDLE);
}

static void bar_action(int b)
{
    switch (b) {
    case BAR_ASK:
        ESP_LOGI(TAG, "ask");
        xSemaphoreGive(s_ask);
        break;
    case BAR_MODEL:
        if (app_ui_page_get() != PAGE_MODELS) models_scan();   // pick up newly copied models
        app_ui_page(app_ui_page_get() == PAGE_MODELS ? PAGE_CHAT : PAGE_MODELS);
        break;
    case BAR_ABOUT:
        app_ui_page(app_ui_page_get() == PAGE_ABOUT ? PAGE_CHAT : PAGE_ABOUT);
        break;
    }
}

static void page_tap(int x, int y)
{
    int tag = app_ui_convo_tap(x, y);
    if (app_ui_page_get() == PAGE_MODELS && tag >= 0 && !app_ui_is_busy() && tag != models_active()) {
        if (models_select(tag)) {
            char st[48];
            snprintf(st, sizeof st, "Model: %s", models_get(tag)->name);
            app_ui_status(st, UI_GREEN);
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
            } else if ((bar = app_ui_bar_hit(x, y)) >= 0) {
                g = app_ui_is_busy() ? G_OTHER : G_BAR;     // bar is disabled while busy
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
            if (g == G_PAGE_MAYBE && abs(dy) > DRAG_SLOP_PX) g = G_SCROLL;
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
            } else if (g == G_PAGE_MAYBE) {
                page_tap(x0, y0);                           // a tap, not a drag
            }
            g = G_NONE;
        }
        down = t;
        restart_expire();
        battery_poll(app_ui_is_busy());
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
