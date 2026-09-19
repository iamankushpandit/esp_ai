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
#define UI_DIM RGB565(110, 110, 110)
#define UI_CYAN RGB565(80, 220, 255)
#define UI_YELLOW RGB565(255, 220, 60)
#define UI_GREEN RGB565(90, 230, 120)
#define UI_RED RGB565(255, 80, 80)
#define UI_ACCENT RGB565(217, 119, 87)   // terracotta, CLI-style accent

// Extra glyphs (byte codes, see tools/gen_font.py). Use as separate string
// literals so a following hex digit isn't swallowed: UI_G_BULLET " " "text".
#define UI_G_TL "\x80"
#define UI_G_TR "\x81"
#define UI_G_BL "\x82"
#define UI_G_BR "\x83"
#define UI_G_H "\x84"
#define UI_G_V "\x85"
#define UI_G_BULLET "\x86"
#define UI_G_RESULT "\x87"
#define UI_G_MIDDOT "\x88"
#define UI_G_ELLIPSIS "\x89"
#define UI_G_SPIN0 0x8A              // spinner frames 0x8A..0x8D
#define UI_SPIN_FRAMES 4

typedef struct {
    int16_t x, y, w;
    uint8_t cols, rows;
    uint16_t fg, bg;
    uint8_t lead_n;          // first lead_n chars of the first row use lead_fg
    uint16_t lead_fg;
    uint8_t hang;            // continuation rows are indented by this many cols
    uint8_t nmarks;          // rows starting with mark_ch[k] draw that char in mark_fg[k]
    char mark_ch[2];
    uint16_t mark_fg[2];
    char shown[UI_MAX_ROWS][UI_MAX_COLS + 1];
} ui_box_t;

// Word-wrap `text` into rows of at most `cols` chars; continuation rows start
// with `hang` spaces. Returns total rows the text needs (may exceed max_rows;
// only the LAST max_rows are written, so streaming text shows its tail).
// Bytes 0x20..0x7E and the UI glyph codes pass through; anything else -> '?'.
// Pure function, host-testable.
int ui_wrap_ex(const char *text, int cols, int hang, char out[][UI_MAX_COLS + 1], int max_rows);
int ui_wrap(const char *text, int cols, char out[][UI_MAX_COLS + 1], int max_rows);

void ui_box_init(ui_box_t *b, int x, int y, int w, int rows, uint16_t fg, uint16_t bg);
// Colored lead (e.g. bullet) + hanging indent.
void ui_box_style(ui_box_t *b, uint8_t lead_n, uint16_t lead_fg, uint8_t hang);
// Per-row marker coloring (e.g. '>' dim, bullet accent) for transcript boxes.
void ui_box_mark(ui_box_t *b, int k, char ch, uint16_t fg);
// Returns number of rows repainted.
int ui_box_set(ui_box_t *b, const char *text);
void ui_box_set_color(ui_box_t *b, uint16_t fg);  // repaints non-empty rows
void ui_box_invalidate(ui_box_t *b);               // next set() repaints all rows

// Render text into an off-screen RGB565 buffer (CPU byte order) of bw x bh
// pixels at (x, y); only glyph pixels are written. For small custom widgets.
void ui_text_into(uint16_t *buf, int bw, int bh, int x, int y, const char *s, uint16_t fg);

// Draw one row of text at (x,y), padded with bg to width w.
void ui_draw_row(int x, int y, int w, const char *s, uint16_t fg, uint16_t bg);
// Same, with a per-character color: palette[attr[i]] (attr may be NULL).
void ui_draw_row_attr(int x, int y, int w, const char *s, const uint8_t *attr,
                      const uint16_t *palette, uint16_t bg);

#ifdef __cplusplus
}
#endif
