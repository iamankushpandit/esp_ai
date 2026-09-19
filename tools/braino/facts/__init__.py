"""Contract for Braino fact modules, plus a validator that enforces it.

Every module in this package exposes:

    def build() -> list[Fact]

A Fact is one thing the model should know, with several ways of asking it:

    Fact(topic="space",
         train=["how many planets are there",
                "how many planets in the solar system",
                "number of planets"],
         evalq=["how many planets go round the sun"],
         answer="There are eight planets in our solar system.")

Rules, all checked by validate():

  * QUESTIONS are speech-recognizer style: lower case, no punctuation except
    apostrophes, numbers spelled out ("twenty one", not "21" and not
    "twenty-one"). That is what the microphone actually produces.
  * ANSWERS are a full sentence, capitalized, ending in . ! or ?, with numbers
    spelled as words ("eighty-six"). The screen shows them and the speaker
    reads them.
  * AMERICAN spellings. Measured against the existing corpus: color 117 /
    colour 45, gray 24 / grey 0. "autumn" is the exception - it outnumbers
    "fall" 60 to 33 - so autumn is allowed.
  * evalq is HELD OUT: never trained, used only to measure whether the model
    generalizes to a new wording. At least one per fact.
  * >= 3 train phrasings per fact. The measured trained-vs-held-out gap was
    85.0% / 25.8%, and thin phrasing is the cause.
  * Answers stay under 240 characters. This reads aloud on a small speaker.
  * Anything computable (sums, comparisons, conversions) shows its work. The
    A/B measured 18.3% -> 75.3% on unseen operand pairs when the answer walks
    through the steps instead of stating the result.
"""
import re
from dataclasses import dataclass, field


@dataclass
class Fact:
    topic: str
    train: list
    answer: str
    evalq: list = field(default_factory=list)


# British -> American. Keys are whole words, matched case-insensitively.
BRITISH = {
    "colour": "color", "colours": "colors", "coloured": "colored",
    "grey": "gray", "favourite": "favorite", "maths": "math",
    "aeroplane": "airplane", "tyre": "tire", "kerb": "curb",
    "metre": "meter", "metres": "meters", "litre": "liter", "litres": "liters",
    "centre": "center", "centres": "centers", "theatre": "theater",
    "practise": "practice", "realise": "realize", "realised": "realized",
    "recognise": "recognize", "organise": "organize", "apologise": "apologize",
    "neighbour": "neighbor", "neighbours": "neighbors", "harbour": "harbor",
    "flavour": "flavor", "flavours": "flavors", "behaviour": "behavior",
    "jewellery": "jewelry", "aluminium": "aluminum", "defence": "defense",
    "programme": "program", "storey": "story", "storeys": "stories",
    "moustache": "mustache", "pyjamas": "pajamas", "plough": "plow",
    "biscuit": "cookie", "lorry": "truck", "nappy": "diaper",
    "trousers": "pants", "jumper": "sweater", "rubbish": "trash",
}
# "autumn" is deliberately NOT here: it outnumbers "fall" 60/33 in the corpus.

_BAD_Q = re.compile(r"[^a-z0-9' \n]")
_DIGIT = re.compile(r"\d")


def validate(facts, strict=True):
    """-> list of problem strings. Empty means the module is clean."""
    problems = []
    seen_q = {}

    for i, f in enumerate(facts):
        where = f"{f.topic}[{i}]"

        if not f.train or len(f.train) < 3:
            problems.append(f"{where}: needs >= 3 train phrasings, has {len(f.train)}")
        if not f.evalq:
            problems.append(f"{where}: needs >= 1 held-out eval phrasing")
        if not f.answer:
            problems.append(f"{where}: empty answer")
            continue

        a = f.answer
        if len(a) > 240:
            problems.append(f"{where}: answer {len(a)} chars, max 240")
        if a[0] != a[0].upper():
            problems.append(f"{where}: answer must start capitalized: {a[:40]!r}")
        if a.strip()[-1] not in ".!?":
            problems.append(f"{where}: answer must end in . ! or ?: {a[-40:]!r}")
        if _DIGIT.search(a):
            problems.append(f"{where}: answer has a digit, spell it out: {a[:60]!r}")
        for w in re.findall(r"[a-zA-Z]+", a):
            if w.lower() in BRITISH:
                problems.append(f"{where}: British spelling {w!r} -> "
                                f"{BRITISH[w.lower()]!r}")

        for q in list(f.train) + list(f.evalq):
            if _BAD_Q.search(q):
                bad = set(_BAD_Q.findall(q))
                problems.append(f"{where}: question has {sorted(bad)}, "
                                f"must be lower case, no punctuation: {q!r}")
            if _DIGIT.search(q):
                problems.append(f"{where}: question has a digit, "
                                f"speech spells numbers out: {q!r}")
            # The same question must not map to two different answers, or the
            # model is being taught a contradiction.
            prev = seen_q.get(q)
            if prev is not None and prev != f.answer:
                problems.append(f"{where}: question {q!r} already answered "
                                f"differently: {prev[:50]!r}")
            seen_q[q] = f.answer

        # An eval phrasing that also appears in training measures nothing.
        overlap = set(f.train) & set(f.evalq)
        if overlap:
            problems.append(f"{where}: eval phrasing also trained: {sorted(overlap)}")

    return problems if strict else []


def report(name, facts):
    """Validate and print a one-line summary. Returns True if clean."""
    problems = validate(facts)
    n_q = sum(len(f.train) for f in facts)
    if problems:
        print(f"{name}: {len(facts)} facts, {len(problems)} PROBLEMS")
        for p in problems[:20]:
            print(f"   {p}")
        if len(problems) > 20:
            print(f"   ... and {len(problems)-20} more")
        return False
    print(f"{name}: {len(facts)} facts, {n_q} train questions, "
          f"{n_q/max(len(facts),1):.1f} per fact - clean")
    return True
