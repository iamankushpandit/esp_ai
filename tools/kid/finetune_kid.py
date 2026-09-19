"""Fine-tune TinyTalk 2 (GPT-Neo 8M) on child-level Q&A, keeping chat/story skills.

Base checkpoint: cardputer-ai data/chat_model_8m (TinyTalk 2, HF format).
Train mix: kid_train.txt (gen_kid_data.py) + a replay sample of the original
TinyTalk chat/story corpus so the model doesn't forget how to chat.
Loss masking follows cardputer-ai tools/finetune_chat.py (MIT): only bot
replies / story bodies + EOS are trained.

After every epoch the model answers held-out kid questions (phrasings never
trained) greedily and we report accuracy.

  python tools/kid/finetune_kid.py --epochs 3
"""
import argparse
import json
import math
import random
import re
import time
from pathlib import Path

import torch
from torch.utils.data import DataLoader, TensorDataset

ROOT = Path(__file__).resolve().parents[2]
REF = ROOT / ".refs" / "cardputer-ai"
EOS_LIT = "<|endoftext|>"


def encode_sample(s, tok):
    eos = tok.eos_token_id
    enc = lambda t: tok(t, add_special_tokens=False).input_ids
    ids, labels = [], []

    def put(t, train):
        ids.extend(t)
        labels.extend(t if train else [-100] * len(t))

    if s.startswith("User: "):
        lines = s.split("\n")
        for i in range(0, len(lines) - 1, 2):
            u, b = lines[i], lines[i + 1]
            if not b.startswith("Bot: "):
                break
            reply = " " + b[len("Bot: "):]
            reply = reply[:-len(EOS_LIT)] if reply.endswith(EOS_LIT) else reply
            if i > 0:
                put([198], False)
            put(enc(u + "\nBot:"), False)
            put(enc(reply), True)
            put([eos], True)
    else:
        body = s.find("\nStory:")
        s = s[:-len(EOS_LIT)] if s.endswith(EOS_LIT) else s
        if body >= 0:
            cut = body + len("\nStory:")
            put(enc(s[:cut]), False)
            put(enc(s[cut:]), True)
        else:
            put(enc(s), True)
        put([eos], True)
    return ids, labels


def pack(samples, tok, seq_len):
    ids, labels = [], []
    for s in samples:
        i, l = encode_sample(s, tok)
        ids += i
        labels += l
    n = len(ids) // seq_len
    x = torch.tensor(ids[:n * seq_len]).view(n, seq_len)
    y = torch.tensor(labels[:n * seq_len]).view(n, seq_len)
    return x, y


def norm(s):
    return re.sub(r"[^a-z0-9 ]", " ", s.lower()).split()


# Words that carry no answer content, so "in all" / "is" / "the" cannot make a
# wrong answer look right.
FILLER = {
    "a", "an", "the", "is", "are", "was", "were", "it", "its", "in", "on", "at",
    "of", "to", "and", "or", "that", "this", "there", "you", "your", "we", "i",
    "all", "so", "then", "do", "does", "did", "have", "has", "called", "makes",
    "make", "get", "gets", "be", "been", "will", "can", "for", "with", "by",
}


def content(words):
    """The words that decide whether an answer is right.

    The old metric compared only want[-2:], so every word problem - which ends
    "in all" - scored as correct regardless of the number. Compare the full
    content payload instead.
    """
    return [w for w in words if w not in FILLER]


@torch.no_grad()
def answer(model, tok, q, max_new=40):
    ids = tok(f"User: {q}\nBot:", return_tensors="pt").input_ids.to(model.device)
    out = model.generate(ids, max_new_tokens=max_new, do_sample=False,
                         pad_token_id=tok.eos_token_id, eos_token_id=tok.eos_token_id)
    return tok.decode(out[0][ids.shape[1]:], skip_special_tokens=True).split("\n")[0].strip()


