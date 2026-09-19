#include "battery.h"
#include "app_ui.h"
#include "board.h"
#include "story_mem.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"

static const char *TAG = "battery";

#define SAMPLE_US 1000000       // one reading a second
#define LOG_US 60000000         // log the voltage once a minute
#define STEP_WINDOW 4           // compare against the reading 4 s ago
#define BAT_STEP_MV 45          // plug/unplug voltage step (charge current ~300 mA)
#define SETTLE_US 5000000       // ignore steps this long after a session (sag recovery)

// Typical 1-cell LiPo resting-voltage curve (mV -> %).
static const struct { int mv, pct; } CURVE[] = {
    {4180, 100}, {4100, 92}, {4030, 85}, {3970, 77}, {3920, 69}, {3870, 60},
    {3830, 50},  {3800, 40}, {3775, 30}, {3750, 22}, {3720, 15}, {3690, 10},
    {3640, 5},   {3500, 2},  {3300, 0},
};

static bool s_host, s_volt_chg;     // USB host seen / voltage step says charging

int battery_pct_from_mv(int mv)
{
    const int n = sizeof CURVE / sizeof CURVE[0];
    if (mv >= CURVE[0].mv) return 100;
    if (mv <= CURVE[n - 1].mv) return 0;
    for (int i = 1; i < n; i++)
        if (mv >= CURVE[i].mv)
            return CURVE[i].pct + (mv - CURVE[i].mv) * (CURVE[i - 1].pct - CURVE[i].pct) /
                                      (CURVE[i - 1].mv - CURVE[i].mv);
    return 0;
}

bool battery_charging(void) { return s_host || s_volt_chg; }

bool battery_poll(bool busy)
{
    static int64_t s_next, s_next_log, s_quiet_since;
    static float s_mv;              // slow average for the percentage
    static int s_hist[STEP_WINDOW + 1], s_nhist;
    static int s_shown = -1;
    static bool s_was_busy, s_was_chg;
    int64_t now = story_time_us();
    if (now < s_next) return false;
    s_next = now + SAMPLE_US;

    s_host = usb_serial_jtag_is_connected();
    if (!s_host && s_was_chg && !s_volt_chg) s_nhist = 0;   // host just left: re-baseline

    if (busy) {
        s_was_busy = true;
    } else {
        if (s_was_busy) {                                   // session just ended
            s_was_busy = false;
            s_quiet_since = now;
            s_nhist = 0;
        }
        int mv = board_battery_mv();
        if (mv > 0) {
            s_mv = s_mv > 0 ? s_mv + 0.1f * (mv - s_mv) : (float)mv;
            // Plug / unplug: a step against the reading STEP_WINDOW seconds ago.
            if (s_nhist == STEP_WINDOW + 1 && now - s_quiet_since > SETTLE_US && !s_host) {
                int step = mv - s_hist[0];
                if (!s_volt_chg && step >= BAT_STEP_MV) {
                    s_volt_chg = true;
                    ESP_LOGI(TAG, "voltage +%d mV: charger connected", step);
                } else if (s_volt_chg && step <= -BAT_STEP_MV) {
                    s_volt_chg = false;
                    ESP_LOGI(TAG, "voltage %d mV: charger removed", step);
                }
                if (step >= BAT_STEP_MV || step <= -BAT_STEP_MV) {
                    s_mv = (float)mv;                       // jump the average too
                    s_nhist = 0;
                }
            }
            if (s_nhist == STEP_WINDOW + 1) {
                for (int i = 0; i < STEP_WINDOW; i++) s_hist[i] = s_hist[i + 1];
                s_nhist--;
            }
            s_hist[s_nhist++] = mv;
        }
        if (now >= s_next_log) {
            s_next_log = now + LOG_US;
            ESP_LOGI(TAG, "%d mV (avg %d) -> %d %%%s", mv, (int)(s_mv + 0.5f), s_shown,
                     battery_charging() ? ", charging" : "");
        }
    }
    if (s_host) s_volt_chg = false;                         // host is the better signal

    bool chg = battery_charging();
    bool plugged = chg && !s_was_chg;
    if (plugged) ESP_LOGI(TAG, "power connected (%s)", s_host ? "USB host" : "voltage step");
    int pct = s_mv > 0 ? battery_pct_from_mv((int)(s_mv + 0.5f)) : -1;
    // Only move on a 2 % change so the number does not flicker between two values.
    if (chg != s_was_chg || s_shown < 0 || pct > s_shown + 1 || pct < s_shown - 1 ||
        (pct != s_shown && (pct == 100 || pct == 0))) {
        s_shown = pct;
        app_ui_battery(pct, chg);
    }
    s_was_chg = chg;
    return plugged;
}
