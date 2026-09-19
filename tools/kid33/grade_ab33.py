"""Grade every saved epoch of an A/B arm with the eval_math_ab metric.

The trainer's own per-epoch number is a string-exact match, which structurally
penalises the worked-steps arm (a long answer has more ways to differ). So the
arms are compared here instead, on the one thing both styles are trying to do:
land on the right final number.

  python tools/kid33/grade_ab33.py --dir <out>/model_33m_direct \
      --unseen <ab>/math_direct_eval.jsonl --seen <ab>/math_direct_eval_seen.jsonl
"""
import argparse
import json
import random
import re
import sys
from pathlib import Path

import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "kid"))
from eval_math_ab import answer, final_number  # noqa: E402


def load(path, n, seed=7):
    rows = [json.loads(l) for l in Path(path).read_text().splitlines() if l.strip()]
    random.Random(seed).shuffle(rows)
    return rows[:n]


def run(model, tok, rows, max_new):
    ok = 0
    misses = []
    for e in rows:
        got = answer(model, tok, e["q"], max_new)
        if final_number(got) == e["n"]:
            ok += 1
        elif len(misses) < 4:
            misses.append((e["q"], got, e["words"]))
    return ok / len(rows), misses


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True, help="dir holding ep1..epN subdirs")
    ap.add_argument("--unseen", required=True)
    ap.add_argument("--seen", required=True)
    ap.add_argument("--n", type=int, default=150)
    ap.add_argument("--max-new", type=int, default=72)
    ap.add_argument("--device", default="cpu")
    a = ap.parse_args()

    eps = sorted(Path(a.dir).glob("ep*"), key=lambda p: int(re.sub(r"\D", "", p.name)))
    if not eps:
        sys.exit(f"no ep* checkpoints under {a.dir}")
    unseen, seen = load(a.unseen, a.n), load(a.seen, a.n)

    print(f"# {Path(a.dir).name}: {len(eps)} epochs, n={a.n} unseen + {a.n} seen")
    for d in eps:
        tok = AutoTokenizer.from_pretrained(d)
        model = AutoModelForCausalLM.from_pretrained(d).to(a.device).eval()
        u, um = run(model, tok, unseen, a.max_new)
        s, _ = run(model, tok, seen, a.max_new)
        print(f"{Path(a.dir).name} {d.name}: unseen {u*100:5.1f}%   seen {s*100:5.1f}%",
              flush=True)
        for q, got, want in um[:2]:
            print(f"      XX {q} -> {got[:90]}   want {want[:60]}")
        del model


if __name__ == "__main__":
    main()
