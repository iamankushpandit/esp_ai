"""Generate age 5-8 (K-2) school Q&A for the TinyTalk 2 fine-tune.

gen_kid_data.py already covers arithmetic, calendar, space, colors, shapes,
letters, counting, animals, nature and the body. This file fills the rest of
the early-primary curriculum a 5-8 year old is actually asked at school:

  number sense  one more/one less, odd/even, ordinals, doubles, halves,
                skip counting, place value, comparing, before/after/between
  time & money  o'clock, half past, units of time, US coins
  science       states of matter, weather, seasons, plants, senses, magnets,
                floating, day/night, shadows, materials, living things
  geography     continents, oceans, directions
  language      vowels, consonants, opposites, rhyming, plurals, syllables
  social        community helpers, safety, manners

"one more / one less" is deliberately dense: the first Mac training run scored
0/4 on that question form at every epoch because it was absent from the data.

Output format matches gen_kid_data.py:  User: <q>\nBot: <a><|endoftext|>

  python tools/kid/gen_school_data.py --out models_out/kid
"""
import argparse
import json
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_kid_data import w, cap, spoken  # noqa: E402  (number words, shared)

FACTS = []


def fact(train, answer, evalq=()):
    FACTS.append((list(train), list(evalq), answer))


# ---------------------------------------------------------------- number sense
def number_sense():
    for n in range(0, 101):
        N = spoken(n)
        if n < 100:
            fact([f"what is one more than {N}", f"what's one more than {N}",
                  f"one more than {N}", f"what comes after {N}",
                  f"what number comes after {N}", f"what is {N} plus one more"],
                 f"One more than {w(n)} is {w(n + 1)}.",
                 [f"hey story what is one more than {N}",
                  f"if i have {N} and get one more how many is that"])
        if n > 0:
            fact([f"what is one less than {N}", f"what's one less than {N}",
                  f"one less than {N}", f"what comes before {N}",
                  f"what number comes before {N}", f"what number is one less than {N}"],
                 f"One less than {w(n)} is {w(n - 1)}.",
                 [f"hey story what is one less than {N}",
                  f"if i have {N} and lose one how many are left"])

    for n in range(0, 21):
        N, kind = spoken(n), "even" if n % 2 == 0 else "odd"
        fact([f"is {N} odd or even", f"is the number {N} odd or even",
              f"is {N} an even number", f"is {N} an odd number"],
             f"{cap(w(n))} is an {kind} number." if kind == "even"
             else f"{cap(w(n))} is an odd number.",
             [f"hey story is {N} odd or even"])

    ORD = ["first", "second", "third", "fourth", "fifth", "sixth", "seventh",
           "eighth", "ninth", "tenth"]
    for i, o in enumerate(ORD, start=1):
        fact([f"what is the {o} number", f"which number is {o}",
              f"what number is {o}"],
             f"The {o} number is {w(i)}.",
             [f"hey story which number is {o}"])

    for n in range(1, 21):
        N = spoken(n)
        fact([f"what is double {N}", f"what's double {N}", f"double {N}",
              f"what is {N} doubled", f"what is two times {N} doubled"],
             f"Double {w(n)} is {w(n * 2)}.",
             [f"hey story what is double {N}"])
    for n in range(1, 11):
        N = spoken(n * 2)
        fact([f"what is half of {N}", f"what's half of {N}", f"half of {N}",
              f"what is one half of {N}"],
             f"Half of {w(n * 2)} is {w(n)}.",
             [f"hey story what is half of {N}"])

    for step, name in ((2, "twos"), (5, "fives"), (10, "tens")):
        seq = ", ".join(w(step * i) for i in range(1, 11))
        fact([f"can you count by {name}", f"count by {name}",
              f"count in {name}", f"how do you count by {name}",
              f"skip count by {name}"],
             f"Sure! {cap(seq)}.",
             [f"hey story can you count by {name}"])

    for n in range(10, 100):
        if n % 10 == 0:
            continue
        N = spoken(n)
        fact([f"how many tens and ones in {N}", f"what is {N} in tens and ones",
              f"how many tens are in {N}"],
             f"{cap(w(n))} is {w(n // 10)} tens and {w(n % 10)} ones.",
             [f"hey story how many tens and ones are in {N}"])

    pairs = [(3, 7), (5, 2), (9, 4), (12, 20), (15, 8), (6, 6), (11, 19), (14, 4)]
    for a, b in pairs:
        A, B = spoken(a), spoken(b)
        if a == b:
            ans = f"{cap(w(a))} and {w(b)} are the same."
        else:
            big = w(max(a, b))
            ans = f"{cap(big)} is bigger."
        fact([f"which is bigger {A} or {B}", f"what is bigger {A} or {B}",
              f"which number is bigger {A} or {B}"],
             ans, [f"hey story which is bigger {A} or {B}"])


