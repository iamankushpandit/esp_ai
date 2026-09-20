"""Score a fine-tuned checkpoint per question *category*.

The training log prints one blended accuracy over all eval files, which hides
which kinds of question actually work. This splits held-out questions into
categories (small arithmetic, two-digit arithmetic, word problems, Roman
numerals, vocabulary, geography, jokes, school topics...) and scores each.

  python tools/kid/eval_by_category.py --model models_out/kid/model_8m_kidgume \
      --eval models_out/kid/kid_eval.jsonl models_out/kid/gume_eval.jsonl -n 80
"""
import argparse
import json
import re
import collections
from pathlib import Path

import torch

ROOT = Path(__file__).resolve().parents[2]


def norm(s):
    return re.sub(r"[^a-z0-9 ]", " ", s.lower()).split()


# See finetune_kid.py: the old "key" metric compared only the last two words of
# the expected answer, so every word problem - all of which end "in all" -
# scored correct no matter what number the model said.
FILLER = {
    "a", "an", "the", "is", "are", "was", "were", "it", "its", "in", "on", "at",
    "of", "to", "and", "or", "that", "this", "there", "you", "your", "we", "i",
    "all", "so", "then", "do", "does", "did", "have", "has", "called", "makes",
    "make", "get", "gets", "be", "been", "will", "can", "for", "with", "by",
}


def content(words):
    return [w for w in words if w not in FILLER]


def category(q, a, source):
    s = (q + " " + a).lower()
    if source.startswith("joke"):
        return "jokes"
    if source.startswith("school"):
        return "school (new)"
    if "roman numeral" in s:
        return "roman numerals"
    if "spelled" in s or q.lower().startswith("spell"):
        return "spelling"
    if "part of speech" in s or re.search(r"is an? (adjective|noun|verb|adverb)", a.lower()):
        return "part of speech"
    if "sentence using" in s:
        return "sentence w/ word"
    if "continent" in s or "capital" in s or " country" in s:
        return "geography"
    nums = [int(x) for x in re.findall(r"\b(\d+)\b", a)]
    words = re.findall(r"\b(plus|minus|times|divided)\b", s)
    # Answers spell numbers as words, so a \d+ scan sees nothing and every
    # two-digit sum read as "small" - the balancer then starved the category
    # it most needed to fix. Detect spoken tens as well.
    big = bool(re.search(r"\b(hundred|thousand|twenty|thirty|forty|fifty|sixty|"
                         r"seventy|eighty|ninety)\b", a.lower()))
    if words or "in all" in s or "altogether" in s or " now" in s:
        wordy = not words          # "if a box holds..." style
        if wordy:
            return "word problems"
        return "arithmetic 2-digit" if big or any(n > 20 for n in nums) else "arithmetic small"
    return _knowledge_field(s, a.lower())


# "other" was 46,495 samples - over half the corpus in one unlabelled bucket.
# The goal is 90% in *all fields*, which cannot even be reported, let alone
# reached, while the largest field is called "other". Ordered most specific
# first; the first match wins.
_FIELDS = [
    ("periodic table", r"periodic table|atomic number|protons|chemical symbol"
                       r"|\bis the symbol\b|which element|element number"),
    ("vocabulary",     r"\bmeans\b.*for example|meaning of the word|what does \w{6,} mean"),
    ("space",          r"\bplanet|\bmoon\b|\bsun\b|solar system|\bstar\b|astronaut"
                       r"|rocket|galaxy|orbit|mercury|venus|mars|jupiter|saturn"
                       r"|uranus|neptune|pluto"),
    ("animals",        r"\banimal|\bbaby (\w+) is called|\bdog\b|\bcat\b|\bcow\b|\bbird"
                       r"|\bfish\b|\bhorse\b|\bsheep\b|\bfrog\b|\bbear\b|\blion\b"
                       r"|\binsect|\bspider|\bsnake\b|\bwhale|\bpenguin|\blegs does"),
    ("body",           r"\bbody\b|\bheart\b|\blungs?\b|\bbones?\b|\bbrain\b|\bteeth\b"
                       r"|\bskeleton\b|\bmuscle|\bsenses?\b|\bblood\b"),
    ("time & calendar", r"\bmonth|\bday of the week|\bweek\b|\byear\b|\bseason|\bclock"
                        r"|\bhour|\bminute|\bo'clock|yesterday|tomorrow|\bautumn|\bwinter"
                        r"|\bspring\b|\bsummer"),
    ("money",          r"\bcents?\b|\bdollar|\bpenny\b|\bnickel\b|\bdime\b|\bquarter\b"
                       r"|\bchange\b"),
    ("colors & shapes", r"\bcolou?r|\bshape|\bsquare\b|\btriangle\b|\bcircle\b"
                        r"|\brectangle\b|\bsides does|\brainbow"),
    ("letters & phonics", r"\bvowel|\bconsonant|\bletter\b|\balphabet|\brhyme|\bsyllable"
                          r"|\bplural\b|\bopposite of"),
    ("comparisons",    r"\bbigger\b|\bsmaller\b|\bmore than\b|\bless than\b|\bcomes (after|before)"),
    ("science & nature", r"\bplant|\bseed\b|\brain\b|\bcloud|\bwater\b|\bice\b|\bmagnet"
                         r"|\bfloat|\bliquid|\bsolid|\bgas\b|\bweather|\bshadow|\bgrow"),
    ("community & safety", r"\bdoctor\b|\bnurse\b|\bfirefighter|\bpolice\b|\bteacher\b"
                           r"|\bdentist\b|\bnine one one\b|\b911\b|\bsafe|\bmanners"),
]
_FIELDS = [(name, re.compile(rx)) for name, rx in _FIELDS]


