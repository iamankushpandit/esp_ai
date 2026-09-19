// Tiny rule-based router in front of the LLM. The model can't reliably know
// who it is or when to switch into its story format, so we decide that here.
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INTENT_CHAT = 0,   // normal question/answer
    INTENT_STORY,      // "tell me a story about X" -> story format, topic filled
    INTENT_IDENTITY,   // "what is your name / who are you" -> fixed answer
    INTENT_BYE,        // short "bye / goodbye / that's all" -> end the session
} story_intent_t;

#define INTENT_IDENTITY_ANSWER "I am ESP Bot, a little talking robot. I like to chat and tell stories."

// Classify a (lower- or mixed-case) transcript. For INTENT_STORY, writes the
// story topic (e.g. "a red robot") to `topic`.
story_intent_t story_intent(const char *text, char *topic, size_t topic_cap);

#ifdef __cplusplus
}
#endif
