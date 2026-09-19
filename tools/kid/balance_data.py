"""Rebalance the training mix so every field gets a fair share of training.

The target is 90% in *all* fields. The first run could not reach that for a
structural reason that has nothing to do with the model: the mix is wildly
lopsided. Counted over the full corpus -

    arithmetic   73,687 samples   scored 22%
    roman         4,341 samples   scored 42%
    capitals      2,933 samples   scored 61%
    spelling        284 samples   scored  7%

Arithmetic gets 260x the samples spelling does, so nearly every gradient step
teaches arithmetic and almost none teach spelling. Uniform `--repeat` cannot
fix this, because it scales every category equally and preserves the ratio.

This resamples per category toward a target share: small categories are
repeated, oversized ones are capped. Repetition is capped too - past a point
the same fact reworded the same way stops adding signal and starts overfitting
the phrasings, so we report what was hit rather than silently inflating.

  python tools/kid/balance_data.py models_out/kid/*_train.txt --out mixed.txt
"""
import argparse
import random
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from eval_by_category import category  # noqa: E402

BLOCK = re.compile(r"User: (.*?)\nBot: (.*?)<\|endoftext\|>", re.S)


def load(paths):
    """-> {category: [block, ...]}"""
    buckets = defaultdict(list)
    for p in paths:
        src = Path(p).name
        text = Path(p).read_text(encoding="utf-8")
        for raw in text.split("\n\n"):
            raw = raw.strip()
            if not raw:
                continue
            m = BLOCK.match(raw)
            if not m:
                continue
            buckets[category(m.group(1), m.group(2), src)].append(raw)
    return buckets


def balance(buckets, target, max_repeat, rng, floor_only=False):
    """Resample each category toward `target` samples.

    floor_only raises small categories without shrinking large ones. That is
    usually what you want once augment_phrasings.py has already equalised
    exposure *per fact*: capping on top of that gave jokes (114 facts) 70
    samples each while "other" (~3,000 facts) got 2.6 - the original imbalance
    pointing the other way. A category is big because it has more facts to
    learn, and those facts still each need their turn.

    Returns (mixed_blocks, report rows).
    """
    out, rows = [], []
    for cat in sorted(buckets):
        blocks = buckets[cat]
        have = len(blocks)
        if have >= target:
            if floor_only:
                take = list(blocks)
                note = "kept"
            else:                                # cap: sample without replacement
                take = rng.sample(blocks, target)
                note = "capped"
        else:
            # Repeat whole passes so every fact is seen the same number of
            # times, then top up with a random remainder.
            reps = min(max_repeat, target // have)
            take = blocks * reps
            short = target - len(take)
            if short > 0 and reps == max_repeat:
                note = f"repeat x{reps} (capped, {len(take)} < {target})"
            else:
                take += rng.sample(blocks, min(short, have))
                note = f"repeat x{reps}"
        out += take
        rows.append((cat, have, len(take), note))
    rng.shuffle(out)
    return out, rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--target", type=int, default=6000,
                    help="samples per category to aim for")
    ap.add_argument("--max-repeat", type=int, default=12,
                    help="most times one sample may be repeated")
    ap.add_argument("--out", required=True)
    ap.add_argument("--floor-only", action="store_true",
                    help="raise small categories but never shrink large ones")
    ap.add_argument("--seed", type=int, default=5)
    a = ap.parse_args()

    rng = random.Random(a.seed)
    buckets = load(a.files)
    before = Counter({c: len(v) for c, v in buckets.items()})
    mixed, rows = balance(buckets, a.target, a.max_repeat, rng, a.floor_only)

    Path(a.out).write_text("\n\n".join(mixed) + "\n", encoding="utf-8")

    print(f"{'category':22s} {'before':>8s} {'after':>8s}   note")
    print("-" * 64)
    for cat, have, got, note in sorted(rows, key=lambda r: -r[1]):
        print(f"{cat:22s} {have:8d} {got:8d}   {note}")
    print("-" * 64)
    print(f"{'TOTAL':22s} {sum(before.values()):8d} {len(mixed):8d}")
    print(f"wrote {a.out}")


if __name__ == "__main__":
    main()
