// Story assistant — entry point.
#include "board.h"
#include "ui.h"
#include "app_ui.h"
#include "phase.h"
#include "think.h"
#include "speak.h"
#include "hear.h"
#include "pipeline.h"
#include "driver/gpio.h"
#include "console.h"
#include "hwtest.h"
#include "upload.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "main";

#define FAST_ARENA_BYTES (264 * 1024)
#define BULK_ARENA_BYTES (7 * 1024 * 1024)

static void ui_stream(void *u, const char *text, int tok, float rate) { app_ui_llm_progress(text, tok, rate); }

static void cmd_ask(const char *q)
{
    static llm_history_t hist;   // test: independent questions
    llm_history_clear(&hist);
    char ans[LLM_ANSWER_CHARS + 1];
    llm_stats_t st;
    app_ui_clear_turn();
    app_ui_you(q);
    app_ui_status("Thinking...", UI_YELLOW);
    if (think_answer(&hist, q, ans, sizeof ans, ui_stream, NULL, &st)) {
        printf("ANSWER: %s\n", ans);
        app_ui_status("Ready", UI_GREEN);
    } else {
        app_ui_story("I can't answer that right now.");
        app_ui_status("LLM error", UI_RED);
        printf("ANSWER ERROR\n");
    }
}

static void ui_status_cb(const char *s) { app_ui_status(s, UI_YELLOW); }

static hear_params_t s_hear;

static void cmd_hear(const char *wav)
{
    static const char *names[] = {"ok", "no speech", "too short", "error"};
    char text[256];
    hear_stats_t st;
    app_ui_clear_turn();
    hear_result_t r = wav ? hear_wav(wav, text, sizeof text, &st)
                          : hear_listen(&s_hear, text, sizeof text, ui_status_cb, &st);
    switch (r) {
    case HEAR_OK: app_ui_you(text[0] ? text : "(nothing recognized)"); break;
    case HEAR_NO_SPEECH: app_ui_you("I didn't hear anything."); break;
    case HEAR_TOO_SHORT: app_ui_you("I didn't catch that."); break;
    default: app_ui_you("Speech recognition error."); break;
    }
    app_ui_status("Ready", UI_GREEN);
    printf("HEAR %s: \"%s\"\n", names[r], text);
}

// Test: play a WAV through the speaker while HEAR listens on the mic.
typedef struct { char path[96]; int delay_ms; } play_args_t;
static play_args_t s_play;

static void wav_player_task(void *arg)
{
    play_args_t *a = (play_args_t *)arg;
    vTaskDelay(pdMS_TO_TICKS(a->delay_ms));
    FILE *f = fopen(a->path, "rb");
    if (f) {
        int16_t buf[256];
        fseek(f, 44, SEEK_SET);
        board_spk_start();
        size_t n;
        while ((n = fread(buf, 2, 256, f)) > 0) board_spk_write(buf, n, 1000);
        board_spk_stop();
        fclose(f);
    }
    vTaskDelete(NULL);
}

static void cmd_hearplay(const char *args)
{
    s_play.delay_ms = 1500;
    int vol = 40;
    if (sscanf(args, "%95s %d %d", s_play.path, &vol, &s_play.delay_ms) < 1) return;
    board_audio_set_volume(vol);
    xTaskCreatePinnedToCore(wav_player_task, "wavplay", 4096, &s_play, 5, NULL, 1);
    cmd_hear(NULL);
    board_audio_set_volume(70);
}

static void dispatch(const char *line)
{
    if (!strncmp(line, "put ", 4)) {
        char path[160];
        unsigned size = 0, crc = 0;
        if (sscanf(line + 4, "%159s %u %x", path, &size, &crc) == 3) upload_file(path, size, crc);
        else printf("usage: put <path> <size> <crc32hex>\n");
        printf("OK\n");
    } else if (!strncmp(line, "say ", 4)) {
        speak_stats_t st;
        app_ui_status("Speaking...", UI_CYAN);
        app_ui_story(line + 4);
        printf("SAY %s\n", speak_text(line + 4, NULL, &st) ? "done" : "FAILED");
        app_ui_status("Ready", UI_GREEN);
        printf("OK\n");
    } else if (!strcmp(line, "hear")) {
        cmd_hear(NULL);
        printf("OK\n");
    } else if (!strncmp(line, "hearplay ", 9)) {
        cmd_hearplay(line + 9);
        printf("OK\n");
    } else if (!strncmp(line, "sttwav ", 7)) {
        cmd_hear(line + 7);
        printf("OK\n");
    } else if (!strncmp(line, "vad ", 4)) {
        sscanf(line + 4, "%d %f %d", &s_hear.vad_min_abs, &s_hear.vad_noise_mult, &s_hear.eos_silent_chunks);
        printf("VAD min_abs=%d mult=%.1f eos=%d\n", s_hear.vad_min_abs, s_hear.vad_noise_mult, s_hear.eos_silent_chunks);
        printf("OK\n");
    } else if (!strncmp(line, "ask ", 4)) {
        cmd_ask(line + 4);
        printf("OK\n");
    } else if (!strcmp(line, "go")) {
        pipeline_turn(&s_hear, NULL);
        printf("OK\n");
    } else if (!strncmp(line, "goplay ", 7)) {
        // End-to-end loopback test: the question is played through the speaker.
        s_play.delay_ms = 1500;
        int vol = 30;
        if (sscanf(line + 7, "%95s %d", s_play.path, &vol) >= 1) {
            board_audio_set_volume(vol);
            xTaskCreatePinnedToCore(wav_player_task, "wavplay", 4096, &s_play, 5, NULL, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            pipeline_turn(&s_hear, NULL);
            board_audio_set_volume(70);
        }
        printf("OK\n");
    } else {
        hwtest_command(line);
    }
}

void app_main(void)
{
    // Reserve the phase arenas before any driver fragments internal RAM.
    story_mem_log("boot");
    bool arenas = phase_arenas_init(FAST_ARENA_BYTES, BULK_ARENA_BYTES);
    console_init();

    esp_err_t lcd = board_lcd_init();
    esp_err_t tp = board_touch_init();
    esp_err_t au = board_audio_init();
    esp_err_t sd = board_sd_mount(false);
    story_mem_log("drivers");
    s_hear = hear_default_params();

    app_ui_init();
    board_backlight(80);
    if (!arenas) app_ui_status("MEMORY ERROR", UI_RED);
    else if (sd != ESP_OK) app_ui_status("SD card error", UI_RED);
    else app_ui_status("Ready", UI_GREEN);
    ESP_LOGI(TAG, "READY lcd=%s touch=%s audio=%s sd=%s arenas=%d", esp_err_to_name(lcd),
             esp_err_to_name(tp), esp_err_to_name(au), esp_err_to_name(sd), arenas);

    // BOOT button (GPIO0, active low) starts a turn: the Stage E trigger until
    // the wake word exists.
    gpio_config_t btn = {.pin_bit_mask = 1ULL << BOARD_BOOT_BTN, .mode = GPIO_MODE_INPUT,
                         .pull_up_en = GPIO_PULLUP_ENABLE};
    gpio_config(&btn);
    app_ui_you("Press BOOT and ask a question.");

    char line[200];
    while (1) {
        if (console_readline(line, sizeof line, 50)) dispatch(line);
        if (gpio_get_level(BOARD_BOOT_BTN) == 0) {
            while (gpio_get_level(BOARD_BOOT_BTN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
            pipeline_turn(&s_hear, NULL);
        }
    }
}
