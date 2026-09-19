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

    if (has(s, "your name") || has(s, "who are you") || has(s, "what are you"))
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
