"""Multiply the number of ways each fact can be asked.

This is the fix for the 55-point gap measured by tools/kid/diag_phrasing.py:
the model holds ~87% of the facts but answers only 26% when the wording is new.
More phrasings per fact, not more parameters.

Facts are grouped by ANSWER, because that is what identifies a fact - the same
capital is asked six different ways across the corpus, and all six seeds should
feed the same variant pool.

Training variants are drawn only from the TRAIN pool in tools/kid/phrasing.py,
and the eval file is regenerated from the reserved EVAL pool, so improvement
cannot come from the eval wordings leaking into training. Without that split,
augmenting with the obvious "hey story ..." prefix would post a huge gain and
mean nothing.

  python tools/kid/augment_phrasings.py --train a_train.txt --eval a_eval.jsonl \
      --out-dir models_out/kid --per-fact 14
"""
import argparse
import json
import random
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from phrasing import pools_disjoint, variants  # noqa: E402

BLOCK = re.compile(r"User: (.*?)\nBot: (.*?)<\|endoftext\|>", re.S)


def read_train(paths):
    """-> {answer: [seed question, ...]}"""
    facts = defaultdict(list)
    for p in paths:
        for raw in Path(p).read_text(encoding="utf-8").split("\n\n"):
            m = BLOCK.match(raw.strip())
            if not m:
                continue
            q, ans = m.group(1).strip(), m.group(2).strip()
            if q not in facts[ans]:
                facts[ans].append(q)
    return facts


def base_form(q):
    """Strip an existing lead-in so variants build on the bare question."""
    q = q.strip().rstrip("?")
    q = re.sub(r"^(hey|hi|ok|okay|um|uh)\s+story\b", "", q, flags=re.I)
    q = re.sub(r"^(hey|hi|ok|okay|um|uh)\b", "", q, flags=re.I)
    return q.strip() or q


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--train", nargs="+", required=True)
    ap.add_argument("--eval", nargs="+", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--prefix", default="aug")
    ap.add_argument("--per-fact", type=int, default=14,
                    help="training questions per fact after augmentation")
    ap.add_argument("--eval-per-fact", type=int, default=2)
    ap.add_argument("--seed", type=int, default=17)
    a = ap.parse_args()

    leaks = pools_disjoint()
    if leaks:
        raise SystemExit("refusing to run, train/eval wording pools overlap: "
                         + ", ".join(leaks))

    rng = random.Random(a.seed)
    facts = read_train(a.train)

    # Eval answers stay the source of truth for what gets evaluated, but the
    # questions are rebuilt from the reserved pool.
    eval_answers = []
    for p in a.eval:
        for line in Path(p).read_text(encoding="utf-8").splitlines():
            if line.strip():
                eval_answers.append(json.loads(line))

    train_out = []
    for ans, seeds in facts.items():
        bases = list(dict.fromkeys(base_form(s) for s in seeds))
        want = a.per_fact
        got = []
        # spread the budget across the seed wordings
        per_seed = max(1, want // len(bases))
        for b in bases:
            got += variants(b, rng, per_seed)
        got = list(dict.fromkeys(got))[:want]
        for q in got:
            train_out.append(f"User: {q}\nBot: {ans}<|endoftext|>")
    rng.shuffle(train_out)

    eval_out = []
    for e in eval_answers:
        for q in variants(base_form(e["q"]), rng, a.eval_per_fact, eval_pool=True):
            eval_out.append(dict(e, q=q))

    out = Path(a.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    tp = out / f"{a.prefix}_train.txt"
    ep = out / f"{a.prefix}_eval.jsonl"
    tp.write_text("\n\n".join(train_out) + "\n", encoding="utf-8")
    ep.write_text("\n".join(json.dumps(e) for e in eval_out) + "\n", encoding="utf-8")

    before = sum(len(v) for v in facts.values())
    print(f"{len(facts)} facts")
    print(f"  train questions {before} -> {len(train_out)} "
          f"({before/max(len(facts),1):.1f} -> {len(train_out)/max(len(facts),1):.1f} per fact)")
    print(f"  eval questions  {len(eval_answers)} -> {len(eval_out)} (reserved wordings)")
    print(f"wrote {tp.name} and {ep.name}")


if __name__ == "__main__":
    main()
