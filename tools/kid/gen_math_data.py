"""Arithmetic training data, in two answer styles, so we can measure which one
a 19.7M model can actually learn.

Background: the first run trained arithmetic as recall - one question, one
memorised answer - and scored 22%. There are ~10,000 two-digit pairs, so recall
was never going to work; the model retrieved the nearest memorised string
("add forty four and forty two" -> "Forty-FIVE plus forty-two is eighty-six").

The claim to test is that addition is a *rule*, and that a small model can
learn the rule if the answer shows the work instead of jumping to the result.
The sub-facts a worked answer needs are small and finite:

    single digit + single digit    100 facts
    carry                          one rule
    digits -> spoken number        compositional

So this generator emits the same questions two ways:

    direct  Bot: Forty-four plus forty-two is eighty-six.
    steps   Bot: Four plus two is six. Forty plus forty is eighty.
                 So forty-four plus forty-two is eighty-six.

Train one model on each, eval both on the same held-out pairs, and let the
numbers decide. Coverage is systematic (every pair in range), not sampled -
the point is to teach a rule, and a rule needs the whole table.

  python tools/kid/gen_math_data.py --style steps --out models_out/kid
"""
import argparse
import json
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "training/story_kid_bundle/tools/kid"))
from gen_kid_data import w, cap, spoken  # noqa: E402

# (train_questions, eval_questions, answer, result) - `result` is the integer
# the question evaluates to, recorded so the two styles can be scored the same
# way: is the final number right? Comparing full answer strings would penalise
# the worked style for showing its work.
FACTS = []


# ---------------------------------------------------------------- answers
def direct_add(a, b):
    return f"{cap(w(a))} plus {w(b)} is {w(a + b)}."


def steps_add(a, b):
    """The column method a 5-8 year old is taught: ones, then tens, then join.

    The joining step is stated explicitly. Without it the answer shows
    "eight plus three is eleven, twenty plus zero is twenty" and then jumps
    straight to thirty-one - leaving the carry, the one hard part, for the
    model to do in its head, which is exactly what we are trying to avoid.
    """
    if a < 10 and b < 10:                      # nothing to decompose
        return direct_add(a, b)
    ao, bo = a % 10, b % 10
    at, bt = a - ao, b - bo
    ones, tens = ao + bo, at + bt
    parts = [f"{cap(w(ao))} plus {w(bo)} is {w(ones)}.",
             f"{cap(w(at))} plus {w(bt)} is {w(tens)}.",
             f"{cap(w(tens))} plus {w(ones)} is {w(a + b)}.",
             f"So {w(a)} plus {w(b)} is {w(a + b)}."]
    return " ".join(parts)


def direct_sub(a, b):
    return f"{cap(w(a))} minus {w(b)} is {w(a - b)}."


def steps_sub(a, b):
    if a < 10 and b < 10:
        return direct_sub(a, b)
    ao, bo = a % 10, b % 10
    at, bt = a - ao, b - bo
    parts = []
    if ao >= bo:                               # no borrowing: ones then tens
        if ao or bo:
            parts.append(f"{cap(w(ao))} minus {w(bo)} is {w(ao - bo)}.")
        if at or bt:
            parts.append(f"{cap(w(at))} minus {w(bt)} is {w(at - bt)}.")
    else:                                      # borrowing: take the tens first
        mid = a - bt
        parts.append(f"{cap(w(a))} minus {w(bt)} is {w(mid)}.")
        parts.append(f"{cap(w(mid))} minus {w(bo)} is {w(mid - bo)}.")
    parts.append(f"So {w(a)} minus {w(b)} is {w(a - b)}.")
    return " ".join(parts)


def direct_mul(a, b):
    return f"{cap(w(a))} times {w(b)} is {w(a * b)}."


def steps_mul(a, b):
    return direct_mul(a, b)                    # 11x11 table is memorisable


def direct_div(a, b):
    return f"{cap(w(a))} divided by {w(b)} is {w(a // b)}."


def steps_div(a, b):
    q = a // b
    if a < 10:
        return direct_div(a, b)
    return (f"{cap(w(b))} times {w(q)} is {w(a)}. "
            f"So {w(a)} divided by {w(b)} is {w(q)}.")


# ---------------------------------------------------------------- questions
ADD_Q = ["what is {A} plus {B}", "what's {A} plus {B}", "{A} plus {B}",
         "what is {A} add {B}", "how much is {A} plus {B}", "what is {a} + {b}",
         "can you add {A} and {B}"]
