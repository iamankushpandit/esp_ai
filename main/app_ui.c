#include "app_ui.h"
#include "board.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

#define ROW_Y(r) (2 + (r) * UI_ROW_H)
#define COLS (BOARD_LCD_W / UI_FONT_W)   // 30

static ui_box_t s_status, s_you, s_story, s_detail, s_footer;
static int s_spin;
static float s_last_rate;

// Copy model/STT text, keeping plain ASCII only (UI glyph codes live above
// 0x7F, so stray UTF-8 bytes must never reach the renderer).
static void sanitize(char *dst, size_t cap, const char *src)
{
    size_t n = 0;
    for (; src && *src && n + 1 < cap; src++) {
        unsigned char c = (unsigned char)*src;
        dst[n++] = (c >= 0x20 && c < 0x7F) || c == '\n' ? (char)c : '?';
    }
    dst[n] = 0;
}

static void draw_header(void)
{
    char row[COLS + 1];
    uint8_t attr[COLS];
    const uint16_t pal[3] = {UI_DIM, UI_ACCENT, UI_WHITE};

    // ╭──────╮
    row[0] = UI_G_TL[0];
    memset(row + 1, UI_G_H[0], COLS - 2);
    row[COLS - 1] = UI_G_TR[0];
    row[COLS] = 0;
    ui_draw_row(0, ROW_Y(0), BOARD_LCD_W, row, UI_DIM, UI_BLACK);

    // │ ✻ Story      │
    memset(row, ' ', COLS);
    memset(attr, 0, sizeof attr);
    row[0] = row[COLS - 1] = UI_G_V[0];
    row[2] = (char)(UI_G_SPIN0 + 3);
    attr[2] = 1;
    memcpy(row + 4, "Story", 5);
    memset(attr + 4, 2, 5);
    ui_draw_row_attr(0, ROW_Y(1), BOARD_LCD_W, row, attr, pal, UI_BLACK);

    // │   offline voice assistant │
    memset(row, ' ', COLS);
    row[0] = row[COLS - 1] = UI_G_V[0];
    memcpy(row + 4, "offline voice assistant", 23);
    ui_draw_row(0, ROW_Y(2), BOARD_LCD_W, row, UI_DIM, UI_BLACK);

    // ╰──────╯
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
    ui_box_init(&s_you, 0, ROW_Y(5), BOARD_LCD_W, 4, UI_WHITE, UI_BLACK);
    ui_box_style(&s_you, 1, UI_DIM, 2);            // dim "> ", indented wrap
    ui_box_init(&s_story, 0, ROW_Y(10), BOARD_LCD_W, 9, UI_WHITE, UI_BLACK);
    ui_box_style(&s_story, 1, UI_ACCENT, 2);       // accent bullet, indented wrap
    ui_box_init(&s_detail, 0, ROW_Y(19), BOARD_LCD_W, 1, UI_DIM, UI_BLACK);
    ui_box_init(&s_status, 0, ROW_Y(20), BOARD_LCD_W, 1, UI_ACCENT, UI_BLACK);
    ui_box_style(&s_status, 1, UI_ACCENT, 0);
    ui_box_init(&s_footer, 0, ROW_Y(21), BOARD_LCD_W, 1, UI_DIM, UI_BLACK);
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
    char clean[256], buf[260];
    sanitize(clean, sizeof clean, text);
    if (clean[0]) snprintf(buf, sizeof buf, "> %s", clean);
    else buf[0] = 0;
    ui_box_set(&s_you, buf);
}

void app_ui_story(const char *text)
{
    static char clean[512], buf[520];
    sanitize(clean, sizeof clean, text);
    if (clean[0]) snprintf(buf, sizeof buf, UI_G_BULLET " %s", clean);
    else buf[0] = 0;
    ui_box_set(&s_story, buf);
}

void app_ui_detail(const char *text)
{
    char buf[48];
    if (text && text[0]) snprintf(buf, sizeof buf, "  " UI_G_RESULT "  %s", text);
    else buf[0] = 0;
    ui_box_set(&s_detail, buf);
}

void app_ui_footer(const char *text)
{
    char buf[48];
    snprintf(buf, sizeof buf, "  %s", text ? text : "");
    ui_box_set(&s_footer, buf);
}

void app_ui_clear_turn(void)
{
    ui_box_set(&s_you, "");
    ui_box_set(&s_story, "");
    ui_box_set(&s_detail, "");
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
