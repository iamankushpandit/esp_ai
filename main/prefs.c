#include "prefs.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "prefs";
static prefs_t s_p = {.volume = 90, .brightness = 90, .screen_s = 30, .wake = true, .voice_gain_db = 6};

static const uint16_t SCREEN_STEPS[] = {15, 30, 60, 120, 300, 0};

prefs_t *prefs(void) { return &s_p; }

void prefs_load(void)
{
    nvs_handle_t h;
    if (nvs_open("braino", NVS_READONLY, &h) == ESP_OK) {
        uint8_t u8;
        uint16_t u16;
        if (nvs_get_u8(h, "vol", &u8) == ESP_OK && u8 >= 10 && u8 <= 100) s_p.volume = u8;
        if (nvs_get_u8(h, "bright", &u8) == ESP_OK && u8 >= 10 && u8 <= 100) s_p.brightness = u8;
        if (nvs_get_u16(h, "scr_s", &u16) == ESP_OK) s_p.screen_s = u16;
        if (nvs_get_u8(h, "wake", &u8) == ESP_OK) s_p.wake = u8 != 0;
        if (nvs_get_u8(h, "vgain", &u8) == ESP_OK && u8 <= 12) s_p.voice_gain_db = (int8_t)u8;
        nvs_close(h);
    }
    ESP_LOGI(TAG, "volume %d%%, brightness %d%%, screen off %us, wake word %s, voice gain +%d dB", s_p.volume,
             s_p.brightness, s_p.screen_s, s_p.wake ? "on" : "off", s_p.voice_gain_db);
}

void prefs_save(void)
{
    nvs_handle_t h;
    if (nvs_open("braino", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "vol", s_p.volume);
    nvs_set_u8(h, "bright", s_p.brightness);
    nvs_set_u16(h, "scr_s", s_p.screen_s);
    nvs_set_u8(h, "wake", s_p.wake);
    nvs_set_u8(h, "vgain", (uint8_t)s_p.voice_gain_db);
    nvs_commit(h);
    nvs_close(h);
}

static int step(uint8_t *v, int dir)
{
    int n = *v + dir * 10;
    *v = (uint8_t)(n < 10 ? 10 : n > 100 ? 100 : n);
    prefs_save();
    return *v;
}

int prefs_step_volume(int dir) { return step(&s_p.volume, dir); }
int prefs_step_brightness(int dir) { return step(&s_p.brightness, dir); }

uint16_t prefs_cycle_screen(void)
{
    const int n = sizeof SCREEN_STEPS / sizeof SCREEN_STEPS[0];
    int i = 0;
    while (i < n && SCREEN_STEPS[i] != s_p.screen_s) i++;
    s_p.screen_s = SCREEN_STEPS[(i + 1) % n];
    prefs_save();
    return s_p.screen_s;
}

bool prefs_toggle_wake(void)
{
    s_p.wake = !s_p.wake;
    prefs_save();
    return s_p.wake;
}

int prefs_step_voice_gain(int dir)
{
    int g = s_p.voice_gain_db + dir * 3;
    s_p.voice_gain_db = (int8_t)(g < 0 ? 0 : g > 12 ? 12 : g);
    prefs_save();
    return s_p.voice_gain_db;
}
