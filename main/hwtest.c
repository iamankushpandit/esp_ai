// Milestone 1 hardware tests, driven by serial commands.
#include "hwtest.h"
#include "board.h"
#include "ui.h"
#include "story_mem.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>


#define REC_MAX_S 10
static int16_t *s_rec;       // PSRAM recording buffer
static size_t s_rec_n;

static void stats(const int16_t *x, size_t n, int *peak, float *rms)
{
    int p = 0;
    double acc = 0;
    for (size_t i = 0; i < n; i++) {
        int v = abs(x[i]);
        if (v > p) p = v;
        acc += (double)x[i] * x[i];
    }
    *peak = p;
    *rms = n ? (float)sqrt(acc / n) : 0;
}

static void play_tone(int hz, int ms, int amp)
{
    const int N = 256;
    int16_t buf[256];
    int total = BOARD_AUDIO_RATE * ms / 1000;
    int fade = BOARD_AUDIO_RATE * 4 / 1000;   // 4 ms fades avoid clicks
    board_spk_start();
    for (int i = 0; i < total; i += N) {
        for (int k = 0; k < N; k++) {
            int t = i + k;
            float g = 1.0f;
            if (t < fade) g = (float)t / fade;
            else if (t > total - fade) g = (float)(total - t) / fade;
            if (g < 0) g = 0;
            buf[k] = (int16_t)(amp * g * sinf(2 * (float)M_PI * hz * t / BOARD_AUDIO_RATE));
        }
        board_spk_write(buf, N, 1000);
    }
    board_spk_stop();
}

static void record(float secs)
{
    if (!s_rec) s_rec = heap_caps_malloc(REC_MAX_S * BOARD_AUDIO_RATE * 2, MALLOC_CAP_SPIRAM);
    size_t want = (size_t)(secs * BOARD_AUDIO_RATE);
    if (want > REC_MAX_S * BOARD_AUDIO_RATE) want = REC_MAX_S * BOARD_AUDIO_RATE;
    board_mic_start();
    s_rec_n = 0;
    int64_t t0 = story_time_us();
    while (s_rec_n < want) {
        size_t n = want - s_rec_n > 512 ? 512 : want - s_rec_n;
        s_rec_n += board_mic_read(s_rec + s_rec_n, n, 1000);
    }
    board_mic_stop();
    int peak;
    float rms;
    stats(s_rec, s_rec_n, &peak, &rms);
    printf("REC n=%u %.2fs wall=%lldms peak=%d rms=%.1f\n", (unsigned)s_rec_n,
           (float)s_rec_n / BOARD_AUDIO_RATE, (story_time_us() - t0) / 1000, peak, rms);
}

// Speaker -> mic acoustic loopback: record while a tone plays in another task.
static void tone_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(300));
    play_tone(1000, 800, 12000);
    vTaskDelete(NULL);
}

static void loopback(void)
{
    record(0.3f);  // warm-up / baseline
    int bpeak; float brms;
    stats(s_rec, s_rec_n, &bpeak, &brms);
    xTaskCreatePinnedToCore(tone_task, "tone", 4096, NULL, 5, NULL, 0);
    // NOTE: board_mic_start mutes the amp; for this test we record without the
    // mute by reading raw.
    size_t want = BOARD_AUDIO_RATE * 3 / 2;
    s_rec_n = 0;
    while (s_rec_n < want) s_rec_n += board_mic_read(s_rec + s_rec_n, 512, 1000);
    // Goertzel at 1 kHz over the tone window
    float coeff = 2 * cosf(2 * (float)M_PI * 1000 / BOARD_AUDIO_RATE);
    float s1 = 0, s2 = 0;
    size_t a = BOARD_AUDIO_RATE * 4 / 10, b = BOARD_AUDIO_RATE * 10 / 10;
    for (size_t i = a; i < b; i++) { float s0 = s_rec[i] + coeff * s1 - s2; s2 = s1; s1 = s0; }
    float pw = sqrtf(s1 * s1 + s2 * s2 - coeff * s1 * s2) / (b - a);
    int peak; float rms;
    stats(s_rec + a, b - a, &peak, &rms);
    printf("LOOPBACK baseline_rms=%.1f tone_window_rms=%.1f peak=%d goertzel1k=%.1f -> %s\n",
           brms, rms, peak, pw, pw > 50 ? "PASS" : "FAIL");
}

static void playback(void)
{
    if (!s_rec_n) { printf("nothing recorded\n"); return; }
    board_spk_start();
    for (size_t i = 0; i < s_rec_n; i += 512) {
        size_t n = s_rec_n - i > 512 ? 512 : s_rec_n - i;
        board_spk_write(s_rec + i, n, 1000);
    }
    board_spk_stop();
    printf("PLAYED %u samples\n", (unsigned)s_rec_n);
}

