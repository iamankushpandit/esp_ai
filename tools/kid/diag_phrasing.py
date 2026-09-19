"""Is the model missing FACTS, or just missing WORDINGS?

v3 finished with training loss 0.22 but held-out exact accuracy stuck at 54%.
Those two numbers together are the whole question. The eval holds out question
*templates*, so a miss can mean either:

  a) the model never learned the fact          -> needs capacity or more data
  b) it learned the fact but only as one phrasing, and the eval asked another
                                               -> needs paraphrase diversity

These have opposite fixes, so guessing is expensive. This asks each fact twice:
once with a wording from training, once with the held-out wording.

    trained wording high, held-out low   -> (b), a phrasing problem
    both low                             -> (a), a capacity problem

  python tools/kid/diag_phrasing.py --model models_out/kid/model_8m_kid_v3 \
      --train models_out/kid/kid_train_knowledge.txt \
      --eval  models_out/kid/kid_eval_knowledge.jsonl
"""
import argparse
import json
import random
import re
import sys
from pathlib import Path

import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

sys.path.insert(0, str(Path(__file__).resolve().parent))
from eval_by_category import content, norm  # noqa: E402

BLOCK = re.compile(r"User: (.*?)\nBot: (.*?)<\|endoftext\|>", re.S)


@torch.no_grad()
def answer(model, tok, q, max_new=48):
    ids = tok(f"User: {q}\nBot:", return_tensors="pt").input_ids.to(model.device)
    out = model.generate(ids, max_new_tokens=max_new, do_sample=False,
                         pad_token_id=tok.eos_token_id, eos_token_id=tok.eos_token_id)
    return tok.decode(out[0][ids.shape[1]:], skip_special_tokens=True).split("\n")[0].strip()


def grade(got, want):
    g, wnt = norm(got), norm(want)
    cw = content(wnt)
    return g == wnt, bool(cw) and all(x in g for x in cw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True)
    ap.add_argument("--train", nargs="+", required=True)
    ap.add_argument("--eval", nargs="+", required=True)
    ap.add_argument("-n", type=int, default=150)
    ap.add_argument("--device", default="cpu")
    ap.add_argument("--seed", type=int, default=4)
    a = ap.parse_args()

    # answer text -> a question wording that WAS trained
    trained = {}
    for p in a.train:
        for raw in Path(p).read_text(encoding="utf-8").split("\n\n"):
            m = BLOCK.match(raw.strip())
            if m:
                trained.setdefault(m.group(2).strip(), m.group(1).strip())

    rows = []
    for p in a.eval:
        for line in Path(p).read_text(encoding="utf-8").splitlines():
            if line.strip():
                rows.append(json.loads(line))
    # only facts we can ask both ways
    rows = [e for e in rows if e["a"].strip() in trained]
    random.Random(a.seed).shuffle(rows)
    rows = rows[:a.n]
    if not rows:
        print("no facts appear in both train and eval - cannot compare")
        return

    tok = AutoTokenizer.from_pretrained(a.model)
    model = AutoModelForCausalLM.from_pretrained(a.model).to(a.device).eval()

    seen_e = seen_k = held_e = held_k = 0
    both_wrong = []
    for e in rows:
        want = e["a"].strip()
        se, sk = grade(answer(model, tok, trained[want]), want)
        he, hk = grade(answer(model, tok, e["q"]), want)
        seen_e += se; seen_k += sk
        held_e += he; held_k += hk
        if not sk and not hk:
            both_wrong.append((e["q"], want))

    n = len(rows)
    print(f"{n} facts, each asked with a trained wording and a held-out wording\n")
    print(f"{'wording':16s} {'exact':>8s} {'key':>8s}")
    print("-" * 34)
    print(f"{'trained':16s} {100*seen_e/n:7.1f}% {100*seen_k/n:7.1f}%")
    print(f"{'held-out':16s} {100*held_e/n:7.1f}% {100*held_k/n:7.1f}%")
    print("-" * 34)
    gap = 100 * (seen_k - held_k) / n
    print(f"gap (key): {gap:+.1f} points")
    print(f"\nfacts wrong BOTH ways (genuinely not learned): {len(both_wrong)}/{n} "
          f"= {100*len(both_wrong)/n:.1f}%")
    for q, want in both_wrong[:8]:
        print(f"  {q}  ->  want: {want}")

    print("\nreading: a large gap means the facts are in there and the wording is "
          "the problem (fix: more paraphrases per fact). A small gap with both "
          "low means the facts were never learned (fix: capacity or more data).")


if __name__ == "__main__":
    main()
