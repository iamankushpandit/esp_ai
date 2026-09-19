"""Question phrasing: the measured bottleneck, and the leak it invites.

tools/kid/diag_phrasing.py on the v1 checkpoint, asking each fact both ways:

    trained wording   85.0% exact
    held-out wording  25.8% exact
    facts wrong both ways: 12.5%

So the model holds ~87% of the facts and loses them the moment the question is
worded differently. The fix is more phrasings per fact, not more parameters.

The trap: if augmentation invents the same wordings the eval holds out, the
score jumps without the model improving at all. The existing eval templates
start "hey story ..." - the single most natural prefix to augment with. So the
surface forms are split into two pools here, in one place, and a test asserts
they stay disjoint. TRAIN_* may be used to generate training data; EVAL_* is
reserved and must never be trained on.
"""
import random
import re

# Wake-word and lead-ins a child actually says. STT gives lower case, no
# punctuation, and sometimes a disfluency.
TRAIN_PREFIXES = [
    "", "", "", "",                      # most questions have no lead-in
    "um", "uh", "ok story", "okay story", "story",
    "i want to know", "i was wondering", "tell me", "please tell me",
    "do you know", "can you tell me", "quick question",
    "my teacher asked", "we learned about this",
]

# Reserved for evaluation. Never generate training data with these.
EVAL_PREFIXES = [
    "hey story", "hi story", "excuse me story",
    "i have a question", "help me with this one",
]

TRAIN_SUFFIXES = ["", "", "", "", "please", "thanks", "for me", "i forgot"]
EVAL_SUFFIXES = ["", "can you help", "i really want to know"]

# "what is" -> "what's" etc. STT writes contractions both ways.
TRAIN_REWRITES = [
    (r"^what is\b", ["what's", "whats", "what is"]),
    (r"^what are\b", ["what're", "what are"]),
    (r"^how do you\b", ["how do i", "how do we", "how do you"]),
    (r"^can you\b", ["could you", "will you", "can you"]),
    (r"^tell me\b", ["say", "tell me"]),
]

EVAL_REWRITES = [
    (r"^what is\b", ["what would you say is"]),
    (r"^how do you\b", ["how would you"]),
]


def _apply(q, rewrites, rng):
    for pat, opts in rewrites:
        if re.search(pat, q):
            return re.sub(pat, rng.choice(opts), q, count=1)
    return q


def variants(q, rng, n, eval_pool=False):
    """`n` distinct surface forms of question `q`.

    Only the question is varied - the answer is the fact and must not drift.
    """
    pre = EVAL_PREFIXES if eval_pool else TRAIN_PREFIXES
    suf = EVAL_SUFFIXES if eval_pool else TRAIN_SUFFIXES
    rew = EVAL_REWRITES if eval_pool else TRAIN_REWRITES

    out = []
    seen = set()
    for _ in range(n * 12):              # oversample, then dedupe
        if len(out) >= n:
            break
        s = _apply(q, rew, rng) if rng.random() < 0.5 else q
        p, x = rng.choice(pre), rng.choice(suf)
        s = " ".join(t for t in (p, s, x) if t)
        if rng.random() < 0.25:          # a typed-style variant
            s = s[0].upper() + s[1:] + "?"
        if s not in seen:
            seen.add(s)
            out.append(s)
    return out


def pools_disjoint():
    """No training surface form may equal a reserved evaluation one."""
    bad = []
    if set(p for p in TRAIN_PREFIXES if p) & set(EVAL_PREFIXES):
        bad.append("prefixes overlap")
    if set(s for s in TRAIN_SUFFIXES if s) & set(s for s in EVAL_SUFFIXES if s):
        bad.append("suffixes overlap")
    tr = {o for _, opts in TRAIN_REWRITES for o in opts}
    ev = {o for _, opts in EVAL_REWRITES for o in opts}
    if tr & ev:
        bad.append("rewrites overlap")
    return bad


if __name__ == "__main__":
    problems = pools_disjoint()
    if problems:
        raise SystemExit("LEAK: " + ", ".join(problems))
    rng = random.Random(0)
    q = "what is the capital of france"
    print("train:")
    for v in variants(q, rng, 8):
        print("   ", v)
    print("eval (reserved):")
    for v in variants(q, rng, 5, eval_pool=True):
        print("   ", v)
    print("\npools disjoint: OK")
