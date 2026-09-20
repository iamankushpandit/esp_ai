#include "app_ui.h"
#include "board.h"
#include "ui.h"
#include "board_lcd_stream.h"
#include "models.h"
#include "builtin.h"
#include "prefs.h"
#include "story_mem.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef STORY_HOST
#include <stdlib.h>
#define UI_LOCK() ((void)0)
#define UI_UNLOCK() ((void)0)
#define UI_ALLOC(n) calloc(1, (n))
#else
#include "esp_heap_caps.h"
#define UI_ALLOC(n) heap_caps_calloc(1, (n), MALLOC_CAP_SPIRAM)
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// The UI is driven from the app task AND the touch task (scrolling, bar), so
// every public entry point takes this recursive lock: LCD writes never interleave.
static SemaphoreHandle_t s_lock;
static void ui_lock_take(void)
{
    // Created on first use: the splash screen draws before app_ui_init().
    if (!s_lock) s_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
}
#define UI_LOCK() ui_lock_take()
#define UI_UNLOCK() xSemaphoreGiveRecursive(s_lock)
#endif

#define ROW_Y(r) (2 + (r) * UI_ROW_H)
#define COLS (BOARD_LCD_W / UI_FONT_W)   // 30

#define MAX_TURNS 4
#define Q_CHARS 160
#define A_CHARS 480

// Page area: a window of VIS rows onto the wrapped page content.
#define CONVO_Y (ROW_Y(5) + 4)
#define VIS 12
#define CONVO_W (BOARD_LCD_W - 8)        // 29 text columns + scrollbar gutter
#define SB_X (BOARD_LCD_W - 5)
#define SB_W 3
#define MAX_ROWS 96

// Bottom bar geometry.
#define BAR_Y 280
#define BAR_H 26
#define BAR_TOUCH_PAD_Y 12
static const struct { int x, w; const char *label; bool spark; } BAR[BAR_COUNT] = {
    [BAR_MODEL] = {4, 66, "/model", false},
    [BAR_ASK] = {74, 72, "Ask", true},
    [BAR_SETTINGS] = {150, 86, "/settings", false},
};
#define BAR_MAX_W 86
#define ASK_BUSY_LABEL "Stop"           // the Ask pill while a request runs
#define BTN_PX_MAX (80 * 32)             // pixel scratch: pills, keys, icons

static ui_box_t s_status, s_convo, s_detail;
static int s_spin;
static float s_last_rate;

// Session transcript (bounded: at most MAX_TURNS turns, oldest dropped).
// All larger UI buffers live in PSRAM (allocated in app_ui_init): internal
// RAM must keep a contiguous 264 KB block free for the phase arena.
typedef struct {
    char q[Q_CHARS];
    char a[A_CHARS];
    bool builtin;                  // answered by plain C code (clock, name), not the model
} turn_t;
static turn_t *s_turns;
static int s_nturns;

static char (*s_rows)[UI_MAX_COLS + 1];            // [MAX_ROWS] wrapped page rows
static signed char *s_tags;                         // [MAX_ROWS] row -> tag (-1 none)
static char (*s_tmp)[UI_MAX_COLS + 1];             // [MAX_ROWS] wrap scratch
static char *s_text;                                // scratch for building pages
#define TEXT_BYTES (MAX_TURNS * (Q_CHARS + A_CHARS + 8) + 256)
static uint16_t *s_btn_px;                          // pill pixels
static int s_total;              // wrapped rows available
static int s_top;                // first visible row
static bool s_follow = true;     // chat: stick to the bottom as text arrives
static int s_drag_top0;
static int s_sb_top = -1, s_sb_len = -1;
static app_page_t s_page = PAGE_CHAT;
static bool s_busy;
static int s_bar_state[BAR_COUNT] = {-1, -1, -1};

