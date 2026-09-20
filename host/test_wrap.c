// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Host test: word wrap used by the dirty-region UI.
#include "ui.h"
#include "check.h"
#include <string.h>

int main(void)
{
    char r[UI_MAX_ROWS][UI_MAX_COLS + 1];
    int n = ui_wrap("Why is the sky blue?", 10, r, 5);
    CHECK(n == 2);
    CHECK(strcmp(r[0], "Why is the") == 0);
    CHECK(strcmp(r[1], "sky blue?") == 0);

    n = ui_wrap("abcdefghijklmnop", 10, r, 5);          // hard break
    CHECK(n == 2 && strcmp(r[0], "abcdefghij") == 0 && strcmp(r[1], "klmnop") == 0);

    n = ui_wrap("one\ntwo", 10, r, 5);
    CHECK(n == 2 && strcmp(r[1], "two") == 0);

    n = ui_wrap("", 10, r, 5);
    CHECK(n == 1 && r[0][0] == 0);

    n = ui_wrap("a b c d e f g h", 3, r, 2);             // tail kept on overflow
    CHECK(n == 4);
    CHECK(strcmp(r[0], "e f") == 0 && strcmp(r[1], "g h") == 0);

    n = ui_wrap("caf\xc3\xa9", 10, r, 2);                // non-ASCII -> '?'
    CHECK(strcmp(r[0], "caf??") == 0);
    // hanging indent: continuation rows start with 2 spaces
    n = ui_wrap_ex("> tell me a story about a cat", 12, 2, r, 5);
    CHECK(n == 4);
    CHECK(strcmp(r[0], "> tell me a") == 0);
    CHECK(strcmp(r[1], "  story") == 0);
    CHECK(strcmp(r[2], "  about a") == 0);
    CHECK(strcmp(r[3], "  cat") == 0);

    // hard newline starts flush left even with a hanging indent
    n = ui_wrap_ex("> hi there friend\n\n> next", 12, 2, r, 6);
    CHECK(n == 4);
    CHECK(strcmp(r[0], "> hi there") == 0);
    CHECK(strcmp(r[1], "  friend") == 0);
    CHECK(strcmp(r[2], "") == 0);
    CHECK(strcmp(r[3], "> next") == 0);

    // large ring (> UI_MAX_ROWS) keeps the tail in order
    static char big[40][UI_MAX_COLS + 1];
    char text[400] = "";
    for (int i = 0; i < 50; i++) { char w[8]; snprintf(w, sizeof w, "w%02d\n", i); strcat(text, w); }
    n = ui_wrap_ex(text, 10, 0, big, 40);
    CHECK(n == 50);
    CHECK(strcmp(big[0], "w10") == 0 && strcmp(big[39], "w49") == 0);

    // row marker carries onto continuation rows of its own line only
    n = ui_wrap_ex(UI_G_ROWMARK "dim text that wraps\nplain", 12, 2, r, 6);
    CHECK(n == 3);
    CHECK(r[0][0] == UI_G_ROWMARK[0] && r[1][0] == UI_G_ROWMARK[0]);
    CHECK(strcmp(r[1] + 1, "that wraps") == 0);   // aligned with line 1
    CHECK(strcmp(r[2], "plain") == 0);

    // UI glyph bytes pass through; other high bytes don't
    n = ui_wrap("\x86 ok \xff", 10, r, 2);
    CHECK((unsigned char)r[0][0] == 0x86 && r[0][5] == '?');
    printf("test_wrap OK\n");
    return 0;
}
