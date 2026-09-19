// Host preview of the device screen: the real ui.c / app_ui.c code drawing
// into a 240x320 RGB565 buffer (LCD calls stubbed), dumped as PPM frames.
// Also counts pixels pushed per update to verify dirty-row behavior.
#include "board.h"
#include "board_lcd_stream.h"
#include "app_ui.h"
#include "ui.h"
#include "models.h"
#include <stdio.h>
#include <string.h>

// Model registry stubs (the real one scans the SD card).
static model_info_t s_m[3] = {
    {"/sd/story/llm", "TinyTalk 3M", 2363073},
    {"/sd/story/llm8m", "TinyTalk 2 8M", 5953257},
    {"/sd/story/llm8m_kid", "TinyTalk 2 8M (kid+Gume)", 6710021},
};
static int s_act = 2;
int models_count(void) { return 3; }
const model_info_t *models_get(int i) { return &s_m[i]; }
int models_active(void) { return s_act; }
bool models_select(int i) { s_act = i; return true; }
int models_scan(void) { return 3; }

// Clock / network stubs (the device versions live in main/builtin.c, main/net.c).
#include "builtin.h"
#include "net.h"
void clock_short(char *out, size_t cap) { snprintf(out, cap, "Sat 12:51 PM"); }
static bool s_wifi_set = true;
bool net_has_credentials(void) { return s_wifi_set; }
const char *net_ssid(void) { return "HomeNet"; }
const char *net_last_msg(void) { return "Clock set"; }
#include "prefs.h"
static prefs_t s_prefs = {.volume = 70, .brightness = 80, .screen_s = 30, .wake = true};
prefs_t *prefs(void) { return &s_prefs; }

static uint16_t fb[BOARD_LCD_H][BOARD_LCD_W];
static uint16_t dma[BOARD_LCD_STREAM_PX];
static int wx, wy, ww, wpos;
static long pushed;

void board_lcd_window(int x, int y, int w, int h) { (void)h; wx = x; wy = y; ww = w; wpos = 0; }
uint16_t *board_lcd_stream_buf(void) { return dma; }
void board_lcd_stream_push(uint16_t *buf, size_t n)
{
    for (size_t i = 0; i < n; i++, wpos++) {
        int x = wx + wpos % ww, y = wy + wpos / ww;
        uint16_t c = (uint16_t)((buf[i] >> 8) | (buf[i] << 8));   // undo byte swap
        if (x < BOARD_LCD_W && y < BOARD_LCD_H) fb[y][x] = c;
    }
    pushed += (long)n;
}
void board_lcd_stream_end(void) {}
void board_lcd_fill(int x, int y, int w, int h, uint16_t c)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) fb[j][i] = c;
    pushed += (long)w * h;
}

static const char *s_dir;
static int s_frame;

static void dump(void)
{
    char path[256];
    snprintf(path, sizeof path, "%s/ui_%d.ppm", s_dir, s_frame++);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", BOARD_LCD_W, BOARD_LCD_H);
    for (int y = 0; y < BOARD_LCD_H; y++)
        for (int x = 0; x < BOARD_LCD_W; x++) {
            uint16_t c = fb[y][x];
            unsigned char px[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2),
                                   (unsigned char)((c & 31) << 3)};
            fwrite(px, 1, 3, f);
        }
    fclose(f);
}

#define STEP(label, code) do { pushed = 0; code; \
    printf("%-34s repainted %5ld px (%4.1f%% of screen)\n", label, pushed, 100.0 * pushed / (BOARD_LCD_W * BOARD_LCD_H)); } while (0)

static void stream(const char *ans, int tok, float rate)
{
    char part[512];
    int n = (int)strlen(ans);
    for (int k = 14, i = 1; k < n; k += 18, i++) {
        memcpy(part, ans, (size_t)k);
        part[k] = 0;
        STEP("  stream update", app_ui_llm_progress(part, i * 3, rate));
    }
    STEP("  answer final", app_ui_llm_progress(ans, tok, rate));
}