// Copy model/STT text, keeping plain ASCII only (UI glyph codes live above
// 0x7F, so stray UTF-8 bytes must never reach the renderer).
static void sanitize(char *dst, size_t cap, const char *src)
{
    size_t n = 0;
    for (; src && *src && n + 1 < cap; src++) {
        unsigned char c = (unsigned char)*src;
        dst[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : (c == '\n' ? ' ' : '?');
    }
    dst[n] = 0;
}

// ---------------------------------------------------------------- page area
// Thin scrollbar in the right gutter; hidden when everything fits. Redrawn
// only when the thumb moves or resizes.
static void draw_scrollbar(void)
{
    const int H = VIS * UI_ROW_H;
    int len = 0, top = 0;
    if (s_total > VIS) {
        len = H * VIS / s_total;
        if (len < 10) len = 10;
        top = (H - len) * s_top / (s_total - VIS);
    }
    if (len == s_sb_len && top == s_sb_top) return;
    s_sb_len = len;
    s_sb_top = top;
    const uint16_t track = RGB565(28, 28, 28), thumb = RGB565(120, 120, 120);
    board_lcd_window(SB_X, CONVO_Y, SB_W, H);
    uint16_t *buf = board_lcd_stream_buf();
    for (int y = 0; y < H; y++) {
        uint16_t c = len == 0 ? UI_BLACK : (y >= top && y < top + len) ? thumb : track;
        uint16_t sc = (uint16_t)((c >> 8) | (c << 8));
        for (int x = 0; x < SB_W; x++) buf[y * SB_W + x] = sc;
    }
    board_lcd_stream_push(buf, (size_t)SB_W * H);
    board_lcd_stream_end();
}

static void show_window(void)
{
    int max_top = s_total > VIS ? s_total - VIS : 0;
    if (s_follow) s_top = max_top;
    if (s_top > max_top) s_top = max_top;
    if (s_top < 0) s_top = 0;
    ui_box_set_rows(&s_convo, (const char (*)[UI_MAX_COLS + 1])s_rows + s_top,
                    s_total - s_top < VIS ? s_total - s_top : VIS);
    draw_scrollbar();
}

// Append a wrapped paragraph to the page rows, tagging its rows. Chat and
// list entries use a 2-column hanging indent; prose uses none.
static void add_para_h(const char *text, int tag, int hang)
{
    if (s_total >= MAX_ROWS) return;
    char (*tmp)[UI_MAX_COLS + 1] = s_tmp;
    int n = ui_wrap_ex(text, s_convo.cols, hang, tmp, MAX_ROWS);
    if (n > MAX_ROWS) n = MAX_ROWS;
    for (int i = 0; i < n && s_total < MAX_ROWS; i++, s_total++) {
        memcpy(s_rows[s_total], tmp[i], UI_MAX_COLS + 1);
        s_tags[s_total] = (signed char)tag;
    }
}

static void add_para(const char *text, int tag) { add_para_h(text, tag, 2); }
static void add_prose(const char *text) { add_para_h(text, -1, 0); }

static void build_chat(void)
{
    if (s_nturns == 0) {
        add_para("Hi, I'm Ivy AI.", -1);
        add_para(UI_G_ROWMARK "Say \"Hey Ivy\" or tap Ask.", -1);
        add_para("", -1);
        add_prose(UI_G_ROWMARK "Answers come from a small AI and may be wrong. See /settings > About.");
        add_para("", -1);
        add_para("  [ Type a question ]", TAG_TYPE);
        return;
    }
    char *buf = s_text;
    size_t n = 0, cap = TEXT_BYTES;
    buf[0] = 0;
    for (int i = 0; i < s_nturns; i++) {
        if (i > 0) n += (size_t)snprintf(buf + n, cap - n, "\n\n");   // blank line between turns
        if (s_turns[i].q[0]) n += (size_t)snprintf(buf + n, cap - n, "> %s", s_turns[i].q);
        if (s_turns[i].a[0])
            n += (size_t)snprintf(buf + n, cap - n, "%s%s %s", s_turns[i].q[0] ? "\n" : "",
                                  s_turns[i].builtin ? UI_G_DIAMOND : UI_G_BULLET, s_turns[i].a);
    }
    add_para(buf, -1);
    if (!s_busy) {
        add_para("", -1);
        add_para("  [ Type a question ]", TAG_TYPE);
    }
}

static void build_models(void)
{
    char line[96];
    add_prose(UI_G_ROWMARK "Tap a model to use it:");
    add_para("", -1);
    int n = models_count();
    if (n == 0) add_para(UI_G_ROWMARK "No models found on the SD card.", -1);
    for (int i = 0; i < n; i++) {
        const model_info_t *m = models_get(i);
        const char *dir = strrchr(m->dir, '/');
        snprintf(line, sizeof line, "%s %s", i == models_active() ? UI_G_BULLET : " ", m->name);
        add_para(line, i);
        snprintf(line, sizeof line, UI_G_ROWMARK "  %.2f MB " UI_G_MIDDOT " %s", m->bytes / 1048576.0,
                 dir ? dir + 1 : m->dir);
        add_para(line, i);
        add_para("", -1);
    }
    add_prose(UI_G_ROWMARK "Add models as SD folders /story/llm*/ with model.bin + tok.bin.");
}

static void build_about(void)
{
    char line[96];
    int a = models_active();
    add_para("Ivy AI", -1);
    add_para(UI_G_ROWMARK "(C) iamankushpandit", -1);
    add_para("", -1);
    add_prose("A voice assistant that listens, thinks and speaks entirely on this device. "
              "Wi-Fi is used only to set the clock.");
    add_para("", -1);
    add_prose(UI_G_ROWMARK "Wakes: \"Hey Ivy\" (Espressif WakeNet)");
    add_prose(UI_G_ROWMARK "Hears: conformer STT (NVIDIA, lspr98; CC-BY-4.0)");
    snprintf(line, sizeof line, UI_G_ROWMARK "Thinks: %s (TinyTalk, therezor)",
             a >= 0 ? models_get(a)->name : "no model");
    add_prose(line);
    add_prose(UI_G_ROWMARK "Speaks: SVOX Pico TTS (Apache-2.0)");
    add_para("", -1);
    add_para(UI_G_BULLET " Notice", -1);
    add_prose("Answers are generated by a small AI model on this device. They can be wrong, "
              "silly or inappropriate. The authors are not responsible for any answers or "
              "behaviour of this device. Children should use it with adult supervision.");
}

// ---------------------------------------------------------------- settings
// Rows are 29 columns: label, then "[-] value [+]" for stepped values.
static void build_settings(void)
{
    char line[96], clk[40];
    const prefs_t *p = prefs();
    clock_short(clk, sizeof clk);
    add_para(UI_G_DIAMOND " Run demo  >", TAG_SET_DEMO);
    add_para(UI_G_ROWMARK "  it asks and answers itself", TAG_SET_DEMO);
    add_para("", -1);
    add_para(UI_G_DIAMOND " Wi-Fi & clock  >", TAG_WIFI_SETUP);
    snprintf(line, sizeof line, UI_G_ROWMARK "  %.12s " UI_G_MIDDOT " %s",
             net_has_credentials() ? net_ssid() : "not set up", clk);
    add_para(line, TAG_WIFI_SETUP);
    add_para("", -1);
    snprintf(line, sizeof line, "  Volume     [-] %3d%% [+]", p->volume);
    add_para(line, TAG_SET_VOLUME);
    add_para("", -1);
    snprintf(line, sizeof line, "  Voice gain [-] +%ddB [+]", p->voice_gain_db);
    add_para(line, TAG_SET_GAIN);
    add_para("", -1);
    snprintf(line, sizeof line, "  Brightness [-] %3d%% [+]", p->brightness);
    add_para(line, TAG_SET_BRIGHT);
    add_para("", -1);
    char scr[16];
    if (p->screen_s == 0) snprintf(scr, sizeof scr, "never");
    else if (p->screen_s < 60) snprintf(scr, sizeof scr, "%u s", p->screen_s);
    else snprintf(scr, sizeof scr, "%u min", p->screen_s / 60);
    snprintf(line, sizeof line, "  Screen off  %s  >", scr);
    add_para(line, TAG_SET_SCREEN);
    add_para("", -1);
    snprintf(line, sizeof line, "  Wake word   \"Hey Ivy\" %s", p->wake ? "On" : "Off");
    add_para(line, TAG_SET_WAKE);
    add_para("", -1);
    add_para(UI_G_DIAMOND " About Ivy AI  >", TAG_SET_ABOUT);
}

int app_ui_tap_col(int x) { return x / UI_FONT_W; }

// ---------------------------------------------------------------- Wi-Fi page
static void render_page(void);
#define MAX_APS 12
static net_ap_t s_aps[MAX_APS];
static int s_nap = -2;                 // -2: never scanned, -1: scan failed
static bool s_scanning;

void app_ui_wifi_scanning(void)
{
    UI_LOCK();
    s_scanning = true;
    if (s_page == PAGE_WIFI) render_page();
    UI_UNLOCK();
}

void app_ui_wifi_results(const net_ap_t *aps, int n)
{
    UI_LOCK();
    s_scanning = false;
    s_nap = n < 0 ? -1 : n > MAX_APS ? MAX_APS : n;
    for (int i = 0; i < s_nap; i++) s_aps[i] = aps[i];
    if (s_page == PAGE_WIFI) render_page();
    UI_UNLOCK();
}

const net_ap_t *app_ui_wifi_ap(int i) { return i >= 0 && i < s_nap ? &s_aps[i] : NULL; }
bool app_ui_wifi_scanned(void) { return s_nap != -2 || s_scanning; }

static void build_wifi(void)
{
    char line[96], clk[40];
    add_prose(UI_G_ROWMARK "Wi-Fi is used only to set the clock (NTP), once an hour. Answers never use the internet.");
    add_para("", -1);
    clock_short(clk, sizeof clk);
    if (net_has_credentials()) {
        snprintf(line, sizeof line, UI_G_DIAMOND " %s", net_ssid());
        add_para(line, -1);
        snprintf(line, sizeof line, UI_G_ROWMARK "  Clock: %s", clk);
        add_para(line, -1);
        if (net_last_msg()[0]) {
            snprintf(line, sizeof line, UI_G_ROWMARK "  Last sync: %s", net_last_msg());
            add_para(line, -1);
        }
        add_para("  [ Sync now ]", TAG_WIFI_SYNC);
        add_para("  [ Forget network ]", TAG_WIFI_FORGET);
    } else {
        snprintf(line, sizeof line, UI_G_ROWMARK "Not set up. Clock: %s", clk);
        add_para(line, -1);
    }
    add_para("", -1);
    add_para("Tap your network:", -1);
    if (s_scanning) add_para(UI_G_ROWMARK "  Scanning" UI_G_ELLIPSIS, -1);
    else if (s_nap == -1) add_para(UI_G_ROWMARK "  Scan failed.", -1);
    else if (s_nap == 0) add_para(UI_G_ROWMARK "  No networks found.", -1);
    for (int i = 0; !s_scanning && i < s_nap; i++) {
        int r = s_aps[i].rssi;
        const char *bars = r > -60 ? "|||" : r > -72 ? "|| " : "|  ";
        snprintf(line, sizeof line, "  %s %.21s%s", bars, s_aps[i].ssid, s_aps[i].open ? " (open)" : "");
        add_para(line, i);
    }
    add_para("  [ Scan again ]", TAG_WIFI_RESCAN);
}

// ---------------------------------------------------------------- keyboard
// Password entry drawn straight into the page area: a title row, the field,
// four 10-key rows and a control row. Only the field and the touched key are
// redrawn on a key press; a layer switch redraws the key rows.
#define KB_TITLE_Y CONVO_Y
#define KB_FIELD_Y (CONVO_Y + UI_ROW_H)
#define KB_KEYS_Y (CONVO_Y + 2 * UI_ROW_H + 4)
#define KB_KEY_W 24
#define KB_KEY_H 24
#define KB_PITCH 26
static const char KB_LAYERS[3][4][11] = {
    {"1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm.@_"},
    {"1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM.@_"},
    {"1234567890", "!@#$%^&*()", "-_=+[]{};:", "'\",.<>/?~\\"},
};
static const struct { int x, w; const char *label; } KB_CTRL[] = {
    {0, 36, "aA"}, {36, 36, "#+"}, {72, 72, "space"}, {144, 40, "del"}, {184, 56, "Join"},
};
enum { KB_SHIFT, KB_SYM, KB_SPACE, KB_DEL, KB_JOIN, KB_NCTRL };
static char s_kb_text[NET_PASS_MAX], s_kb_ssid[NET_SSID_MAX];
static int s_kb_len, s_kb_layer;

static void draw_key(int x, int y, int w, const char *label, bool pressed, bool accent)
{
    const int h = KB_KEY_H;
    uint16_t bg = pressed ? UI_ACCENT : accent ? RGB565(70, 50, 56) : RGB565(40, 40, 40);
    uint16_t fg = pressed ? UI_BLACK : UI_WHITE;
    uint16_t *px = s_btn_px;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++) {
            bool corner = (xx == 0 || xx == w - 1) && (yy == 0 || yy == h - 1);
            bool gap = xx == w - 1 || yy == h - 1;        // 1 px gutter right/bottom
            px[yy * w + xx] = corner || gap ? UI_BLACK : bg;
        }
    int lw = (int)strlen(label) * UI_FONT_W;
    ui_text_into(px, w, h, (w - 1 - lw) / 2, (h - 1 - UI_FONT_H) / 2, label, fg);
    board_lcd_window(x, y, w, h);
    uint16_t *buf = board_lcd_stream_buf();
    for (int i = 0; i < w * h; i++) buf[i] = (uint16_t)((px[i] >> 8) | (px[i] << 8));
    board_lcd_stream_push(buf, (size_t)w * h);
    board_lcd_stream_end();
}

static void kb_draw_char_key(int row, int col, bool pressed)
{
    char s[2] = {KB_LAYERS[s_kb_layer][row][col], 0};
    draw_key(col * KB_KEY_W, KB_KEYS_Y + row * KB_PITCH, KB_KEY_W, s, pressed, false);
}

static void kb_draw_ctrl(int k, bool pressed)
{
    bool on = (k == KB_SHIFT && s_kb_layer == 1) || (k == KB_SYM && s_kb_layer == 2) || k == KB_JOIN;
    draw_key(KB_CTRL[k].x, KB_KEYS_Y + 4 * KB_PITCH, KB_CTRL[k].w, KB_CTRL[k].label, pressed, on);
}

static void kb_draw_keys(void)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 10; c++) kb_draw_char_key(r, c, false);
    for (int k = 0; k < KB_NCTRL; k++) kb_draw_ctrl(k, false);
}

