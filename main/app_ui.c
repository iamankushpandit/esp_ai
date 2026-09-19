#include "app_ui.h"
#include "board.h"
#include "ui.h"
#include "board_lcd_stream.h"
#include "models.h"
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
#define CONVO_Y (ROW_Y(4) + 4)
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
    [BAR_MODEL] = {4, 70, "/model", false},
    [BAR_ASK] = {80, 80, "Ask", true},
    [BAR_ABOUT] = {166, 70, "/about", false},
};
#define BAR_MAX_W 80

static ui_box_t s_status, s_convo, s_detail;
static int s_spin;
static float s_last_rate;

// Session transcript (bounded: at most MAX_TURNS turns, oldest dropped).
// All larger UI buffers live in PSRAM (allocated in app_ui_init): internal
// RAM must keep a contiguous 264 KB block free for the phase arena.
typedef struct {
    char q[Q_CHARS];
    char a[A_CHARS];
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
        add_para(UI_G_ROWMARK "Tap Ask and ask a question.", -1);
        add_para("", -1);
        add_prose(UI_G_ROWMARK "Answers come from a small AI and may be wrong. See /about.");
        return;
    }
    char *buf = s_text;
    size_t n = 0, cap = TEXT_BYTES;
    buf[0] = 0;
    for (int i = 0; i < s_nturns; i++) {
        if (i > 0) n += (size_t)snprintf(buf + n, cap - n, "\n\n");   // blank line between turns
        if (s_turns[i].q[0]) n += (size_t)snprintf(buf + n, cap - n, "> %s", s_turns[i].q);
        if (s_turns[i].a[0])
            n += (size_t)snprintf(buf + n, cap - n, "%s" UI_G_BULLET " %s",
                                  s_turns[i].q[0] ? "\n" : "", s_turns[i].a);
    }
    add_para(buf, -1);
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
    add_para("Braino AI", -1);
    add_para(UI_G_ROWMARK "(c) iamankushpandit", -1);
    add_para("", -1);
    add_prose("An offline voice assistant. Everything runs on this device: no internet, no cloud.");
    add_para("", -1);
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

static void render_page(void)
{
    s_total = 0;
    switch (s_page) {
    case PAGE_CHAT: build_chat(); break;
    case PAGE_MODELS: build_models(); break;
    case PAGE_ABOUT: build_about(); break;
    }
    show_window();
}

// ---------------------------------------------------------------- logo
// 4-bit coverage masks generated by tools/gen_logo.py from assets/braino_logo.png.
#include "logo_data.h"

