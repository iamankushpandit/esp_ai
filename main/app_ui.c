#include "app_ui.h"
#include "board.h"
#include "ui.h"
#include "board_lcd_stream.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ROW_Y(r) (2 + (r) * UI_ROW_H)
#define COLS (BOARD_LCD_W / UI_FONT_W)   // 30

#define MAX_TURNS 4
#define Q_CHARS 160
#define A_CHARS 480

static ui_box_t s_status, s_convo, s_detail;
static int s_spin;
static float s_last_rate;

// Session transcript (bounded: at most MAX_TURNS turns, oldest dropped).
static struct {
    char q[Q_CHARS];
    char a[A_CHARS];
} s_turns[MAX_TURNS];
static int s_nturns;

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

static void render_convo(void)
{
    static char buf[MAX_TURNS * (Q_CHARS + A_CHARS + 8)];
    size_t n = 0;
    buf[0] = 0;
    for (int i = 0; i < s_nturns; i++) {
        if (i > 0) n += (size_t)snprintf(buf + n, sizeof buf - n, "\n\n");   // blank line between turns
        if (s_turns[i].q[0]) n += (size_t)snprintf(buf + n, sizeof buf - n, "> %s", s_turns[i].q);
        if (s_turns[i].a[0])
            n += (size_t)snprintf(buf + n, sizeof buf - n, "%s" UI_G_BULLET " %s",
                                  s_turns[i].q[0] ? "\n" : "", s_turns[i].a);
    }
    ui_box_set(&s_convo, buf);
}

static void header_row(int r, const char *text, int accent_at, int white_from, int white_n)
{
    char row[COLS + 1];
    uint8_t attr[COLS] = {0};
    const uint16_t pal[3] = {UI_DIM, UI_ACCENT, UI_WHITE};
    memset(row, ' ', COLS);
    row[COLS] = 0;
    row[0] = row[COLS - 1] = UI_G_V[0];
    size_t n = strlen(text);
    memcpy(row + 2, text, n > COLS - 4 ? COLS - 4 : n);
    if (accent_at >= 0) attr[2 + accent_at] = 1;
    for (int k = 0; k < white_n; k++) attr[2 + white_from + k] = 2;
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

    char title[16] = {(char)(UI_G_SPIN0 + 3), ' '};
    strcpy(title + 2, "Braino AI");
    header_row(1, title, 0, 2, 9);                  // ✻ accent, "Braino AI" white
    header_row(2, "  (c) iamankushpandit", -1, 0, 0);

    row[0] = UI_G_BL[0];
    memset(row + 1, UI_G_H[0], COLS - 2);
    row[COLS - 1] = UI_G_BR[0];
    ui_draw_row(0, ROW_Y(3), BOARD_LCD_W, row, UI_DIM, UI_BLACK);
}

void app_ui_init(void)
{
    // The only full-screen fill: once at boot, before the backlight is on.
    board_lcd_fill(0, 0, BOARD_LCD_W, BOARD_LCD_H, UI_BLACK);
    draw_header();
    ui_box_init(&s_convo, 0, ROW_Y(4) + 4, BOARD_LCD_W, 12, UI_WHITE, UI_BLACK);
    ui_box_style(&s_convo, 0, UI_WHITE, 2);        // 2-col hanging indent
    ui_box_mark(&s_convo, 0, '>', UI_DIM);
    ui_box_mark(&s_convo, 1, UI_G_BULLET[0], UI_ACCENT);
    ui_box_init(&s_detail, 0, ROW_Y(16) + 6, BOARD_LCD_W, 1, UI_DIM, UI_BLACK);
    ui_box_init(&s_status, 0, ROW_Y(17) + 6, BOARD_LCD_W, 1, UI_ACCENT, UI_BLACK);
    ui_box_style(&s_status, 1, UI_ACCENT, 0);
    app_ui_button(BTN_IDLE);
}

void app_ui_status(const char *s, uint16_t color)
{
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
}

void app_ui_you(const char *text)
{
    if (s_nturns == MAX_TURNS) {   // drop the oldest turn
        memmove(&s_turns[0], &s_turns[1], sizeof s_turns[0] * (MAX_TURNS - 1));
        s_nturns--;
    }
    sanitize(s_turns[s_nturns].q, Q_CHARS, text);
    s_turns[s_nturns].a[0] = 0;
    s_nturns++;
    app_ui_detail("");
    render_convo();
}

void app_ui_story(const char *text)
{
    if (s_nturns == 0) {           // an answer with no question (e.g. an error)
        s_turns[0].q[0] = 0;
        s_nturns = 1;
    }
    sanitize(s_turns[s_nturns - 1].a, A_CHARS, text);
    render_convo();
}

void app_ui_detail(const char *text)
{
    char buf[48];
    if (text && text[0]) snprintf(buf, sizeof buf, "  " UI_G_RESULT "  %s", text);
    else buf[0] = 0;
    ui_box_set(&s_detail, buf);
}

