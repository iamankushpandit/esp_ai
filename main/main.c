// Story assistant — entry point.
#include "board.h"
#include "ui.h"
#include "app_ui.h"
#include "battery.h"
#include "builtin.h"
#include "net.h"
#include "prefs.h"
#include "timer.h"
#include <math.h>
#include "phase.h"
#include "think.h"
#include "speak.h"
#include "hear.h"
#include "pipeline.h"
#include "touch_ui.h"
#include "models.h"
#include "wake.h"
#include "driver/gpio.h"
#include "console.h"
#include "hwtest.h"
#include "upload.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "main";

#define FAST_ARENA_BYTES (264 * 1024)
#define BULK_ARENA_BYTES (7 * 1024 * 1024)

static void ui_stream(void *u, const char *text, int in, int out, float rate) { app_ui_llm_progress(text, in, out, rate); }

static void cmd_ask(const char *q)
{
    static llm_history_t hist;   // test: independent questions
    llm_history_clear(&hist);
    char ans[LLM_ANSWER_CHARS + 1];
    llm_stats_t st;
    app_ui_clear_turn();
    app_ui_you(q);
    const char *src;
    if (builtin_answer(q, ans, sizeof ans, &src)) {
        app_ui_builtin(ans, src);
        printf("ANSWER (%s): %s\n", src, ans);
        app_ui_status("Ready", UI_OK);
        return;
    }
    app_ui_status("Thinking...", UI_BUSY);
    if (think_answer(&hist, q, ans, sizeof ans, ui_stream, NULL, &st)) {
        printf("ANSWER: %s\n", ans);
        app_ui_status("Ready", UI_OK);
    } else {
        app_ui_story("I can't answer that right now.");
        app_ui_status("LLM error", UI_ERR);
        printf("ANSWER ERROR\n");
    }
}

static void ui_status_cb(const char *s) { app_ui_status(s, UI_BUSY); }

// Timer finished: say so, then beep in bursts until the screen is touched
// (or a minute passes). WakeNet must be stopped by the caller (speaker).
static void timer_ring(void)
{
    screen_on();
    app_ui_status("Timer done! Tap to stop", UI_ERR);
    speak_text("Your timer is done.", NULL, NULL);
    const int64_t t0 = story_time_us(), touched0 = screen_last_activity_us();
    enum { BEEP_N = 16000 * 15 / 100 };                // 150 ms of 880 Hz
    static int16_t *beep;                             // PSRAM: internal RAM is for the arenas
    if (!beep && (beep = heap_caps_malloc(BEEP_N * sizeof(int16_t), MALLOC_CAP_SPIRAM))) {
        for (int i = 0; i < BEEP_N; i++) {
            float env = i < 160 ? i / 160.0f : i > BEEP_N - 160 ? (BEEP_N - i) / 160.0f : 1.0f;   // no clicks
            beep[i] = (int16_t)(12000 * env * sinf(6.2831853f * 880.0f * i / 16000.0f));
        }
    }
    if (!beep) return;
    board_spk_start();
    while (story_time_us() - t0 < 60LL * 1000000 && screen_last_activity_us() == touched0) {
        for (int k = 0; k < 3; k++) {
            board_spk_write(beep, BEEP_N, 1000);
            vTaskDelay(pdMS_TO_TICKS(90));
        }
        for (int i = 0; i < 12 && screen_last_activity_us() == touched0; i++) vTaskDelay(pdMS_TO_TICKS(50));
    }
    board_spk_stop();
    app_ui_status("Ready", UI_OK);
}

// Set the clock over Wi-Fi/NTP, showing progress on the status line.
static void clock_sync_ui(void)
{
    char msg[48];
    app_ui_status("Setting clock (Wi-Fi)...", UI_INFO);
    // Wi-Fi borrows the idle internal arena for the few seconds it's on.
    bool lent = phase_fast_lend("NET");
    bool ok = net_sync_time(msg, sizeof msg);
    if (lent && !phase_fast_reclaim()) app_ui_status("MEMORY ERROR", UI_ERR);
    printf("TIMESYNC %s: %s\n", ok ? "ok" : "failed", msg);
    app_ui_status(ok ? "Ready" : msg, ok ? UI_OK : UI_ERR);
    app_ui_refresh_page();
}

