#include "story_intent.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void lower_copy(char *dst, const char *src, size_t cap)
{
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = 0;
}

static int has(const char *s, const char *w) { return strstr(s, w) != NULL; }

story_intent_t story_intent(const char *text, char *topic, size_t topic_cap)
{
    char s[256];
    lower_copy(s, text ? text : "", sizeof s);
    if (topic && topic_cap) topic[0] = 0;

    // Goodbye: only for short utterances, so "a story about a bus stop" or
    // "why do birds say goodbye" style questions still get answered.
    int words = 0;
    for (const char *c = s; *c; c++)
        if (isalpha((unsigned char)*c) && (c == s || !isalpha((unsigned char)c[-1]))) words++;
    if (words > 0 && words <= 5) {
        static const char *bye[] = {"bye", "goodbye", "good bye", "see you", "that's all", "thats all",
                                    "that is all", "i'm done", "im done", "stop", "go to sleep", "never mind"};
        for (size_t i = 0; i < sizeof bye / sizeof bye[0]; i++) {
            const char *m = strstr(s, bye[i]);
            size_t n = strlen(bye[i]);
            // whole-word match
            if (m && (m == s || !isalpha((unsigned char)m[-1])) && !isalpha((unsigned char)m[n]))
                return INTENT_BYE;
        }
    }

    if (has(s, "your name") || has(s, "who are you") || has(s, "what are you") || has(s, "call you"))
        return INTENT_IDENTITY;

    if (has(s, "story") || has(s, "fairy tale") || has(s, "bedtime")) {
        const char *about = strstr(s, "about ");
        const char *t = about ? about + 6 : NULL;
        if (!t || !*t) t = "a happy day";   // no topic given
        if (topic && topic_cap) {
            snprintf(topic, topic_cap, "%s", t);
            size_t n = strlen(topic);
            while (n > 0 && (ispunct((unsigned char)topic[n - 1]) || topic[n - 1] == ' ')) topic[--n] = 0;
        }
        return INTENT_STORY;
    }
    return INTENT_CHAT;
}