def _knowledge_field(s, a):
    for name, rx in _FIELDS:
        if rx.search(s):
            return name
    return "other"


@torch.no_grad()
def answer(model, tok, q, max_new=40):
    ids = tok(f"User: {q}\nBot:", return_tensors="pt").input_ids.to(model.device)
    out = model.generate(ids, max_new_tokens=max_new, do_sample=False,
                         pad_token_id=tok.eos_token_id, eos_token_id=tok.eos_token_id)
    return tok.decode(out[0][ids.shape[1]:], skip_special_tokens=True).split("\n")[0].strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True)
    ap.add_argument("--eval", nargs="+", required=True)
    ap.add_argument("-n", type=int, default=80, help="questions sampled per category")
    ap.add_argument("--device", default="cpu")
    ap.add_argument("--seed", type=int, default=3)
    ap.add_argument("--show", type=int, default=2, help="example misses per category")
    a = ap.parse_args()

    import random
    from transformers import GPT2TokenizerFast, GPTNeoForCausalLM
    rng = random.Random(a.seed)

    buckets = collections.defaultdict(list)
    for f in a.eval:
        src = Path(f).name
        for line in Path(f).read_text(encoding="utf-8").splitlines():
            if line.strip():
                e = json.loads(line)
                # An explicit label from the generator beats guessing from
                # the text. The guesser put "what do you know about carbon"
                # in ANIMALS because the answer says "animal", which made two
                # fields look permanently broken when they were mislabelled.
                lab = e.get("topic")
                buckets[lab or category(e["q"], e["a"], src)].append(e)

    tok = GPT2TokenizerFast.from_pretrained(a.model)
    model = GPTNeoForCausalLM.from_pretrained(a.model).to(a.device).eval()

    print(f"model {a.model}  device {a.device}\n")
    print(f"{'category':20s} {'n':>5s} {'exact':>7s} {'key':>7s}   pool")
    print("-" * 60)
    overall_e = overall_k = overall_n = 0
    misses = {}
    for cat in sorted(buckets, key=lambda c: -len(buckets[c])):
        rows = buckets[cat]
        sample = rng.sample(rows, min(a.n, len(rows)))
        ex = ky = 0
        bad = []
        for e in sample:
            got = answer(model, tok, e["q"])
            g, want = norm(got), norm(e["a"])
            if g == want:
                ex += 1
            else:
                bad.append((e["q"], got, e["a"]))
            cw = content(want)
            if cw and all(wd in g for wd in cw):
                ky += 1
        n = len(sample)
        overall_e += ex
        overall_k += ky
        overall_n += n
        misses[cat] = bad
        print(f"{cat:20s} {n:5d} {100*ex/n:6.1f}% {100*ky/n:6.1f}%   {len(rows)}")
    print("-" * 60)
    print(f"{'WEIGHTED':20s} {overall_n:5d} {100*overall_e/overall_n:6.1f}% "
          f"{100*overall_k/overall_n:6.1f}%")

    if a.show:
        print("\nexample misses")
        for cat, bad in misses.items():
            for q, got, want in bad[:a.show]:
                print(f"  [{cat}] {q}\n      got  {got}\n      want {want}")


if __name__ == "__main__":
    main()
