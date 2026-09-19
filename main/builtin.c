#include "builtin.h"
#include "story_intent.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define NOT_SET_ANSWER "I don't know the time yet. Connect me to Wi-Fi on the about page so I can set my clock."

static const char *DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *MONTHS[] = {"January", "February", "March",     "April",   "May",      "June",
                               "July",    "August",   "September", "October", "November", "December"};

bool clock_is_set(void)
{
    return time(NULL) > 1767225600;   // after 2026-01-01: set by NTP, not the 1970 power-on default
}

static bool now_local(struct tm *t)
{
    if (!clock_is_set()) return false;
    time_t now = time(NULL);
    localtime_r(&now, t);
    return true;
}

void clock_say_time(char *out, size_t cap)
{
    struct tm t;
    if (!now_local(&t)) { snprintf(out, cap, NOT_SET_ANSWER); return; }
    int h = t.tm_hour % 12 ? t.tm_hour % 12 : 12;
    if (t.tm_hour == 12 && t.tm_min == 0) snprintf(out, cap, "It is twelve noon.");
    else if (t.tm_hour == 0 && t.tm_min == 0) snprintf(out, cap, "It is midnight.");
    else if (t.tm_min == 0) snprintf(out, cap, "It is %d o'clock %s.", h, t.tm_hour < 12 ? "in the morning"
                                     : t.tm_hour < 17 ? "in the afternoon" : t.tm_hour < 21 ? "in the evening" : "at night");
    else snprintf(out, cap, "It is %d:%02d %s.", h, t.tm_min, t.tm_hour < 12 ? "AM" : "PM");
}

void clock_say_date(char *out, size_t cap)
{
    struct tm t;
    if (!now_local(&t)) { snprintf(out, cap, NOT_SET_ANSWER); return; }
    snprintf(out, cap, "Today is %s, %s %d, %d.", DAYS[t.tm_wday], MONTHS[t.tm_mon], t.tm_mday,
             t.tm_year + 1900);
}

void clock_short(char *out, size_t cap)
{
    struct tm t;
    if (!now_local(&t)) { snprintf(out, cap, "not set"); return; }
    int h = t.tm_hour % 12 ? t.tm_hour % 12 : 12;
    snprintf(out, cap, "%.3s %d:%02d %s", DAYS[t.tm_wday], h, t.tm_min, t.tm_hour < 12 ? "AM" : "PM");
}

bool builtin_answer(const char *question, char *out, size_t cap, const char **source)
{
    char topic[8];
    switch (story_intent(question, topic, sizeof topic)) {
    case INTENT_IDENTITY:
        snprintf(out, cap, "%s", INTENT_IDENTITY_ANSWER);
        *source = "built-in answer";
        return true;
    case INTENT_TIME:
        clock_say_time(out, cap);
        *source = "device clock";
        return true;
    case INTENT_DATE:
        clock_say_date(out, cap);
        *source = "device clock";
        return true;
    default:
        return false;
    }
}
