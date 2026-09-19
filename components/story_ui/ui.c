#include "ui.h"
#include "font_8x13.h"
#include "board.h"
#include "board_lcd_stream.h"
#include <string.h>

void ui_draw_row(int x, int y, int w, const char *s, uint16_t fg, uint16_t bg)
{
    // One row (w x UI_ROW_H px) fits in one DMA buffer (240*14 = 3360 < 4096).
    uint16_t sfg = (uint16_t)((fg >> 8) | (fg << 8));
    uint16_t sbg = (uint16_t)((bg >> 8) | (bg << 8));
    int n = (int)strlen(s);
    board_lcd_window(x, y, w, UI_ROW_H);
    uint16_t *buf = board_lcd_stream_buf();
    uint16_t *p = buf;
    for (int r = 0; r < UI_ROW_H; r++) {
        for (int px = 0; px < w; px++) {
            int ci = px / UI_FONT_W;
            uint8_t bits = 0;
            if (ci < n && r < UI_FONT_H) {
                unsigned char c = (unsigned char)s[ci];
                if (c >= 0x20 && c < 0x7F) bits = font8x13[c - 0x20][r];
            }
            *p++ = (bits & (0x80 >> (px % UI_FONT_W))) ? sfg : sbg;
        }
    }
    board_lcd_stream_push(buf, (size_t)w * UI_ROW_H);
    board_lcd_stream_end();
}

void ui_box_init(ui_box_t *b, int x, int y, int w, int rows, uint16_t fg, uint16_t bg)
{
    memset(b, 0, sizeof(*b));
    b->x = x;
    b->y = y;
    b->w = w;
    b->cols = (uint8_t)(w / UI_FONT_W);
    b->rows = (uint8_t)(rows > UI_MAX_ROWS ? UI_MAX_ROWS : rows);
    b->fg = fg;
    b->bg = bg;
}

void ui_box_invalidate(ui_box_t *b)
{
    for (int i = 0; i < b->rows; i++) b->shown[i][0] = '\x01';  // never equals real text
}

int ui_box_set(ui_box_t *b, const char *text)
{
    char next[UI_MAX_ROWS][UI_MAX_COLS + 1];
    memset(next, 0, sizeof next);
    int need = ui_wrap(text, b->cols, next, b->rows);
    int used = need < b->rows ? need : b->rows;
    int painted = 0;
    for (int i = 0; i < b->rows; i++) {
        const char *want = i < used ? next[i] : "";
        if (strcmp(want, b->shown[i]) != 0) {
            ui_draw_row(b->x, b->y + i * UI_ROW_H, b->w, want, b->fg, b->bg);
            strcpy(b->shown[i], want);
            painted++;
        }
    }
    return painted;
}

void ui_box_set_color(ui_box_t *b, uint16_t fg)
{
    if (b->fg == fg) return;
    b->fg = fg;
    for (int i = 0; i < b->rows; i++) {
        if (b->shown[i][0]) {
            ui_draw_row(b->x, b->y + i * UI_ROW_H, b->w, b->shown[i], b->fg, b->bg);
        }
    }
}
