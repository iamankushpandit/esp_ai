"""Structural paraphrases: change the SHAPE of a question, not its decoration.

Why this file exists. v7 raised paraphrases per fact from 14 to 20 and the
phrasing gap got *worse* - +53.3 to +55.0 points - while trained-wording
accuracy hit a perfect 100.0% and held-out accuracy fell to 42.5%. The model
memorised harder instead of generalising better.

The reason, as far as we can tell: `phrasing.py` varies decoration around a
fixed stem.

    what is the capital of france
    um what is the capital of france
    what's the capital of france please
    ok story whats the capital of france for me

Twenty of those are not twenty shapes; they are one shape wearing twenty hats.
The stem "what is the capital of X" is present in every single one, so more of
them makes that stem *more* memorable, not less load-bearing.

This module rewrites the stem itself:

    what is the capital of france
    which city is the capital of france
    france's capital is what city
    name the capital of france
    capital of france

Same fact, genuinely different syntax and word order. That is the variation the
held-out set is actually testing for.

Pools are split TRAIN / EVAL exactly as in phrasing.py, and asserted disjoint,
because the trap is identical: invent the same rewrite the eval reserves and
the score rises without the model improving.

This is an experiment with a real chance of failing. Two previous remedies for
the phrasing gap - floor balancing and more decoration - both made it worse.
"""
import re

# Each rule: (pattern, [replacement templates]). Groups from the pattern are
# available as \1, \2. Templates must stay lower case and punctuation-free.

