#pragma once
#include "hear.h"
#include "story_llm.h"

typedef enum {
    PIPE_OK = 0,
    PIPE_NO_SPEECH,
    PIPE_NOT_UNDERSTOOD,
    PIPE_ERROR,
    PIPE_BYE,
    PIPE_CANCELLED,     // Stop button
} pipeline_result_t;

#define SESSION_MAX_TURNS 4   // first question + three follow-ups

// One question/answer turn (turn is 1-based). `hist` may be NULL. The last
// turn's answer gets " Bye bye." appended.
pipeline_result_t pipeline_turn(const hear_params_t *hp, llm_history_t *hist, int turn, int max_turns);

// A question typed on the T9 keypad: answered (built-in or AI), shown and
// spoken, like a voice turn but without listening.
pipeline_result_t pipeline_typed(const char *question);

// Stop button: cancels whatever the running turn is doing (listening,
// generating, speaking) and ends the session. Safe from any task.
void pipeline_cancel(void);

// Full conversation session; returns when it ends (screen off afterwards).
void pipeline_session(const hear_params_t *hp, int max_turns);

// Demo (/settings): a scripted conversation with no microphone. Each question
// is spoken in a second voice, answered by the model or by the built-ins, and
// the whole thing stays on screen as one chat. For filming. Stop ends it.
void pipeline_demo(bool full);