# ---------------------------------------------------------------- time & money
def time_money():
    fact(["how many hours are in a day", "how many hours in a day"],
         "There are twenty-four hours in a day.", ["hey story how many hours in one day"])
    fact(["how many minutes are in an hour", "how many minutes in an hour"],
         "There are sixty minutes in an hour.", ["hey story how many minutes in one hour"])
    fact(["how many seconds are in a minute", "how many seconds in a minute"],
         "There are sixty seconds in a minute.", ["hey story how many seconds in one minute"])
    fact(["how many minutes are in half an hour", "how many minutes in half an hour"],
         "There are thirty minutes in half an hour.", ["hey story how long is half an hour"])
    fact(["what do we use to tell the time", "what tells us the time"],
         "We use a clock to tell the time.", ["hey story what do we use to tell time"])
    fact(["how many hands does a clock have", "how many hands on a clock"],
         "A clock has two hands, a short one for hours and a long one for minutes.",
         ["hey story how many hands does a clock have"])

    for h in range(1, 13):
        H = spoken(h)
        fact([f"where is the big hand at {H} o'clock",
              f"what does the clock look like at {H} o'clock",
              f"what time is it when the clock says {H} o'clock"],
             f"At {w(h)} o'clock the big hand points at twelve and the little hand points at {w(h)}.",
             [f"hey story what happens at {H} o'clock"])

    COINS = [("penny", 1), ("nickel", 5), ("dime", 10), ("quarter", 25)]
    for name, v in COINS:
        fact([f"how much is a {name} worth", f"how many cents is a {name}",
              f"what is a {name} worth", f"how much is one {name}"],
             f"A {name} is worth {w(v)} cent{'s' if v > 1 else ''}.",
             [f"hey story how much is a {name} worth"])
    fact(["how many cents are in a dollar", "how many cents in a dollar"],
         "There are one hundred cents in a dollar.", ["hey story how many cents in one dollar"])
    for name, v in COINS[1:]:
        fact([f"how many pennies make a {name}", f"how many pennies in a {name}"],
             f"It takes {w(v)} pennies to make a {name}.",
             [f"hey story how many pennies equal a {name}"])
    fact(["how many quarters make a dollar", "how many quarters in a dollar"],
         "It takes four quarters to make a dollar.", ["hey story how many quarters equal a dollar"])
    fact(["how many dimes make a dollar", "how many dimes in a dollar"],
         "It takes ten dimes to make a dollar.", ["hey story how many dimes equal a dollar"])