static void sd_bench(int mb)
{
    if (!board_sd_mounted()) { printf("SD not mounted\n"); return; }
    const size_t CH = 64 * 1024;
    uint8_t *buf = heap_caps_malloc(CH, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf) { printf("no DMA buf\n"); return; }
    for (size_t i = 0; i < CH; i++) buf[i] = (uint8_t)(i * 7);
    mkdir(BOARD_SD_MOUNT "/story", 0777);
    const char *path = BOARD_SD_MOUNT "/story/bench.bin";
    FILE *f = fopen(path, "wb");
    if (!f) { printf("open fail errno=%d\n", errno); free(buf); return; }
    int64_t t0 = story_time_us();
    for (int i = 0; i < mb * 16; i++) fwrite(buf, 1, CH, f);
    fclose(f);
    int64_t t1 = story_time_us();
    f = fopen(path, "rb");
    size_t tot = 0, r;
    while ((r = fread(buf, 1, CH, f)) > 0) tot += r;
    fclose(f);
    int64_t t2 = story_time_us();
    // PSRAM destination read (what model loads do)
    uint8_t *pbuf = heap_caps_malloc(1 << 20, MALLOC_CAP_SPIRAM);
    f = fopen(path, "rb");
    int64_t t3 = story_time_us();
    size_t ptot = 0;
    while (pbuf && ptot < (size_t)mb << 20 && (r = fread(pbuf, 1, 1 << 20, f)) > 0) ptot += r;
    int64_t t4 = story_time_us();
    fclose(f);
    free(pbuf);
    printf("SDBENCH %d MB: write %.2f MB/s, read(int 64K) %.2f MB/s, read(psram 1M) %.2f MB/s\n", mb,
           mb / ((t1 - t0) / 1e6), tot / 1048576.0 / ((t2 - t1) / 1e6), ptot / 1048576.0 / ((t4 - t3) / 1e6));
    free(buf);
}

static void sd_ls(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) { printf("opendir %s failed\n", dir); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        char p[300];
        struct stat st;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        stat(p, &st);
        printf("  %-40s %10ld\n", e->d_name, (long)st.st_size);
    }
    closedir(d);
}

static void touch_test(int secs)
{
    int64_t end = story_time_us() + (int64_t)secs * 1000000;
    int lx = -1, ly = -1;
    while (story_time_us() < end) {
        int x, y;
        if (board_touch_read(&x, &y)) {
            if (x != lx || y != ly) {
                printf("TOUCH %d %d\n", x, y);
                board_lcd_fill(x - 2, y - 2, 5, 5, UI_BUSY);
                lx = x; ly = y;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    printf("TOUCH done\n");
}

void hwtest_command(const char *line)
{
    char cmd[32] = {0};
    float arg = 0;
    sscanf(line, "%31s %f", cmd, &arg);
    if (!strcmp(cmd, "mem")) {
        story_mem_log("cmd");
    } else if (!strcmp(cmd, "bat")) {
        printf("BAT %d mV\n", board_battery_mv());
    } else if (!strcmp(cmd, "tone")) {
        play_tone(arg > 0 ? (int)arg : 1000, 800, 12000);
        printf("TONE done\n");
    } else if (!strcmp(cmd, "rec")) {
        record(arg > 0 ? arg : 3);
    } else if (!strcmp(cmd, "play")) {
        playback();
    } else if (!strcmp(cmd, "loop")) {
        loopback();
    } else if (!strcmp(cmd, "vol")) {
        board_audio_set_volume((int)arg);
        printf("VOL %d\n", (int)arg);
    } else if (!strcmp(cmd, "gain")) {
        board_audio_set_mic_gain((int)arg);
        printf("GAIN %d\n", (int)arg);
    } else if (!strcmp(cmd, "sdmount")) {
        printf("SDMOUNT %s\n", esp_err_to_name(board_sd_mount(false)));
    } else if (!strcmp(cmd, "sdformat")) {
        printf("SDFORMAT %s\n", esp_err_to_name(board_sd_format()));
        board_sd_info();
    } else if (!strcmp(cmd, "sdbench")) {
        sd_bench(arg > 0 ? (int)arg : 16);
    } else if (!strcmp(cmd, "ls")) {
        char dir[128] = BOARD_SD_MOUNT;
        sscanf(line, "%*s %127s", dir);
        sd_ls(dir);
    } else if (!strcmp(cmd, "head")) {
        char path[128] = {0};
        long off = 0;
        sscanf(line, "%*s %127s %ld", path, &off);
        FILE *f = fopen(path, "rb");
        uint8_t b[16] = {0};
        int sk = f ? fseek(f, off, SEEK_SET) : -1;
        size_t r = f ? fread(b, 1, 16, f) : 0;
        printf("HEAD %s @%ld: open=%d seek=%d read=%u errno=%d ferror=%d:", path, off, f != NULL, sk,
               (unsigned)r, errno, f ? ferror(f) : -1);
        for (int i = 0; i < 16; i++) printf(" %02x", b[i]);
        printf("\n");
        if (f) fclose(f);
    } else if (!strcmp(cmd, "touch")) {
        touch_test(arg > 0 ? (int)arg : 10);
    } else if (!strcmp(cmd, "bl")) {
        board_backlight((uint8_t)arg);
    } else {
        printf("? commands: mem bat tone rec play loop vol gain sdmount sdformat sdbench ls touch bl\n");
    }
    printf("OK\n");
}
