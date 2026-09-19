// Model-agnostic language-model interface used by the assistant.
//
// The app only sees llm_* functions. A model is described by an llm_model_t
// (engine + prompt template); today that is TinyTalk (GPT-Neo Q4). Swapping in
// TinyTalk 2 8M is a data change (different blob files); a different
// architecture adds another engine behind the same calls.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "story_arena.h"
#include "neo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LLM_MAX_TURNS 4          // history exchanges kept for prompting
#define LLM_TURN_CHARS 160       // per-utterance cap stored in history
#define LLM_ANSWER_CHARS 480     // max characters of one answer

typedef struct {
    float temperature;           // 0 = greedy
    float top_p;
    int soft_max_tokens;         // after this, stop at the next sentence end
    int hard_max_tokens;         // never exceed
    int kv_len;                  // context window (<= model seq_len)
    uint64_t seed;
} llm_params_t;

typedef struct {
    const uint8_t *model;        // 16-byte aligned (PSRAM copy or mmap)
    size_t model_len;
    const uint8_t *tok;
    size_t tok_len;
} llm_blobs_t;

typedef struct {
    int prompt_tokens;
    int gen_tokens;
    int64_t load_us, prefill_us, gen_us;
    const char *stop_reason;     // "eos", "turn", "soft", "hard", "stopped", "ctx"
} llm_stats_t;

// Called with each decoded text piece; return false to stop generation.
typedef bool (*llm_piece_cb)(void *user, const char *piece, int len);

typedef struct {
    char user[LLM_MAX_TURNS][LLM_TURN_CHARS];
    char bot[LLM_MAX_TURNS][LLM_TURN_CHARS];
    int n;
} llm_history_t;

typedef struct {
    neo_t net;
    neo_tok_t tok;
    llm_params_t params;
    int *prompt;                 // arena: [kv_len] token ids
    volatile bool stop;          // set from another task to abort
    bool loaded;
} llm_t;

llm_params_t llm_default_params(void);

// Binds blobs + allocates run state from the arenas (which the caller owns
// for the THINKING phase).
bool llm_load(llm_t *l, const llm_blobs_t *b, const llm_params_t *p,
              story_arena_t *fast, story_arena_t *bulk);
void llm_unload(llm_t *l);

// Generates an answer to `question` given `hist`. Writes a NUL-terminated,
// trimmed answer (<= LLM_ANSWER_CHARS) to `answer`. Returns false on failure.
bool llm_answer(llm_t *l, const llm_history_t *hist, const char *question,
                char *answer, size_t answer_cap, llm_piece_cb cb, void *user,
                llm_stats_t *st);

// Tells a short story using TinyStories-Instruct's native prompt format
// ("Summary: <topic>\nStory:"). No history.
bool llm_story(llm_t *l, const char *topic, char *answer, size_t answer_cap, llm_piece_cb cb,
               void *user, llm_stats_t *st);

void llm_history_clear(llm_history_t *h);
void llm_history_push(llm_history_t *h, const char *user, const char *bot);

#ifdef __cplusplus
}
#endif