TRAIN_STRUCTURES = [
    # "what is the capital of france" -> several genuinely different frames
    (r"^what is the (\w+) of (.+)$", [
        r"which \1 does \2 have",
        r"\2's \1 is what",
        r"name the \1 of \2",
        r"the \1 of \2 is what",
        r"\1 of \2",
        r"tell me the \1 of \2",
    ]),
    (r"^what is the (.+?) for (.+)$", [
        r"\2's \1 is what",
        r"name the \1 for \2",
        r"the \1 for \2 is what",
        r"which \1 goes with \2",
    ]),
    # "how many planets are there" -> counting frames
    (r"^how many (.+?) are there$", [
        r"the number of \1 is what",
        r"count the \1",
        r"how many \1 exist",
        r"give me the number of \1",
    ]),
    (r"^how many (.+?) (?:are|is) in (.+)$", [
        r"\2 has how many \1",
        r"the number of \1 in \2 is what",
        r"count the \1 in \2",
        r"how many \1 does \2 have",
    ]),
    (r"^how many (.+?) does (.+?) have$", [
        r"the number of \1 \2 has is what",
        r"count \2's \1",
        r"how many \1 are on \2",
        r"\2 has how many \1",
    ]),
    # "how do you spell elephant"
    (r"^how do you spell (.+)$", [
        r"spell \1",
        r"how is \1 spelled",
        r"what letters make \1",
        r"give me the letters in \1",
    ]),
    # "what is a noun" -> definition frames
    (r"^what is an? (.+)$", [
        r"what does \1 mean",
        r"explain \1",
        r"tell me what \1 means",
        r"describe \1",
    ]),
    # "what does huge mean"
    (r"^what does (.+?) mean$", [
        r"what is \1",
        r"explain \1",
        r"the meaning of \1 is what",
        r"describe \1",
    ]),
    # "how does a knight move"
    (r"^how does (?:a |an |the )?(.+?) move$", [
        r"what way does the \1 go",
        r"describe how the \1 moves",
        r"the \1 moves how",
        r"tell me the way a \1 moves",
    ]),
    # "what is the opposite of hot"
    (r"^what is the opposite of (.+)$", [
        r"the opposite of \1 is what",
        r"name the opposite of \1",
        r"which word is the opposite of \1",
        r"opposite of \1",
    ]),
    # "what is forty two plus sixteen"
    (r"^what is (.+?) plus (.+)$", [
        r"add \1 and \2",
        r"\1 plus \2 is what",
        r"what do you get adding \1 and \2",
        r"\1 and \2 added together",
    ]),
    (r"^what is (.+?) minus (.+)$", [
        r"subtract \2 from \1",
        r"\1 minus \2 is what",
        r"take \2 away from \1",
        r"what is left taking \2 from \1",
    ]),
    (r"^what is (.+?) times (.+)$", [
        r"multiply \1 and \2",
        r"\1 times \2 is what",
        r"what do you get multiplying \1 by \2",
        r"\1 lots of \2",
    ]),
    # "what is half of twelve"
    (r"^what is (half|a third|a quarter|a fifth|a sixth|an eighth) of (.+)$", [
        r"\1 of \2 is what",
        r"work out \1 of \2",
        r"split \2 and take \1",
        r"\1 of \2",
    ]),
    # --- frames measured as the most common in the actual corpus ---
    (r"^what continent is (.+?) in$", [
        r"which continent has \1",
        r"\1 is in which continent",
        r"name the continent \1 is in",
        r"which continent is \1 part of",
    ]),
    (r"^which continent is (.+?) in$", [
        r"what continent has \1",
        r"\1 sits in which continent",
        r"tell me the continent for \1",
    ]),
    (r"^what part of speech is (.+)$", [
        r"what word type is \1",
        r"\1 is what part of speech",
        r"tell me the part of speech for \1",
        r"which word class is \1",
    ]),
    (r"^is (.+?) a noun or a verb$", [
        r"what part of speech is \1",
        r"what word type is \1",
        r"is \1 a noun or verb",
    ]),
    (r"^what city is (.+?)'s capital$", [
        r"\1's capital city is what",
        r"name \1's capital city",
        r"which city runs \1",
    ]),
    (r"^where is (.+)$", [
        r"which country is \1 in",
        r"\1 is where",
        r"tell me where \1 is",
        r"locate \1",
    ]),
    (r"^define (.+)$", [
        r"what does \1 mean",
        r"explain \1",
        r"the meaning of \1 is what",
    ]),
    (r"^what is the meaning of (.+)$", [
        r"what does \1 mean",
        r"define \1",
        r"explain the word \1",
    ]),
    (r"^what number is (.+)$", [
        r"\1 is which number",
        r"tell me the number for \1",
        r"give me the number \1",
    ]),
    (r"^can you spell (.+?) for me$", [
        r"spell \1",
        r"how is \1 spelled",
        r"what letters make \1",
    ]),
    (r"^what kind of (.+?) is (.+)$", [
        r"\2 is what kind of \1",
        r"which type of \1 is \2",
        r"tell me what kind of \1 \2 is",
    ]),
    (r"^which country has (.+)$", [
        r"\1 belongs to which country",
        r"what country has \1",
        r"name the country with \1",
    ]),
    (r"^what is the atomic number of (.+)$", [
        r"\1's atomic number is what",
        r"how many protons does \1 have",
        r"name the atomic number of \1",
    ]),
    # "what is fifty percent of twenty"
    (r"^what is (.+?) percent of (.+)$", [
        r"\1 percent of \2 is what",
        r"work out \1 percent of \2",
        r"take \1 percent of \2",
        r"\1 percent of \2",
    ]),
]

# Reserved for evaluation. Never used to generate training data.
EVAL_STRUCTURES = [
    (r"^what is the (\w+) of (.+)$", [
        r"do you know \2's \1",
        r"could you name the \1 of \2",
    ]),
    (r"^how many (.+?) are there$", [
        r"could you count the \1",
        r"do you know how many \1 there are",
    ]),
    (r"^how do you spell (.+)$", [
        r"could you spell \1 for me",
        r"do you know how to spell \1",
    ]),
    (r"^what is an? (.+)$", [
        r"could you explain \1",
        r"do you know what \1 is",
    ]),
    (r"^how does (?:a |an |the )?(.+?) move$", [
        r"could you describe the \1 moving",
        r"do you know how the \1 moves",
    ]),
    (r"^what is (.+?) plus (.+)$", [
        r"could you add \1 and \2",
        r"do you know \1 plus \2",
    ]),
    (r"^what continent is (.+?) in$", [
        r"do you know which continent \1 is in",
        r"could you name \1's continent",
    ]),
    (r"^what part of speech is (.+)$", [
        r"do you know the part of speech for \1",
        r"could you tell me what word type \1 is",
    ]),
    (r"^where is (.+)$", [
        r"do you know where \1 is",
        r"could you tell me where \1 is",
    ]),
    (r"^define (.+)$", [
        r"could you define \1",
        r"do you know what \1 means",
    ]),
]