static uint16_t blend565(uint16_t bg, uint16_t fg, int a15)
{
    int br = bg >> 11, bgg = (bg >> 5) & 63, bb = bg & 31;
    int fr = fg >> 11, fgg = (fg >> 5) & 63, fb = fg & 31;
    int r = br + (fr - br) * a15 / 15, g = bgg + (fgg - bgg) * a15 / 15, b = bb + (fb - bb) * a15 / 15;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// Draw a 4-bit mask tinted `fg` on `bg`, streamed in strips that fit one DMA buffer.
static void blit_mask(const uint8_t *mask, int w, int h, int x, int y, uint16_t fg, uint16_t bg)
{
    const int stride = (w + 1) / 2;
    const int strip = BOARD_LCD_STREAM_PX / w;
    for (int y0 = 0; y0 < h; y0 += strip) {
        int rows = h - y0 < strip ? h - y0 : strip;
        board_lcd_window(x, y + y0, w, rows);
        uint16_t *buf = board_lcd_stream_buf();
        for (int r = 0; r < rows; r++) {
            const uint8_t *src = mask + (size_t)(y0 + r) * stride;
            for (int c = 0; c < w; c++) {
                int a = (c & 1) ? (src[c >> 1] & 15) : (src[c >> 1] >> 4);
                uint16_t col = a == 0 ? bg : a == 15 ? fg : blend565(bg, fg, a);
                buf[r * w + c] = (uint16_t)((col >> 8) | (col << 8));
            }
        }
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
    blit_mask(logo_full, LOGO_FULL_W, LOGO_FULL_H, lx, ly, UI_ACCENT, UI_BLACK);
    const char *c = "(c) iamankushpandit";
    int cw = (int)strlen(c) * UI_FONT_W;
    ui_draw_row((BOARD_LCD_W - cw) / 2, ly + LOGO_FULL_H + 18, cw, c, UI_DIM, UI_BLACK);
    UI_UNLOCK();
}

// ---------------------------------------------------------------- header
// ╭──────────────────────────╮
// │ [brain] Braino AI        │   brain icon (orange) spans the two text rows
// │         (c) iamankushpandit
// ╰──────────────────────────╯
#define HDR_TEXT_COL 5                   // text starts right of the icon

static void header_row(int r, const char *text, uint16_t text_fg)
{
    char row[COLS + 1];
    uint8_t attr[COLS];
    const uint16_t pal[2] = {UI_DIM, text_fg};
    memset(row, ' ', COLS);
    memset(attr, 0, sizeof attr);
    row[COLS] = 0;
    row[0] = row[COLS - 1] = UI_G_V[0];
    size_t n = strlen(text);
    if (n > COLS - 1 - HDR_TEXT_COL) n = COLS - 1 - HDR_TEXT_COL;
    memcpy(row + HDR_TEXT_COL, text, n);
    memset(attr + HDR_TEXT_COL, 1, n);
    ui_draw_row_attr(0, ROW_Y(r), BOARD_LCD_W, row, attr, pal, UI_BLACK);
}

static void draw_header(void)
{
    char row[COLS + 1];
    row[COLS] = 0;
    row[0] = UI_G_TL[0];
    memset(row + 1, UI_G_H[0], COLS - 2);
    row[COLS - 1] = UI_G_TR[0];
    ui_draw_row(0, ROW_Y(0), BOARD_LCD_W, row, UI_DIM, UI_BLACK);

    header_row(1, "Braino AI", UI_WHITE);
    header_row(2, "(c) iamankushpandit", UI_DIM);
    // Brain icon over the two text rows, left of the title (after the rows,
    // so their background doesn't cover it).
    blit_mask(logo_brain, LOGO_BRAIN_W, LOGO_BRAIN_H,
              UI_FONT_W + ((HDR_TEXT_COL - 1) * UI_FONT_W - LOGO_BRAIN_W) / 2,   // centered in cols 1..4
              ROW_Y(1), UI_ACCENT, UI_BLACK);

    row[0] = UI_G_BL[0];
    memset(row + 1, UI_G_H[0], COLS - 2);
    row[COLS - 1] = UI_G_BR[0];
    ui_draw_row(0, ROW_Y(3), BOARD_LCD_W, row, UI_DIM, UI_BLACK);
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
        s_btn_px = UI_ALLOC(sizeof(uint16_t) * BAR_MAX_W * BAR_H);
    }
    UI_LOCK();
    // The only full-screen fill: once at boot, before the backlight is on.
    board_lcd_fill(0, 0, BOARD_LCD_W, BOARD_LCD_H, UI_BLACK);
    draw_header();
    ui_box_init(&s_convo, 0, CONVO_Y, CONVO_W, VIS, UI_WHITE, UI_BLACK);
    ui_box_mark(&s_convo, 0, '>', UI_DIM);
    ui_box_mark(&s_convo, 1, UI_G_BULLET[0], UI_ACCENT);
    ui_box_mark_row(&s_convo, 2, UI_G_ROWMARK[0], UI_DIM);
    ui_box_init(&s_detail, 0, ROW_Y(16) + 6, BOARD_LCD_W, 1, UI_DIM, UI_BLACK);
    ui_box_init(&s_status, 0, ROW_Y(17) + 6, BOARD_LCD_W, 1, UI_ACCENT, UI_BLACK);
    ui_box_style(&s_status, 1, UI_ACCENT, 0);
    for (int b = 0; b < BAR_COUNT; b++) app_ui_bar_state((app_bar_t)b, BTN_IDLE);
    render_page();
    UI_UNLOCK();
}

void app_ui_status(const char *s, uint16_t color)
{
    UI_LOCK();
    char buf[64];
    bool idle = color == UI_GREEN || color == UI_GREY;
    char lead = idle ? UI_G_MIDDOT[0] : (char)(UI_G_SPIN0 + (s_spin++ % UI_SPIN_FRAMES));
    snprintf(buf, sizeof buf, "%c %s", lead, s);
    char *dots = strstr(buf, "...");
    if (dots) { dots[0] = UI_G_ELLIPSIS[0]; memmove(dots + 1, dots + 3, strlen(dots + 3) + 1); }
    uint16_t fg = idle ? UI_DIM : (color == UI_RED ? UI_RED : UI_ACCENT);
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

void app_ui_detail(const char *text)
{
    UI_LOCK();
    char buf[48];
    if (text && text[0]) snprintf(buf, sizeof buf, "  " UI_G_RESULT "  %s", text);
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

void app_ui_llm_progress(const char *text, int tokens, float tok_per_s)
{
    UI_LOCK();
    char d[40];
    if (tok_per_s > 0) snprintf(d, sizeof d, "%d tok " UI_G_MIDDOT " %.1f tok/s", tokens, tok_per_s);
    else snprintf(d, sizeof d, "%d tok", tokens);
    s_last_rate = tok_per_s;
    app_ui_status("Thinking...", UI_ACCENT);   // advances the spinner
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
        app_ui_bar_state(BAR_MODEL, p == PAGE_MODELS ? BTN_ACTIVE : BTN_IDLE);
        app_ui_bar_state(BAR_ABOUT, p == PAGE_ABOUT ? BTN_ACTIVE : BTN_IDLE);
    }
    UI_UNLOCK();
}

app_page_t app_ui_page_get(void) { return s_page; }

void app_ui_refresh_page(void)
{
    UI_LOCK();
    render_page();
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
    bool dim = st == BTN_BUSY;
    uint16_t fill = st == BTN_PRESSED ? RGB565(48, 28, 22) : bg;
    uint16_t edge = st == BTN_PRESSED || st == BTN_ACTIVE ? UI_ACCENT
                  : dim ? RGB565(55, 55, 55) : RGB565(95, 95, 95);
    uint16_t icon = dim ? RGB565(110, 70, 58) : UI_ACCENT;
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
    int label_w = (int)strlen(BAR[b].label) * UI_FONT_W;
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
    ui_text_into(px, W, H, gx, (H - UI_FONT_H) / 2, BAR[b].label, text);

    board_lcd_window(BAR[b].x, BAR_Y, W, H);
    uint16_t *buf = board_lcd_stream_buf();
    for (int i = 0; i < W * H; i++) buf[i] = (uint16_t)((px[i] >> 8) | (px[i] << 8));
    board_lcd_stream_push(buf, (size_t)W * H);
    board_lcd_stream_end();
    UI_UNLOCK();
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
    if (busy && s_page != PAGE_CHAT) app_ui_page(PAGE_CHAT);
    app_ui_bar_state(BAR_ASK, busy ? BTN_BUSY : BTN_IDLE);
    app_ui_bar_state(BAR_MODEL, busy ? BTN_BUSY : BTN_IDLE);
    app_ui_bar_state(BAR_ABOUT, busy ? BTN_BUSY : BTN_IDLE);
    UI_UNLOCK();
}

bool app_ui_is_busy(void) { return s_busy; }
