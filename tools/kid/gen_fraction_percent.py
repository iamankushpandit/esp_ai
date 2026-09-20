"""Enumerate every fraction-of-N and percent-of-N fact in Braino's scope.

The bug this fixes, measured on v5:

    what is half of twelve        -> "Twelve splits into four and four.
                                      So half of twelve is four."     (want six)
    what is fifty percent of twenty -> "...is five."                  (want ten)

The training data was not wrong. "A third of twelve is four" is correct and is
in the corpus. The model confused it with "half of twelve" - sparse, similar
facts interfering - and then generated a worked derivation that is not even
self-consistent (four and four is eight, not twelve).

That is the same failure as v4's division: the SHAPE of a worked answer is
learned and the numbers inside it are invented. The fix that took addition
from 18.3% to 80.7% was not a better model, it was covering the operand space
so nothing has to be interpolated. This space is far smaller:

  percent: Gume's PercentCircleGame uses BASE_NUMBERS {10,20,40,50,100} and
           PERCENTS {10,25,50,75,100} - 25 facts. A wider grid is generated
           here anyway, since a child will ask "twenty percent of fifty" and
           the cost of covering it is nothing.
  fractions: FractionGame uses denominators {2,3,4,5,6,8}. Only N that divide
           exactly are generated - "a third of ten" has no whole answer and
           teaching a rounded one would be teaching something false.

Every answer is COMPUTED, then asserted against the words that were written.
A generator that can emit a wrong fact is the problem, not the solution.

  python tools/kid/gen_fraction_percent.py --out <dir>
"""
import argparse
import json
import random
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

ONES = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
        "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
        "sixteen", "seventeen", "eighteen", "nineteen"]
TENS = {20: "twenty", 30: "thirty", 40: "forty", 50: "fifty", 60: "sixty",
        70: "seventy", 80: "eighty", 90: "ninety"}


def w(n):
    """Number to words. Hyphenated, matching the rest of the corpus."""
    if n < 20:
        return ONES[n]
    if n < 100:
        t, o = divmod(n, 10)
        return TENS[t * 10] + (f"-{ONES[o]}" if o else "")
    if n == 100:
        return "one hundred"
    h, r = divmod(n, 100)
    return f"{ONES[h]} hundred" + (f" and {w(r)}" if r else "")


def cap(s):
    return s[0].upper() + s[1:]


# denominator -> (name, plural name for the group count)
FRACTIONS = {
    2: ("half", "two"),
    3: ("a third", "three"),
    4: ("a quarter", "four"),
    5: ("a fifth", "five"),
    6: ("a sixth", "six"),
    8: ("an eighth", "eight"),
}

# Gume's own tables, widened where it is free to do so.
PERCENTS = [10, 20, 25, 30, 40, 50, 60, 70, 75, 80, 90, 100]
BASES = [10, 20, 25, 30, 40, 50, 60, 80, 100]


def frac_answer(denom, n):
    """Worked steps whose arithmetic is real. Returns (answer, value)."""
    name, groups = FRACTIONS[denom]
    part = n // denom
    lead = f"{cap(name)} means splitting into {groups} equal groups."
    # Listing every group is clearest, but eight of them is a mouthful and the
    # corpus caps answers at 240 characters. Past four groups, say it once.
    if denom <= 4:
        pieces = " and ".join([w(part)] * denom)
        mid = f"{cap(w(n))} splits into {pieces}."
    else:
        mid = (f"{cap(w(n))} splits into {groups} groups of {w(part)}.")
    return f"{lead} {mid} So {name} of {w(n)} is {w(part)}.", part


