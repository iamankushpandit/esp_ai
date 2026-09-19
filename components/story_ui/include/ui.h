// Minimal dirty-region text UI. No framebuffer, no LVGL.
// A ui_box_t is a rectangle of fixed-pitch text rows. ui_box_set() word-wraps
// new text and repaints ONLY rows whose content changed.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_FONT_W 8
#define UI_FONT_H 13
#define UI_ROW_H 14                 // font + 1 px leading
#define UI_MAX_COLS 30              // 240 / 8
#define UI_MAX_ROWS 22

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define UI_BLACK RGB565(0, 0, 0)
#define UI_WHITE RGB565(255, 255, 255)
#define UI_GREY RGB565(140, 140, 140)
#define UI_CYAN RGB565(80, 220, 255)
#define UI_YELLOW RGB565(255, 220, 60)
#define UI_GREEN RGB565(90, 230, 120)
#define UI_RED RGB565(255, 80, 80)

typedef struct {
    int16_t x, y, w;
    uint8_t cols, rows;
    uint16_t fg, bg;
    char shown[UI_MAX_ROWS][UI_MAX_COLS + 1];
} ui_box_t;

// Word-wrap `text` into rows of at most `cols` chars. Returns total rows the
// text needs (may exceed max_rows; only the LAST max_rows are written, so
// streaming text shows its tail). Pure function, host-testable.
int ui_wrap(const char *text, int cols, char out[][UI_MAX_COLS + 1], int max_rows);

void ui_box_init(ui_box_t *b, int x, int y, int w, int rows, uint16_t fg, uint16_t bg);
// Returns number of rows repainted.
int ui_box_set(ui_box_t *b, const char *text);
void ui_box_set_color(ui_box_t *b, uint16_t fg);  // forces repaint of non-empty rows
void ui_box_invalidate(ui_box_t *b);               // next set() repaints all rows

// Draw one row of text at (x,y), padded with bg to width w.
void ui_draw_row(int x, int y, int w, const char *s, uint16_t fg, uint16_t bg);

#ifdef __cplusplus
}
#endif
