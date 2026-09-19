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

    // UI glyph bytes pass through; other high bytes don't
    n = ui_wrap("\x86 ok \xff", 10, r, 2);
    CHECK((unsigned char)r[0][0] == 0x86 && r[0][5] == '?');
    printf("test_wrap OK\n");
    return 0;
}
