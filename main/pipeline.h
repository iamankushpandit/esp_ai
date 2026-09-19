#pragma once
#include "hear.h"
#include "story_llm.h"

typedef enum {
    PIPE_OK = 0,
    PIPE_NO_SPEECH,
    PIPE_NOT_UNDERSTOOD,
    PIPE_ERROR,
} pipeline_result_t;

// One question/answer turn. `hist` may be NULL (no conversation context).
pipeline_result_t pipeline_turn(const hear_params_t *hp, llm_history_t *hist);