static void kb_draw_field(void)
{
    // Masked except the last character typed, like a phone keyboard.
    char row[UI_MAX_COLS + 1];
    int shown = s_kb_len > 26 ? 26 : s_kb_len, start = s_kb_len - shown, n = 0;
    row[n++] = '>';
    row[n++] = ' ';
    for (int i = start; i < s_kb_len; i++) row[n++] = i == s_kb_len - 1 ? s_kb_text[i] : '*';
    row[n++] = '_';
    row[n] = 0;
    ui_draw_row(0, KB_FIELD_Y, BOARD_LCD_W, row, UI_WHITE, UI_BLACK);
}

static void kb_draw_all(void)
{
    ui_box_set_rows(&s_convo, NULL, 0);       // blank the text rows (dirty rows only)
    ui_box_invalidate(&s_convo);              // text pages repaint over the keys later
    s_total = 0;
    draw_scrollbar();
    char title[UI_MAX_COLS + 1];
    snprintf(title, sizeof title, "Password: %.19s", s_kb_ssid);
    ui_draw_row(0, KB_TITLE_Y, BOARD_LCD_W, title, UI_ACCENT, UI_BLACK);
    kb_draw_field();
    kb_draw_keys();
}

void app_ui_kb_open(const char *ssid)
{
    UI_LOCK();
    snprintf(s_kb_ssid, sizeof s_kb_ssid, "%s", ssid);
    s_kb_text[0] = 0;
    s_kb_len = 0;
    s_kb_layer = 0;
    app_ui_page(PAGE_KEYBOARD);
    UI_UNLOCK();
}

const char *app_ui_kb_text(void) { return s_kb_text; }
const char *app_ui_kb_ssid(void) { return s_kb_ssid; }

static void kb_flash_pause(void)
{
#ifndef STORY_HOST
    vTaskDelay(pdMS_TO_TICKS(70));
#endif
}

int app_ui_kb_tap(int x, int y)
{
    if (s_page != PAGE_KEYBOARD || y < KB_KEYS_Y) return 0;
    int row = (y - KB_KEYS_Y) / KB_PITCH;
    if (row > 4) return 0;
    UI_LOCK();
    int ret = 0;
    if (row < 4) {
        int col = x / KB_KEY_W;
        if (col > 9) col = 9;
        if (s_kb_len < NET_PASS_MAX - 1) {
            s_kb_text[s_kb_len++] = KB_LAYERS[s_kb_layer][row][col];
            s_kb_text[s_kb_len] = 0;
        }
        kb_draw_char_key(row, col, true);
        kb_draw_field();
        kb_flash_pause();
        kb_draw_char_key(row, col, false);
    } else {
        int k = 0;
        while (k < KB_NCTRL - 1 && x >= KB_CTRL[k].x + KB_CTRL[k].w) k++;
        kb_draw_ctrl(k, true);
        kb_flash_pause();
        switch (k) {
        case KB_SHIFT: s_kb_layer = s_kb_layer == 1 ? 0 : 1; kb_draw_keys(); break;
        case KB_SYM: s_kb_layer = s_kb_layer == 2 ? 0 : 2; kb_draw_keys(); break;
        case KB_SPACE:
            if (s_kb_len < NET_PASS_MAX - 1) { s_kb_text[s_kb_len++] = ' '; s_kb_text[s_kb_len] = 0; }
            kb_draw_ctrl(k, false);
            kb_draw_field();
            break;
        case KB_DEL:
            if (s_kb_len > 0) s_kb_text[--s_kb_len] = 0;
            kb_draw_ctrl(k, false);
            kb_draw_field();
            break;
        case KB_JOIN: kb_draw_ctrl(k, false); ret = 1; break;
        }
    }
    UI_UNLOCK();
    return ret;
}

// ---------------------------------------------------------------- T9 keypad
// Typed questions on a phone-style 3x4 keypad in the page area. Multi-tap:
// tapping the same key again within T9_CYCLE_US cycles its letters (2: a b c 2),
// otherwise a new character starts. 0 is space then zero, as on a phone, and
// [#+] on the title row swaps the nine letter keys for symbols so that no
// character needs more than four taps.
#define T9_TITLE_Y CONVO_Y
#define T9_FIELD_Y (CONVO_Y + UI_ROW_H)
#define T9_KEYS_Y (CONVO_Y + 2 * UI_ROW_H + 4)
#define T9_KEY_W 80
#define T9_KEY_H 32
#define T9_PITCH 34
#define T9_CYCLE_US 900000
#define T9_MAX 120
enum { T9_DEL = 9, T9_SPACE = 10, T9_ASK = 11 };
typedef struct { const char *top, *sub, *cycle; } t9_key_t;
static const t9_key_t T9_KEYS[12] = {
    {"1", ".,?!", ".,?!'1"}, {"2", "abc", "abc2"}, {"3", "def", "def3"},
    {"4", "ghi", "ghi4"},    {"5", "jkl", "jkl5"}, {"6", "mno", "mno6"},
    {"7", "pqrs", "pqrs7"},  {"8", "tuv", "tuv8"}, {"9", "wxyz", "wxyz9"},
    {"del", "", NULL},       {"0", "space", " 0"}, {"Ask", "", NULL},
};
// [#+] layer: every symbol within four taps. del / 0 / Ask keep their meaning.
static const t9_key_t T9_SYM[12] = {
    {".,?!", "", ".,?!"},    {"'\"`", "", "'\"`"},  {"-_=+", "", "-_=+"},
    {"()[]", "", "()[]"},    {"{}<>", "", "{}<>"},  {"/\\|~", "", "/\\|~"},
    {"@#$%", "", "@#$%"},    {"^&*", "", "^&*"},    {":;", "", ":;"},
    {"del", "", NULL},       {"0", "space", " 0"},  {"Ask", "", NULL},
};
static char s_t9_text[T9_MAX + 1];
static int s_t9_len, s_t9_last = -1, s_t9_idx;
static bool s_t9_sym;
static int64_t s_t9_last_us;

static const t9_key_t *t9_key(int k) { return (s_t9_sym ? T9_SYM : T9_KEYS) + k; }

static void draw_key2(int x, int y, int w, int h, const char *l1, const char *l2, bool pressed, bool accent)
{
    uint16_t bg = pressed ? UI_ACCENT : accent ? RGB565(70, 50, 56) : RGB565(40, 40, 40);
    uint16_t fg = pressed ? UI_BLACK : UI_WHITE, fg2 = pressed ? UI_BLACK : UI_ACCENT;
    uint16_t *px = s_btn_px;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++) {
            bool corner = (xx == 0 || xx == w - 2) && (yy == 0 || yy == h - 2);
            bool gap = xx == w - 1 || yy == h - 1;           // 1 px gutter right/bottom
            px[yy * w + xx] = corner || gap ? UI_BLACK : bg;
        }
    int w1 = (int)strlen(l1) * UI_FONT_W, w2 = (int)strlen(l2) * UI_FONT_W;
    if (l2[0]) {
        ui_text_into(px, w, h, (w - 1 - w1) / 2, 2, l1, fg);
        ui_text_into(px, w, h, (w - 1 - w2) / 2, 16, l2, fg2);
    } else {
        ui_text_into(px, w, h, (w - 1 - w1) / 2, (h - 1 - UI_FONT_H) / 2, l1, fg);
    }
    board_lcd_window(x, y, w, h);
    uint16_t *buf = board_lcd_stream_buf();
    for (int i = 0; i < w * h; i++) buf[i] = (uint16_t)((px[i] >> 8) | (px[i] << 8));
    board_lcd_stream_push(buf, (size_t)w * h);
    board_lcd_stream_end();
}

static void t9_draw_key(int k, bool pressed)
{
    int r = k / 3, c = k % 3;
    const t9_key_t *key = t9_key(k);
    draw_key2(c * T9_KEY_W, T9_KEYS_Y + r * T9_PITCH, T9_KEY_W, T9_KEY_H, key->top, key->sub,
              pressed, k == T9_ASK);
}

static void t9_draw_field(void)
{
    char row[UI_MAX_COLS + 1];
    int shown = s_t9_len > 26 ? 26 : s_t9_len, n = 0;
    row[n++] = '>';
    row[n++] = ' ';
    memcpy(row + n, s_t9_text + s_t9_len - shown, (size_t)shown);
    n += shown;
    row[n++] = '_';
    row[n] = 0;
    ui_draw_row(0, T9_FIELD_Y, BOARD_LCD_W, row, UI_WHITE, UI_BLACK);
}

// "Type a question    [#+]  [x]" - the layer toggle and cancel are tap targets
// (T9_SYM_COL, T9_X_COL below).
#define T9_SYM_COL 19
#define T9_X_COL 26
static void t9_draw_title(void)
{
    char row[UI_MAX_COLS + 1];
    memset(row, ' ', UI_MAX_COLS);
    row[UI_MAX_COLS] = 0;
    memcpy(row, "Type a question", 15);
    memcpy(row + T9_SYM_COL, s_t9_sym ? "[abc]" : "[#+]", s_t9_sym ? 5 : 4);
    memcpy(row + T9_X_COL, "[x]", 3);
    row[T9_X_COL + 3] = 0;
    ui_draw_row(0, T9_TITLE_Y, BOARD_LCD_W, row, UI_ACCENT, UI_BLACK);
}

static void t9_draw_all(void)
{
    ui_box_set_rows(&s_convo, NULL, 0);       // blank the text rows (dirty rows only)
    ui_box_invalidate(&s_convo);              // text pages repaint over the keys later
    s_total = 0;
    draw_scrollbar();
    t9_draw_title();
    t9_draw_field();
    for (int k = 0; k < 12; k++) t9_draw_key(k, false);
}

