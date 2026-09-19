"""Assemble every fact module into one training corpus.

The pipeline, and why each step is here:

  1. Import every module in facts/ and concatenate its build().
  2. validate() the COMBINED list. Per-module validation cannot see that
     numbers.py and mathconcepts.py both claim "what is a half" with
     different answers - only the combined pass catches a contradiction
     across modules, and a contradiction is worse than a gap.
  3. Expand phrasings. Hand-written facts carry 3-4 wordings; the measured
     phrasing gap (85.0% trained / 25.8% held-out) says that is not enough.
     Each fact is lifted to --per-fact surface forms drawn from the TRAIN
     pool only.
  4. Eval is built from `evalq` using the reserved EVAL pool, so a held-out
     score means the model generalized rather than memorized the wording.
     The two pools are asserted disjoint before anything is written, and the
     realized question sets are checked for overlap after.

Output is the trainer's format: "User: <q>\nBot: <a><|endoftext|>", blank
line separated.

  python tools/braino/build_corpus.py --out models_out/braino/data
"""
import argparse
import importlib
import json
import pkgutil
import random
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.braino.facts import validate                      # noqa: E402
from tools.braino.facts.outofscope import TOPIC as OOS_TOPIC  # noqa: E402
from tools.kid.phrasing import pools_disjoint, variants      # noqa: E402


def load_facts():
    """-> (facts, per-module counts). Every module in facts/ contributes."""
    import tools.braino.facts as pkg

    facts, counts = [], {}
    for mod in sorted(m.name for m in pkgutil.iter_modules(pkg.__path__)):
        m = importlib.import_module(f"tools.braino.facts.{mod}")
        if not hasattr(m, "build"):
            continue
        got = m.build()
        counts[mod] = len(got)
        facts += got
    return facts, counts


def expand(facts, per_fact, seed, refusal_per_fact=None):
    """-> (train_rows, eval_rows). Answers never vary; only the question does.

    refusal_per_fact overrides per_fact for the out-of-scope topic. It needs
    its own number because refusal is a BEHAVIOUR rather than a fact: there
    are only ~32 refusal subjects against ~20,000 real facts, so at the same
    per-fact rate refusals are ~0.5% of the corpus and too dilute to stick.
    Push it too far the other way and the model starts refusing things it
    knows. The right value is found by measuring both directions with
    tools/braino/eval_refusal.py, not by argument.
    """
    rng = random.Random(seed)
    train, ev = [], []

    # Build eval FIRST and reserve it. Generated training variants are then
    # filtered against it: "tell me a joke please" is a wording the train
    # generator will reach on its own, and if it is also a held-out eval
    # question the score is measuring recall. Dropping the generated variant
    # is free - there are plenty more. A collision between two HAND-WRITTEN
    # phrasings is a different thing: that is an authoring mistake, and the
    # caller fails the build on it rather than papering over it.
    for f in facts:
        for base in f.evalq:
            ev.append({"q": base, "a": f.answer, "topic": f.topic})
            for q in variants(base, rng, 2, eval_pool=True):
                ev.append({"q": q, "a": f.answer, "topic": f.topic})
    reserved = {norm(r["q"]) for r in ev}

    dropped = 0
    for f in facts:
        target = (refusal_per_fact if (refusal_per_fact and f.topic == OOS_TOPIC)
                  else per_fact)
        # Keep the hand-written wordings verbatim - they are the most natural
        # ones - then top up to target with generated surface forms.
        qs = list(dict.fromkeys(f.train))
        need = max(0, target - len(qs))
        if need:
            pool = []
            for base in f.train:
                pool += variants(base, rng, need)
            have = {x.lower() for x in qs}
            for q in pool:
                if len(qs) >= target:
                    break
                if q.lower() in have:
                    continue
                if norm(q) in reserved:
                    dropped += 1
                    continue
                qs.append(q)
                have.add(q.lower())
        for q in qs:
            train.append({"q": q, "a": f.answer, "topic": f.topic})

    if dropped:
        print(f"dropped {dropped} generated train variants that collided "
              f"with reserved eval wordings")
    return train, ev


def norm(q):
    return re.sub(r"[^a-z0-9 ]+", "", q.lower()).strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "models_out/braino/data"))
    ap.add_argument("--per-fact", type=int, default=14)
    ap.add_argument("--refusal-per-fact", type=int, default=None,
                    help="phrasings for the out-of-scope topic. Defaults to "
                         "--per-fact, which leaves refusals at ~0.5%% of a "
                         "full-parity corpus - usually too dilute to learn.")
    ap.add_argument("--seed", type=int, default=0)
    a = ap.parse_args()

    leaks = pools_disjoint()
    if leaks:
        raise SystemExit("phrasing pools leak: " + ", ".join(leaks))

    facts, counts = load_facts()
    if not facts:
        raise SystemExit("no facts found - are the modules written yet?")
    print(f"{len(facts)} facts from {len(counts)} modules")
    for m, n in sorted(counts.items(), key=lambda kv: -kv[1]):
        print(f"   {m:14s} {n:5d}")

    problems = validate(facts)
    if problems:
        print(f"\n{len(problems)} VALIDATION PROBLEMS across the combined set:")
        for p in problems[:40]:
            print(f"   {p}")
        if len(problems) > 40:
            print(f"   ... and {len(problems)-40} more")
        raise SystemExit("refusing to build a corpus that contradicts itself")
    print("combined validation: clean")

    train, ev = expand(facts, a.per_fact, a.seed, a.refusal_per_fact)

    # The whole point of the two pools is that this number is zero. Check the
    # realized questions, not just the pool definitions - a hand-written evalq
    # could coincide with a hand-written train phrasing in another fact.
    tq = {norm(r["q"]) for r in train}
    eq = {norm(r["q"]) for r in ev}
    overlap = tq & eq
    if overlap:
        print(f"\nLEAK: {len(overlap)} eval questions also appear in training")
        for q in sorted(overlap)[:10]:
            print(f"   {q!r}")
        raise SystemExit("eval would measure recall, not generalization")

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    rng = random.Random(a.seed)
    rng.shuffle(train)

    tp = out / "braino_train.txt"
    with tp.open("w") as fh:
        for r in train:
            fh.write(f"User: {r['q']}\nBot: {r['a']}<|endoftext|>\n\n")

    epath = out / "braino_eval.jsonl"
    with epath.open("w") as fh:
        for r in ev:
            fh.write(json.dumps(r) + "\n")

    by_topic = Counter(r["topic"] for r in train)
    print(f"\nwrote {tp}  {len(train)} samples, {tp.stat().st_size/1e6:.1f} MB")
    print(f"wrote {epath}  {len(ev)} held-out questions")
    print(f"{len(train)/len(facts):.1f} questions per fact, 0 train/eval overlap")
    print("\ntopics, smallest first (the thin ones are where accuracy dies):")
    for t, n in sorted(by_topic.items(), key=lambda kv: kv[1]):
        print(f"   {t:24s} {n:6d}")


if __name__ == "__main__":
    main()
