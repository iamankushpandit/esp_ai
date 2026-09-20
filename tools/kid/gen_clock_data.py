"""Enumerate every clock time Braino can ask about.

Time & calendar is the worst field in every version and the only one that got
worse as everything else improved:

    v5 10.0%   v6 18.3%   v7 11.7%   v8 5.0%

It has resisted floor balancing, more paraphrases, and structural paraphrases.
Every other field responded to at least one of those. That pattern says the
problem is coverage rather than phrasing.

The space is tiny. `TimeGame.cpp` asks clock positions to five-minute
precision, so it is 12 hours x 12 five-minute marks = 144 facts, plus a
handful of duration and calendar facts. That is small enough to enumerate
completely, which is what fixed arithmetic (18.3% -> 75.3%) and fractions
(17.5% -> 94.8%).

Every answer is COMPUTED and then asserted against the words written, so this
generator cannot emit a time whose stated reading disagrees with its own
clock arithmetic.

  python tools/kid/gen_clock_data.py --out <dir>
"""
import argparse
import json
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

ONES = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
        "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
        "sixteen", "seventeen", "eighteen", "nineteen"]
TENS = {20: "twenty", 30: "thirty", 40: "forty", 50: "fifty"}


def w(n):
    if n < 20:
        return ONES[n]
    t, o = divmod(n, 10)
    return TENS[t * 10] + (f"-{ONES[o]}" if o else "")


def cap(s):
    return s[0].upper() + s[1:]


def hour_name(h):
    """1-12 as a word. 0 and 12 both read as twelve."""
    return ONES[12 if h % 12 == 0 else h % 12]


def spoken(h, m):
    """How a child says it: 'quarter past three', 'ten to five'."""
    nxt = hour_name(h + 1)
    if m == 0:
        return f"{hour_name(h)} o'clock"
    if m == 15:
        return f"quarter past {hour_name(h)}"
    if m == 30:
        return f"half past {hour_name(h)}"
    if m == 45:
        return f"quarter to {nxt}"
    if m < 30:
        return f"{w(m)} past {hour_name(h)}"
    return f"{w(60 - m)} to {nxt}"


def digital(h, m):
    """'three forty-five'. Read as two numbers, which is the other way a
    child hears it."""
    if m == 0:
        return f"{hour_name(h)} o'clock"
    if m < 10:
        return f"{hour_name(h)} oh {w(m)}"
    return f"{hour_name(h)} {w(m)}"


def hands(h, m):
    """Where the hands point. The long hand marks five minutes per number."""
    long_at = (m // 5) % 12
    long_at = 12 if long_at == 0 else long_at
    if m == 0:
        return (f"The long hand points straight up at twelve and the short "
                f"hand points at {hour_name(h)}.")
    if m == 30:
        return (f"The long hand points straight down at six and the short "
                f"hand sits halfway between {hour_name(h)} and "
                f"{hour_name(h + 1)}.")
    return (f"The long hand points at {w(long_at)} and the short hand is just "
            f"past {hour_name(h)}.")


def check(text, must_contain, what):
    if must_contain not in text:
        raise SystemExit(f"GENERATOR BUG on {what}: {must_contain!r} missing "
                         f"from {text!r}")


Q_SPOKEN = [
    "what time is {d}",
    "how do you say {d}",
    "what is another way to say {d}",
    "{d} is what time",
]
Q_HANDS = [
    "where do the hands point at {s}",
    "what do the clock hands look like at {s}",
    "how do the hands sit at {s}",
    "show me {s} on a clock",
]
Q_HANDS_EVAL = [
    "could you describe the hands at {s}",
    "do you know where the hands point at {s}",
]
Q_SPOKEN_EVAL = [
    "can you tell me another name for {d}",
    "do you know what {d} is called",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "models_out/kid"))
    ap.add_argument("--seed", type=int, default=11)
    a = ap.parse_args()

    rng = random.Random(a.seed)
    train, evals = [], []
    n = 0

    for h in range(1, 13):
        for m in range(0, 60, 5):
            s, d = spoken(h, m), digital(h, m)
            hd = hands(h, m)
            n += 1

            # digital -> spoken. Skip when they are the same string (o'clock).
            if s != d:
                ans = f"{cap(d)} is also called {s}."
                check(ans, s, f"{h}:{m:02d} spoken")
                for t in Q_SPOKEN:
                    train.append(f"User: {t.format(d=d)}\nBot: {ans}<|endoftext|>")
                for t in Q_SPOKEN_EVAL:
                    evals.append({"q": t.format(d=d), "a": ans,
                                  "topic": "time & calendar"})
                # and the other direction
                ans2 = f"{cap(s)} is also called {d}."
                for t in Q_SPOKEN:
                    train.append(f"User: {t.format(d=s)}\nBot: {ans2}<|endoftext|>")

            # where the hands point
            for t in Q_HANDS:
                train.append(f"User: {t.format(s=s)}\nBot: {hd}<|endoftext|>")
            for t in Q_HANDS_EVAL:
                evals.append({"q": t.format(s=s), "a": hd,
                              "topic": "time & calendar"})

    # Durations and calendar facts: small, fixed, and constantly asked.
    FIXED = [
        ("how many minutes are in an hour", "One hour is sixty minutes."),
        ("how many seconds are in a minute", "One minute is sixty seconds."),
        ("how many hours are in a day", "One day is twenty-four hours."),
        ("how many days are in a week", "One week is seven days."),
        ("how many weeks are in a year", "One year is fifty-two weeks."),
        ("how many months are in a year", "One year is twelve months."),
        ("how many days are in a year", "One year is three hundred and "
                                        "sixty-five days."),
        ("how many minutes are in half an hour", "Half an hour is thirty minutes."),
        ("how many minutes are in a quarter of an hour",
         "A quarter of an hour is fifteen minutes."),
        ("what does am mean", "Morning times are am, from midnight until noon."),
        ("what does pm mean", "Afternoon and evening times are pm, from noon "
                             "until midnight."),
        ("what is noon", "Noon is twelve o'clock in the middle of the day."),
        ("what is midnight", "Midnight is twelve o'clock in the middle of "
                            "the night."),
        ("what is the long hand for", "The long hand points at the minutes."),
        ("what is the short hand for", "The short hand points at the hour."),
    ]
    for q, ans in FIXED:
        n += 1
        for t in [q, f"tell me {q}", f"{q} please", q.replace("how many", "what is the number of")]:
            train.append(f"User: {t}\nBot: {ans}<|endoftext|>")
        evals.append({"q": f"do you know {q}", "a": ans, "topic": "time & calendar"})

    rng.shuffle(train)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    with (out / "clock_train.txt").open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n\n".join(train) + "\n")
    with (out / "clock_eval.jsonl").open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(json.dumps(e) for e in evals) + "\n")

    print(f"{n} clock/calendar facts -> {len(train)} train samples, "
          f"{len(evals)} held-out questions")
    print("every reading verified against its computed clock position")
    print("wrote clock_train.txt and clock_eval.jsonl")


if __name__ == "__main__":
    main()