int main(int argc, char **argv)
{
    s_dir = argc > 1 ? argv[1] : ".";
    STEP("splash", app_ui_splash());
    dump();
    STEP("boot (init)", app_ui_init(); app_ui_status("Ready", UI_OK); app_ui_battery(87, false));
    dump();

    STEP("restart armed", app_ui_restart_state(BTN_ACTIVE));
    dump();
    STEP("restart idle", app_ui_restart_state(BTN_IDLE));
    STEP("battery low", app_ui_battery(12, false));
    dump();
    STEP("charging", app_ui_battery(64, true));
    STEP("/model page", app_ui_page(PAGE_MODELS));
    dump();
    STEP("select model 0", models_select(0); app_ui_refresh_page());
    STEP("/settings page", app_ui_page(PAGE_SETTINGS));
    dump();
    STEP("volume +", s_prefs.volume = 80; app_ui_refresh_page());
    STEP("about page", app_ui_page(PAGE_ABOUT));
    dump();
    {
        static const net_ap_t aps[] = {{"HomeNet", -52, false}, {"Neighbors 5G", -68, false},
                                       {"CoffeeShop", -80, true}};
        STEP("wifi page (scanning)", app_ui_page(PAGE_WIFI); app_ui_wifi_scanning());
        STEP("wifi page (results)", app_ui_wifi_results(aps, 3));
        dump();
        STEP("keyboard", app_ui_kb_open("HomeNet"));
        STEP("type p", app_ui_kb_tap(9 * 24 + 5, 62 + 32 + 26 + 5));
        STEP("type w", app_ui_kb_tap(1 * 24 + 5, 62 + 32 + 26 + 5));
        dump();
        STEP("back to wifi", app_ui_page(PAGE_WIFI));
    }
    STEP("back to chat", app_ui_page(PAGE_CHAT));
    STEP("ask pressed", app_ui_button(BTN_PRESSED));
    dump();
    STEP("turn 1: listening", app_ui_busy(true); app_ui_clear_turn(); app_ui_status("Listening...", UI_ACCENT));
    STEP("turn 1: transcribing", app_ui_status("Transcribing...", UI_ACCENT));
    STEP("turn 1: transcript", app_ui_you("what color is a banana"));
    stream("I know! The banana is yellow.", 8, 6.1f);
    STEP("turn 1: speaking", app_ui_status("Speaking...", UI_ACCENT));
    STEP("follow-up 1 listening", app_ui_status("Listening... follow-up 1/3", UI_ACCENT));
    STEP("turn 2: transcript", app_ui_you("how many legs does a dog have"));
    stream("A dog has four legs.", 6, 6.3f);
    STEP("turn 3: built-in", app_ui_you("what time is it"); app_ui_builtin("It is 12:51 PM.", "device clock"));
    dump();

    STEP("follow-up 2 listening", app_ui_status("Listening... follow-up 2/3", UI_ACCENT));
    STEP("turn 3: transcript (scrolls)", app_ui_you("tell me a story about a cat"));
    stream("Once upon a time, there was a big, strong cat. The cat saw a little mouse. "
           "They became good friends. Bye bye.", 34, 6.4f);
    STEP("turn 3: speaking", app_ui_status("Speaking...", UI_ACCENT));
    dump();
    STEP("scroll: drag down 3 rows", app_ui_scroll_begin(); app_ui_scroll_drag(3 * UI_ROW_H));
    STEP("scroll: drag further (to top)", app_ui_scroll_drag(20 * UI_ROW_H));
    dump();
    STEP("new text while scrolled (no jump)", app_ui_llm_progress("Once upon a time, there was a big, strong cat. The cat saw a little mouse. They became good friends. Bye bye!", 35, 6.4f));
    STEP("scroll back to bottom", app_ui_scroll_begin(); app_ui_scroll_drag(-40 * UI_ROW_H));
    STEP("session end", app_ui_status("Session ended", UI_GREY); app_ui_busy(false));
    return 0;
}
