// Host test: rule-based intent router.
#include "story_intent.h"
#include "check.h"
#include <string.h>

int main(void)
{
    char t[64];
    CHECK(story_intent("what is your name", t, sizeof t) == INTENT_IDENTITY);
    CHECK(story_intent("Who are you?", t, sizeof t) == INTENT_IDENTITY);
    CHECK(story_intent("tell me a short story about a red robot.", t, sizeof t) == INTENT_STORY);
    CHECK(strcmp(t, "a red robot") == 0);
    CHECK(story_intent("can you tell me a bedtime story", t, sizeof t) == INTENT_STORY);
    CHECK(strcmp(t, "a happy day") == 0);
    CHECK(story_intent("why is the sky blue", t, sizeof t) == INTENT_CHAT);
    CHECK(story_intent("", t, sizeof t) == INTENT_CHAT);
    CHECK(story_intent("bye", t, sizeof t) == INTENT_BYE);
    CHECK(story_intent("okay goodbye story", t, sizeof t) == INTENT_BYE);
    CHECK(story_intent("that's all thank you", t, sizeof t) == INTENT_BYE);
    CHECK(story_intent("stop", t, sizeof t) == INTENT_BYE);
    CHECK(story_intent("tell me a story about a bus stop", t, sizeof t) == INTENT_STORY);
    CHECK(story_intent("what is a butterfly", t, sizeof t) == INTENT_CHAT);      // "bye" inside a word
    CHECK(story_intent("why do people say goodbye when they leave the house", t, sizeof t) == INTENT_CHAT);
    printf("test_intent OK\n");
    return 0;
}