# ---------------------------------------------------------------- science
def science():
    fact(["what are the three states of matter", "what are the states of matter"],
         "The three states of matter are solid, liquid and gas.",
         ["hey story name the three states of matter"])
    fact(["is ice a solid or a liquid", "what state of matter is ice"],
         "Ice is a solid. It is water that has frozen.", ["hey story is ice solid or liquid"])
    fact(["what happens to ice when it gets warm", "what happens when ice melts"],
         "When ice gets warm it melts and turns into water.",
         ["hey story what happens if ice gets hot"])
    fact(["what happens to water when it freezes", "what happens when water gets very cold"],
         "When water freezes it turns into ice.", ["hey story what does water become when it freezes"])
    fact(["what happens to water when it boils", "what happens when water gets very hot"],
         "When water boils it turns into steam, which is a gas.",
         ["hey story what does boiling water turn into"])
    fact(["what is a solid", "what does solid mean"],
         "A solid keeps its own shape, like a rock or a block.", ["hey story what is a solid"])
    fact(["what is a liquid", "what does liquid mean"],
         "A liquid flows and takes the shape of its cup, like water or milk.",
         ["hey story what is a liquid"])
    fact(["what is a gas", "what does gas mean"],
         "A gas spreads out to fill the space it is in, like the air around us.",
         ["hey story what is a gas"])

    fact(["what do plants need to grow", "what does a plant need"],
         "Plants need water, sunlight, air and soil to grow.",
         ["hey story what does a plant need to grow"])
    fact(["what are the parts of a plant", "name the parts of a plant"],
         "A plant has roots, a stem, leaves and often a flower.",
         ["hey story what parts does a plant have"])
    fact(["what do roots do", "what is the job of the roots"],
         "Roots hold the plant in the ground and drink up water.",
         ["hey story what do roots do for a plant"])
    fact(["what do leaves do", "what is the job of a leaf"],
         "Leaves catch sunlight and make food for the plant.",
         ["hey story what is a leaf for"])
    fact(["where do plants get their energy", "what gives plants energy"],
         "Plants get their energy from sunlight.", ["hey story where do plants get energy"])

    fact(["what are the five senses", "name the five senses"],
         "The five senses are sight, hearing, smell, taste and touch.",
         ["hey story what are my five senses"])
    for organ, sense in (("eyes", "see"), ("ears", "hear"), ("nose", "smell"),
                         ("tongue", "taste"), ("skin", "feel")):
        fact([f"what do we use our {organ} for", f"what are {organ} for"],
             f"We use our {organ} to {sense}.", [f"hey story what do my {organ} do"])

    fact(["what do magnets do", "what is a magnet"],
         "A magnet pulls things made of iron or steel towards it.",
         ["hey story what does a magnet do"])
    fact(["what things do magnets stick to", "what do magnets stick to"],
         "Magnets stick to things made of metal like iron and steel, but not to wood or plastic.",
         ["hey story what can a magnet pick up"])
    fact(["why do things float", "what makes something float"],
         "Things float when they are light for their size, and sink when they are heavy for their size.",
         ["hey story why does a boat float"])

    fact(["why do we have day and night", "what makes day and night"],
         "We have day and night because the Earth spins around.",
         ["hey story why does it get dark at night"])
    fact(["what makes a shadow", "how is a shadow made"],
         "A shadow is made when something blocks the light.",
         ["hey story how do shadows happen"])
    fact(["what are the four seasons", "name the four seasons"],
         "The four seasons are spring, summer, autumn and winter.",
         ["hey story what are the seasons called"])
    fact(["which season is the hottest", "what is the warmest season"],
         "Summer is the hottest season.", ["hey story which season is warmest"])
    fact(["which season is the coldest", "what is the coldest season"],
         "Winter is the coldest season.", ["hey story which season is coldest"])
    fact(["what do we use to measure temperature", "what measures how hot it is"],
         "We use a thermometer to measure temperature.",
         ["hey story what tells us how hot it is"])
    fact(["where does rain come from", "how does rain happen"],
         "Rain falls from clouds when the tiny drops of water in them get too heavy.",
         ["hey story how does rain form"])
    fact(["what is a rainbow made of", "how does a rainbow happen"],
         "A rainbow happens when sunlight shines through raindrops and splits into colors.",
         ["hey story how do rainbows form"])

    fact(["what do living things need", "what do all living things need"],
         "Living things need food, water, air and a place to live.",
         ["hey story what does a living thing need"])
    fact(["is a rock living or not living", "is a rock alive"],
         "A rock is not living. It does not grow, eat or breathe.",
         ["hey story is a rock a living thing"])


