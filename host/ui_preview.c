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
static int wx, wy, ww, wpos;
static long pushed;

void board_lcd_window(int x, int y, int w, int h) { (void)h; wx = x; wy = y; ww = w; wpos = 0; }
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

static const char *s_dir;
static int s_frame;

static void dump(void)
{
    char path[256];
    snprintf(path, sizeof path, "%s/ui_%d.ppm", s_dir, s_frame++);
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

static void stream(const char *ans, int tok, float rate)
{
    char part[512];
    int n = (int)strlen(ans);
    for (int k = 14, i = 1; k < n; k += 18, i++) {
        memcpy(part, ans, (size_t)k);
        part[k] = 0;
        STEP("  stream update", app_ui_llm_progress(part, i * 3, rate));
    }
    STEP("  answer final", app_ui_llm_progress(ans, tok, rate));
}

int main(int argc, char **argv)
{
    s_dir = argc > 1 ? argv[1] : ".";
    STEP("boot (init)", app_ui_init(); app_ui_status("Ready", UI_GREEN));
    dump();

    STEP("button pressed", app_ui_button(BTN_PRESSED));
    dump();
    STEP("turn 1: listening", app_ui_button(BTN_BUSY); app_ui_clear_turn(); app_ui_status("Listening...", UI_ACCENT));
    STEP("turn 1: transcribing", app_ui_status("Transcribing...", UI_ACCENT));
    STEP("turn 1: transcript", app_ui_you("what color is a banana"));
    stream("I know! The banana is yellow.", 8, 6.1f);
    STEP("turn 1: speaking", app_ui_status("Speaking...", UI_ACCENT));
    STEP("follow-up 1 listening", app_ui_status("Listening... follow-up 1/3", UI_ACCENT));
    STEP("turn 2: transcript", app_ui_you("how many legs does a dog have"));
    stream("A dog has four legs.", 6, 6.3f);
    dump();

    STEP("follow-up 2 listening", app_ui_status("Listening... follow-up 2/3", UI_ACCENT));
    STEP("turn 3: transcript (scrolls)", app_ui_you("tell me a story about a cat"));
    stream("Once upon a time, there was a big, strong cat. The cat saw a little mouse. "
           "They became good friends. Bye bye.", 34, 6.4f);
    STEP("turn 3: speaking", app_ui_status("Speaking...", UI_ACCENT));
    dump();
    STEP("session end", app_ui_status("Session ended", UI_GREY); app_ui_button(BTN_IDLE));
    return 0;
}