// ---------------------------------------------------------------- ask button
// A slim outlined pill "✻ Ask" in the CLI style: 1 px border, small
// symmetric spark, white label. Anti-aliased edges (4x4 supersampling for the
// outline, 2x2 for the spark). Only this 84x26 rect is ever redrawn.
#define BTN_W 84
#define BTN_H 26
#define BTN_X ((BOARD_LCD_W - BTN_W) / 2)
#define BTN_Y 280
#define BTN_TOUCH_PAD 14                 // touch target larger than the drawing

static int s_btn_state = -1;

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

void app_ui_button(app_btn_state_t st)
{
    if ((int)st == s_btn_state) return;                  // dirty-region: only on change
    s_btn_state = (int)st;
    const uint16_t bg = UI_BLACK;
    uint16_t fill = st == BTN_PRESSED ? RGB565(48, 28, 22) : bg;
    uint16_t edge = st == BTN_PRESSED ? UI_ACCENT : st == BTN_BUSY ? RGB565(60, 60, 60) : RGB565(95, 95, 95);
    uint16_t icon = st == BTN_BUSY ? RGB565(110, 70, 58) : UI_ACCENT;
    uint16_t text = st == BTN_BUSY ? RGB565(90, 90, 90) : UI_WHITE;

    static uint16_t px[BTN_W * BTN_H];
    const float cx = (BTN_W - 1) / 2.0f, cy = (BTN_H - 1) / 2.0f;
    const float hx = BTN_W / 2.0f - 0.5f, hy = BTN_H / 2.0f - 0.5f, rad = hy;
    for (int y = 0; y < BTN_H; y++) {
        for (int x = 0; x < BTN_W; x++) {
            // outline coverage: |distance| < 0.5 px band, supersampled
            float ring = 0, inside = 0;
            for (int s = 0; s < 16; s++) {
                float sx = x - cx + ((s & 3) - 1.5f) * 0.25f, sy = y - cy + ((s >> 2) - 1.5f) * 0.25f;
                float d = rrect(sx, sy, hx, hy, rad);
                if (fabsf(d + 0.5f) <= 0.5f) ring += 1;
                else if (d < -1.0f) inside += 1;
            }
            uint16_t c = mix565(bg, fill, inside / 16.0f);
            px[y * BTN_W + x] = mix565(c, edge, ring / 16.0f);
        }
    }
    // Icon + label, centered as a group: [spark 11 px][gap 6][Ask 24 px]
    const float R = 5.5f;
    const int group = 11 + 6 + 3 * UI_FONT_W, gx = (BTN_W - group) / 2;
    const float icx = gx + 5.0f, icy = cy;
    for (int y = 0; y < BTN_H; y++) {
        for (int x = gx - 1; x < gx + 12; x++) {
            float cov = 0;
            for (int s = 0; s < 4; s++)
                cov += spark(x - icx + ((s & 1) ? 0.25f : -0.25f), y - icy + ((s & 2) ? 0.25f : -0.25f), R);
            if (cov > 0) px[y * BTN_W + x] = mix565(px[y * BTN_W + x], icon, cov / 4.0f);
        }
    }
    ui_text_into(px, BTN_W, BTN_H, gx + 17, (BTN_H - UI_FONT_H) / 2, "Ask", text);

    board_lcd_window(BTN_X, BTN_Y, BTN_W, BTN_H);
    uint16_t *buf = board_lcd_stream_buf();
    for (int i = 0; i < BTN_W * BTN_H; i++) buf[i] = (uint16_t)((px[i] >> 8) | (px[i] << 8));
    board_lcd_stream_push(buf, BTN_W * BTN_H);
    board_lcd_stream_end();
}

bool app_ui_button_hit(int x, int y)
{
    return x >= BTN_X - BTN_TOUCH_PAD && x < BTN_X + BTN_W + BTN_TOUCH_PAD &&
           y >= BTN_Y - BTN_TOUCH_PAD && y < BTN_Y + BTN_H + BTN_TOUCH_PAD;
}

void app_ui_clear_turn(void)
{
    s_nturns = 0;
    render_convo();
    app_ui_detail("");
}

void app_ui_llm_progress(const char *text, int tokens, float tok_per_s)
{
    char d[40];
    if (tok_per_s > 0) snprintf(d, sizeof d, "%d tok " UI_G_MIDDOT " %.1f tok/s", tokens, tok_per_s);
    else snprintf(d, sizeof d, "%d tok", tokens);
    s_last_rate = tok_per_s;
    app_ui_status("Thinking...", UI_ACCENT);   // advances the spinner
    app_ui_story(text);
    app_ui_detail(d);
}

float app_ui_last_tok_rate(void) { return s_last_rate; }