void app_ui_t9_open(void)
{
    UI_LOCK();
    s_t9_text[0] = 0;
    s_t9_len = 0;
    s_t9_last = -1;
    s_t9_sym = false;
    app_ui_page(PAGE_T9);
    UI_UNLOCK();
}

const char *app_ui_t9_text(void) { return s_t9_text; }

int app_ui_t9_tap(int x, int y)
{
    if (s_page != PAGE_T9) return 0;
    if (y < T9_FIELD_Y) {                                     // title row
        if (x >= T9_X_COL * UI_FONT_W) return 2;              // [x]: cancel
        if (x >= T9_SYM_COL * UI_FONT_W) {                    // [#+] / [abc]: swap layers
            UI_LOCK();
            s_t9_sym = !s_t9_sym;
            s_t9_last = -1;                                   // commits the letter being cycled
            t9_draw_title();
            for (int k = 0; k < 9; k++) t9_draw_key(k, false);
            UI_UNLOCK();
        }
        return 0;
    }
    if (y < T9_KEYS_Y) return 0;
    int r = (y - T9_KEYS_Y) / T9_PITCH, c = x / T9_KEY_W;
    if (r > 3) return 0;
    if (c > 2) c = 2;
    int k = r * 3 + c, ret = 0;
    int64_t now = story_time_us();
    UI_LOCK();
    t9_draw_key(k, true);
    if (t9_key(k)->cycle) {
        const char *cy = t9_key(k)->cycle;
        if (k == s_t9_last && now - s_t9_last_us < T9_CYCLE_US && s_t9_len > 0) {
            s_t9_idx = (s_t9_idx + 1) % (int)strlen(cy);       // same key again: next letter
            s_t9_text[s_t9_len - 1] = cy[s_t9_idx];
        } else if (s_t9_len < T9_MAX) {
            s_t9_idx = 0;
            s_t9_text[s_t9_len++] = cy[0];
            s_t9_text[s_t9_len] = 0;
        }
        s_t9_last = k;
        s_t9_last_us = now;
    } else {
        s_t9_last = -1;                                        // commits the letter being cycled
        if (k == T9_DEL && s_t9_len > 0) s_t9_text[--s_t9_len] = 0;
        else if (k == T9_ASK && s_t9_len > 0) ret = 1;
    }
    t9_draw_field();
    kb_flash_pause();
    t9_draw_key(k, false);
    UI_UNLOCK();
    return ret;
}

static void render_page(void)
{
    if (s_page == PAGE_KEYBOARD) { kb_draw_all(); return; }
    if (s_page == PAGE_T9) { t9_draw_all(); return; }
    s_total = 0;
    switch (s_page) {
    case PAGE_CHAT: build_chat(); break;
    case PAGE_MODELS: build_models(); break;
    case PAGE_SETTINGS: build_settings(); break;
    case PAGE_ABOUT: build_about(); break;
    case PAGE_WIFI: build_wifi(); break;
    default: break;
    }
    show_window();
}

// ---------------------------------------------------------------- logo
// RGB565 images generated by tools/gen_logo.py from assets/ivy_ai_logo.svg.
#include "logo_data.h"

// Draw an RGB565 image, streamed in strips that fit one DMA buffer.
static void blit_rgb(const uint16_t *img, int w, int h, int x, int y)
{
    const int strip = BOARD_LCD_STREAM_PX / w;
    for (int y0 = 0; y0 < h; y0 += strip) {
        int rows = h - y0 < strip ? h - y0 : strip;
        board_lcd_window(x, y + y0, w, rows);
        uint16_t *buf = board_lcd_stream_buf();
        const uint16_t *src = img + (size_t)y0 * w;
        for (int i = 0; i < w * rows; i++) buf[i] = (uint16_t)((src[i] >> 8) | (src[i] << 8));
        board_lcd_stream_push(buf, (size_t)w * rows);
        board_lcd_stream_end();
    }
}

void app_ui_splash(void)
{
    UI_LOCK();
    // Boot welcome screen (drawn before the backlight comes on).
    board_lcd_fill(0, 0, BOARD_LCD_W, BOARD_LCD_H, UI_BLACK);
    const int lx = (BOARD_LCD_W - LOGO_FULL_W) / 2, ly = 34;
    blit_rgb(logo_full, LOGO_FULL_W, LOGO_FULL_H, lx, ly);
    const char *c = "(C) iamankushpandit";
    int cw = (int)strlen(c) * UI_FONT_W;
    ui_draw_row((BOARD_LCD_W - cw) / 2, ly + LOGO_FULL_H + 18, cw, c, UI_DIM, UI_BLACK);
    UI_UNLOCK();
}

// ---------------------------------------------------------------- sleep
// The lock button (and the idle timeout) put the screen to sleep: the logo
// alone on black, its colour drifting slowly through a wheel. Only the logo
// rectangle is ever redrawn, one frame every SLEEP_FRAME_US; after
// SLEEP_DIM_US even that stops and the backlight goes off, so a locked
// device on battery costs nothing.
static uint16_t mix565(uint16_t a, uint16_t b, float t);

#define SLEEP_FRAME_US 150000
#define SLEEP_CYCLE_US 9000000            // one full trip around the wheel
#define SLEEP_LOGO_Y ((BOARD_LCD_H - LOGO_FULL_H) / 2)
static bool s_asleep;
static int64_t s_sleep_t0, s_sleep_last;

// The logo drawn as a silhouette in one colour: each pixel keeps its own
// brightness (so the black circuit tracery stays black) and takes the tint's
// hue. Streamed in strips like blit_rgb.
static void blit_logo_tint(uint16_t tint)
{
    const int w = LOGO_FULL_W, x = (BOARD_LCD_W - w) / 2;
    const int tr = (tint >> 11) & 0x1F, tg = (tint >> 5) & 0x3F, tb = tint & 0x1F;
    const int strip = BOARD_LCD_STREAM_PX / w;
    for (int y0 = 0; y0 < LOGO_FULL_H; y0 += strip) {
        int rows = LOGO_FULL_H - y0 < strip ? LOGO_FULL_H - y0 : strip;
        board_lcd_window(x, SLEEP_LOGO_Y + y0, w, rows);
        uint16_t *buf = board_lcd_stream_buf();
        const uint16_t *src = logo_full + (size_t)y0 * w;
        for (int i = 0; i < w * rows; i++) {
            int r = (src[i] >> 11) & 0x1F, g = (src[i] >> 5) & 0x3F, b = src[i] & 0x1F;
            int lum = r > b ? r : b;                      // 0..31, the leaf's own shading
            if (g / 2 > lum) lum = g / 2;
            uint16_t c = (uint16_t)(((tr * lum / 31) << 11) | ((tg * lum / 31) << 5) | (tb * lum / 31));
            buf[i] = (uint16_t)((c >> 8) | (c << 8));
        }
        board_lcd_stream_push(buf, (size_t)w * rows);
        board_lcd_stream_end();
    }
}

// Colour wheel for the sleeping logo: pink home, then around and back.
static const uint16_t SLEEP_HUES[] = {
    RGB565(244, 194, 194), RGB565(214, 130, 220), RGB565(120, 150, 245),
    RGB565(110, 220, 210), RGB565(160, 230, 130), RGB565(245, 200, 120),
};
#define SLEEP_HUE_N (int)(sizeof SLEEP_HUES / sizeof SLEEP_HUES[0])

static void draw_sleep_frame(int64_t now)
{
    float t = (float)((now - s_sleep_t0) % SLEEP_CYCLE_US) / SLEEP_CYCLE_US * SLEEP_HUE_N;
    int i = (int)t;
    blit_logo_tint(mix565(SLEEP_HUES[i % SLEEP_HUE_N], SLEEP_HUES[(i + 1) % SLEEP_HUE_N], t - i));
}

void app_ui_sleep_enter(void)
{
    UI_LOCK();
    s_asleep = true;
    s_sleep_t0 = s_sleep_last = story_time_us();
    board_lcd_fill(0, 0, BOARD_LCD_W, BOARD_LCD_H, UI_BLACK);
    draw_sleep_frame(s_sleep_t0);
    UI_UNLOCK();
}

bool app_ui_sleeping(void) { return s_asleep; }

void app_ui_sleep_tick(void)
{
    if (!s_asleep) return;
    int64_t now = story_time_us();
    if (now - s_sleep_last < SLEEP_FRAME_US) return;
    UI_LOCK();
    s_sleep_last = now;
    draw_sleep_frame(now);
    UI_UNLOCK();
}

// ---------------------------------------------------------------- header
// ╭──────────────────────────╮
// │ [leaf]  Ivy AI    87% [#] (↻)
// │         (C) iamankushpandit
// │ Model: TinyTalk 2 8M    (lock)
// ╰──────────────────────────╯
#define HDR_TEXT_COL 5                   // text starts right of the icon
#define LOCK_S 14                        // padlock at the right end of the model row
#define LOCK_X (BOARD_LCD_W - UI_FONT_W - LOCK_S - 2)
#define LOCK_Y ROW_Y(3)
static int s_lock_state = -1;
#define ICON_X (UI_FONT_W + ((HDR_TEXT_COL - 1) * UI_FONT_W - LOGO_ICON_W) / 2)   // centered in cols 1..4
#define ICON_Y ROW_Y(1)

