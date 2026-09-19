#include "battery.h"
#include "app_ui.h"
#include "board.h"
#include "story_mem.h"
#include "esp_log.h"

static const char *TAG = "battery";

#define SAMPLE_US 5000000       // one reading every 5 s
#define LOG_US 60000000         // log the voltage once a minute

// Typical 1-cell LiPo resting-voltage curve (mV -> %).
static const struct { int mv, pct; } CURVE[] = {
    {4180, 100}, {4100, 92}, {4030, 85}, {3970, 77}, {3920, 69}, {3870, 60},
    {3830, 50},  {3800, 40}, {3775, 30}, {3750, 22}, {3720, 15}, {3690, 10},
    {3640, 5},   {3500, 2},  {3300, 0},
};

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

void battery_poll(bool busy)
{
    static int64_t s_next, s_next_log;
    static float s_mv;          // smoothed
    static int s_shown = -1;
    int64_t now = story_time_us();
    if (now < s_next || (busy && s_mv > 0)) return;   // first reading even if busy
    s_next = now + SAMPLE_US;

    int mv = board_battery_mv();
    if (mv <= 0) {
        app_ui_battery(-1);
        return;
    }
    s_mv = s_mv > 0 ? s_mv + 0.25f * (mv - s_mv) : (float)mv;
    int pct = battery_pct_from_mv((int)(s_mv + 0.5f));
    // Only move on a 2 % change so the number does not flicker between two values.
    if (s_shown < 0 || pct > s_shown + 1 || pct < s_shown - 1 || pct == 100 || pct == 0) {
        s_shown = pct;
        app_ui_battery(pct);
    }
    if (now >= s_next_log) {
        s_next_log = now + LOG_US;
        ESP_LOGI(TAG, "%d mV (smoothed %d) -> %d %%", mv, (int)(s_mv + 0.5f), s_shown);
    }
}
