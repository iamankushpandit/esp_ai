"""Spelling data for ages 5-8, sized so the category can actually be learned.

Two measured problems with the existing spelling data:

1. Only 71 distinct words, against 73,687 arithmetic samples. Repeating those
   71 harder just overfits the phrasings - see tools/kid/balance_data.py, where
   spelling is the category that hits the repeat cap.

2. The answer format "F, U, N." costs 35% more tokens than "f u n." because
   each comma is its own token. For "elephant" that is 20 tokens instead of 13,
   and every extra token is another chance to derail.

Worth being clear about what the model can and cannot do here: the tokenizer
splits "elephant" into ['Ele', 'phant'], so the model never sees the letters.
Spelling is pure memorization for it - there is no rule to generalize. That
means coverage is the whole game: a word not in this list will be spelled
wrong, and no amount of training changes that. The list below is the Dolch
sight words plus common ages 5-8 nouns, which is the vocabulary a child of
that age would actually ask about.

  python tools/kid/gen_spelling_data.py --out models_out/kid
"""
import argparse
import json
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "training/story_kid_bundle/tools/kid"))
from gen_kid_data import cap  # noqa: E402

# Dolch sight words (pre-primer through third grade) - the words a 5-8 year
# old is explicitly taught to spell.
DOLCH = """
a and away big blue can come down find for funny go help here i in is it jump
little look make me my not one play red run said see the three to two up we
where yellow you all am are at ate be black brown but came did do eat four get
good have he into like must new no now on our out please pretty ran ride saw
say she so soon that there they this too under want was well went what white
who will with yes after again an any as ask by could every fly from give going
had has her him his how just know let live may of old once open over put round
some stop take thank them then think walk were when always around because been
before best both buy call cold does dont fast first five found gave goes green
its made many off or pull read right sing sit sleep tell their these those
upon us use very wash which why wish work would write your about better bring
carry clean cut done draw drink eight fall far full got grow hold hot hurt if
keep kind laugh light long much myself never only own pick seven shall show
six small start ten today together try warm
""".split()

# Common nouns a child of this age asks about.
NOUNS = """
apple ball bear bed bird boat book box boy bus cake car cat chair child clock
cloud coat corn cow cup dad desk dog door duck egg eye farm fish flag floor
flower foot frog game girl glass goat grass hand hat head hill home horse
house ice jar kite lake leaf leg lion man map milk moon mom mouse nest nose
nut owl pan park pen pig pin plant rabbit rain ring road rock roof room rope
sand school sea seed sheep ship shoe sister sky snow sock song star stone
storm street sun table tail tree truck water wind window wing winter wolf
worm yard zoo bread butter candy cheese chicken cookie dinner fruit grape
juice lunch orange pizza potato salad soup sugar banana carrot
elephant giraffe monkey penguin rabbit spider tiger turtle zebra dolphin
""".split()

TRAIN_Q = ["spell {w}", "how do you spell {w}", "can you spell {w}",
           "how do i spell {w}", "can you spell {w} for me",
           "what are the letters in {w}"]
EVAL_Q = ["hey story how do you spell {w}", "please spell the word {w}"]


def build():
    words, seen = [], set()
    for lst in (DOLCH, NOUNS):
        for word in lst:
            if word in seen:
                continue
            seen.add(word)
            words.append(word)
    return words


def answer(word):
    # "f u n", not "F, U, N" - same information, 35% fewer tokens.
    return f"{cap(word)} is spelled {' '.join(word)}."


def styled(q, rng):
    return q if rng.random() < 0.7 else cap(q) + "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "models_out/kid"))
    ap.add_argument("--repeat", type=int, default=2)
    ap.add_argument("--seed", type=int, default=11)
    a = ap.parse_args()

    rng = random.Random(a.seed)
    words = build()

    train, evals = [], []
    for word in words:
        ans = answer(word)
        for _ in range(a.repeat):
            for q in TRAIN_Q:
                train.append(f"User: {styled(q.format(w=word), rng)}\nBot: {ans}<|endoftext|>")
        for q in EVAL_Q:
            evals.append({"q": q.format(w=word), "a": ans})
    rng.shuffle(train)

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    tp, ep = out / "spelling_train.txt", out / "spelling_eval.jsonl"
    tp.write_text("\n\n".join(train) + "\n", encoding="utf-8")
    ep.write_text("\n".join(json.dumps(e) for e in evals) + "\n", encoding="utf-8")
    print(f"{len(words)} words  {len(train)} train samples  {len(evals)} eval questions")
    print(f"wrote {tp.name} and {ep.name}")


if __name__ == "__main__":
    main()