// ---------------------------------------------------------------- AI activity
// While the AI works (listening, transcribing, thinking) its spark symbol
// flashes: the Ask pill's spark pulses in pink (cosine, 25..100 %) and the
// status-line spinner keeps turning even when the status text doesn't
// change. Only the spark's 14x14 patch and the status row are redrawn.
#define PULSE_PERIOD_US 1000000
#define PULSE_FRAME_US 80000
#define SPIN_FRAME_US 200000
static bool s_pulse;
static int64_t s_pulse_t0, s_pulse_last, s_spin_last;
static char s_stat_text[64];
static uint16_t s_stat_color;

static float spark(float x, float y, float R);
static uint16_t mix565(uint16_t a, uint16_t b, float t);

// The Ask pill's spark, drawn over the pill's black interior. Its coverage
// mask is computed once; each frame only recolours it (cheap: the LLM is
// running on both cores while this animates).
#define SPARK_W 14
#define SPARK_H 14
#define SPARK_Y0 6
static uint8_t s_spark_mask[SPARK_W * SPARK_H];   // coverage 0..255
static int s_spark_x0 = -1;

static void spark_mask_init(void)
{
    const int W = BAR[BAR_ASK].w, H = BAR_H;
    const int label_w = (int)strlen(ASK_BUSY_LABEL) * UI_FONT_W;   // pulses only while busy
    const int gx = (W - (label_w + 17)) / 2;
    const float icx = gx + 5.0f, icy = (H - 1) / 2.0f, R = 5.5f;
    s_spark_x0 = gx - 1;
    for (int y = 0; y < SPARK_H; y++)
        for (int x = 0; x < SPARK_W; x++) {
            float cov = 0;
            for (int k = 0; k < 4; k++)
                cov += spark(s_spark_x0 + x - icx + ((k & 1) ? 0.25f : -0.25f),
                             SPARK_Y0 + y - icy + ((k & 2) ? 0.25f : -0.25f), R);
            s_spark_mask[y * SPARK_W + x] = (uint8_t)(cov / 4.0f * 255.0f + 0.5f);
        }
}

static void draw_ask_spark(float level)
{
    if (s_spark_x0 < 0) spark_mask_init();
    uint16_t col = mix565(RGB565(40, 26, 30), UI_ACCENT, level);
    board_lcd_window(BAR[BAR_ASK].x + s_spark_x0, BAR_Y + SPARK_Y0, SPARK_W, SPARK_H);
    uint16_t *buf = board_lcd_stream_buf();
    for (int i = 0; i < SPARK_W * SPARK_H; i++) {
        uint16_t c = s_spark_mask[i] ? mix565(UI_BLACK, col, s_spark_mask[i] / 255.0f) : UI_BLACK;
        buf[i] = (uint16_t)((c >> 8) | (c << 8));
    }
    board_lcd_stream_push(buf, SPARK_W * SPARK_H);
    board_lcd_stream_end();
}

static void icon_pulse(bool on)
{
    if (on == s_pulse) return;
    s_pulse = on;
    s_pulse_t0 = story_time_us();
    s_pulse_last = s_spin_last = 0;
    if (!on && s_btn_px) {                       // restore the pill's normal look
        int st = s_bar_state[BAR_ASK];
        s_bar_state[BAR_ASK] = -1;
        if (st >= 0) app_ui_bar_state(BAR_ASK, (app_btn_state_t)st);
    }
}

void app_ui_tick(void)
{
    if (!s_pulse || !s_btn_px || !s_busy) return;
    UI_LOCK();
    int64_t now = story_time_us();
    if (s_pulse && now - s_pulse_last >= PULSE_FRAME_US) {
        s_pulse_last = now;
        float ph = (float)((now - s_pulse_t0) % PULSE_PERIOD_US) / PULSE_PERIOD_US;
        draw_ask_spark(0.25f + 0.75f * (0.5f + 0.5f * cosf(6.2831853f * ph)));
    }
    if (s_pulse && now - s_spin_last >= SPIN_FRAME_US) {
        s_spin_last = now;
        char text[64];
        snprintf(text, sizeof text, "%s", s_stat_text);
        app_ui_status(text, s_stat_color);       // next spinner frame, same text
    }
    UI_UNLOCK();
}

static int s_bat_pct;
static void draw_battery(int pct);

// One header text row: text from column `col`, truncated (with an ellipsis)
// at the right border.
static void header_row_at(int r, int col, const char *text, uint16_t text_fg)
{
    char row[COLS + 1];
    uint8_t attr[COLS];
    const uint16_t pal[2] = {UI_DIM, text_fg};
    memset(row, ' ', COLS);
    memset(attr, 0, sizeof attr);
    row[COLS] = 0;
    row[0] = row[COLS - 1] = UI_G_V[0];
    size_t room = COLS - 1 - col, n = strlen(text);
    bool cut = n > room;
    if (cut) n = room;
    memcpy(row + col, text, n);
    if (cut) row[col + n - 1] = UI_G_ELLIPSIS[0];
    memset(attr + col, 1, n);
    ui_draw_row_attr(0, ROW_Y(r), BOARD_LCD_W, row, attr, pal, UI_BLACK);
}

static void header_row(int r, const char *text, uint16_t text_fg)
{
    header_row_at(r, HDR_TEXT_COL, text, text_fg);
}

// Shortens a name to `room` columns by dropping its middle, keeping the tail
// from a word boundary: "TinyTalk 2 8M v6 (Braino)" -> "TinyTalk…v6 (Braino)".
// The tail is what distinguishes two models, so it is never the part cut.
static void elide_middle(char *dst, size_t cap, const char *src, size_t room)
{
    size_t n = strlen(src);
    if (room >= cap) room = cap - 1;
    if (n <= room) { snprintf(dst, cap, "%s", src); return; }
    // Split the room in half, then back the tail up to the start of its word.
    size_t tail = n - (room - 1) / 2;
    while (tail > 0 && src[tail - 1] != ' ' && n - tail < room - 2) tail--;
    size_t head = room - 1 - (n - tail);
    while (head > 1 && src[head - 1] == ' ') head--;   // no space before the ellipsis
    memcpy(dst, src, head);
    dst[head] = UI_G_ELLIPSIS[0];
    memcpy(dst + head + 1, src + tail, n - tail);
    dst[head + 1 + (n - tail)] = 0;
}

// Third header row: the model that will answer. It clears the leaf icon (which
// spans rows 1-2 only), so it starts at column 1 and long names are elided
// rather than cut off. Redrawn on its own when the choice changes.
#define MODEL_LABEL "Model: "
// Columns left for the name: from column 1 up to the lock button.
#define MODEL_ROOM (LOCK_X / UI_FONT_W - 1 - ((int)sizeof(MODEL_LABEL) - 1))
static void draw_model_row(void)
{
    const model_info_t *m = models_get(models_active());
    char name[COLS], shown[COLS], line[COLS + 8];
    sanitize(name, sizeof name, m ? m->name : "none loaded");
    elide_middle(shown, sizeof shown, name, MODEL_ROOM);
    snprintf(line, sizeof line, MODEL_LABEL "%s", shown);
    header_row_at(3, 1, line, m ? UI_ACCENT : UI_ERR);
    s_lock_state = -1;                     // the row's background wiped the padlock
    app_ui_lock_state(BTN_IDLE);
}

void app_ui_model_changed(void)
{
    UI_LOCK();
    draw_model_row();
    UI_UNLOCK();
}

static void draw_header(void)
{
    char row[COLS + 1];
    row[COLS] = 0;
    row[0] = UI_G_TL[0];
    memset(row + 1, UI_G_H[0], COLS - 2);
    row[COLS - 1] = UI_G_TR[0];
    ui_draw_row(0, ROW_Y(0), BOARD_LCD_W, row, UI_DIM, UI_BLACK);

    header_row(1, "Ivy AI", UI_WHITE);
    header_row(2, "(C) iamankushpandit", UI_DIM);
    draw_model_row();
    // Leaf icon over the first two text rows, left of the title (after the
    // rows, so their background doesn't cover it).
    blit_rgb(logo_icon, LOGO_ICON_W, LOGO_ICON_H, ICON_X, ICON_Y);
    if (s_bat_pct != -2) draw_battery(s_bat_pct);

    row[0] = UI_G_BL[0];
    memset(row + 1, UI_G_H[0], COLS - 2);
    row[COLS - 1] = UI_G_BR[0];
    ui_draw_row(0, ROW_Y(4), BOARD_LCD_W, row, UI_DIM, UI_BLACK);
}

