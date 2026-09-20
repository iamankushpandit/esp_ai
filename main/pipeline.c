// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

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
// history between turns so it reads as one chat rather than a list of lookups.
//
// The questions are drawn at random from a pool, one per subject, so no two
// runs ask the same thing in the same order. That is the point: a fixed script
// looks like a recording, and the whole claim here is that a model on the chip
// is answering. Every question in the pool was checked against v8 on the host
// first, so randomness never costs correctness.
//
// A turn costs about 5.5 s + 0.43 s per generated token (question spoken,
// model loaded, answer generated, answer spoken), so the short demo fills a
// time budget instead of a fixed count, and lands near a minute whatever it
// draws.
typedef enum {
    DQ_FACT, DQ_MATH, DQ_FRAC, DQ_NATURE, DQ_TIMEY, DQ_LANG, DQ_CHESS, DQ_FUN, DQ_SUBJECTS
} demo_cat_t;

typedef struct {
    const char *q;
    uint8_t tok;        // generated tokens, measured on the host
    uint8_t cat;
} demo_q_t;

static const demo_q_t DEMO_POOL[] = {
    {"what is the capital of france", 7, DQ_FACT},
    {"what is the capital of japan", 7, DQ_FACT},
    {"what is the capital of italy", 7, DQ_FACT},
    {"what is the capital of egypt", 7, DQ_FACT},
    {"what is the capital of texas", 7, DQ_FACT},
    {"what continent is kenya in", 5, DQ_FACT},
    {"what continent is brazil in", 6, DQ_FACT},
    {"what is two plus three", 5, DQ_MATH},
    {"what is nine plus six", 6, DQ_MATH},
    {"what is seven plus eight", 7, DQ_MATH},
    {"what is three times four", 31, DQ_MATH},
    {"what is half of twelve", 24, DQ_FRAC},
    {"what is half of eight", 24, DQ_FRAC},
    {"what is a quarter of eight", 28, DQ_FRAC},
    {"what is fifty percent of twenty", 29, DQ_FRAC},
    {"how many legs does a dog have", 5, DQ_NATURE},
    {"how many legs does a spider have", 23, DQ_NATURE},
    {"what is the fastest land animal", 27, DQ_NATURE},
    {"what is the biggest planet", 27, DQ_NATURE},
    {"why is the sky blue", 29, DQ_NATURE},
    {"how many days are in a week", 12, DQ_TIMEY},
    {"how many months are in a year", 18, DQ_TIMEY},
    {"what is a noun", 27, DQ_LANG},
    {"what is a verb", 24, DQ_LANG},
    {"how does a bishop move", 26, DQ_CHESS},
    {"how does a knight move in chess", 24, DQ_CHESS},
    {"tell me a joke", 11, DQ_FUN},
};
#define DEMO_POOL_N (int)(sizeof DEMO_POOL / sizeof DEMO_POOL[0])

// Answered by plain C, not the model - the screen marks them "no AI", which is
// worth showing next to the model's answers.
static const char *DEMO_BUILTIN[] = {"what are you", "what time is it", "what day is it"};
#define DEMO_BUILTIN_N (int)(sizeof DEMO_BUILTIN / sizeof DEMO_BUILTIN[0])

// What it gets wrong. Every line below is what v8 actually answers, greedily,
// so the demo is reproducible rather than a fishing trip. The first pair is
// the whole point: the same fact, asked two ways.
static const char *DEMO_PAIR[] = {
    "what is the capital of japan",       // -> Tokyo. Correct.
    "japans capital city is what",        // -> "The capital of Michigan is Lansing."
};
#define DEMO_PAIR_N (int)(sizeof DEMO_PAIR / sizeof DEMO_PAIR[0])

static const demo_q_t DEMO_FAILS[] = {
    // Confidently wrong, with invented working - the failure that matters most.
    {"what is forty divided by eight", 14, 0},      // "Eight times ten is forty. So ... is ten."
    // Outside the trained number range.
    {"what is two hundred plus three hundred", 7, 0},   // "Two hundred plus three is five."
    // Starts right, then drifts into nonsense.
    {"what do bees make", 17, 0},                   // "...They use the toilet so they go to the store"
    // Spelling was never learned properly.
    {"how do you spell friend", 9, 0},              // "Friends is spelled out of eight letters."
    // Out of its world entirely - and it does not notice.
    {"tell me about quantum physics", 22, 0},       // answers about percussion instruments
    // These it refuses honestly, which is the other half of the story.
    {"who is the president of france", 20, 1},
    {"what is the square root of sixteen", 20, 1},
    {"what colour is the sky", 20, 1},              // British spelling; "color" works
};
#define DEMO_FAILS_N (int)(sizeof DEMO_FAILS / sizeof DEMO_FAILS[0])

#define DEMO_SECS_SHORT 58           // target length of the short demo
#define DEMO_SECS_LIMITS 60
#define DEMO_TURN_FIXED_S 4.8f       // spoken question + model load + prefill
#define DEMO_TURN_PER_TOK_S 0.43f
#define DEMO_BUILTIN_S 6.5f
#define DEMO_MAX_TURNS 20

#ifdef STORY_HOST
#include <stdlib.h>
#define DEMO_RAND() ((uint32_t)rand())
#else
#include "esp_random.h"
#define DEMO_RAND() esp_random()
#endif

