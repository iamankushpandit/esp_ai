"""Per-topic accuracy for the Braino model.

The old evaluator inferred a category from the source FILENAME, which is why
46,495 samples ended up in a bucket called "other". Here every eval row
carries its own `topic`, so the breakdown is exact and needs no guessing.

Reports two numbers per topic:
  exact - the answer matches word for word
  key   - every content word of the expected answer appears

Trust `exact`. `key` is still generous with spoken numbers: "twenty-two"
tokenizes to ["twenty", "two"], so a wrong answer of "twenty" contains a
content word of the right one. That is a known hole, documented rather than
silently relied on.

  python tools/braino/eval_braino.py --model <dir> --eval <braino_eval.jsonl>
"""
import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.kid.finetune_kid import answer, content, norm    # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True)
    ap.add_argument("--eval", required=True)
    ap.add_argument("--per-topic", type=int, default=25,
                    help="questions sampled per topic")
    ap.add_argument("--device", default="cpu",
                    help="cpu is right here: greedy decoding on mps measured "
                         "~22x slower end to end")
    ap.add_argument("--show", type=int, default=3, help="example misses per topic")
    a = ap.parse_args()

    from transformers import GPT2TokenizerFast, GPTNeoForCausalLM
    tok = GPT2TokenizerFast.from_pretrained(a.model)
    model = GPTNeoForCausalLM.from_pretrained(a.model).to(a.device).eval()

    rows = [json.loads(l) for l in
            Path(a.eval).read_text(encoding="utf-8").splitlines() if l.strip()]
    by_topic = defaultdict(list)
    for r in rows:
        by_topic[r.get("topic", "unknown")].append(r)

    results, misses = {}, {}
    tot_ex = tot_ky = tot_n = 0
    for topic in sorted(by_topic):
        sample = by_topic[topic][:a.per_topic]
        ex = ky = 0
        bad = []
        for r in sample:
            got = answer(model, tok, r["q"])
            g, want = norm(got), norm(r["a"])
            is_ex = g == want
            cw = content(want)
            is_ky = bool(cw) and all(w in g for w in cw)
            ex += is_ex
            ky += is_ky
            if not is_ky and len(bad) < a.show:
                bad.append((r["q"], got, r["a"]))
        n = len(sample)
        results[topic] = (ex / n, ky / n, n)
        misses[topic] = bad
        tot_ex += ex
        tot_ky += ky
        tot_n += n
        print(f"  {topic:24s} exact {ex/n*100:5.1f}%  key {ky/n*100:5.1f}%  "
              f"({n} q)", flush=True)

    print(f"\n{'OVERALL':26s} exact {tot_ex/tot_n*100:5.1f}%  "
          f"key {tot_ky/tot_n*100:5.1f}%  ({tot_n} q)")

    print("\nworst topics:")
    for topic, (ex, ky, n) in sorted(results.items(), key=lambda kv: kv[1][0])[:8]:
        print(f"  {topic:24s} exact {ex*100:5.1f}%")
        for q, got, want in misses[topic]:
            print(f"      Q {q}")
            print(f"      got  {got}")
            print(f"      want {want}")


if __name__ == "__main__":
    main()