void app_ui_init(void)
{
#ifndef STORY_HOST
    if (!s_lock) s_lock = xSemaphoreCreateRecursiveMutex();
#endif
    if (!s_turns) {
        s_turns = UI_ALLOC(sizeof(turn_t) * MAX_TURNS);
        s_rows = UI_ALLOC(sizeof(*s_rows) * MAX_ROWS);
        s_tags = UI_ALLOC(MAX_ROWS);
        s_tmp = UI_ALLOC(sizeof(*s_tmp) * MAX_ROWS);
        s_text = UI_ALLOC(TEXT_BYTES);
        s_btn_px = UI_ALLOC(sizeof(uint16_t) * BTN_PX_MAX);   // largest widget: T9 key
    }
    UI_LOCK();
    // The only full-screen fill: once at boot, before the backlight is on.
    board_lcd_fill(0, 0, BOARD_LCD_W, BOARD_LCD_H, UI_BLACK);
    draw_header();
    ui_box_init(&s_convo, 0, CONVO_Y, CONVO_W, VIS, UI_WHITE, UI_BLACK);
    ui_box_mark(&s_convo, 0, '>', UI_DIM);
    ui_box_mark(&s_convo, 1, UI_G_BULLET[0], UI_ACCENT);
    ui_box_mark_row(&s_convo, 2, UI_G_ROWMARK[0], UI_DIM);
    ui_box_mark(&s_convo, 3, UI_G_DIAMOND[0], UI_INFO);   // built-in (non-AI) answers
    ui_box_init(&s_detail, 0, ROW_Y(17) + 6, BOARD_LCD_W, 1, UI_DIM, UI_BLACK);
    ui_box_init(&s_status, 0, ROW_Y(18) + 6, BOARD_LCD_W, 1, UI_ACCENT, UI_BLACK);
    ui_box_style(&s_status, 1, UI_ACCENT, 0);
    for (int b = 0; b < BAR_COUNT; b++) app_ui_bar_state((app_bar_t)b, BTN_IDLE);
    app_ui_restart_state(BTN_IDLE);
    render_page();
    UI_UNLOCK();
}

void app_ui_status(const char *s, uint16_t color)
{
    UI_LOCK();
    char buf[64];
    if (s != s_stat_text) snprintf(s_stat_text, sizeof s_stat_text, "%s", s);
    s_stat_color = color;
    bool idle = color == UI_OK || color == UI_GREY;
    char lead = idle ? UI_G_MIDDOT[0] : (char)(UI_G_SPIN0 + (s_spin++ % UI_SPIN_FRAMES));
    snprintf(buf, sizeof buf, "%c %s", lead, s);
    char *dots = strstr(buf, "...");
    if (dots) { dots[0] = UI_G_ELLIPSIS[0]; memmove(dots + 1, dots + 3, strlen(dots + 3) + 1); }
    uint16_t fg = idle ? UI_DIM : (color == UI_ERR ? UI_ERR : UI_ACCENT);
    icon_pulse(color == UI_BUSY);           // listening / transcribing / thinking
    if (fg != s_status.fg) {
        // Color change: restyle, then let the single set() below repaint once.
        ui_box_style(&s_status, 1, fg, 0);
        s_status.fg = fg;
        ui_box_invalidate(&s_status);
    }
    ui_box_set(&s_status, buf);
    UI_UNLOCK();
}

void app_ui_you(const char *text)
{
    UI_LOCK();
    if (s_nturns == MAX_TURNS) {   // drop the oldest turn
        memmove(&s_turns[0], &s_turns[1], sizeof s_turns[0] * (MAX_TURNS - 1));
        s_nturns--;
    }
    sanitize(s_turns[s_nturns].q, Q_CHARS, text);
    s_turns[s_nturns].a[0] = 0;
    s_turns[s_nturns].builtin = false;
    s_nturns++;
    s_follow = true;               // a new question always jumps to the bottom
    s_page = PAGE_CHAT;
    app_ui_detail("");
    render_page();
    UI_UNLOCK();
}

void app_ui_story(const char *text)
{
    UI_LOCK();
    if (s_nturns == 0) {           // an answer with no question (e.g. an error)
        s_turns[0].q[0] = 0;
        s_nturns = 1;
    }
    sanitize(s_turns[s_nturns - 1].a, A_CHARS, text);
    if (s_page == PAGE_CHAT) render_page();
    UI_UNLOCK();
}

void app_ui_builtin(const char *text, const char *source)
{
    UI_LOCK();
    if (s_nturns == 0) {
        s_turns[0].q[0] = 0;
        s_nturns = 1;
    }
    s_turns[s_nturns - 1].builtin = true;
    sanitize(s_turns[s_nturns - 1].a, A_CHARS, text);
    if (s_page == PAGE_CHAT) render_page();
    char d[40];
    snprintf(d, sizeof d, "%s " UI_G_MIDDOT " no AI", source);
    app_ui_detail(d);
    UI_UNLOCK();
}

static char s_detail_text[64];

void app_ui_detail(const char *text)
{
    UI_LOCK();
    if (text != s_detail_text) snprintf(s_detail_text, sizeof s_detail_text, "%s", text ? text : "");
    char buf[80];
    if (s_detail_text[0]) snprintf(buf, sizeof buf, " " UI_G_RESULT " %s", s_detail_text);
    else buf[0] = 0;
    ui_box_set(&s_detail, buf);
    UI_UNLOCK();
}

void app_ui_clear_turn(void)
{
    UI_LOCK();
    s_nturns = 0;
    s_follow = true;
    if (s_page == PAGE_CHAT) render_page();
    app_ui_detail("");
    UI_UNLOCK();
}

void app_ui_llm_progress(const char *text, int in_tokens, int out_tokens, float tok_per_s)
{
    UI_LOCK();
    char d[48];
    // "AI" marks answers generated by the language model (vs "no AI" built-ins):
    // prompt tokens in, generated tokens out, generation speed.
    if (tok_per_s > 0)
        snprintf(d, sizeof d, "AI %d in " UI_G_MIDDOT " %d out " UI_G_MIDDOT " %.1f/s", in_tokens, out_tokens, tok_per_s);
    else
        snprintf(d, sizeof d, "AI %d in " UI_G_MIDDOT " %d out", in_tokens, out_tokens);
    s_last_rate = tok_per_s;
    app_ui_status("Thinking...", UI_BUSY);     // advances the spinner, pulses the icon
    app_ui_story(text);
    app_ui_detail(d);
    UI_UNLOCK();
}

float app_ui_last_tok_rate(void) { return s_last_rate; }

// ---------------------------------------------------------------- pages
void app_ui_page(app_page_t p)
{
    UI_LOCK();
    if (p != s_page) {
        s_page = p;
        s_follow = p == PAGE_CHAT;     // pages open at the top, chat at the bottom
        s_top = 0;
        render_page();
        bool settings = app_page_in_settings(p);
        app_ui_bar_state(BAR_MODEL, p == PAGE_MODELS ? BTN_ACTIVE : BTN_IDLE);
        app_ui_bar_state(BAR_SETTINGS, settings ? BTN_ACTIVE : BTN_IDLE);
        // While a keypad is open, its own Ask key is the one to press: grey
        // out the voice pill so there is no question which does what.
        app_ui_bar_state(BAR_ASK, app_ui_typing() || s_busy ? BTN_BUSY : BTN_IDLE);
    }
    UI_UNLOCK();
}

app_page_t app_ui_page_get(void) { return s_page; }

// A keypad is open: the question is being typed, not spoken.
bool app_ui_typing(void) { return s_page == PAGE_T9 || s_page == PAGE_KEYBOARD; }

void app_ui_refresh_page(void)
{
    UI_LOCK();
    if (s_page != PAGE_KEYBOARD && s_page != PAGE_T9) render_page();   // don't redraw a keypad under a finger
    UI_UNLOCK();
}

// ---------------------------------------------------------------- scrolling / taps
bool app_ui_convo_hit(int x, int y)
{
    (void)x;
    return y >= CONVO_Y && y < CONVO_Y + VIS * UI_ROW_H;
}

void app_ui_scroll_begin(void)
{
    UI_LOCK();
    s_drag_top0 = s_top;
    UI_UNLOCK();
}

void app_ui_scroll_drag(int dy_px)
{
    UI_LOCK();
    int max_top = s_total > VIS ? s_total - VIS : 0;
    int top = s_drag_top0 - dy_px / UI_ROW_H;     // drag down = see earlier rows
    if (top < 0) top = 0;
    if (top > max_top) top = max_top;
    s_follow = s_page == PAGE_CHAT && top >= max_top;   // back at the bottom: follow again
    if (top != s_top) {
        s_top = top;
        show_window();
    }
    UI_UNLOCK();
}

int app_ui_convo_tap(int x, int y)
{
    (void)x;
    if (!app_ui_convo_hit(x, y)) return -1;
    UI_LOCK();
    int row = s_top + (y - CONVO_Y) / UI_ROW_H;
    int tag = (row >= 0 && row < s_total) ? s_tags[row] : -1;
    if (tag < 0 && row >= 0 && row < s_total && s_rows[row][0] == 0) {
        // Blank spacer row: fingers land a few px off, so give the tap to
        // the nearer neighbouring row that has a tag.
        bool upper = (y - CONVO_Y) % UI_ROW_H < UI_ROW_H / 2;
        int first = upper ? row - 1 : row + 1, second = upper ? row + 1 : row - 1;
        if (first >= 0 && first < s_total && s_tags[first] >= 0) tag = s_tags[first];
        else if (second >= 0 && second < s_total && s_tags[second] >= 0) tag = s_tags[second];
    }
    UI_UNLOCK();
    return tag;
}

