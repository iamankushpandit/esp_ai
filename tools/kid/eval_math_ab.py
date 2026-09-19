"""Score an arithmetic model on one thing only: is the final number right?

The two answer styles produce different strings for the same question -

    direct  Forty-four plus forty-two is eighty-six.
    steps   Four plus two is six. Forty plus forty is eighty.
            So forty-four plus forty-two is eighty-six.

- so comparing whole strings would penalise the worked style for showing its
work. Both are asked the same question and judged on the number they land on,
which is what a child actually gets wrong or right.

  python tools/kid/eval_math_ab.py --model models_out/kid/math_steps \
      --eval models_out/kid/math_steps_eval.jsonl
"""
import argparse
import json
import re
from pathlib import Path

import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

ROOT = Path(__file__).resolve().parents[2]

ONES = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
        "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
        "eighteen", "nineteen"]
TENS = {"twenty": 20, "thirty": 30, "forty": 40, "fifty": 50, "sixty": 60,
        "seventy": 70, "eighty": 80, "ninety": 90}


UNITS = set(ONES[1:10])          # "one".."nine", the only hundreds multipliers


def words_to_int(toks):
    """Parse ONE spoken number off the front of `toks` -> (value, consumed).

    A grammar, not a running sum. Summing greedily merged "twenty twenty" -
    two separate numbers - into forty.
    """
    i, total, seen = 0, 0, False

    if len(toks) > 1 and toks[0] in UNITS and toks[1] == "hundred":
        total += ONES.index(toks[0]) * 100
        i, seen = 2, True
        # "one hundred and five": the joining "and" belongs to the number only
        # if a number follows it.
        if i < len(toks) and toks[i] == "and":
            if i + 1 < len(toks) and (toks[i + 1] in ONES or toks[i + 1] in TENS):
                i += 1
            else:
                return total, i

    if i < len(toks):
        t = toks[i]
        if t in TENS:
            total += TENS[t]
            i += 1
            seen = True
            if i < len(toks) and toks[i] in UNITS:   # "eighty six", not "eighty eighty"
                total += ONES.index(toks[i])
                i += 1
        elif t in ONES:
            total += ONES.index(t)
            i += 1
            seen = True

    return (total, i) if seen else (None, 0)


# Both answer styles state the result after "is" (or "=" when the question was
# typed in digits). Anchoring on that instead of "last number anywhere" stops
# the "one" in "I do not know that one" from scoring as the number 1.
ANCHORS = {"is", "are", "equals", "makes", "="}


def at(toks, i):
    """The number starting exactly at toks[i], or None."""
    if i >= len(toks):
        return None
    if toks[i].isdigit():
        return int(toks[i])
    v, used = words_to_int(toks[i:])
    return v if used else None


def final_number(text):
    """The number the model lands on - its answer, after any working.

    Takes the number stated by the LAST anchor that is actually followed by
    one, walking backwards. Small models ramble, so the reply may end
    "...is thirty-one. Good job!" - anchoring on the final "is" alone would
    find nothing there. Walking back also means a reply truncated mid-working
    resolves to an intermediate value, which is still scored wrong.

    Returns None when no anchor states a number at all; that is a malformed
    answer and must not be scored as anything else.
    """
    toks = re.sub(r"[^a-z0-9=]+", " ", text.lower()).split()
    for i in range(len(toks) - 1, -1, -1):
        if toks[i] in ANCHORS:
            n = at(toks, i + 1)
            if n is not None:
                return n
    return None


@torch.no_grad()
def answer(model, tok, q, max_new):
    ids = tok(f"User: {q}\nBot:", return_tensors="pt").input_ids.to(model.device)
    out = model.generate(ids, max_new_tokens=max_new, do_sample=False,
                         pad_token_id=tok.eos_token_id, eos_token_id=tok.eos_token_id)
    return tok.decode(out[0][ids.shape[1]:], skip_special_tokens=True).split("\n")[0].strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True)
    ap.add_argument("--eval", required=True)
    ap.add_argument("--n", type=int, default=300)
    ap.add_argument("--max-new", type=int, default=72)
    ap.add_argument("--device", default="cpu")
    ap.add_argument("--show", type=int, default=8)
    a = ap.parse_args()

    tok = AutoTokenizer.from_pretrained(a.model)
    model = AutoModelForCausalLM.from_pretrained(a.model).to(a.device).eval()

    rows = [json.loads(l) for l in Path(a.eval).read_text().splitlines() if l.strip()]
    import random
    random.Random(7).shuffle(rows)
    rows = rows[:a.n]

    ok = 0
    misses = []
    for e in rows:
        got = answer(model, tok, e["q"], a.max_new)
        n = final_number(got)
        if n == e["n"]:
            ok += 1
        else:
            misses.append((e["q"], got, e["words"]))

    print(f"{Path(a.model).name}: final number correct {100*ok/len(rows):.1f}% "
          f"({ok}/{len(rows)})")
    for q, got, want in misses[:a.show]:
        print(f"  XX  {q}\n        got  {got}\n        want {want}")


if __name__ == "__main__":
    main()
