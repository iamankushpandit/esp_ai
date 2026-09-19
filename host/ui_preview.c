// Host preview of the device screen: the real ui.c / app_ui.c code drawing
// into a 240x320 RGB565 buffer (LCD calls stubbed), dumped as PPM frames.
// Also counts pixels pushed per update to verify dirty-row behavior.
#include "board.h"
#include "board_lcd_stream.h"
#include "app_ui.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

static uint16_t fb[BOARD_LCD_H][BOARD_LCD_W];
static uint16_t dma[BOARD_LCD_STREAM_PX];
static int wx, wy, ww, wh, wpos;
static long pushed;

void board_lcd_window(int x, int y, int w, int h) { wx = x; wy = y; ww = w; wh = h; wpos = 0; }
uint16_t *board_lcd_stream_buf(void) { return dma; }
void board_lcd_stream_push(uint16_t *buf, size_t n)
{
    for (size_t i = 0; i < n; i++, wpos++) {
        int x = wx + wpos % ww, y = wy + wpos / ww;
        uint16_t c = (uint16_t)((buf[i] >> 8) | (buf[i] << 8));   // undo byte swap
        if (x < BOARD_LCD_W && y < BOARD_LCD_H) fb[y][x] = c;
    }
    pushed += (long)n;
}
void board_lcd_stream_end(void) {}
void board_lcd_fill(int x, int y, int w, int h, uint16_t c)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) fb[j][i] = c;
    pushed += (long)w * h;
}

static void dump(const char *path)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", BOARD_LCD_W, BOARD_LCD_H);
    for (int y = 0; y < BOARD_LCD_H; y++)
        for (int x = 0; x < BOARD_LCD_W; x++) {
            uint16_t c = fb[y][x];
            unsigned char px[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2),
                                   (unsigned char)((c & 31) << 3)};
            fwrite(px, 1, 3, f);
        }
    fclose(f);
}

#define STEP(label, code) do { pushed = 0; code; \
    printf("%-34s repainted %5ld px (%4.1f%% of screen)\n", label, pushed, 100.0 * pushed / (BOARD_LCD_W * BOARD_LCD_H)); } while (0)

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char p[256];
    STEP("boot (init)", app_ui_init(); app_ui_status("Ready", UI_GREEN); app_ui_footer("BOOT to ask " UI_G_MIDDOT " offline"));
    snprintf(p, sizeof p, "%s/ui_0_idle.ppm", dir); dump(p);
    STEP("listening", app_ui_clear_turn(); app_ui_status("Listening...", UI_ACCENT));
    STEP("transcribing", app_ui_status("Transcribing...", UI_ACCENT));
    STEP("transcript shown", app_ui_you("tell me a story about a cat"));
    snprintf(p, sizeof p, "%s/ui_1_listen.ppm", dir); dump(p);
    const char *ans = "Once upon a time, there was a big, strong cat. The cat was very good at jumping "
                      "and played all day. One day, the cat saw a little mouse.";
    char part[256];
    int n = (int)strlen(ans);
    for (int k = 12, i = 1; k < n; k += 16, i++) {
        memcpy(part, ans, k); part[k] = 0;
        char lbl[40]; snprintf(lbl, sizeof lbl, "stream update %d", i);
        STEP(lbl, app_ui_llm_progress(part, i * 3, 6.2f));
    }
    STEP("answer final", app_ui_llm_progress(ans, 34, 6.3f));
    snprintf(p, sizeof p, "%s/ui_2_thinking.ppm", dir); dump(p);
    STEP("speaking", app_ui_status("Speaking...", UI_ACCENT));
    STEP("done", app_ui_status("Done", UI_GREEN));
    snprintf(p, sizeof p, "%s/ui_3_done.ppm", dir); dump(p);
    return 0;
}