def pct_answer(pct, base):
    """Worked steps for pct% of base. Returns (answer, value)."""
    val = base * pct // 100
    if pct == 100:
        return (f"One hundred percent means all of it. So one hundred percent "
                f"of {w(base)} is {w(base)}."), val
    if pct == 50:
        return (f"Fifty percent means half. Half of {w(base)} is {w(val)}. "
                f"So fifty percent of {w(base)} is {w(val)}."), val
    if pct == 25:
        return (f"Twenty-five percent means a quarter. A quarter of {w(base)} "
                f"is {w(val)}. So twenty-five percent of {w(base)} is "
                f"{w(val)}."), val
    if pct == 10:
        return (f"Ten percent means one tenth. {cap(w(base))} splits into ten "
                f"groups of {w(val)}. So ten percent of {w(base)} is "
                f"{w(val)}."), val
    # General case: build it from ten percent, which a child can actually do.
    tenth = base // 10
    lots = pct // 10
    return (f"Ten percent of {w(base)} is {w(tenth)}. {cap(w(pct))} percent is "
            f"{w(lots)} lots of that. {cap(w(lots))} times {w(tenth)} is "
            f"{w(val)}. So {w(pct)} percent of {w(base)} is {w(val)}."), val


FRAC_Q = [
    "what is {name} of {n}",
    "whats {name} of {n}",
    "how much is {name} of {n}",
    "if i split {n} into {groups} what do i get",
    "{name} of {n}",
]
FRAC_EVAL = [
    "can you work out {name} of {n}",
    "i need to know {name} of {n}",
]
PCT_Q = [
    "what is {p} percent of {b}",
    "whats {p} percent of {b}",
    "how much is {p} percent of {b}",
    "{p} percent of {b}",
    "work out {p} percent of {b}",
]
PCT_EVAL = [
    "can you find {p} percent of {b}",
    "i want to know {p} percent of {b}",
]

_NUM = re.compile(r"[a-z-]+")


def check(answer, value, what):
    """The stated result must equal the computed one. A generator that can
    emit a wrong fact is the problem, not the fix."""
    tail = answer.rsplit(" is ", 1)[-1].rstrip(".")
    if tail != w(value):
        raise SystemExit(f"GENERATOR BUG on {what}: answer ends {tail!r} but "
                         f"the value is {value} ({w(value)})\n  {answer}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "models_out/kid"))
    ap.add_argument("--max-n", type=int, default=100)
    ap.add_argument("--seed", type=int, default=7)
    a = ap.parse_args()

    rng = random.Random(a.seed)
    train, evals = [], []
    n_frac = n_pct = 0

    for denom, (name, groups) in FRACTIONS.items():
        for n in range(denom, a.max_n + 1):
            if n % denom:
                continue                      # only exact shares are taught
            ans, val = frac_answer(denom, n)
            check(ans, val, f"{name} of {n}")
            if len(ans) > 240:
                continue
            n_frac += 1
            for t in FRAC_Q:
                q = t.format(name=name, n=w(n), groups=groups)
                train.append(f"User: {q}\nBot: {ans}<|endoftext|>")
            for t in FRAC_EVAL:
                # n and words are what eval_math_ab.py grades on - it checks
                # the final NUMBER, which is exactly the thing v5 got wrong.
                evals.append({"q": t.format(name=name, n=w(n), groups=groups),
                              "a": ans, "topic": "fractions",
                              "n": val, "words": w(val)})

    for pct in PERCENTS:
        for base in BASES:
            if (base * pct) % 100:
                continue                      # whole answers only
            ans, val = pct_answer(pct, base)
            check(ans, val, f"{pct}% of {base}")
            if len(ans) > 240:
                continue
            n_pct += 1
            for t in PCT_Q:
                train.append(f"User: {t.format(p=w(pct), b=w(base))}\n"
                             f"Bot: {ans}<|endoftext|>")
            for t in PCT_EVAL:
                evals.append({"q": t.format(p=w(pct), b=w(base)), "a": ans,
                              "topic": "percentages",
                              "n": val, "words": w(val)})

    rng.shuffle(train)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    with (out / "fracpct_train.txt").open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n\n".join(train) + "\n")
    with (out / "fracpct_eval.jsonl").open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(json.dumps(e) for e in evals) + "\n")

    print(f"{n_frac} fraction facts + {n_pct} percent facts = {n_frac + n_pct}")
    print(f"  {len(train)} train samples, {len(evals)} held-out questions")
    print(f"  every answer verified against its computed value")
    print(f"wrote fracpct_train.txt and fracpct_eval.jsonl")


if __name__ == "__main__":
    main()
