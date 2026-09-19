"""Train the Braino model FROM SCRATCH on the 4,096-token vocabulary.

Not a fine-tune. There is no base checkpoint, because the point is to spend
the parameter budget differently:

                     TinyTalk 2 8M          Braino
    vocab                 50,257             4,096
    hidden                   256               384
    layers                     8                10
    embeddings        12,865,792  (65.3%)   1,572,864  (8.1%)
    transformer body   6,836,736             ~17.7M
    total             19,702,528             ~19.3M

Same on-device footprint, a transformer body 2.6x larger. The old model spent
two thirds of itself on 50,257 embedding rows when the corpus only ever uses
4,496 of them.

Two costs, both measured rather than assumed:
  * The 4k tokenizer needs 10.5% MORE tokens per answer. I predicted fewer and
    was wrong: a small vocabulary means longer token sequences, not shorter.
  * Against that, the output projection is 12x cheaper per token
    (384 x 4,096 = 1.6M MACs vs 384 x 50,257 = 19.3M), which more than pays
    for 10.5% more of them.

No replay corpus: the original TinyTalk chat data is in the old tokenizer's
token space and means nothing here. Chat ability has to come from the corpus
itself (the "about me" and "jokes & fun" topics).

  python tools/braino/train_braino.py --train .../braino_train.txt \
      --eval .../braino_eval.jsonl --tokenizer .../tokenizer --out .../model
"""
import argparse
import json
import math
import random
import sys
import time
from pathlib import Path

import torch
from torch.utils.data import DataLoader, TensorDataset

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.kid.finetune_kid import pack, score              # noqa: E402


def build_tokenizer(path):
    """Load the byte-level BPE and make it usable as a causal-LM tokenizer."""
    from transformers import GPT2TokenizerFast

    tok = GPT2TokenizerFast.from_pretrained(str(path))
    before = len(tok)
    tok.add_special_tokens({"eos_token": "<|endoftext|>",
                            "pad_token": "<|endoftext|>"})
    # <|endoftext|> is already token 0 in the trained vocab, so this should
    # only be relabelling it - not appending a 4,097th row that the model's
    # embedding table would then have to be resized to match.
    if len(tok) != before:
        raise SystemExit(f"tokenizer grew {before} -> {len(tok)}; "
                         "<|endoftext|> was missing from the trained vocab")
    return tok


