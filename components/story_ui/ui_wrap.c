#include "ui.h"
#include <string.h>

// Greedy word wrap into a ring of max_rows rows, so we keep the tail.
int ui_wrap(const char *text, int cols, char out[][UI_MAX_COLS + 1], int max_rows)
{
    if (cols > UI_MAX_COLS) cols = UI_MAX_COLS;
    int nrows = 0;
    char row[UI_MAX_COLS + 1];
    int len = 0;
    const char *p = text ? text : "";

#define EMIT() do { row[len] = 0; memcpy(out[nrows % max_rows], row, len + 1); nrows++; len = 0; } while (0)

    while (*p) {
        if (*p == '\n') { EMIT(); p++; continue; }
        if (*p == ' ' && len == 0 && nrows > 0) { p++; continue; }  // no leading spaces on wrapped rows
        // measure next word (or single space)
        const char *q = p;
        if (*q == ' ') q++;
        else while (*q && *q != ' ' && *q != '\n') q++;
        int wl = (int)(q - p);
        if (len + wl <= cols) {
            for (int i = 0; i < wl; i++) {
                unsigned char c = (unsigned char)p[i];
                row[len++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
            }
            p = q;
        } else if (wl > cols || len == 0) {
            // hard-break a word longer than a row
            while (len < cols && *p && *p != ' ' && *p != '\n') {
                unsigned char c = (unsigned char)*p++;
                row[len++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
            }
            EMIT();
        } else {
            // strip trailing space before wrapping
            while (len > 0 && row[len - 1] == ' ') len--;
            EMIT();
        }
    }
    if (len > 0 || nrows == 0) EMIT();
#undef EMIT

    // Rotate ring so rows are in order when we overflowed.
    if (nrows > max_rows) {
        char tmp[UI_MAX_ROWS][UI_MAX_COLS + 1];
        int start = nrows % max_rows;
        for (int i = 0; i < max_rows; i++) memcpy(tmp[i], out[(start + i) % max_rows], UI_MAX_COLS + 1);
        memcpy(out, tmp, sizeof(tmp[0]) * max_rows);
    }
    return nrows;
}
