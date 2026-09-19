#include "ui.h"
#include <string.h>

#define ROWMARK ((char)0x8E)

static char clean(unsigned char c)
{
    if (c >= 0x20 && c < 0x7F) return (char)c;
    if (c >= 0x80 && c <= 0x8E) return (char)c;   // UI glyphs + row marker (see ui.h)
    return '?';
}

static void rev_rows(char out[][UI_MAX_COLS + 1], int a, int b)
{
    char t[UI_MAX_COLS + 1];
    for (; a < b; a++, b--) {
        memcpy(t, out[a], sizeof t);
        memcpy(out[a], out[b], sizeof t);
        memcpy(out[b], t, sizeof t);
    }
}

// Greedy word wrap into a ring of max_rows rows, so we keep the tail.
// Soft wraps continue with a `hang`-column indent; a '\n' starts flush left.
// A line that begins with the row marker keeps the marker as the first char of
// its continuation rows (so the whole wrapped line can share one color).
int ui_wrap_ex(const char *text, int cols, int hang, char out[][UI_MAX_COLS + 1], int max_rows)
{
    if (cols > UI_MAX_COLS) cols = UI_MAX_COLS;
    if (hang < 0 || hang >= cols) hang = 0;
    int nrows = 0;
    char row[UI_MAX_COLS + 1];
    int len = 0, base = 0;          // base = indent chars at the start of this row
    bool marked = text && text[0] == ROWMARK;
    const char *p = text ? text : "";

    // soft=1: continuation of the same line (hanging indent, keeps the row
    // marker, which then occupies exactly the marker's column so the text
    // aligns with line 1); soft=0: hard newline, flush left.
#define EMIT(soft) do {                                             \
        row[len] = 0;                                               \
        memcpy(out[nrows % max_rows], row, (size_t)len + 1);        \
        nrows++;                                                    \
        len = 0;                                                    \
        base = !(soft) ? 0 : marked ? 1 : hang;                     \
        for (int h_ = 0; h_ < base; h_++) row[len++] = ' ';         \
        if ((soft) && marked) row[0] = ROWMARK;                     \
    } while (0)
#define ROW_EMPTY() (len == base)

    while (*p) {
        if (*p == '\n') {
            EMIT(0);
            p++;
            marked = *p == ROWMARK;
            continue;
        }
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
            EMIT(1);
        } else {
            while (len > 0 && row[len - 1] == ' ') len--;   // strip trailing space before wrapping
            EMIT(1);
        }
    }
    if (!ROW_EMPTY() || nrows == 0) EMIT(0);
#undef EMIT
#undef ROW_EMPTY

    // Rotate the ring in place (three reversals) so rows are in order when we
    // overflowed; works for any max_rows without a temp buffer.
    if (nrows > max_rows) {
        int start = nrows % max_rows;
        rev_rows(out, 0, start - 1);
        rev_rows(out, start, max_rows - 1);
        rev_rows(out, 0, max_rows - 1);
    }
    return nrows;
}

int ui_wrap(const char *text, int cols, char out[][UI_MAX_COLS + 1], int max_rows)
{
    return ui_wrap_ex(text, cols, 0, out, max_rows);
}
