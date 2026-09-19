// Story assistant — entry point.
#include "board.h"
#include "ui.h"
#include "app_ui.h"
#include "phase.h"
#include "think.h"
#include "speak.h"
#include "hear.h"
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

static void ui_stream(void *u, const char *text) { app_ui_story(text); }

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

    char line[200];
    while (1) {
        if (console_readline(line, sizeof line, 0)) dispatch(line);
    }
}
