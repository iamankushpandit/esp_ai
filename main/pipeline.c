// Stage E vertical slice: MIC -> STT -> LLM -> TTS -> SPEAKER, one question
// per turn, each stage in its own memory phase.
#include "pipeline.h"
#include "app_ui.h"
#include "board.h"
#include "hear.h"
#include "phase.h"
#include "speak.h"
#include "think.h"
#include "ui.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "turn";

static void status_cb(const char *s) { app_ui_status(s, UI_YELLOW); }
static void stream_cb(void *u, const char *t, int tok, float rate) { app_ui_llm_progress(t, tok, rate); }

static void speak_error(const char *msg)
{
    app_ui_story(msg);
    speak_text(msg, NULL, NULL);
}

pipeline_result_t pipeline_turn(const hear_params_t *hp, llm_history_t *hist)
{
    static char question[256];
    static char answer[LLM_ANSWER_CHARS + 1];
    hear_stats_t hs = {0};
    llm_stats_t ls = {0};
    speak_stats_t ss = {0};
    pipeline_result_t res = PIPE_OK;
    int turn = hist ? hist->n + 1 : 1;
    story_mem_log("turn-start");
    int64_t t0 = story_time_us();

    app_ui_clear_turn();
    board_backlight(80);

    // ---- LISTEN + TRANSCRIBE
    hear_result_t hr = hear_listen(hp, question, sizeof question, status_cb, &hs);
    int64_t t_hear = story_time_us();
    if (hr == HEAR_NO_SPEECH) {
        app_ui_status("No speech", UI_GREY);
        speak_error("I didn't hear anything.");
        res = PIPE_NO_SPEECH;
        goto done;
    }
    if (hr != HEAR_OK || question[0] == 0) {
        app_ui_status(hr == HEAR_ERROR ? "STT error" : "Didn't catch that", UI_RED);
        app_ui_you(question[0] ? question : "...");
        speak_error("I didn't catch that.");
        res = hr == HEAR_ERROR ? PIPE_ERROR : PIPE_NOT_UNDERSTOOD;
        goto done;
    }
    app_ui_you(question);

    // ---- THINK
    app_ui_status("Thinking...", UI_YELLOW);
    int64_t t_think0 = story_time_us();
    if (!think_answer(hist, question, answer, sizeof answer, stream_cb, NULL, &ls) || !answer[0]) {
        app_ui_status("LLM error", UI_RED);
        speak_error("I can't answer that right now.");
        res = PIPE_ERROR;
        goto done;
    }
    int64_t t_think = story_time_us() - t_think0;
    app_ui_story(answer);
    if (hist) llm_history_push(hist, question, answer);

    // ---- SPEAK
    app_ui_status("Speaking...", UI_ACCENT);   // detail line keeps the tok/s
    int64_t t_speak0 = story_time_us();
    if (!speak_text(answer, NULL, &ss)) {
        app_ui_status("TTS error (answer shown)", UI_RED);   // answer stays on screen
        res = PIPE_ERROR;
    }
    int64_t t_speak = story_time_us() - t_speak0;

    ESP_LOGI(TAG, "================ TURN %d ================", turn);
    ESP_LOGI(TAG, "RECORD    %.2f s (speech chunks %d, noise %d, peak %d)", hs.record_us / 1e6,
             hs.speech_chunks, hs.noise_floor, hs.peak_energy);
    ESP_LOGI(TAG, "STT       %.2f s (open %lld ms, pre-encode %lld ms, infer %lld ms, SD %lld ms)",
             (t_hear - t0) / 1e6 - hs.record_us / 1e6, hs.stt.open_us / 1000, hs.stt.preenc_us / 1000,
             hs.stt.infer_us / 1000, hs.stt.sd_us / 1000);
    ESP_LOGI(TAG, "LLM       %.2f s (load %lld ms, prompt %d tok, gen %d tok, %.1f tok/s, stop=%s)",
             t_think / 1e6, ls.load_us / 1000, ls.prompt_tokens, ls.gen_tokens,
             ls.gen_tokens / (ls.gen_us / 1e6 + 1e-9), ls.stop_reason ? ls.stop_reason : "?");
    ESP_LOGI(TAG, "TTS INIT  %lld ms, first audio %lld ms", ss.init_us / 1000, ss.first_audio_us / 1000);
    ESP_LOGI(TAG, "TTS       %.2f s (%ld ms audio)", t_speak / 1e6, ss.audio_ms);
    ESP_LOGI(TAG, "TOTAL     %.2f s   end-of-speech -> first audio %.2f s", (story_time_us() - t0) / 1e6,
             (t_speak0 + ss.init_us + ss.first_audio_us - (t0 + hs.record_us)) / 1e6);
    ESP_LOGI(TAG, "TRANSCRIPT: %s", question);
    ESP_LOGI(TAG, "RESPONSE:   %s", answer);
    printf("TURN %d Q=\"%s\" A=\"%s\"\n", turn, question, answer);

done:
    story_mem_log("turn-end");
    if (res == PIPE_OK) {
        app_ui_status("Done", UI_GREEN);
    } else {
        app_ui_status("Ready", UI_GREY);
    }
    return res;
}