_BAD = re.compile(r"[^a-z0-9' ]")

# Contractions are the single biggest reason rules miss: the corpus writes
# "what's the capital of kentucky" 245 times and the rules expect "what is".
# Normalise before matching, never after - the OUTPUT should stay varied.
_CONTRACTIONS = [
    (r"^what's\b", "what is"), (r"^whats\b", "what is"),
    (r"^where's\b", "where is"), (r"^wheres\b", "where is"),
    (r"^who's\b", "who is"), (r"^how's\b", "how is"),
]


def _normalise(q):
    q = q.strip()
    for pat, rep in _CONTRACTIONS:
        q = re.sub(pat, rep, q)
    return q


def _degenerate(original, rewritten):
    """Reject rewrites that are empty, unchanged, or nonsense.

    The "which \\1 is the \\1 of \\2" template produces "which capital is the
    capital of france" - grammatical but silly. Rules that repeat a captured
    group next to itself are filtered here rather than removed from the table,
    because the same template is fine for other nouns.
    """
    r = rewritten.strip()
    if not r or r == original.strip():
        return True
    if _BAD.search(r):
        return True
    words = r.split()
    if len(words) < 2:
        return True
    # A word immediately repeated: "the the".
    for i in range(len(words) - 1):
        if words[i] == words[i + 1]:
            return True
    # A content word repeated anywhere when the original used it once. This
    # catches "which capital is the capital of france", which is grammatical
    # and useless - adjacency checks alone do not find it.
    ow = original.strip().split()
    for w in set(words):
        if len(w) > 3 and words.count(w) > max(1, ow.count(w)):
            return True
    return False


def restructure(q, eval_pool=False, limit=None):
    """-> list of structurally different forms of `q`. May be empty.

    Empty is normal and expected: most questions match no rule, and inventing
    a rewrite for them would mean guessing at grammar we cannot check.
    """
    table = EVAL_STRUCTURES if eval_pool else TRAIN_STRUCTURES
    norm = _normalise(q)
    out, seen = [], {q.strip(), norm}
    for pat, templates in table:
        m = re.match(pat, norm)
        if not m:
            continue
        for t in templates:
            try:
                r = m.expand(t)
            except re.error:
                continue
            r = re.sub(r"\s+", " ", r).strip()
            if _degenerate(norm, r) or r in seen:
                continue
            seen.add(r)
            out.append(r)
            if limit and len(out) >= limit:
                return out
    return out


def pools_disjoint():
    """No training rewrite may equal a reserved evaluation one."""
    tr = {t for _, ts in TRAIN_STRUCTURES for t in ts}
    ev = {t for _, ts in EVAL_STRUCTURES for t in ts}
    both = tr & ev
    return [f"templates overlap: {sorted(both)}"] if both else []


if __name__ == "__main__":
    bad = pools_disjoint()
    if bad:
        raise SystemExit("LEAK: " + "; ".join(bad))
    samples = [
        "what is the capital of france",
        "how many planets are there",
        "how do you spell elephant",
        "how does a knight move",
        "what is forty two plus sixteen",
        "what is half of twelve",
        "what is the opposite of hot",
        "what is a noun",
        "who painted the ceiling",          # matches nothing, by design
    ]
    hit = 0
    for q in samples:
        tr = restructure(q)
        ev = restructure(q, eval_pool=True)
        hit += bool(tr)
        print(f"\n{q}")
        for r in tr:
            print(f"   train  {r}")
        for r in ev:
            print(f"   EVAL   {r}")
    print(f"\n{hit}/{len(samples)} questions matched at least one rule")
    print("pools disjoint: OK")