def build_model(tok, hidden, layers, heads, seq_len, dev):
    from transformers import GPTNeoConfig, GPTNeoForCausalLM

    cfg = GPTNeoConfig(
        vocab_size=len(tok),
        hidden_size=hidden,
        num_layers=layers,
        num_heads=heads,
        max_position_embeddings=seq_len,
        # GPT-Neo alternates local and global attention. At seq_len 128 the
        # window covers the whole sequence anyway, so this costs nothing and
        # keeps the architecture identical in shape to the 8M base.
        attention_types=[[["global", "local"], layers // 2]],
        window_size=seq_len,
        bos_token_id=tok.eos_token_id,
        eos_token_id=tok.eos_token_id,
    )
    model = GPTNeoForCausalLM(cfg).to(dev)
    emb = model.get_input_embeddings().weight.numel()
    tot = sum(p.numel() for p in model.parameters())
    print(f"[+] fresh model: {tot/1e6:.2f}M params "
          f"({emb/1e6:.2f}M embeddings = {emb/tot*100:.1f}%, "
          f"body {(tot-emb)/1e6:.2f}M), device {dev}")
    return model


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--train", nargs="+", required=True)
    ap.add_argument("--eval", nargs="+", required=True)
    ap.add_argument("--tokenizer", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--hidden", type=int, default=384)
    ap.add_argument("--layers", type=int, default=10)
    ap.add_argument("--heads", type=int, default=6)
    ap.add_argument("--epochs", type=int, default=30)
    ap.add_argument("--seq-len", type=int, default=128)
    ap.add_argument("--batch-size", type=int, default=64)
    # From scratch wants a higher LR than a fine-tune: 3e-4 is tuned for
    # nudging pretrained weights, not for learning from random init.
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--eval-n", type=int, default=400)
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()

    torch.manual_seed(a.seed)
    rng = random.Random(a.seed)
    dev = "cuda" if torch.cuda.is_available() else \
          ("mps" if torch.backends.mps.is_available() else "cpu")

    tok = build_tokenizer(a.tokenizer)
    model = build_model(tok, a.hidden, a.layers, a.heads, a.seq_len, dev)
    model.train()

    samples = []
    for f in a.train:
        part = [s for s in Path(f).read_text(encoding="utf-8").split("\n\n")
                if s.strip()]
        print(f"[+] {Path(f).name}: {len(part)} samples")
        samples += part
    rng.shuffle(samples)
    x, y = pack(samples, tok, a.seq_len)
    print(f"[+] {len(samples)} samples -> {len(x)} blocks "
          f"({len(x)*a.seq_len/1e6:.2f}M tokens, "
          f"{(y != -100).float().mean()*100:.0f}% in loss)")

    # Score on a CPU copy, never on the training device. Greedy decoding is
    # one kernel launch per token, and on MPS that measured ~22x slower end to
    # end in the 8M trainer. This file was written before that was found, so
    # it had the slow path: measured at step 300, evaluation was about 60% of
    # the whole run.
    # Imported here because build_model() imports it in its own scope; at
    # module level this name does not exist.
    from transformers import GPTNeoForCausalLM

    eval_model = GPTNeoForCausalLM(model.config).to("cpu").eval()

    def cpu_state():
        return {k: v.detach().to("cpu", copy=True)
                for k, v in model.state_dict().items()}

    def score_cpu(rows):
        eval_model.load_state_dict(cpu_state())
        return score(eval_model, tok, rows)

    evals = []
    for f in a.eval:
        rows = [json.loads(l) for l in
                Path(f).read_text(encoding="utf-8").splitlines() if l.strip()]
        evals += rng.sample(rows, min(a.eval_n // len(a.eval), len(rows)))
    print(f"[+] {len(evals)} held-out eval questions")

    loader = DataLoader(TensorDataset(x, y), batch_size=a.batch_size, shuffle=True)
    total = len(loader) * a.epochs
    warm = min(500, total // 10)
    opt = torch.optim.AdamW(model.parameters(), lr=a.lr, weight_decay=0.01,
                            betas=(0.9, 0.95))
    step, t0 = 0, time.time()
    best_ex = best_ky = -1.0
    best_ep, best_state = 0, None

    for ep in range(a.epochs):
        for bx, by in loader:
            lr = a.lr * step / max(1, warm) if step < warm else \
                1e-5 + 0.5 * (a.lr - 1e-5) * \
                (1 + math.cos(math.pi * (step - warm) / max(1, total - warm)))
            for g in opt.param_groups:
                g["lr"] = lr
            bx, by = bx.to(dev), by.to(dev)
            loss = model(bx, labels=by).loss
            opt.zero_grad(set_to_none=True)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
            step += 1
            if step % 100 == 0:
                el = time.time() - t0
                print(f"    step {step}/{total} loss {loss.item():.3f} "
                      f"lr {lr:.1e} "
                      f"{step*a.batch_size*a.seq_len/el/1e3:.1f}K tok/s, "
                      f"eta {el/step*(total-step)/60:.1f} min", flush=True)

        ex, ky, rows = score_cpu(evals)
        print(f"[+] epoch {ep+1}: held-out exact {ex*100:.1f}%  "
              f"key {ky*100:.1f}% ({len(evals)} q)", flush=True)
        for q, got, e, k in rows[:6]:
            print(f"      {'OK ' if k else 'XX '} {q} -> {got}")

        if (ex, ky) > (best_ex, best_ky):
            best_ex, best_ky, best_ep = ex, ky, ep + 1
            best_state = cpu_state()
            # Write the best checkpoint as soon as it exists. Holding it only
            # in memory means an interruption costs the whole run, which on a
            # long job is the wrong trade.
            d = Path(a.out)
            d.mkdir(parents=True, exist_ok=True)
            model.save_pretrained(d, state_dict=best_state)
            tok.save_pretrained(d)

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    if best_state is not None:
        model.load_state_dict(best_state)
        print(f"[+] restoring epoch {best_ep} (exact {best_ex*100:.1f}%  "
              f"key {best_ky*100:.1f}%) - the best, not the last")
    model.cpu().save_pretrained(out)
    tok.save_pretrained(out)
    print(f"[+] saved {out}")


if __name__ == "__main__":
    main()