# ---------------------------------------------------------------- geography
def geography():
    fact(["how many continents are there", "how many continents are on earth"],
         "There are seven continents.", ["hey story how many continents are there"])
    fact(["what are the seven continents", "name the continents"],
         "The continents are Africa, Antarctica, Asia, Australia, Europe, "
         "North America and South America.",
         ["hey story can you name all the continents"])
    fact(["what is the biggest continent", "which continent is the largest"],
         "Asia is the biggest continent.", ["hey story which is the largest continent"])
    fact(["how many oceans are there", "how many oceans are on earth"],
         "There are five oceans.", ["hey story how many oceans are there"])
    fact(["what are the five oceans", "name the oceans"],
         "The oceans are the Pacific, Atlantic, Indian, Southern and Arctic.",
         ["hey story can you name the oceans"])
    fact(["what is the biggest ocean", "which ocean is the largest"],
         "The Pacific is the biggest ocean.", ["hey story which is the largest ocean"])
    fact(["what is the coldest place on earth", "where is the coldest place"],
         "Antarctica is the coldest place on Earth.", ["hey story where is the coldest place"])
    fact(["what are the four directions", "name the directions on a map"],
         "The four directions are north, south, east and west.",
         ["hey story what are the main directions"])
    fact(["which direction does the sun rise", "where does the sun come up"],
         "The Sun rises in the east.", ["hey story where does the sun rise"])
    fact(["which direction does the sun set", "where does the sun go down"],
         "The Sun sets in the west.", ["hey story where does the sun set"])
    fact(["what do we use to find places", "what shows us where places are"],
         "We use a map to find places.", ["hey story what helps us find a place"])


# ---------------------------------------------------------------- language
def language():
    fact(["what are the vowels", "name the vowels", "which letters are vowels"],
         "The vowels are a, e, i, o and u.", ["hey story can you tell me the vowels"])
    fact(["how many vowels are there", "how many vowels in the alphabet"],
         "There are five vowels.", ["hey story how many vowels are there"])
    fact(["what is a consonant", "what does consonant mean"],
         "A consonant is any letter that is not a vowel.", ["hey story what is a consonant"])
    fact(["how many consonants are there", "how many consonants in the alphabet"],
         "There are twenty-one consonants.", ["hey story how many consonants are there"])

    OPPOSITES = [("big", "small"), ("hot", "cold"), ("up", "down"), ("day", "night"),
                 ("fast", "slow"), ("happy", "sad"), ("wet", "dry"), ("old", "new"),
                 ("open", "closed"), ("hard", "soft"), ("long", "short"), ("full", "empty"),
                 ("loud", "quiet"), ("light", "dark"), ("in", "out"), ("over", "under"),
                 ("clean", "dirty"), ("high", "low"), ("push", "pull"), ("give", "take")]
    for a, b in OPPOSITES:
        fact([f"what is the opposite of {a}", f"what's the opposite of {a}",
              f"opposite of {a}"],
             f"The opposite of {a} is {b}.", [f"hey story what is the opposite of {a}"])
        fact([f"what is the opposite of {b}", f"what's the opposite of {b}",
              f"opposite of {b}"],
             f"The opposite of {b} is {a}.", [f"hey story what is the opposite of {b}"])

    RHYMES = [("cat", "hat"), ("dog", "log"), ("sun", "fun"), ("star", "car"),
              ("tree", "bee"), ("moon", "spoon"), ("book", "look"), ("cake", "lake"),
              ("bird", "word"), ("mouse", "house")]
    for a, b in RHYMES:
        fact([f"what rhymes with {a}", f"tell me a word that rhymes with {a}",
              f"what word rhymes with {a}"],
             f"{cap(b)} rhymes with {a}.", [f"hey story what rhymes with {a}"])

    PLURALS = [("cat", "cats"), ("dog", "dogs"), ("box", "boxes"), ("baby", "babies"),
               ("child", "children"), ("foot", "feet"), ("mouse", "mice"), ("tooth", "teeth"),
               ("man", "men"), ("woman", "women"), ("goose", "geese"), ("sheep", "sheep"),
               ("leaf", "leaves"), ("bus", "buses")]
    for one, many in PLURALS:
        ans = (f"More than one {one} is still called {many}." if one == many
               else f"More than one {one} is {many}.")
        fact([f"what is more than one {one}", f"what is the plural of {one}",
              f"how do you say more than one {one}"],
             ans, [f"hey story what is the plural of {one}"])

    SYLL = [("cat", 1), ("apple", 2), ("banana", 3), ("elephant", 3), ("dog", 1),
            ("rabbit", 2), ("butterfly", 3), ("computer", 3), ("sun", 1), ("table", 2)]
    for word, n in SYLL:
        fact([f"how many syllables are in {word}", f"how many syllables in {word}",
              f"how many beats in the word {word}"],
             f"The word {word} has {w(n)} syllable{'s' if n > 1 else ''}.",
             [f"hey story how many syllables does {word} have"])