// ---------------------------------------------------------------- bottom bar
// Slim outlined pills in the CLI style: 1 px anti-aliased border; the Ask pill
// has a small symmetric spark. Each pill redraws only its own rect, only when
// its state changes.
static uint16_t mix565(uint16_t a, uint16_t b, float t)   // t: 0 -> a, 1 -> b
{
    if (t <= 0) return a;
    if (t >= 1) return b;
    int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
    int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    int r = ar + (int)((br - ar) * t + 0.5f), g = ag + (int)((bg - ag) * t + 0.5f),
        bl = ab + (int)((bb - ab) * t + 0.5f);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

// Signed distance to a rounded rectangle centered at 0 (half sizes hx, hy, radius r).
static float rrect(float x, float y, float hx, float hy, float r)
{
    float qx = fabsf(x) - (hx - r), qy = fabsf(y) - (hy - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = (qx > qy ? qx : qy);
    return sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
}

// Small symmetric 8-ray spark, radius R, centered at 0: 1 inside, 0 outside.
static float spark(float x, float y, float R)
{
    float r = sqrtf(x * x + y * y);
    if (r < 0.22f * R) return 1.0f;
    for (int k = 0; k < 8; k++) {
        float a = (float)k * 0.78539816f;
        float dx = cosf(a), dy = sinf(a);
        float along = x * dx + y * dy;
        if (along <= 0 || along > R) continue;
        float perp = fabsf(-x * dy + y * dx);
        if (perp <= 0.16f * R * (1.0f - 0.8f * along / R) + 0.3f) return 1.0f;
    }
    return 0.0f;
}

void app_ui_bar_state(app_bar_t b, app_btn_state_t st)
{
    if (b < 0 || b >= BAR_COUNT) return;
    UI_LOCK();
    if ((int)st == s_bar_state[b]) { UI_UNLOCK(); return; }   // dirty-region: only on change
    s_bar_state[b] = (int)st;
    const int W = BAR[b].w, H = BAR_H;
    const uint16_t bg = UI_BLACK;
    // While busy the Ask pill is the Stop button (active, pink); the others dim.
    const bool stop = b == BAR_ASK && s_busy;
    const char *label = stop ? ASK_BUSY_LABEL : BAR[b].label;
    if (stop && st == BTN_BUSY) st = BTN_ACTIVE;
    bool dim = st == BTN_BUSY;
    uint16_t fill = st == BTN_PRESSED ? RGB565(52, 34, 38) : bg;
    uint16_t edge = st == BTN_PRESSED || st == BTN_ACTIVE ? UI_ACCENT
                  : dim ? RGB565(55, 55, 55) : RGB565(95, 95, 95);
    uint16_t icon = dim ? RGB565(112, 84, 88) : UI_ACCENT;
    uint16_t text = dim ? RGB565(85, 85, 85) : st == BTN_ACTIVE ? UI_ACCENT : UI_WHITE;

    uint16_t *px = s_btn_px;
    const float cx = (W - 1) / 2.0f, cy = (H - 1) / 2.0f;
    const float hx = W / 2.0f - 0.5f, hy = H / 2.0f - 0.5f, rad = hy;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float ring = 0, inside = 0;          // 4x4 supersampled outline
            for (int s = 0; s < 16; s++) {
                float sx = x - cx + ((s & 3) - 1.5f) * 0.25f, sy = y - cy + ((s >> 2) - 1.5f) * 0.25f;
                float d = rrect(sx, sy, hx, hy, rad);
                if (fabsf(d + 0.5f) <= 0.5f) ring += 1;
                else if (d < -1.0f) inside += 1;
            }
            uint16_t c = mix565(bg, fill, inside / 16.0f);
            px[y * W + x] = mix565(c, edge, ring / 16.0f);
        }
    }
    int label_w = (int)strlen(label) * UI_FONT_W;
    int group = label_w + (BAR[b].spark ? 17 : 0), gx = (W - group) / 2;
    if (BAR[b].spark) {
        const float R = 5.5f, icx = gx + 5.0f, icy = cy;
        for (int y = 0; y < H; y++)
            for (int x = gx - 1; x < gx + 12; x++) {
                float cov = 0;
                for (int s = 0; s < 4; s++)
                    cov += spark(x - icx + ((s & 1) ? 0.25f : -0.25f), y - icy + ((s & 2) ? 0.25f : -0.25f), R);
                if (cov > 0) px[y * W + x] = mix565(px[y * W + x], icon, cov / 4.0f);
            }
        gx += 17;
    }
    ui_text_into(px, W, H, gx, (H - UI_FONT_H) / 2, label, text);

    board_lcd_window(BAR[b].x, BAR_Y, W, H);
    uint16_t *buf = board_lcd_stream_buf();
    for (int i = 0; i < W * H; i++) buf[i] = (uint16_t)((px[i] >> 8) | (px[i] << 8));
    board_lcd_stream_push(buf, (size_t)W * H);
    board_lcd_stream_end();
    UI_UNLOCK();
}

// ---------------------------------------------------------------- restart button
// A thin circle with a circular arrow (↻), top-right of the header box.
#define RST_S 24
#define RST_X (BOARD_LCD_W - UI_FONT_W - RST_S - 3)   // inside the box's right border
#define RST_Y (ROW_Y(1) + 2)
#define RST_TOUCH_PAD 10
static int s_rst_state = -1;

static bool in_tri(float px, float py, const float t[6])
{
    float d1 = (px - t[2]) * (t[1] - t[3]) - (t[0] - t[2]) * (py - t[3]);
    float d2 = (px - t[4]) * (t[3] - t[5]) - (t[2] - t[4]) * (py - t[5]);
    float d3 = (px - t[0]) * (t[5] - t[1]) - (t[4] - t[0]) * (py - t[1]);
    bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
    return !(neg && pos);
}

// Coverage test for the arrow icon at (x, y) relative to the center.
static bool restart_icon(float x, float y)
{
    const float r = 6.0f, hw = 0.95f;
    float d = sqrtf(x * x + y * y);
    float ang = atan2f(y, x);                    // screen coords: +y is down
    // Arc everywhere except a 70-degree gap at the upper right (-80..-10 deg).
    bool in_gap = ang > -1.40f && ang < -0.17f;
    if (!in_gap && fabsf(d - r) <= hw) return true;
    // Arrowhead at the gap's clockwise end (-80 deg), pointing clockwise.
    const float a = -1.40f, cx = r * cosf(a), cy = r * sinf(a);
    const float tx = -sinf(a), ty = cosf(a);     // tangent toward increasing angle (into the gap)
    const float nx = cosf(a), ny = sinf(a);      // outward normal
    const float t[6] = {cx + tx * 3.2f, cy + ty * 3.2f,          // tip, pointing into the gap
                        cx + nx * 3.0f, cy + ny * 3.0f,          // outer base
                        cx - nx * 3.0f, cy - ny * 3.0f};         // inner base
    return in_tri(x, y, t);
}

void app_ui_restart_state(app_btn_state_t st)
{
    UI_LOCK();
    if ((int)st == s_rst_state) { UI_UNLOCK(); return; }     // dirty-region: only on change
    s_rst_state = (int)st;
    uint16_t fill = st == BTN_PRESSED ? RGB565(52, 34, 38) : UI_BLACK;
    uint16_t edge = st == BTN_ACTIVE || st == BTN_PRESSED ? UI_ACCENT
                  : st == BTN_BUSY ? RGB565(55, 55, 55) : RGB565(95, 95, 95);
    uint16_t ink = st == BTN_ACTIVE || st == BTN_PRESSED ? UI_ACCENT
                 : st == BTN_BUSY ? RGB565(70, 70, 70) : RGB565(170, 170, 170);
    uint16_t *px = s_btn_px;
    const float c = (RST_S - 1) / 2.0f, R = RST_S / 2.0f - 0.5f;
    for (int y = 0; y < RST_S; y++)
        for (int x = 0; x < RST_S; x++) {
            float ring = 0, inside = 0, icon = 0;
            for (int s = 0; s < 16; s++) {                 // 4x4 supersampling
                float sx = x - c + ((s & 3) - 1.5f) * 0.25f, sy = y - c + ((s >> 2) - 1.5f) * 0.25f;
                float d = sqrtf(sx * sx + sy * sy) - R;
                if (fabsf(d + 0.5f) <= 0.5f) ring += 1;
                else if (d < -1.0f) inside += 1;
                if (restart_icon(sx, sy)) icon += 1;
            }
            uint16_t col = mix565(UI_BLACK, fill, inside / 16.0f);
            col = mix565(col, edge, ring / 16.0f);
            px[y * RST_S + x] = mix565(col, ink, icon / 16.0f);
        }
    board_lcd_window(RST_X, RST_Y, RST_S, RST_S);
    uint16_t *buf = board_lcd_stream_buf();
    for (int i = 0; i < RST_S * RST_S; i++) buf[i] = (uint16_t)((px[i] >> 8) | (px[i] << 8));
    board_lcd_stream_push(buf, RST_S * RST_S);
    board_lcd_stream_end();
    UI_UNLOCK();
}

// ---------------------------------------------------------------- lock button
// A small padlock at the right end of the model row: one tap puts the screen
// to sleep (the colour-cycling logo); any touch or "Hey Ivy" brings it back.
#define LOCK_TOUCH_PAD 12

// Padlock coverage at (x, y) relative to the icon's top-left, in a 14x14 box:
// a 2 px-thick shackle arc over a rounded body with a keyhole.
static bool lock_icon(float x, float y)
{
    const float cx = 6.5f;
    if (y < 6.5f) {                                   // shackle: half ring, r 3.2
        float dx = x - cx, dy = y - 6.0f;
        float d = sqrtf(dx * dx + dy * dy);
        return dy <= 0 && fabsf(d - 3.2f) <= 1.0f;
    }
    if (x < cx - 5.0f || x > cx + 5.0f || y > 13.0f) return false;
    if (fabsf(x - cx) <= 0.9f && y >= 8.5f && y <= 11.0f) return false;   // keyhole
    return true;
}

