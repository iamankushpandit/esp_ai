"""Train a small byte-level BPE sized for the Braino question space.

Why this exists, in one measurement: the 8M model is 19,702,528 parameters,
and 12,865,792 of them - 65.3% - are the embedding table for a 50,257-token
GPT-2 vocabulary. The transformer body, the part that actually reasons, is
only 6,836,736 parameters.

The corpus uses 4,496 distinct tokens of those 50,257 (8.9%), and 4,398 of
them cover 99.99% of all token occurrences. So roughly 46,000 embedding rows
are dead weight on a device with a few megabytes of PSRAM.

Dropping to a 4,096-token vocabulary frees ~11.8M parameters - nearly twice
the entire current transformer body - at identical on-device size.

Byte-level BPE is kept (not a character or word tokenizer) so the model can
never hit an unknown token: any byte sequence is representable. That matters
because speech recognition output is unpredictable.

  python tools/braino/train_tokenizer.py --out models_out/braino/tokenizer

NOTE: the on-device BPE decoder reads vocab.json + merges.txt. This writes
both, but the firmware's tokenizer tables must be regenerated to match or the
model's output will be decoded as garbage. That is a required firmware change,
not an optional one.
"""
import argparse
from pathlib import Path

from tokenizers import ByteLevelBPETokenizer

ROOT = Path(__file__).resolve().parents[2]

# ONLY the end-of-text sentinel is special. "User:" and "Bot:" are deliberately
# left as ordinary text: the model has to *generate* "Bot:" itself, and special
# tokens are stripped on decode, so marking them special made every answer come
# back with the turn markers silently removed. BPE learns them as single units
# anyway - they are the most frequent strings in the corpus.
SPECIALS = ["<|endoftext|>"]


def corpus_files(paths):
    out = []
    for p in paths:
        p = Path(p)
        if p.is_dir():
            out += sorted(str(f) for f in p.glob("*.txt"))
        elif p.exists():
            out.append(str(p))
        else:
            raise SystemExit(f"no such corpus path: {p}")
    if not out:
        raise SystemExit("no corpus files found")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("corpus", nargs="+", help=".txt files or dirs of them")
    ap.add_argument("--vocab-size", type=int, default=4096)
    ap.add_argument("--min-frequency", type=int, default=2)
    ap.add_argument("--out", default=str(ROOT / "models_out/braino/tokenizer"))
    a = ap.parse_args()

    files = corpus_files(a.corpus)
    total = sum(Path(f).stat().st_size for f in files)
    print(f"training on {len(files)} files, {total/1e6:.1f} MB")

    tok = ByteLevelBPETokenizer()
    tok.train(files=files, vocab_size=a.vocab_size,
              min_frequency=a.min_frequency, special_tokens=SPECIALS)

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    tok.save_model(str(out))
    print(f"wrote {out}/vocab.json and {out}/merges.txt")
    print(f"vocab size: {tok.get_vocab_size()}")

    # A tokenizer is easy to get subtly wrong, so prove it round-trips before
    # anything is trained on it.
    checks = [
        "User: what is forty four plus forty two\nBot: Forty-four plus forty-two "
        "is eighty-six.<|endoftext|>",
        "User: what is the capital of france\nBot: Paris is the capital of France.",
        "User: how do you spell elephant\nBot: Elephant is spelled e l e p h a n t.",
        "User: tell me a joke about cows\nBot: Why did the cow go to space? "
        "To see the Moooon!",
    ]
    bad = 0
    lens = []
    for s in checks:
        enc = tok.encode(s)
        # skip_special_tokens=False: <|endoftext|> is part of the training
        # format and must survive the round trip, not be silently dropped.
        dec = tok.decode(enc.ids, skip_special_tokens=False)
        lens.append(len(enc.ids))
        if dec != s:
            bad += 1
            print("ROUND-TRIP FAILED")
            print(f"  in  {s!r}")
            print(f"  out {dec!r}")
    if bad:
        raise SystemExit(f"{bad} round-trip failure(s) - refusing to ship this tokenizer")
    print(f"round-trip OK on {len(checks)} samples, "
          f"{sum(lens)/len(lens):.1f} tokens average")


if __name__ == "__main__":
    main()