// Builds one run's script: a built-in, then one random question from each
// subject in random order, taking them while the time budget lasts. Returns
// how many were chosen.
static int demo_pick(const char **out, int cap, demo_mode_t mode)
{
    int n = 0;
    float budget = mode == DEMO_MODE_FULL ? 1e9f : (float)DEMO_SECS_SHORT;

    if (mode == DEMO_MODE_LIMITS) {
        // The pair first (it knows Tokyo; reword the question and it does
        // not), then a random sample of the other failures, so the reel is
        // different each run like the working one.
        budget = DEMO_SECS_LIMITS;
        for (int i = 0; i < DEMO_PAIR_N && n < cap; i++) {
            out[n++] = DEMO_PAIR[i];
            budget -= DEMO_TURN_FIXED_S + DEMO_TURN_PER_TOK_S * 7;
        }
        uint8_t idx[DEMO_FAILS_N];
        for (int i = 0; i < DEMO_FAILS_N; i++) idx[i] = (uint8_t)i;
        for (int i = DEMO_FAILS_N - 1; i > 0; i--) {
            int j = (int)(DEMO_RAND() % (uint32_t)(i + 1));
            uint8_t t = idx[i];
            idx[i] = idx[j];
            idx[j] = t;
        }
        bool refusal_shown = false;
        for (int k = 0; k < DEMO_FAILS_N && n < cap; k++) {
            const demo_q_t *f = &DEMO_FAILS[idx[k]];
            if (f->cat == 1 && refusal_shown) continue;      // one honest refusal is enough
            float cost = DEMO_TURN_FIXED_S + DEMO_TURN_PER_TOK_S * f->tok;
            if (cost > budget) continue;
            out[n++] = f->q;
            budget -= cost;
            if (f->cat == 1) refusal_shown = true;
        }
        return n;
    }

    out[n++] = DEMO_BUILTIN[DEMO_RAND() % DEMO_BUILTIN_N];
    budget -= DEMO_BUILTIN_S;

    // Subjects in random order (Fisher-Yates), one question from each.
    uint8_t order[DQ_SUBJECTS];
    for (int i = 0; i < DQ_SUBJECTS; i++) order[i] = (uint8_t)i;
    for (int i = DQ_SUBJECTS - 1; i > 0; i--) {
        int j = (int)(DEMO_RAND() % (uint32_t)(i + 1));
        uint8_t t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (int k = 0; k < DQ_SUBJECTS && n < cap - 2; k++) {
        // Reservoir-pick one question of this subject, so the pool can grow
        // without counting entries per category by hand.
        const demo_q_t *pick = NULL;
        int seen = 0;
        for (int i = 0; i < DEMO_POOL_N; i++) {
            if (DEMO_POOL[i].cat != order[k]) continue;
            seen++;
            if (DEMO_RAND() % (uint32_t)seen == 0) pick = &DEMO_POOL[i];
        }
        if (!pick) continue;
        float cost = DEMO_TURN_FIXED_S + DEMO_TURN_PER_TOK_S * pick->tok;
        if (cost > budget) continue;            // try the next subject, it may be cheaper
        out[n++] = pick->q;
        budget -= cost;
    }
    if (mode == DEMO_MODE_FULL) {               // the long tour ends with the extras
        if (n < cap) out[n++] = "set a timer for two minutes";
        if (n < cap) out[n++] = "tell me a story about a cat";
    }
    return n;
}

void pipeline_demo(demo_mode_t mode)
{
    const char *script[DEMO_MAX_TURNS];
    const int demo_n = demo_pick(script, DEMO_MAX_TURNS, mode);
    static llm_history_t hist;
    llm_history_clear(&hist);
    cancel_reset();
    timer_cancel();          // so "set a timer" reads "Timer set", not "I changed your timer"
    app_ui_clear_turn();
    screen_on();
    story_mem_log("demo-start");
    int64_t t0 = story_time_us();
    // Every demo turn is asked with no conversation history. The transcript on
    // screen still reads as a chat, but the model sees one question at a time,
    // which is how it was trained, evaluated, and verified for these scripts.
    // Feeding it the previous turns measurably corrupts answers: with history
    // in the prompt "what is nine plus six" came back "Sixty-nine plus six is
    // eighty-five", and the limits reel stopped reproducing its own failures.
    // It is also much faster - prefill was reaching 10.5 s by the later turns.
    llm_history_t *h = NULL;
    (void)hist;
    int n = 0;
    for (; n < demo_n; n++) {
        // max_turns is demo_n + 1 so no turn is treated as the last one and
        // gets "Bye bye." appended mid-demo.
        pipeline_result_t r = turn_impl(NULL, h, n + 1, demo_n + 1, script[n], true);
        if (r == PIPE_CANCELLED || r == PIPE_ERROR) break;
        if (n + 1 < demo_n) vTaskDelay(pdMS_TO_TICKS(300));   // a beat between turns
    }
    llm_history_clear(&hist);
    static const char *MODE[] = {"short", "full", "limits"};
    ESP_LOGI(TAG, "DEMO END (%s) after %d/%d turn(s) in %.1f s", MODE[mode], n, demo_n,
             (story_time_us() - t0) / 1e6);
    printf("DEMO END mode=%s turns=%d secs=%.1f\n", MODE[mode], n, (story_time_us() - t0) / 1e6);
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