void app_ui_lock_state(app_btn_state_t st)
{
    UI_LOCK();
    if ((int)st == s_lock_state) { UI_UNLOCK(); return; }
    s_lock_state = (int)st;
    uint16_t ink = st == BTN_PRESSED || st == BTN_ACTIVE ? UI_ACCENT
                 : st == BTN_BUSY ? RGB565(70, 70, 70) : RGB565(170, 170, 170);
    uint16_t *px = s_btn_px;
    for (int y = 0; y < LOCK_S; y++)
        for (int x = 0; x < LOCK_S; x++) {
            float cov = 0;
            for (int s = 0; s < 4; s++)
                cov += lock_icon(x + ((s & 1) ? 0.25f : -0.25f), y + ((s & 2) ? 0.25f : -0.25f)) ? 1 : 0;
            px[y * LOCK_S + x] = mix565(UI_BLACK, ink, cov / 4.0f);
        }
    board_lcd_window(LOCK_X, LOCK_Y, LOCK_S, LOCK_S);
    uint16_t *buf = board_lcd_stream_buf();
    for (int i = 0; i < LOCK_S * LOCK_S; i++) buf[i] = (uint16_t)((px[i] >> 8) | (px[i] << 8));
    board_lcd_stream_push(buf, LOCK_S * LOCK_S);
    board_lcd_stream_end();
    UI_UNLOCK();
}

bool app_ui_lock_hit(int x, int y)
{
    return x >= LOCK_X - LOCK_TOUCH_PAD && x < LOCK_X + LOCK_S + LOCK_TOUCH_PAD &&
           y >= LOCK_Y - 2 && y < LOCK_Y + LOCK_S + LOCK_TOUCH_PAD;
}

// Waking from sleep: the logo covered everything, so the whole screen is
// rebuilt once (the only full repaint besides boot).
void app_ui_sleep_exit(void)
{
    UI_LOCK();
    if (s_asleep) {
        s_asleep = false;
        board_lcd_fill(0, 0, BOARD_LCD_W, BOARD_LCD_H, UI_BLACK);
        s_rst_state = s_lock_state = -1;
        draw_header();
        app_ui_restart_state(BTN_IDLE);
        app_ui_lock_state(BTN_IDLE);
        ui_box_invalidate(&s_convo);
        ui_box_invalidate(&s_detail);
        ui_box_invalidate(&s_status);
        render_page();                              // convo
        app_ui_detail(s_detail_text);               // both repaint from their
        app_ui_status(s_stat_text, s_stat_color);   // last text (invalidated above)
        for (int b = 0; b < BAR_COUNT; b++) {
            int st = s_bar_state[b];
            s_bar_state[b] = -1;
            app_ui_bar_state((app_bar_t)b, st < 0 ? BTN_IDLE : (app_btn_state_t)st);
        }
    }
    UI_UNLOCK();
}

// ---------------------------------------------------------------- battery
// "87% ⚡[|||| ]" on the title row, right-aligned just left of the restart
// button. The bolt slot is blank unless charging.
#define BAT_BODY_W 20                                   // 18 px body + 2 px nub
#define BAT_ICON_H 10
#define BAT_BOLT_W 9                                    // 7 px bolt + 2 px gap
#define BAT_ICON_W (BAT_BOLT_W + BAT_BODY_W)
#define BAT_ICON_X (RST_X - 6 - BAT_ICON_W)
#define BAT_TEXT_CH 4                                   // "100%"
#define BAT_TEXT_X (BAT_ICON_X - 3 - BAT_TEXT_CH * UI_FONT_W)
#define UI_BOLT UI_ACCENT                               // same pink as the battery and logo
static int s_bat_pct = -2;                              // -2: never drawn, -1: unknown
static bool s_bat_chg;

// Lightning bolt: a zig-zag polygon in a 7x12 box (even-odd point test).
static bool bolt_hit(float x, float y)
{
    static const float P[][2] = {{2.6f, 0.0f}, {7.0f, 0.0f}, {4.4f, 4.6f}, {7.0f, 4.6f},
                                 {1.0f, 12.2f}, {2.8f, 6.8f}, {0.0f, 6.8f}};
    const int n = sizeof P / sizeof P[0];
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((P[i][1] > y) != (P[j][1] > y) &&
            x < (P[j][0] - P[i][0]) * (y - P[i][1]) / (P[j][1] - P[i][1]) + P[i][0])
            in = !in;
    return in;
}

static void draw_battery(int pct)
{
    const bool chg = s_bat_chg;
    char txt[16];
    if (pct >= 0) snprintf(txt, sizeof txt, "%3d%%", pct);
    else snprintf(txt, sizeof txt, "  --");
    const bool low = pct >= 0 && pct <= 15 && !chg;
    uint16_t lvl = pct < 0 ? RGB565(95, 95, 95) : low ? UI_ERR : UI_ACCENT;
    ui_draw_row(BAT_TEXT_X, ROW_Y(1), BAT_TEXT_CH * UI_FONT_W, txt, low ? UI_ERR : UI_ACCENT, UI_BLACK);

    // Bolt (anti-aliased) then an 18x10 body with 1 px outline and clipped
    // corners, 2x4 nub on the right.
    const int H = UI_ROW_H, y0 = (UI_ROW_H - BAT_ICON_H) / 2;
    const uint16_t edge = pct < 0 ? RGB565(95, 95, 95) : low ? UI_ERR : UI_ACCENT;
    const int fill_w = pct <= 0 ? 0 : (14 * pct + 50) / 100 < 1 ? 1 : (14 * pct + 50) / 100;
    uint16_t *px = s_btn_px;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < BAT_ICON_W; x++) {
            uint16_t c = UI_BLACK;
            if (x < BAT_BOLT_W) {
                if (chg) {
                    float cov = 0;
                    for (int s = 0; s < 16; s++)              // 4x4 supersampling
                        cov += bolt_hit(x + ((s & 3) + 0.5f) * 0.25f, y - 1 + ((s >> 2) + 0.5f) * 0.25f);
                    c = mix565(UI_BLACK, UI_BOLT, cov / 16.0f);
                }
                px[y * BAT_ICON_W + x] = c;
                continue;
            }
            int bx = x - BAT_BOLT_W, by = y - y0;
            bool in_body = by >= 0 && by < BAT_ICON_H && bx < 18;
            bool corner = (bx == 0 || bx == 17) && (by == 0 || by == BAT_ICON_H - 1);
            if (in_body && !corner) {
                bool border = bx == 0 || bx == 17 || by == 0 || by == BAT_ICON_H - 1;
                if (border) c = edge;
                else if (bx >= 2 && bx < 2 + fill_w && by >= 2 && by < BAT_ICON_H - 2) c = lvl;
            } else if (bx >= 18 && by >= 3 && by < BAT_ICON_H - 3) {
                c = edge;                                // nub
            }
            px[y * BAT_ICON_W + x] = c;
        }
    board_lcd_window(BAT_ICON_X, ROW_Y(1), BAT_ICON_W, H);
    uint16_t *buf = board_lcd_stream_buf();
    for (int i = 0; i < BAT_ICON_W * H; i++) buf[i] = (uint16_t)((px[i] >> 8) | (px[i] << 8));
    board_lcd_stream_push(buf, (size_t)BAT_ICON_W * H);
    board_lcd_stream_end();
}

void app_ui_battery(int pct, bool charging)
{
    if (pct > 100) pct = 100;
    UI_LOCK();
    if (pct != s_bat_pct || charging != s_bat_chg) {     // dirty-region: only on change
        s_bat_pct = pct;
        s_bat_chg = charging;
        if (s_btn_px) draw_battery(pct);                 // before init: drawn with the header
    }
    UI_UNLOCK();
}

bool app_ui_restart_hit(int x, int y)
{
    // Stops short of the lock button one row below: the two must not overlap.
    int bottom = RST_Y + RST_S + RST_TOUCH_PAD;
    if (bottom > LOCK_Y - 2) bottom = LOCK_Y - 2;
    return x >= RST_X - RST_TOUCH_PAD && x < RST_X + RST_S + RST_TOUCH_PAD &&
           y >= RST_Y - RST_TOUCH_PAD && y < bottom;
}

int app_ui_bar_hit(int x, int y)
{
    if (y < BAR_Y - BAR_TOUCH_PAD_Y || y >= BAR_Y + BAR_H + BAR_TOUCH_PAD_Y) return -1;
    for (int b = 0; b < BAR_COUNT; b++)
        if (x >= BAR[b].x - 2 && x < BAR[b].x + BAR[b].w + 2) return b;
    return -1;
}

void app_ui_busy(bool busy)
{
    UI_LOCK();
    s_busy = busy;
    if (busy && s_page != PAGE_CHAT) app_ui_page(PAGE_CHAT);   // keypads and pages close while answering
    else if (s_page == PAGE_CHAT) render_page();               // show/hide the "Type a question" row
    app_ui_bar_state(BAR_ASK, busy ? BTN_BUSY : BTN_IDLE);
    app_ui_bar_state(BAR_MODEL, busy ? BTN_BUSY : BTN_IDLE);
    app_ui_bar_state(BAR_SETTINGS, busy ? BTN_BUSY : BTN_IDLE);
    app_ui_restart_state(busy ? BTN_BUSY : BTN_IDLE);
    UI_UNLOCK();
}

bool app_ui_is_busy(void) { return s_busy; }
