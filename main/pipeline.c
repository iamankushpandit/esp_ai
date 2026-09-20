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
#include "timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "turn";

static void status_cb(const char *s) { app_ui_status(s, UI_BUSY); }
static void stream_cb(void *u, const char *t, int in, int out, float rate) { app_ui_llm_progress(t, in, out, rate); }

static volatile bool s_cancel;

void pipeline_cancel(void)
{
    s_cancel = true;
    g_hear_abort = true;
    think_stop();
}

static void cancel_reset(void)
{
    s_cancel = false;
    g_hear_abort = false;
}

static void speak_error(const char *msg)
{
    app_ui_story(msg);
    speak_text(msg, NULL, NULL);
}

// typed != NULL: the question was typed on the T9 keypad; skip listening.
// ask_aloud: demo mode - speak the question first, in the other voice.
static pipeline_result_t turn_impl(const hear_params_t *hp, llm_history_t *hist, int turn, int max_turns,
                                   const char *typed, bool ask_aloud)
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
    hear_result_t hr = HEAR_OK;
    if (typed) snprintf(question, sizeof question, "%s", typed);
    else hr = hear_listen(hp, question, sizeof question, status_cb, &hs);
    int64_t t_hear = story_time_us();
    if (s_cancel) { res = PIPE_CANCELLED; goto done; }
    if (hr == HEAR_NO_SPEECH) {
        res = PIPE_NO_SPEECH;
        if (followup) goto done;          // quiet follow-up window: end normally
        app_ui_status("No speech", UI_GREY);
        speak_error("I didn't hear anything.");
        goto done;
    }
    if (hr != HEAR_OK || question[0] == 0) {
        app_ui_status(hr == HEAR_ERROR ? "STT error" : "Didn't catch that", UI_ERR);
        app_ui_you(question[0] ? question : "...");
        speak_error("I didn't catch that.");
        res = hr == HEAR_ERROR ? PIPE_ERROR : PIPE_NOT_UNDERSTOOD;
        goto done;
    }
    app_ui_you(question);                 // appends a new turn to the transcript
    if (ask_aloud) {                      // demo: the question, in the asking voice
        app_ui_status("Asking...", UI_BUSY);
        speak_text_voice(question, VOICE_ASKER, &s_cancel, NULL);
        if (s_cancel) { res = PIPE_CANCELLED; goto done; }
    }

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
    if (!builtin) app_ui_status("Thinking...", UI_BUSY);
    if (!builtin && (!think_answer(hist, question, answer, sizeof answer, stream_cb, NULL, &ls) || !answer[0])) {
        const char *why = think_last_error();
        app_ui_status(why[0] ? why : "LLM error", UI_ERR);
        if (why[0]) {                     // e.g. a GGUF model that can't run here
            app_ui_story(why);
            speak_text("Sorry, I can't use that model.", NULL, NULL);
        } else {
            speak_error("I can't answer that right now.");
        }
        res = PIPE_ERROR;
        goto done;
    }
    int64_t t_think = story_time_us() - t_think0;
    if (s_cancel) {                       // stopped mid-answer: keep what was shown, don't speak
        app_ui_status("Stopped", UI_GREY);
        res = PIPE_CANCELLED;
        goto done;
    }
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
    if (!speak_text(answer, &s_cancel, &ss) && !s_cancel) {
        app_ui_status("TTS error (answer shown)", UI_ERR);   // answer stays on screen
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

pipeline_result_t pipeline_turn(const hear_params_t *hp, llm_history_t *hist, int turn, int max_turns)
{
    return turn_impl(hp, hist, turn, max_turns, NULL, false);
}

pipeline_result_t pipeline_typed(const char *question)
{
    // One stand-alone turn: answer on screen and spoken, no follow-ups.
    cancel_reset();
    pipeline_result_t r = turn_impl(NULL, NULL, 1, 2, question, false);
    app_ui_status(r == PIPE_CANCELLED ? "Stopped" : "Ready", r == PIPE_CANCELLED ? UI_GREY : UI_OK);
    return r;
}

// ---------------------------------------------------------------- demo
// A hands-free conversation for filming: the device asks each question in the
// en-GB voice and answers in its own, keeping the transcript on screen and the
// history between turns so it reads as one chat rather than six lookups. The
// script mixes what the model answers (facts, arithmetic, a story) with what
// plain C answers (identity, the clock), because the screen marks the
// difference and that is worth showing. Stop ends it.
// A turn costs roughly 5.5 s + 0.43 s per generated token (question spoken,
// model loaded, generated, answer spoken), so the 60-second script is chosen
// for short answers: identity, geography, arithmetic, fractions, a joke, the
// clock. The full one adds what does not fit in a minute.
static const char *DEMO_SHORT[] = {
    "what are you",                      // built-in: no AI, the screen says so
    "what is the capital of japan",      // 7 tokens
    "what is half of twelve",            // 22 - the fractions work v8 fixed,
                                         //      and it shows its arithmetic
    "tell me a joke",                    // 19
    "what time is it",                   // built-in: the device clock
};
static const char *DEMO_FULL[] = {
    "hello",
    "what are you",
    "what is the capital of japan",
    "what continent is kenya in",
    "what is nine plus six",
    "what is half of twelve",
    "what is fifty percent of twenty",
    "how many legs does a spider have",
    "why is the sky blue",
    "what is a noun",
    "how does a bishop move",
    "tell me a joke",
    "what time is it",
    "set a timer for two minutes",       // built-in: no AI
    "tell me a story about a cat",       // sampled, so every take differs
};
#define DEMO_SHORT_N (int)(sizeof DEMO_SHORT / sizeof DEMO_SHORT[0])
#define DEMO_FULL_N (int)(sizeof DEMO_FULL / sizeof DEMO_FULL[0])

void pipeline_demo(bool full)
{
    const char **script = full ? DEMO_FULL : DEMO_SHORT;
    const int DEMO_N = full ? DEMO_FULL_N : DEMO_SHORT_N;
    static llm_history_t hist;
    llm_history_clear(&hist);
    cancel_reset();
    timer_cancel();          // so "set a timer" reads "Timer set", not "I changed your timer"
    app_ui_clear_turn();
    screen_on();
    story_mem_log("demo-start");
    int64_t t0 = story_time_us();
    int n = 0;
    for (; n < DEMO_N; n++) {
        // max_turns is DEMO_N + 1 so no turn is treated as the last one and
        // gets "Bye bye." appended mid-demo.
        pipeline_result_t r = turn_impl(NULL, &hist, n + 1, DEMO_N + 1, script[n], true);
        if (r == PIPE_CANCELLED || r == PIPE_ERROR) break;
        if (n + 1 < DEMO_N) vTaskDelay(pdMS_TO_TICKS(300));   // a beat between turns
    }
    llm_history_clear(&hist);
    ESP_LOGI(TAG, "DEMO END after %d/%d turn(s) in %.1f s", n, DEMO_N,
             (story_time_us() - t0) / 1e6);
    printf("DEMO END turns=%d secs=%.1f\n", n, (story_time_us() - t0) / 1e6);
    app_ui_status(s_cancel ? "Stopped" : "Demo ended", UI_GREY);
    story_mem_log("demo-end");
}

// A session: first question + up to (max_turns - 1) follow-ups without the
// wake trigger. Ends on "bye", after the last answer, on a silent follow-up
// window, or on an error. Context lives only for the session.
void pipeline_session(const hear_params_t *hp, int max_turns)
{
    static llm_history_t hist;
    llm_history_clear(&hist);
    static const char *why[] = {"answered", "no speech", "not understood", "error", "bye", "stopped"};
    cancel_reset();
    const char *reason = "last turn";
    int turn = 1;
    story_mem_log("session-start");
    for (; turn <= max_turns; turn++) {
        pipeline_result_t r = pipeline_turn(hp, &hist, turn, max_turns);
        if (r == PIPE_BYE || r == PIPE_ERROR || r == PIPE_NO_SPEECH || r == PIPE_CANCELLED) { reason = why[r]; break; }
        if (turn < max_turns) {
            char st[64];
            snprintf(st, sizeof st, "Listening... follow-up %d/%d", turn, max_turns - 1);
            app_ui_status(st, UI_BUSY);
        }
    }
    ESP_LOGI(TAG, "SESSION END after %d turn(s): %s", turn > max_turns ? max_turns : turn, reason);
    printf("SESSION END turns=%d reason=%s\n", turn > max_turns ? max_turns : turn, reason);
    llm_history_clear(&hist);           // no conversation state survives the session
    if (s_cancel) {                     // Stop button: stay on, keep the transcript
        app_ui_status("Stopped", UI_GREY);
        story_mem_log("session-end");
        return;
    }
    app_ui_status("Session ended", UI_GREY);
    screen_off();                       // screen off; a tap wakes it
    app_ui_clear_turn();                // cleared while dark, so the next session starts clean
    app_ui_status("Ready", UI_OK);
    story_mem_log("session-end");
}
