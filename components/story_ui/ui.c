#include "ui.h"
#include "font_8x13.h"
#include "board.h"
#include "board_lcd_stream.h"
#include <string.h>

static inline const uint8_t *glyph(unsigned char c, bool *extends)
{
    *extends = false;
    if (c >= 0x20 && c < 0x7F) return font8x13[c - 0x20];
    if (c >= FONT8X13_EXTRA_FIRST && c < FONT8X13_EXTRA_FIRST + FONT8X13_EXTRA_COUNT) {
        *extends = true;   // box-drawing glyphs continue through the leading row
        return font8x13_extra[c - FONT8X13_EXTRA_FIRST];
    }
    return NULL;
}

void ui_draw_row_attr(int x, int y, int w, const char *s, const uint8_t *attr,
                      const uint16_t *palette, uint16_t bg)
{
    // One row (w x UI_ROW_H px) fits in one DMA buffer (240*14 = 3360 < 4096).
    uint16_t sbg = (uint16_t)((bg >> 8) | (bg << 8));
    int n = (int)strlen(s);
    board_lcd_window(x, y, w, UI_ROW_H);
    uint16_t *buf = board_lcd_stream_buf();
    uint16_t *p = buf;
    for (int r = 0; r < UI_ROW_H; r++) {
        for (int ci = 0; ci * UI_FONT_W < w; ci++) {
            uint8_t bits = 0;
            uint16_t fg = palette[0];
            if (ci < n) {
                bool ext;
                const uint8_t *g = glyph((unsigned char)s[ci], &ext);
                if (g) {
                    if (r < UI_FONT_H) bits = g[r];
                    else if (ext) bits = g[UI_FONT_H - 1];
                }
                if (attr) fg = palette[attr[ci]];
            }
            uint16_t sfg = (uint16_t)((fg >> 8) | (fg << 8));
            for (int b = 0; b < UI_FONT_W && ci * UI_FONT_W + b < w; b++)
                *p++ = (bits & (0x80 >> b)) ? sfg : sbg;
        }
    }
    board_lcd_stream_push(buf, (size_t)w * UI_ROW_H);
    board_lcd_stream_end();
}

void ui_text_into(uint16_t *buf, int bw, int bh, int x, int y, const char *s, uint16_t fg)
{
    for (int ci = 0; s[ci]; ci++) {
        bool ext;
        const uint8_t *g = glyph((unsigned char)s[ci], &ext);
        if (!g) continue;
        for (int r = 0; r < UI_FONT_H; r++) {
            int py = y + r;
            if (py < 0 || py >= bh) continue;
            for (int b = 0; b < UI_FONT_W; b++) {
                int px = x + ci * UI_FONT_W + b;
                if (px >= 0 && px < bw && (g[r] & (0x80 >> b))) buf[py * bw + px] = fg;
            }
        }
    }
}

void ui_draw_row(int x, int y, int w, const char *s, uint16_t fg, uint16_t bg)
{
    ui_draw_row_attr(x, y, w, s, NULL, &fg, bg);
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

void ui_box_style(ui_box_t *b, uint8_t lead_n, uint16_t lead_fg, uint8_t hang)
{
    b->lead_n = lead_n;
    b->lead_fg = lead_fg;
    b->hang = hang;
}

void ui_box_invalidate(ui_box_t *b)
{
    for (int i = 0; i < b->rows; i++) b->shown[i][0] = '\x01';  // never equals real text
}

void ui_box_mark(ui_box_t *b, int k, char ch, uint16_t fg)
{
    if (k < 0 || k > 2) return;
    b->mark_ch[k] = ch;
    b->mark_fg[k] = fg;
    b->mark_row[k] = false;
    if (b->nmarks < k + 1) b->nmarks = (uint8_t)(k + 1);
}

void ui_box_mark_row(ui_box_t *b, int k, char ch, uint16_t fg)
{
    ui_box_mark(b, k, ch, fg);
    if (k >= 0 && k <= 2) b->mark_row[k] = true;
}

static void paint(ui_box_t *b, int i, const char *s)
{
    int y = b->y + i * UI_ROW_H;
    if (i == 0 && b->lead_n) {
        uint8_t attr[UI_MAX_COLS] = {0};
        for (int k = 0; k < b->lead_n && k < UI_MAX_COLS; k++) attr[k] = 1;
        uint16_t pal[2] = {b->fg, b->lead_fg};
        ui_draw_row_attr(b->x, y, b->w, s, attr, pal, b->bg);
        return;
    }
    for (int k = 0; k < b->nmarks; k++) {
        if (s[0] && s[0] == b->mark_ch[k]) {
            uint8_t attr[UI_MAX_COLS] = {1};
            if (b->mark_row[k]) memset(attr, 1, sizeof attr);
            uint16_t pal[2] = {b->fg, b->mark_fg[k]};
            ui_draw_row_attr(b->x, y, b->w, s, attr, pal, b->bg);
            return;
        }
    }
    ui_draw_row(b->x, y, b->w, s, b->fg, b->bg);
}

int ui_box_set(ui_box_t *b, const char *text)
{
    char next[UI_MAX_ROWS][UI_MAX_COLS + 1];
    memset(next, 0, sizeof next);
    int need = ui_wrap_ex(text, b->cols, b->hang, next, b->rows);
    return ui_box_set_rows(b, (const char (*)[UI_MAX_COLS + 1])next, need < b->rows ? need : b->rows);
}

int ui_box_set_rows(ui_box_t *b, const char (*rows)[UI_MAX_COLS + 1], int n)
{
    int used = n < b->rows ? n : b->rows;
    int painted = 0;
    for (int i = 0; i < b->rows; i++) {
        const char *want = i < used ? rows[i] : "";
        if (strcmp(want, b->shown[i]) != 0) {
            paint(b, i, want);
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
    for (int i = 0; i < b->rows; i++)
        if (b->shown[i][0]) paint(b, i, b->shown[i]);
}
