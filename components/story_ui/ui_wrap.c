#include "ui.h"
#include <string.h>

static char clean(unsigned char c)
{
    if (c >= 0x20 && c < 0x7F) return (char)c;
    if (c >= 0x80 && c <= 0x8D) return (char)c;   // UI glyphs (see ui.h)
    return '?';
}

// Greedy word wrap into a ring of max_rows rows, so we keep the tail.
int ui_wrap_ex(const char *text, int cols, int hang, char out[][UI_MAX_COLS + 1], int max_rows)
{
    if (cols > UI_MAX_COLS) cols = UI_MAX_COLS;
    if (hang < 0 || hang >= cols) hang = 0;
    int nrows = 0;
    char row[UI_MAX_COLS + 1];
    int len = 0;
    const char *p = text ? text : "";

#define EMIT() do { row[len] = 0; memcpy(out[nrows % max_rows], row, len + 1); nrows++; \
                    len = 0; for (int h_ = 0; h_ < hang; h_++) row[len++] = ' '; } while (0)
#define ROW_EMPTY() (len == (nrows > 0 ? hang : 0))

    while (*p) {
        if (*p == '\n') { EMIT(); p++; continue; }
        if (*p == ' ' && ROW_EMPTY() && nrows > 0) { p++; continue; }  // no leading spaces on wrapped rows
        // measure next word (or single space)
        const char *q = p;
        if (*q == ' ') q++;
        else while (*q && *q != ' ' && *q != '\n') q++;
        int wl = (int)(q - p);
        if (len + wl <= cols) {
            for (int i = 0; i < wl; i++) row[len++] = clean((unsigned char)p[i]);
            p = q;
        } else if (wl > cols - hang || ROW_EMPTY()) {
            // hard-break a word longer than a row
            while (len < cols && *p && *p != ' ' && *p != '\n') row[len++] = clean((unsigned char)*p++);
            EMIT();
        } else {
            while (len > 0 && row[len - 1] == ' ') len--;   // strip trailing space before wrapping
            EMIT();
        }
    }
    if (!ROW_EMPTY() || nrows == 0) EMIT();
#undef EMIT
#undef ROW_EMPTY

    // Rotate ring so rows are in order when we overflowed.
    if (nrows > max_rows) {
        char tmp[UI_MAX_ROWS][UI_MAX_COLS + 1];
        int start = nrows % max_rows;
        for (int i = 0; i < max_rows; i++) memcpy(tmp[i], out[(start + i) % max_rows], UI_MAX_COLS + 1);
        memcpy(out, tmp, sizeof(tmp[0]) * max_rows);
    }
    return nrows;
}

int ui_wrap(const char *text, int cols, char out[][UI_MAX_COLS + 1], int max_rows)
{
    return ui_wrap_ex(text, cols, 0, out, max_rows);
}