# ---------------------------------------------------------------- social
def social():
    HELPERS = [("doctor", "helps people get better when they are sick"),
               ("nurse", "takes care of people who are sick or hurt"),
               ("teacher", "helps children learn at school"),
               ("firefighter", "puts out fires and keeps people safe"),
               ("police officer", "keeps people safe and helps them follow the rules"),
               ("farmer", "grows food and looks after animals"),
               ("dentist", "takes care of our teeth"),
               ("vet", "takes care of animals when they are sick"),
               ("chef", "cooks food for people"),
               ("mail carrier", "brings letters and parcels to our homes"),
               ("librarian", "looks after the books in the library"),
               ("pilot", "flies an airplane")]
    for who, does in HELPERS:
        fact([f"what does a {who} do", f"what is a {who}", f"what do {who}s do"],
             f"A {who} {does}.", [f"hey story what does a {who} do"])

    fact(["what number do you call in an emergency", "what is the emergency number",
          "who do you call if there is an emergency"],
         "In an emergency you call nine one one for help.",
         ["hey story what number is for emergencies"])
    fact(["what should you do before crossing the road", "how do you cross the road safely"],
         "Stop, look both ways and listen, and hold a grown up's hand.",
         ["hey story how do i cross the street safely"])
    fact(["what should you wear when you ride a bike", "what keeps you safe on a bike"],
         "You should always wear a helmet when you ride a bike.",
         ["hey story what do i wear to ride a bike"])
    fact(["why should you wash your hands", "when should you wash your hands"],
         "Washing your hands with soap washes away germs, especially before you eat.",
         ["hey story why do i need to wash my hands"])
    fact(["what do you say when someone gives you something", "what do you say to be polite"],
         "You say thank you.", ["hey story what should i say when someone helps me"])
    fact(["what do you say when you want something", "how do you ask politely"],
         "You say please.", ["hey story how do i ask for something nicely"])
    fact(["what do you say when you bump into someone", "what do you say if you make a mistake"],
         "You say sorry.", ["hey story what do i say when i do something wrong"])
    fact(["how many hours should a child sleep", "how much sleep do kids need"],
         "Most children need about ten hours of sleep each night.",
         ["hey story how much sleep should i get"])


def build():
    number_sense()
    time_money()
    science()
    geography()
    language()
    social()


def styled(q: str, rng: random.Random) -> str:
    return q if rng.random() < 0.7 else cap(q) + "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="models_out/kid")
    ap.add_argument("--repeat", type=int, default=3)
    ap.add_argument("--seed", type=int, default=7)
    a = ap.parse_args()
    rng = random.Random(a.seed)
    build()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    train, evals = [], []
    for tq, eq, ans in FACTS:
        for q in tq:
            for _ in range(a.repeat):
                train.append(f"User: {styled(q, rng)}\nBot: {ans}<|endoftext|>")
        for q in eq:
            evals.append({"q": q, "a": ans})
    rng.shuffle(train)
    (out / "school_train.txt").write_text("\n\n".join(train) + "\n", encoding="utf-8")
    (out / "school_eval.jsonl").write_text("\n".join(json.dumps(e) for e in evals) + "\n",
                                           encoding="utf-8")
    print(f"facts {len(FACTS)}, train samples {len(train)}, eval questions {len(evals)}")


if __name__ == "__main__":
    main()
