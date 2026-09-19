// Stage E vertical slice: MIC -> STT -> LLM -> TTS -> SPEAKER, one question
// per turn, each stage in its own memory phase.
#include "pipeline.h"
#include "app_ui.h"
#include "board.h"
#include "hear.h"
#include "phase.h"
#include "speak.h"
#include "think.h"
#include "builtin.h"
#include "story_intent.h"
#include "touch_ui.h"
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

pipeline_result_t pipeline_turn(const hear_params_t *hp, llm_history_t *hist, int turn, int max_turns)
{
    bool followup = turn > 1;
    bool last = turn >= max_turns;
    static char question[256];
    static char answer[LLM_ANSWER_CHARS + 1];
    hear_stats_t hs = {0};
    llm_stats_t ls = {0};
    speak_stats_t ss = {0};
    pipeline_result_t res = PIPE_OK;
    story_mem_log("turn-start");
    int64_t t0 = story_time_us();

    // A session starts on a clean transcript; follow-ups append to it.
    if (!followup) app_ui_clear_turn();
    screen_on();

    // ---- LISTEN + TRANSCRIBE
    hear_result_t hr = hear_listen(hp, question, sizeof question, status_cb, &hs);
    int64_t t_hear = story_time_us();
    if (hr == HEAR_NO_SPEECH) {
        res = PIPE_NO_SPEECH;
        if (followup) goto done;          // quiet follow-up window: end normally
        app_ui_status("No speech", UI_GREY);
        speak_error("I didn't hear anything.");
        goto done;
    }
    if (hr != HEAR_OK || question[0] == 0) {
        app_ui_status(hr == HEAR_ERROR ? "STT error" : "Didn't catch that", UI_RED);
        app_ui_you(question[0] ? question : "...");
        speak_error("I didn't catch that.");
        res = hr == HEAR_ERROR ? PIPE_ERROR : PIPE_NOT_UNDERSTOOD;
        goto done;
    }
    app_ui_you(question);                 // appends a new turn to the transcript

    // ---- BYE (no model needed)
    char topic[8];
    if (story_intent(question, topic, sizeof topic) == INTENT_BYE) {
        app_ui_story("Bye bye.");
        app_ui_status("Speaking...", UI_ACCENT);
        speak_text("Bye bye.", NULL, NULL);
        res = PIPE_BYE;
        goto done;
    }

    // ---- BUILT-IN (name, time, date): plain C, no model, labelled as such
    const char *src = NULL;
    bool builtin = builtin_answer(question, answer, sizeof answer, &src);
    if (builtin) ESP_LOGI(TAG, "built-in answer (%s): %s", src, answer);

    // ---- THINK
    int64_t t_think0 = story_time_us();
    if (!builtin) app_ui_status("Thinking...", UI_YELLOW);
    if (!builtin && (!think_answer(hist, question, answer, sizeof answer, stream_cb, NULL, &ls) || !answer[0])) {
        app_ui_status("LLM error", UI_RED);
        speak_error("I can't answer that right now.");
        res = PIPE_ERROR;
        goto done;
    }
    int64_t t_think = story_time_us() - t_think0;
    if (hist && !builtin) llm_history_push(hist, question, answer);   // context without the sign-off
    if (last) {
        size_t n = strlen(answer);
        snprintf(answer + n, sizeof answer - n, " Bye bye.");
    }
    if (builtin) app_ui_builtin(answer, src);
    else app_ui_story(answer);

    // ---- SPEAK
    app_ui_status("Speaking...", UI_ACCENT);   // detail line keeps the tok/s
    int64_t t_speak0 = story_time_us();
    if (!speak_text(answer, NULL, &ss)) {
        app_ui_status("TTS error (answer shown)", UI_RED);   // answer stays on screen
        res = PIPE_ERROR;
    }
    int64_t t_speak = story_time_us() - t_speak0;

    ESP_LOGI(TAG, "================ TURN %d/%d ================", turn, max_turns);
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
    return res;
}

// A session: first question + up to (max_turns - 1) follow-ups without the
// wake trigger. Ends on "bye", after the last answer, on a silent follow-up
// window, or on an error. Context lives only for the session.
void pipeline_session(const hear_params_t *hp, int max_turns)
{
    static llm_history_t hist;
    llm_history_clear(&hist);
    static const char *why[] = {"answered", "no speech", "not understood", "error", "bye"};
    const char *reason = "last turn";
    int turn = 1;
    story_mem_log("session-start");
    for (; turn <= max_turns; turn++) {
        pipeline_result_t r = pipeline_turn(hp, &hist, turn, max_turns);
        if (r == PIPE_BYE || r == PIPE_ERROR || (r == PIPE_NO_SPEECH)) { reason = why[r]; break; }
        if (turn < max_turns) {
            char st[64];
            snprintf(st, sizeof st, "Listening... follow-up %d/%d", turn, max_turns - 1);
            app_ui_status(st, UI_ACCENT);
        }
    }
    ESP_LOGI(TAG, "SESSION END after %d turn(s): %s", turn > max_turns ? max_turns : turn, reason);
    printf("SESSION END turns=%d reason=%s\n", turn > max_turns ? max_turns : turn, reason);
    llm_history_clear(&hist);           // no conversation state survives the session
    app_ui_status("Session ended", UI_GREY);
    screen_off();                       // screen off; a tap wakes it
    app_ui_clear_turn();                // cleared while dark, so the next session starts clean
    app_ui_status("Ready", UI_GREEN);
    story_mem_log("session-end");
}
