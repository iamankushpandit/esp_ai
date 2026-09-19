"""Strip *computable* questions out of the training/eval data.

Measured on the first Mac run (tools/kid/eval_by_category.py), accuracy tracks
pool size inversely: geography with 682 facts scored 77%, arithmetic with
9,100 scored 22%. A 19.7M model cannot memorise ~14,000 arithmetic facts and
has no way to calculate, so it retrieves the nearest memorised string:

    "add forty four and forty two" -> "Forty-FIVE plus forty-two is eighty-six."

Every one of these categories has an exact algorithm that fits in a few dozen
lines of C on the device, where it is 100% right and costs zero model capacity:

    arithmetic      a + b, a - b, a * b, a / b
    word problems   same, wrapped in a sentence
    roman numerals  pure conversion both ways
    spelling        emit the letters of the word
    counting        count to N, skip count by N
    number sense    one more/less, double, half, odd/even, tens and ones

What is left is genuine knowledge - capitals, continents, animals, science,
the body, colors, shapes, jokes - which is what the model should spend its
parameters on.

  python tools/kid/filter_computable.py models_out/kid/gume_train.txt ...
"""
import argparse
import json
import re
from pathlib import Path

# Each rule is (label, regex tested against "question answer" lowercased).
RULES = [
    ("arithmetic", re.compile(
        r"\b(plus|minus|times|multiplied by|divided by|add|added|subtract|take away)\b"
        r"|[-+x/*]\s*\d|\d\s*[-+x/*]")),
    ("word problem", re.compile(
        r"\bin all\b|\baltogether\b|how many .* (did|does) \w+ (pick|have|find)"
        r"|if a box holds|then found|how many are left"
        # "gus gives seventy one of ninety three grapes away", "... sit on a
        # table and someone takes ...", "Eli has fifty-four shells left."
        r"|\bgives\b.*\baway\b|sits? on a table|stays? on the table"
        r"|\bhas \w[\w-]* (left|now)\b|how many are (left|still)")),
    ("roman numerals", re.compile(r"roman numeral")),
    ("spelling", re.compile(r"^spell |is spelled|what letters are in|how do you spell")),
    ("counting", re.compile(r"\bcount (to|by|in)\b|skip count")),
    ("number sense", re.compile(
        r"one more than|one less than|what comes (after|before) (zero|one|two|three|four|five|"
        r"six|seven|eight|nine|ten|eleven|twelve|thirteen|fourteen|fifteen|sixteen|seventeen|"
        r"eighteen|nineteen|twenty|thirty|forty|fifty|sixty|seventy|eighty|ninety|hundred)"
        r"|\bdouble \w+|half of|odd or even|odd number|even number|tens and ones"
        r"|which (is|number is) bigger")),
]


# Not computable, but SAT-level vocabulary ("acquiesce", "taciturn") is wasted
# capacity for a 5-8 year old. Optional because it is a judgement call, not a
# correctness one.
VOCAB_RULE = re.compile(
    r"part of speech|is an? (adjective|noun|verb|adverb)\b|sentence using|use the word"
    r"|\bmeans\b.*\bfor example\b|what does \w+ mean")


def computable(q, a, drop_vocab=False):
    s = f"{q} {a}".lower()
    for label, rx in RULES:
        if rx.search(s):
            return label
    if drop_vocab and VOCAB_RULE.search(s):
        return "advanced vocabulary"
    return None


def filter_train(path: Path, out: Path, drop_vocab=False):
    blocks = [b for b in path.read_text(encoding="utf-8").split("\n\n") if b.strip()]
    kept, dropped = [], {}
    for b in blocks:
        m = re.match(r"User: (.*)\nBot: (.*?)<\|endoftext\|>", b, re.S)
        if not m:
            kept.append(b)
            continue
        label = computable(m.group(1), m.group(2), drop_vocab)
        if label:
            dropped[label] = dropped.get(label, 0) + 1
        else:
            kept.append(b)
    out.write_text("\n\n".join(kept) + "\n", encoding="utf-8")
    return len(blocks), len(kept), dropped


def filter_eval(path: Path, out: Path, drop_vocab=False):
    rows = [json.loads(l) for l in path.read_text(encoding="utf-8").splitlines() if l.strip()]
    kept, dropped = [], {}
    for e in rows:
        label = computable(e["q"], e["a"], drop_vocab)
        if label:
            dropped[label] = dropped.get(label, 0) + 1
        else:
            kept.append(e)
    out.write_text("\n".join(json.dumps(e) for e in kept) + "\n", encoding="utf-8")
    return len(rows), len(kept), dropped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--drop-vocab", action="store_true",
                    help="also drop SAT-level vocabulary / part-of-speech facts")
    ap.add_argument("--suffix", default="_knowledge",
                    help="written next to the input, e.g. gume_train_knowledge.txt")
    a = ap.parse_args()
    for f in a.files:
        p = Path(f)
        out = p.with_name(p.stem + a.suffix + p.suffix)
        fn = filter_eval if p.suffix == ".jsonl" else filter_train
        total, kept, dropped = fn(p, out, a.drop_vocab)
        drop_s = ", ".join(f"{k} {v}" for k, v in sorted(dropped.items(), key=lambda x: -x[1]))
        print(f"{p.name:26s} {total:6d} -> {kept:6d} kept  ({100*kept/max(total,1):4.1f}%)"
              f"   dropped: {drop_s or 'none'}")
        print(f"{'':26s} wrote {out.name}")


if __name__ == "__main__":
    main()
