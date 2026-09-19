// Host test: rule-based intent router.
#include "story_intent.h"
#include "check.h"
#include <string.h>

int main(void)
{
    char t[64];
    CHECK(story_intent("what is your name", t, sizeof t) == INTENT_IDENTITY);
    CHECK(story_intent("Who are you?", t, sizeof t) == INTENT_IDENTITY);
    CHECK(story_intent("what should i call you", t, sizeof t) == INTENT_IDENTITY);
    CHECK(story_intent("whats your name", t, sizeof t) == INTENT_IDENTITY);
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
    CHECK(story_intent("what time is it", t, sizeof t) == INTENT_TIME);
    CHECK(story_intent("Hey, what's the time?", t, sizeof t) == INTENT_TIME);
    CHECK(story_intent("what day is it today", t, sizeof t) == INTENT_DATE);
    CHECK(story_intent("what is the date", t, sizeof t) == INTENT_DATE);
    CHECK(story_intent("what month is it", t, sizeof t) == INTENT_DATE);
    CHECK(story_intent("what comes after tuesday", t, sizeof t) == INTENT_CHAT);   // a fact, not the clock
    CHECK(story_intent("how many days are in a week", t, sizeof t) == INTENT_CHAT);
    CHECK(story_intent("what time do owls wake up", t, sizeof t) == INTENT_CHAT);
    CHECK(story_intent("tell me a story about time travel", t, sizeof t) == INTENT_STORY);
    printf("test_intent OK\n");
    return 0;
}