def score(model, tok, evals):
    model.eval()
    exact = key = 0
    rows = []
    for e in evals:
        got = answer(model, tok, e["q"])
        g, want = norm(got), norm(e["a"])
        ex = g == want
        # key = every content word of the expected answer is present. NOT
        # want[-2:] - that scored "in all" as a correct word problem.
        cw = content(want)
        k = bool(cw) and all(wd in g for wd in cw)
        exact += ex
        key += k
        rows.append((e["q"], got, ex, k))
    model.train()
    return exact / len(evals), key / len(evals), rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--train", nargs="+", default=[str(ROOT / "models_out/kid/kid_train.txt")])
    ap.add_argument("--eval", nargs="+", default=[str(ROOT / "models_out/kid/kid_eval.jsonl")])
    ap.add_argument("--replay", type=int, default=20000, help="original chat/story samples mixed in")
    # The checkout lives under training/story_kid_bundle/.refs, but ROOT here is
    # the repo root, so the default below does not exist for every caller. Make
    # it an argument rather than a hardcoded path that silently points nowhere.
    ap.add_argument("--replay-file", default=str(REF / "data/chat_train.txt"),
                    help="original chat corpus to sample replay from")
    ap.add_argument("--base", default=str(REF / "data/chat_model_8m"))
    ap.add_argument("--out", default=str(ROOT / "models_out/kid/model_8m_kid"))
    ap.add_argument("--epochs", type=int, default=3)
    ap.add_argument("--seq-len", type=int, default=128)
    ap.add_argument("--batch-size", type=int, default=32)
    ap.add_argument("--lr", type=float, default=3e-4)
    ap.add_argument("--eval-n", type=int, default=300)
    ap.add_argument("--save-every-epoch", action="store_true",
                    help="also dump <out>/ep<N> each epoch. On a long run this "
                         "is the difference between an interruption costing an "
                         "epoch and costing everything.")
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()

    from transformers import GPT2TokenizerFast, GPTNeoForCausalLM
    torch.manual_seed(a.seed)
    rng = random.Random(a.seed)
    dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    tok = GPT2TokenizerFast.from_pretrained(a.base)
    model = GPTNeoForCausalLM.from_pretrained(a.base).to(dev)
    model.train()
    print(f"[+] base {a.base}: {sum(p.numel() for p in model.parameters())/1e6:.1f}M params, "
          f"device {dev}")

    kid = []
    for f in a.train:
        part = [s for s in Path(f).read_text(encoding="utf-8").split("\n\n") if s.strip()]
        print(f"[+] {Path(f).name}: {len(part)} samples")
        kid += part
    # Fail here, before the training loop, rather than after loading the model
    # and packing 91k samples. A missing replay corpus is a setup mistake and
    # should say so immediately.
    replay = []
    if a.replay:
        rf = Path(a.replay_file)
        if not rf.exists():
            raise SystemExit(f"--replay {a.replay} needs --replay-file, but "
                             f"{rf} does not exist. Pass the right path, or "
                             f"--replay 0 to train without it.")
        orig = [s for s in rf.read_text(encoding="utf-8").split("\n\n") if s.strip()]
        replay = rng.sample(orig, min(a.replay, len(orig)))
    mix = kid + replay
    rng.shuffle(mix)
    x, y = pack(mix, tok, a.seq_len)
    print(f"[+] kid {len(kid)} + replay {len(replay)} samples -> {len(x)} blocks "
          f"({len(x)*a.seq_len/1e6:.2f}M tokens, {(y != -100).float().mean()*100:.0f}% in loss)")

    evals = []
    for f in a.eval:   # equal share per eval file so small sets are represented
        rows = [json.loads(l) for l in Path(f).read_text(encoding="utf-8").splitlines() if l.strip()]
        evals += rng.sample(rows, min(a.eval_n // len(a.eval), len(rows)))
    # Score on a CPU copy, not on the training device. Greedy generation is
    # sequential - one kernel launch per token - and on MPS that measured ~64
    # minutes for a 400-question pass, against ~24 minutes to train a whole
    # epoch. Evaluation was costing more than three times what training cost,
    # and it also poisoned the tok/s readout, which divides training tokens by
    # a wall clock that includes eval.
    eval_model = GPTNeoForCausalLM.from_pretrained(a.base).to("cpu").eval()

    def cpu_state():
        return {k: v.detach().to("cpu", copy=True)
                for k, v in model.state_dict().items()}

    def score_cpu(rows):
        eval_model.load_state_dict(cpu_state())
        return score(eval_model, tok, rows)

    ex, ky, _ = score_cpu(evals[:100])
    print(f"[+] before: held-out exact {ex*100:.1f}%  key {ky*100:.1f}% (100 q)")

    loader = DataLoader(TensorDataset(x, y), batch_size=a.batch_size, shuffle=True)
    total = len(loader) * a.epochs
    warm = min(200, total // 10)
    opt = torch.optim.AdamW(model.parameters(), lr=a.lr, weight_decay=0.01)
    step, t0 = 0, time.time()
    best_ex = best_ky = -1.0
    best_ep, best_state = 0, None
    for ep in range(a.epochs):
        for bx, by in loader:
            lr = a.lr * step / warm if step < warm else \
                1e-5 + 0.5 * (a.lr - 1e-5) * (1 + math.cos(math.pi * (step - warm) / max(1, total - warm)))
            for g in opt.param_groups:
                g["lr"] = lr
            bx, by = bx.to(dev), by.to(dev)
            with torch.autocast(device_type=dev, dtype=torch.float16, enabled=dev == "cuda"):
                loss = model(bx, labels=by).loss
            opt.zero_grad(set_to_none=True)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
            step += 1
            if step % 100 == 0:
                el = time.time() - t0
                print(f"    step {step}/{total} loss {loss.item():.3f} lr {lr:.1e} "
                      f"{step*a.batch_size*a.seq_len/el/1e3:.1f}K tok/s, eta {el/step*(total-step)/60:.1f} min",
                      flush=True)
        ex, ky, rows = score_cpu(evals)
        print(f"[+] epoch {ep+1}: held-out exact {ex*100:.1f}%  key {ky*100:.1f}% ({len(evals)} q)")
        for q, got, e, k in rows[:6]:
            print(f"      {'OK ' if k else 'XX '} {q} -> {got}")

        # Keep the BEST epoch, not the last one. The v3 run peaked at epoch 5
        # (55.5% exact) and then overfitted down to 50.5% by epoch 8 - and it
        # was epoch 8 that got saved, throwing away five points for nothing.
        if a.save_every_epoch:
            d = Path(a.out) / f"ep{ep+1}"
            d.mkdir(parents=True, exist_ok=True)
            model.save_pretrained(d, state_dict=cpu_state())
            tok.save_pretrained(d)
            print(f"[+] wrote {d}", flush=True)

        if (ex, ky) > (best_ex, best_ky):
            best_ex, best_ky, best_ep = ex, ky, ep + 1
            best_state = cpu_state()

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