ADD_E = ["hey story what does {A} plus {B} make", "what do you get if you add {A} and {B}"]
SUB_Q = ["what is {A} minus {B}", "what's {A} minus {B}", "{A} minus {B}",
         "what is {A} take away {B}", "what is {a} - {b}", "what is {A} subtract {B}"]
SUB_E = ["if i have {A} and take away {B} how many are left",
         "hey story what does {A} minus {B} make"]
MUL_Q = ["what is {A} times {B}", "what's {A} times {B}", "{A} times {B}",
         "what is {A} multiplied by {B}", "what is {a} x {b}"]
MUL_E = ["hey story what does {A} times {B} make", "can you multiply {A} and {B}"]
DIV_Q = ["what is {A} divided by {B}", "what's {A} divided by {B}",
         "{A} divided by {B}", "what is {a} / {b}", "how many {B}s are in {A}"]
DIV_E = ["hey story what is {A} shared between {B}", "what do you get if you divide {A} by {B}"]


def emit(a, b, qs, es, answer, result):
    f = dict(A=spoken(a), B=spoken(b), a=a, b=b)
    FACTS.append(([q.format(**f) for q in qs], [q.format(**f) for q in es],
                  answer, result))


def build(style, limit):
    """Systematic coverage of the operand space, not a sample."""
    add, sub = (steps_add, steps_sub) if style == "steps" else (direct_add, direct_sub)
    mul, div = (steps_mul, steps_div) if style == "steps" else (direct_mul, direct_div)

    for a in range(0, limit + 1):
        for b in range(0, limit + 1):
            if a + b > 2 * limit:
                continue
            emit(a, b, ADD_Q, ADD_E, add(a, b), a + b)
    for a in range(0, limit + 1):
        for b in range(0, a + 1):
            emit(a, b, SUB_Q, SUB_E, sub(a, b), a - b)
    for a in range(0, 13):
        for b in range(0, 13):
            emit(a, b, MUL_Q, MUL_E, mul(a, b), a * b)
    for b in range(1, 13):
        for q in range(0, 13):
            emit(b * q, b, DIV_Q, DIV_E, div(b * q, b), q)


def styled(q, rng):
    return q if rng.random() < 0.7 else cap(q) + "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--style", choices=["direct", "steps"], required=True)
    ap.add_argument("--limit", type=int, default=30,
                    help="largest operand for add/subtract")
    ap.add_argument("--repeat", type=int, default=1)
    ap.add_argument("--holdout", type=float, default=0.15,
                    help="fraction of operand PAIRS kept out of training entirely. "
                         "Held-out phrasings only measure recall; held-out pairs "
                         "are the test of whether the model learned the rule.")
    ap.add_argument("--out", default=str(ROOT / "models_out/kid"))
    ap.add_argument("--suffix", default=None)
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()

    rng = random.Random(a.seed)
    build(a.style, a.limit)

    # Split on FACTS (i.e. on operand pairs) before emitting anything, so a
    # held-out pair contributes no training sample in any phrasing.
    idx = list(range(len(FACTS)))
    rng.shuffle(idx)
    n_hold = int(len(idx) * a.holdout)
    held = set(idx[:n_hold])

    train, evals, evals_seen = [], [], []
    for k, (tq, eq, ans, res) in enumerate(FACTS):
        row = {"a": ans, "n": res, "words": w(res)}
        if k in held:
            # Unseen pair: every phrasing is fair game as an eval question.
            for q in list(tq) + list(eq):
                evals.append(dict(row, q=q))
            continue
        for _ in range(a.repeat):
            for q in tq:
                train.append(f"User: {styled(q, rng)}\nBot: {ans}<|endoftext|>")
        for q in eq:
            evals_seen.append(dict(row, q=q))
    rng.shuffle(train)

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    sfx = a.suffix or a.style
    tp = out / f"math_{sfx}_train.txt"
    ep = out / f"math_{sfx}_eval.jsonl"          # unseen pairs - the real test
    sp = out / f"math_{sfx}_eval_seen.jsonl"     # seen pairs, new phrasing
    tp.write_text("\n\n".join(train) + "\n", encoding="utf-8")
    ep.write_text("\n".join(json.dumps(e) for e in evals) + "\n", encoding="utf-8")
    sp.write_text("\n".join(json.dumps(e) for e in evals_seen) + "\n", encoding="utf-8")
    print(f"{len(FACTS)} pairs ({n_hold} held out)  {len(train)} train samples")
    print(f"  {len(evals)} eval questions on unseen pairs, {len(evals_seen)} on seen pairs")
    print(f"wrote {tp.name}, {ep.name}, {sp.name}")


if __name__ == "__main__":
    main()