// Parse up to two double-quoted strings: "a" "b". Returns how many were found.
static int parse_quoted(const char *s, char *a, size_t acap, char *b, size_t bcap)
{
    char *outs[2] = {a, b};
    size_t caps[2] = {acap, bcap};
    int n = 0;
    while (n < 2) {
        const char *q = strchr(s, '"');
        if (!q) break;
        const char *e = strchr(q + 1, '"');
        if (!e) break;
        size_t len = (size_t)(e - q - 1);
        if (len >= caps[n]) len = caps[n] - 1;
        memcpy(outs[n], q + 1, len);
        outs[n][len] = 0;
        n++;
        s = e + 1;
    }
    return n;
}

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
    app_ui_status("Ready", UI_OK);
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
    board_audio_set_volume(prefs()->volume);
}

// Test: a whole session with pre-recorded questions. Each WAV is played
// through the speaker shortly after the device starts listening.
typedef struct { char paths[4][96]; int n; int vol; } sess_play_t;
static sess_play_t s_sess;

static void session_player_task(void *arg)
{
    sess_play_t *s = (sess_play_t *)arg;
    int seen = g_hear_listen_count;
    for (int i = 0; i < s->n; i++) {
        while (g_hear_listen_count == seen) vTaskDelay(pdMS_TO_TICKS(20));
        seen = g_hear_listen_count;
        vTaskDelay(pdMS_TO_TICKS(1200));
        FILE *f = fopen(s->paths[i], "rb");
        if (!f) continue;
        int16_t buf[256];
        size_t n;
        fseek(f, 44, SEEK_SET);
        board_audio_set_volume(s->vol);
        board_spk_start();
        while ((n = fread(buf, 2, 256, f)) > 0) board_spk_write(buf, n, 1000);
        board_spk_stop();
        fclose(f);
    }
    vTaskDelete(NULL);
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
        app_ui_status("Speaking...", UI_INFO);
        app_ui_story(line + 4);
        printf("SAY %s\n", speak_text(line + 4, NULL, &st) ? "done" : "FAILED");
        app_ui_status("Ready", UI_OK);
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
    } else if (!strcmp(line, "models") || !strncmp(line, "model ", 6)) {
        // "models" lists; "model <n>" selects (same as tapping it on /model).
        models_scan();
        if (line[5] == ' ') {
            models_select(atoi(line + 6));
            app_ui_model_changed();
        }
        for (int i = 0; i < models_count(); i++)
            printf("MODEL %d %c %s (%s)\n", i, i == models_active() ? '*' : ' ', models_get(i)->name,
                   models_get(i)->dir);
        app_ui_refresh_page();
        printf("OK\n");
    } else if (!strncmp(line, "llmdir ", 7)) {
        think_set_model_dir(line + 7);
        // Keep the header row honest when the path is a known model.
        for (int i = 0; i < models_count(); i++)
            if (!strcmp(models_get(i)->dir, think_model_dir())) models_select(i);
        app_ui_model_changed();
        printf("LLM dir %s\n", think_model_dir());
        printf("OK\n");
    } else if (!strncmp(line, "sessionplay ", 12)) {
        // sessionplay <vol> <wav1> [wav2 .. wav4]: fewer WAVs than turns
        // exercises the silent follow-up timeout.
        memset(&s_sess, 0, sizeof s_sess);
        s_sess.n = sscanf(line + 12, "%d %95s %95s %95s %95s", &s_sess.vol, s_sess.paths[0],
                          s_sess.paths[1], s_sess.paths[2], s_sess.paths[3]) - 1;
        if (s_sess.n > 0) {
            xTaskCreatePinnedToCore(session_player_task, "sessplay", 4096, &s_sess, 5, NULL, 1);
            pipeline_session(&s_hear, SESSION_MAX_TURNS);
            board_audio_set_volume(prefs()->volume);
        }
        printf("OK\n");
    } else if (!strncmp(line, "wifi ", 5)) {
        // wifi "<network>" "<password>"   |   wifi clear
        char ssid[NET_SSID_MAX] = "", pass[NET_PASS_MAX] = "";
        if (!strcmp(line + 5, "clear")) {
            net_clear_credentials();
            printf("WIFI cleared\n");
        } else if (parse_quoted(line + 5, ssid, sizeof ssid, pass, sizeof pass) >= 1 &&
                   net_set_credentials(ssid, pass)) {
            printf("WIFI saved \"%s\"\n", ssid);
            clock_sync_ui();
        } else {
            printf("usage: wifi \"<network>\" \"<password>\" | wifi clear\n");
        }
        app_ui_refresh_page();
        printf("OK\n");
    } else if (!strcmp(line, "wifiscan")) {
        net_ap_t aps[12];
        bool lent = phase_fast_lend("NET");
        int n = net_scan(aps, 12);
        if (lent) phase_fast_reclaim();
        for (int i = 0; i < n; i++) printf("AP %d %s %d dBm%s\n", i, aps[i].ssid, aps[i].rssi, aps[i].open ? " open" : "");
        printf("OK\n");
    } else if (!strcmp(line, "timesync")) {
        clock_sync_ui();
        printf("OK\n");
    } else if (!strncmp(line, "tz ", 3)) {
        net_set_tz(line + 3);
        printf("TZ %s\n", net_tz());
        printf("OK\n");
    } else if (!strcmp(line, "time")) {
        char t[80], d[80];
        clock_say_time(t, sizeof t);
        clock_say_date(d, sizeof d);
        printf("TIME %s %s (tz %s)\n", t, d, net_tz()[0] ? net_tz() : "UTC");
        printf("OK\n");
    } else if (!strncmp(line, "ui ", 3)) {
        // ui chat|model|settings|about|wifi : open a page (tests)
        static const char *names[] = {"chat", "model", "settings", "about", "wifi"};
        for (int i = 0; i < 5; i++)
            if (!strcmp(line + 3, names[i])) app_ui_page((app_page_t)i);
        printf("OK\n");
    } else if (!strcmp(line, "screen off") || !strcmp(line, "screen on")) {
        if (line[8] == 'f') screen_off();
        else screen_on();
        printf("OK\n");
    } else if (!strncmp(line, "tap ", 4)) {
        int x = 0, y = 0;
        if (sscanf(line + 4, "%d %d", &x, &y) == 2) touch_ui_sim_tap(x, y);
        printf("PREFS vol %d bright %d screen %u wake %d\n", prefs()->volume, prefs()->brightness,
               prefs()->screen_s, prefs()->wake);
        printf("OK\n");
    } else if (!strncmp(line, "wakemon ", 8)) {
        wake_set_monitor(line[8] == 'o' && line[9] == 'n');
        printf("OK\n");
    } else if (!strcmp(line, "bat")) {
        int mv = board_battery_mv();
        printf("BAT %d mV -> %d %%%s\n", mv, battery_pct_from_mv(mv), battery_charging() ? ", charging" : "");
        printf("OK\n");
    } else if (!strcmp(line, "restart")) {
        printf("OK\n");
        app_ui_status("Restarting...", UI_ERR);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
    } else if (!strncmp(line, "gostop ", 7) || !strncmp(line, "askstop ", 8)) {
        // Test the Stop button: "gostop <ms>" (voice session) or
        // "askstop <ms> <question>" (typed), cancelled after <ms>.
        static int stop_ms;
        char q[128] = "";
        bool typed = line[0] == 'a';
        sscanf(line + (typed ? 8 : 7), "%d %127[^\n]", &stop_ms, q);
        TimerHandle_t t = xTimerCreate("stop", pdMS_TO_TICKS(stop_ms > 0 ? stop_ms : 1), pdFALSE, NULL,
                                       (TimerCallbackFunction_t)pipeline_cancel);
        xTimerStart(t, 0);
        app_ui_busy(true);
        if (typed) pipeline_typed(q);
        else pipeline_session(&s_hear, SESSION_MAX_TURNS);
        app_ui_busy(false);
        xTimerDelete(t, 0);
        printf("OK\n");
    } else if (!strcmp(line, "go")) {
        pipeline_session(&s_hear, SESSION_MAX_TURNS);
        printf("OK\n");
    } else if (!strncmp(line, "goplay ", 7)) {
        // End-to-end loopback test: the question is played through the speaker.
        s_play.delay_ms = 1500;
        int vol = 30;
        if (sscanf(line + 7, "%95s %d", s_play.path, &vol) >= 1) {
            board_audio_set_volume(vol);
            xTaskCreatePinnedToCore(wav_player_task, "wavplay", 4096, &s_play, 5, NULL, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            pipeline_turn(&s_hear, NULL, 1, 2);   // single turn, no sign-off
            board_audio_set_volume(prefs()->volume);
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
    net_warmup();   // Wi-Fi's permanent first-start allocations go below the arena
    prefs_load();   // volume, brightness, screen timeout, wake word (NVS is up now)
    bool arenas = phase_arenas_init(FAST_ARENA_BYTES, BULK_ARENA_BYTES);
    console_init();

    esp_err_t lcd = board_lcd_init();
    // Welcome screen while the rest of the hardware and models come up.
    int64_t t_splash = story_time_us();
    app_ui_splash();
    screen_on();
    esp_err_t tp = board_touch_init();
    esp_err_t au = board_audio_init();
    board_audio_set_volume(prefs()->volume);
    esp_err_t sd = board_sd_mount(false);
    if (sd == ESP_OK) models_scan();    // selectable LLMs on the SD card + saved choice
    net_init();                         // Wi-Fi credentials + time zone (clock via NTP)
    story_mem_log("drivers");
    s_hear = hear_default_params();
    const int64_t SPLASH_MIN_US = 2000000;
    int64_t shown = story_time_us() - t_splash;
    if (shown < SPLASH_MIN_US) vTaskDelay(pdMS_TO_TICKS((SPLASH_MIN_US - shown) / 1000));

    app_ui_init();
    if (!arenas) app_ui_status("MEMORY ERROR", UI_ERR);
    else if (sd != ESP_OK) app_ui_status("SD card error", UI_ERR);
    else app_ui_status("Ready", UI_OK);
    ESP_LOGI(TAG, "READY lcd=%s touch=%s audio=%s sd=%s arenas=%d", esp_err_to_name(lcd),
             esp_err_to_name(tp), esp_err_to_name(au), esp_err_to_name(sd), arenas);

    // Triggers: the on-screen spark button (tap = press + release inside it).
    // The physical BOOT button still works as a hidden backup.
    gpio_config_t btn = {.pin_bit_mask = 1ULL << BOARD_BOOT_BTN, .mode = GPIO_MODE_INPUT,
                         .pull_up_en = GPIO_PULLUP_ENABLE};
    gpio_config(&btn);

    touch_ui_start();   // wake / Ask pill / scroll, also during sessions

    // Wake word (WakeNet): listens whenever no session runs. It owns the mic
    // while idle, so it is stopped around sessions and serial test commands.
    bool wake_ok = wake_init() && (!prefs()->wake || wake_start(touch_ui_post_ask));
    if (!wake_ok) app_ui_status("Wake word unavailable", UI_ERR);

    char line[200];
    while (1) {
        bool start = touch_ui_wait_ask(10);
        if (console_readline(line, sizeof line, 20)) {
            if (!strncmp(line, "wakeplay ", 9)) {
                // Test: play a WAV through the speaker while WakeNet keeps
                // listening (a detection starts a session like "Hey Ivy" would).
                s_play.delay_ms = 300;
                int vol = 30;
                if (sscanf(line + 9, "%95s %d", s_play.path, &vol) >= 1) {
                    board_audio_set_volume(vol);
                    xTaskCreatePinnedToCore(wav_player_task, "wavplay", 4096, &s_play, 5, NULL, 1);
                }
                printf("OK\n");
            } else {
                wake_stop();
                dispatch(line);
                if (wake_ok && prefs()->wake) wake_start(touch_ui_post_ask);
            }
            screen_note_activity();
        }
        if (gpio_get_level(BOARD_BOOT_BTN) == 0) {
            while (gpio_get_level(BOARD_BOOT_BTN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
            start = true;
        }
        if (start) {
            wake_stop();                                   // frees the mic + memory
            screen_on();
            app_ui_busy(true);
            pipeline_session(&s_hear, SESSION_MAX_TURNS);   // ends with the screen off
            app_ui_busy(false);
            if (wake_ok && prefs()->wake) wake_start(touch_ui_post_ask);
        }
        if (!start && !app_ui_is_busy() && net_take_scan_request()) {
            static net_ap_t aps[12];
            wake_stop();
            bool lent = phase_fast_lend("NET");
            int n = net_scan(aps, 12);
            if (lent && !phase_fast_reclaim()) app_ui_status("MEMORY ERROR", UI_ERR);
            app_ui_wifi_results(aps, n);
            if (wake_ok && prefs()->wake) wake_start(touch_ui_post_ask);
        }
        if (!start && !app_ui_is_busy() && net_sync_due()) {
            // Clock: at boot, then once per hour. The radio is on only for
            // this; WakeNet pauses so Wi-Fi gets its internal RAM.
            wake_stop();
            clock_sync_ui();
            if (wake_ok && prefs()->wake) wake_start(touch_ui_post_ask);
        }
        if (!start && !app_ui_is_busy()) {
            // Timer: ring when done; otherwise show the countdown once a second.
            static int shown = -1;
            if (timer_take_fired()) {
                wake_stop();
                timer_ring();
                shown = -1;
                if (wake_ok && prefs()->wake) wake_start(touch_ui_post_ask);
            } else if (timer_active()) {
                int left = timer_remaining_s();
                if (left != shown) {
                    char st[32];
                    if (left >= 3600) snprintf(st, sizeof st, "Timer %d:%02d:%02d", left / 3600, left % 3600 / 60, left % 60);
                    else snprintf(st, sizeof st, "Timer %d:%02d", left / 60, left % 60);
                    app_ui_status(st, UI_OK);
                    shown = left;
                }
            } else if (shown >= 0) {               // cancelled
                app_ui_status("Ready", UI_OK);
                shown = -1;
            }
        }
        int demo_mode = 0;
        if (!start && !app_ui_is_busy() && touch_ui_take_demo(&demo_mode)) {
            // /settings > Run demo: a scripted conversation, no microphone.
            wake_stop();
            screen_on();
            app_ui_busy(true);
            pipeline_demo((demo_mode_t)demo_mode);
            app_ui_busy(false);
            if (wake_ok && prefs()->wake) wake_start(touch_ui_post_ask);
        }
        static char typed[128];
        if (!start && !app_ui_is_busy() && touch_ui_take_typed(typed, sizeof typed)) {
            // Typed on the T9 keypad: same answer path as a spoken question.
            wake_stop();
            screen_on();
            app_ui_busy(true);                             // closes the keypad
            pipeline_typed(typed);
            app_ui_busy(false);
            if (wake_ok && prefs()->wake) wake_start(touch_ui_post_ask);
        }
        if (!start && !app_ui_is_busy() && touch_ui_take_volume_preview()) {
            // Volume changed in /settings: say something at the new level.
            wake_stop();                                   // speaker + memory
            app_ui_status("Speaking...", UI_BUSY);
            speak_text("Hi there.", NULL, NULL);
            app_ui_status("Ready", UI_OK);
            if (wake_ok && prefs()->wake) wake_start(touch_ui_post_ask);
        }
        if (wake_ok && !app_ui_is_busy() && prefs()->wake != wake_running()) {
            // Settings toggled the wake word.
            if (prefs()->wake) wake_start(touch_ui_post_ask);
            else wake_stop();
        }
        const int64_t screen_us = (int64_t)prefs()->screen_s * 1000000;   // 0 = never
        if (screen_us && screen_is_on() && story_time_us() - screen_last_activity_us() > screen_us) {
            screen_off();
        }
    }
}
