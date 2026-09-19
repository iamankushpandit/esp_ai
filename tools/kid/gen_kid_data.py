"""Generate a child-level (ages 5-8) Q&A fine-tuning set for TinyTalk.

Output (TinyTalk chat format, blank-line separated):
    User: what is three plus five
    Bot: Three plus five is eight.<|endoftext|>

Questions are written the way our speech recognizer outputs them (lower case,
numbers as words, no punctuation) plus some typed-style variants. Answers are
short, friendly, and spell numbers as words (the screen shows them and the
TTS speaks them).

Some question *templates* are reserved for evaluation only (never trained),
so the eval measures generalization to new phrasings, not recall.

  python tools/kid/gen_kid_data.py --out models_out/kid
"""
import argparse
import json
import random
from pathlib import Path

ONES = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
        "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
        "eighteen", "nineteen"]
TENS = ["", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"]


def w(n: int) -> str:
    if n < 20:
        return ONES[n]
    if n < 100:
        t, o = divmod(n, 10)
        return TENS[t] + ("" if o == 0 else "-" + ONES[o])
    if n == 100:
        return "one hundred"
    h, r = divmod(n, 100)
    return ONES[h] + " hundred" + ("" if r == 0 else " and " + w(r))


def cap(s: str) -> str:
    return s[0].upper() + s[1:]


def spoken(n: int) -> str:
    # STT writes "twenty one" (no hyphen)
    return w(n).replace("-", " ")


# ---------------------------------------------------------------- facts
# Each fact: (train_questions, eval_questions, answer)
FACTS = []


def fact(train, answer, evalq=()):
    FACTS.append((list(train), list(evalq), answer))


def arithmetic():
    for a in range(0, 21):
        for b in range(0, 21):
            if a + b > 30:
                continue
            A, B, S = spoken(a), spoken(b), w(a + b)
            fact([f"what is {A} plus {B}", f"what's {A} plus {B}", f"{A} plus {B}",
                  f"what is {A} add {B}", f"how much is {A} plus {B}", f"what is {a} + {b}",
                  f"can you add {A} and {B}"],
                 f"{cap(w(a))} plus {w(b)} is {S}.",
                 [f"hey story what does {A} plus {B} make", f"what do you get if you add {A} and {B}"])
    for a in range(0, 21):
        for b in range(0, a + 1):
            A, B, D = spoken(a), spoken(b), w(a - b)
            fact([f"what is {A} minus {B}", f"what's {A} minus {B}", f"{A} minus {B}",
                  f"what is {A} take away {B}", f"what is {a} - {b}",
                  f"what is {A} subtract {B}"],
                 f"{cap(w(a))} minus {w(b)} is {D}.",
                 [f"if i have {A} and take away {B} how many are left",
                  f"hey story what does {A} minus {B} make"])
    for a in range(0, 11):
        for b in range(0, 11):
            A, B, P = spoken(a), spoken(b), w(a * b)
            fact([f"what is {A} times {B}", f"what's {A} times {B}", f"{A} times {B}",
                  f"what is {A} multiplied by {B}", f"what is {a} x {b}",
                  f"what is {A} x {B}"],
                 f"{cap(w(a))} times {w(b)} is {P}.",
                 [f"hey story what does {A} times {B} make", f"can you multiply {A} and {B}"])
    for b in range(1, 11):
        for q in range(0, 11):
            a = b * q
            A, B, Q = spoken(a), spoken(b), w(q)
            fact([f"what is {A} divided by {B}", f"what's {A} divided by {B}",
                  f"{A} divided by {B}", f"what is {a} / {b}"],
                 f"{cap(w(a))} divided by {w(b)} is {Q}.",
                 [f"hey story what does {A} divided by {B} make",
                  f"if you share {A} cookies with {B} friends how many does each get"])


DAYS = ["monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"]
MONTHS = ["january", "february", "march", "april", "may", "june", "july", "august",
          "september", "october", "november", "december"]


def calendar():
    D = [cap(d) for d in DAYS]
    fact(["how many days are in a week", "how many days in a week", "how many days does a week have"],
         "There are seven days in a week.", ["how long is a week"])
    fact(["what are the days of the week", "tell me the days of the week", "can you say the days of the week",
          "name the days of the week"],
         "The days of the week are " + ", ".join(D[:-1]) + " and " + D[-1] + ".",
         ["list all the days of the week"])
    fact(["what days are the weekend", "which days are the weekend", "what is the weekend"],
         "The weekend is Saturday and Sunday.", ["when is the weekend"])
    for i, d in enumerate(DAYS):
        n, p = DAYS[(i + 1) % 7], DAYS[(i - 1) % 7]
        fact([f"what day comes after {d}", f"what day is after {d}", f"which day comes after {d}"],
             f"{cap(n)} comes after {cap(d)}.", [f"if today is {d} what day is tomorrow"])
        fact([f"what day comes before {d}", f"what day is before {d}", f"which day comes before {d}"],
             f"{cap(p)} comes before {cap(d)}.", [f"if today is {d} what day was yesterday"])
    M = [cap(m) for m in MONTHS]
    fact(["how many months are in a year", "how many months in a year", "how many months does a year have"],
         "There are twelve months in a year.", ["how many months make a year"])
    fact(["what are the months of the year", "tell me the months of the year", "name the months of the year"],
         "The months are " + ", ".join(M[:-1]) + " and " + M[-1] + ".", ["list all the months"])
    fact(["what is the first month of the year", "which month is first", "what month does the year start with"],
         "January is the first month of the year.", ["what month comes first in the year"])
    fact(["what is the last month of the year", "which month is last", "what month does the year end with"],
         "December is the last month of the year.", ["what month comes last in the year"])
    for i, m in enumerate(MONTHS):
        n, p = MONTHS[(i + 1) % 12], MONTHS[(i - 1) % 12]
        fact([f"what month comes after {m}", f"what month is after {m}", f"which month comes after {m}"],
             f"{cap(n)} comes after {cap(m)}.", [f"what is the month after {m}"])
        fact([f"what month comes before {m}", f"what month is before {m}", f"which month comes before {m}"],
             f"{cap(p)} comes before {cap(m)}.", [f"what is the month before {m}"])
    days_in = {"january": 31, "february": 28, "march": 31, "april": 30, "may": 31, "june": 30, "july": 31,
               "august": 31, "september": 30, "october": 31, "november": 30, "december": 31}
    for m, n in days_in.items():
        extra = " In a leap year it has twenty-nine." if m == "february" else ""
        fact([f"how many days are in {m}", f"how many days does {m} have"],
             f"{cap(m)} has {w(n)} days.{extra}", [f"how long is {m}"])
    fact(["how many seasons are there", "how many seasons are in a year", "what are the seasons",
          "name the four seasons"],
         "There are four seasons: spring, summer, autumn and winter.", ["tell me the seasons"])
    seasons = ["spring", "summer", "autumn", "winter"]
    for i, s in enumerate(seasons):
        n = seasons[(i + 1) % 4]
        fact([f"what season comes after {s}", f"which season is after {s}", f"what comes after {s}"],
             f"{cap(n)} comes after {s}.", [f"what is the season after {s}"])
    fact(["which season is the hottest", "what is the hottest season"], "Summer is the hottest season.",
         ["when is it hot"])
    fact(["which season is the coldest", "what is the coldest season"], "Winter is the coldest season.",
         ["when is it cold"])
    fact(["what is fall", "what is autumn", "when do leaves fall off trees"],
         "Autumn, or fall, is the season when leaves change color and fall off trees.", ["tell me about autumn"])
    fact(["how many days are in a year", "how many days in a year"],
         "There are three hundred sixty-five days in a year.", ["how long is a year in days"])
    fact(["how many weeks are in a year", "how many weeks in a year"],
         "There are fifty-two weeks in a year.", ["how long is a year in weeks"])
    fact(["how many hours are in a day", "how many hours in a day"], "There are twenty-four hours in a day.",
         ["how long is a day"])
    fact(["how many minutes are in an hour", "how many minutes in an hour"],
         "There are sixty minutes in an hour.", ["how long is an hour"])
    fact(["how many seconds are in a minute", "how many seconds in a minute"],
         "There are sixty seconds in a minute.", ["how long is a minute"])
    fact(["how many days are in a month", "how many days in a month"],
         "Most months have thirty or thirty-one days. February has twenty-eight.", ["how long is a month"])


PLANETS = ["mercury", "venus", "earth", "mars", "jupiter", "saturn", "uranus", "neptune"]


def space():
    P = [cap(p) for p in PLANETS]
    fact(["how many planets are there", "how many planets are in our solar system", "how many planets are there in space"],
         "There are eight planets in our solar system.", ["how many planets go around the sun"])
    fact(["what are the planets", "name the planets", "tell me the planets", "what are the eight planets"],
         "The planets are " + ", ".join(P[:-1]) + " and " + P[-1] + ".", ["list the planets"])
    fact(["what planet do we live on", "which planet do we live on", "what is our planet called", "where do we live in space"],
         "We live on planet Earth.", ["what is the name of our planet"])
    fact(["what is the biggest planet", "which planet is the biggest", "what is the largest planet"],
         "Jupiter is the biggest planet.", ["which planet is the largest one"])
    fact(["what is the smallest planet", "which planet is the smallest"], "Mercury is the smallest planet.",
         ["which planet is the tiniest"])
    fact(["which planet is closest to the sun", "what planet is nearest to the sun"],
         "Mercury is the closest planet to the sun.", ["what planet is next to the sun"])
    fact(["which planet is the red planet", "what is the red planet", "why is mars red"],
         "Mars is called the red planet because its dirt is red and rusty.", ["tell me about mars"])
    fact(["which planet has rings", "what planet has big rings"], "Saturn has big beautiful rings of ice and rock.",
         ["what is special about saturn"])
    fact(["which planet is the hottest", "what is the hottest planet"], "Venus is the hottest planet.",
         ["what planet is really hot"])
    for i, p in enumerate(PLANETS):
        fact([f"what number planet is {p}", f"where is {p} from the sun"],
             f"{cap(p)} is planet number {w(i + 1)} from the sun.", [f"how far from the sun is {p} in the order"])
    fact(["is the moon a planet", "what is the moon", "is the moon a star"],
         "The moon is not a planet. It is a big ball of rock that goes around the Earth.", ["tell me about the moon"])
    fact(["is the sun a star", "what is the sun", "is the sun a planet"],
         "The sun is a star. It is a giant ball of hot glowing gas.", ["tell me about the sun"])
    fact(["why is the sun hot", "why is the sun so hot"],
         "The sun is hot because it is a huge star that makes heat and light.", ["what makes the sun hot"])
    fact(["what are stars", "what is a star"], "Stars are giant balls of hot gas, very far away. The sun is a star.",
         ["tell me about stars"])
    fact(["why does the moon change shape", "why does the moon look different"],
         "The moon looks different because we see different parts of it lit up by the sun.",
         ["why is the moon sometimes round"])
    fact(["why is it dark at night", "why is night dark", "why does it get dark"],
         "It is dark at night because our side of the Earth turns away from the sun.", ["where does the sun go at night"])
    fact(["who goes to space", "what is an astronaut"], "An astronaut is a person who travels to space in a rocket.",
         ["who flies rockets"])
    fact(["how long does the earth take to go around the sun", "how long is one trip around the sun"],
         "The Earth goes around the sun once every year.", ["how often does earth go around the sun"])


def colors():
    mixes = [("blue", "yellow", "green"), ("red", "yellow", "orange"), ("red", "blue", "purple"),
             ("red", "white", "pink"), ("black", "white", "gray")]
    for a, b, c in mixes:
        for x, y in ((a, b), (b, a)):
            fact([f"what color do you get when you mix {x} and {y}", f"what does {x} and {y} make",
                  f"what color is {x} and {y} mixed", f"what do you get if you mix {x} and {y}"],
                 f"{cap(x)} and {y} make {c}.", [f"if i mix {x} paint with {y} paint what color do i get"])
    fact(["what are the colors of the rainbow", "what colors are in a rainbow", "name the rainbow colors"],
         "The rainbow colors are red, orange, yellow, green, blue, indigo and violet.", ["tell me the rainbow colors"])
    fact(["what are the primary colors", "what are the three primary colors"], "The primary colors are red, yellow and blue.",
         ["which colors are primary"])
    things = [("the sky", "blue"), ("grass", "green"), ("a banana", "yellow"), ("snow", "white"),
              ("a strawberry", "red"), ("an orange", "orange"), ("the sun", "yellow"), ("a carrot", "orange"),
              ("coal", "black"), ("a leaf", "green"), ("a tomato", "red"), ("milk", "white"),
              ("chocolate", "brown"), ("a lemon", "yellow"), ("the sea", "blue")]
    for t, c in things:
        fact([f"what color is {t}", f"what colour is {t}"], f"{cap(t)} is {c}.", [f"tell me the color of {t}"])


def shapes_letters_counting():
    shapes = [("triangle", 3), ("square", 4), ("rectangle", 4), ("pentagon", 5), ("hexagon", 6), ("octagon", 8)]
    for s, n in shapes:
        fact([f"how many sides does a {s} have", f"how many sides has a {s}", f"how many corners does a {s} have"],
             f"A {s} has {w(n)} sides.", [f"count the sides of a {s}"])
        fact([f"what shape has {spoken(n)} sides", f"which shape has {spoken(n)} sides"] if s not in ("rectangle",) else [],
             f"A {s} has {w(n)} sides." if s != "square" else "A square has four equal sides. A rectangle also has four sides.",
             [f"name a shape with {spoken(n)} sides"] if s not in ("rectangle",) else [])
    fact(["how many sides does a circle have", "does a circle have corners"],
         "A circle is round. It has no corners and no straight sides.", ["is a circle pointy"])
    fact(["how many letters are in the alphabet", "how many letters are there in the alphabet"],
         "There are twenty-six letters in the alphabet.", ["how many letters does the alphabet have"])
    fact(["what is the first letter of the alphabet", "what letter comes first"], "A is the first letter of the alphabet.",
         ["which letter starts the alphabet"])
    fact(["what is the last letter of the alphabet", "what letter comes last"], "Z is the last letter of the alphabet.",
         ["which letter ends the alphabet"])
    fact(["what are the vowels", "which letters are vowels"], "The vowels are A, E, I, O and U.", ["name the vowels"])
    letters = "abcdefghijklmnopqrstuvwxyz"
    for i, L in enumerate(letters):
        if i + 1 < 26:
            fact([f"what comes after the letter {L}", f"what letter comes after {L}", f"what letter is after {L}"],
                 f"{letters[i + 1].upper()} comes after {L.upper()}.", [f"which letter follows {L}"])
        if i > 0:
            fact([f"what comes before the letter {L}", f"what letter comes before {L}"],
                 f"{letters[i - 1].upper()} comes before {L.upper()}.", [f"which letter is just before {L}"])
    for n in (5, 10, 20):
        seq = ", ".join(cap(w(i)) if i == 1 else w(i) for i in range(1, n + 1))
        fact([f"can you count to {spoken(n)}", f"count to {spoken(n)}", f"please count to {spoken(n)}"],
             f"Sure! {seq}.", [f"let's count to {spoken(n)}"])
    for n in range(0, 100):
        fact([f"what number comes after {spoken(n)}", f"what comes after {spoken(n)}", f"what is after {spoken(n)}"],
             f"{cap(w(n + 1))} comes after {w(n)}.", [f"what number is one more than {spoken(n)}"])
        if n > 0:
            fact([f"what number comes before {spoken(n)}", f"what comes before {spoken(n)}"],
                 f"{cap(w(n - 1))} comes before {w(n)}.", [f"what number is one less than {spoken(n)}"])
    for n in range(0, 21):
        eo = "even" if n % 2 == 0 else "odd"
        fact([f"is {spoken(n)} even or odd", f"is {spoken(n)} an even number", f"is {spoken(n)} odd"],
             f"{cap(w(n))} is an {eo} number.", [f"tell me if {spoken(n)} is odd or even"])
    for a in range(0, 21, 2):
        for b in range(1, 21, 3):
            if a == b:
                continue
            big = max(a, b)
            fact([f"which is bigger {spoken(a)} or {spoken(b)}", f"what is bigger {spoken(a)} or {spoken(b)}"],
                 f"{cap(w(big))} is bigger.", [f"is {spoken(a)} more than {spoken(b)}"])


def animals_nature_body():
    legs = [("spider", 8), ("insect", 6), ("ant", 6), ("bee", 6), ("butterfly", 6), ("dog", 4), ("cat", 4),
            ("cow", 4), ("horse", 4), ("elephant", 4), ("bird", 2), ("chicken", 2), ("duck", 2), ("person", 2),
            ("octopus", 8), ("fish", 0), ("snake", 0), ("crab", 10)]
    for a, n in legs:
        art = "an" if a[0] in "aeiou" else "a"
        if a == "octopus":
            ans = "An octopus has eight arms."
        elif n == 0:
            ans = f"A {a} has no legs."
        else:
            ans = f"{cap(art)} {a} has {w(n)} legs."
        fact([f"how many legs does {art} {a} have", f"how many legs has {art} {a}", f"how many legs do {a}s have"],
             ans, [f"count the legs on {art} {a}"])
    sounds = [("cow", "moo"), ("dog", "woof"), ("cat", "meow"), ("duck", "quack"), ("sheep", "baa"),
              ("pig", "oink"), ("horse", "neigh"), ("lion", "roar"), ("owl", "hoot"), ("frog", "ribbit"),
              ("bee", "buzz"), ("chicken", "cluck"), ("mouse", "squeak"), ("snake", "hiss")]
    for a, s in sounds:
        art = "an" if a[0] in "aeiou" else "a"
        fact([f"what sound does {art} {a} make", f"what does {art} {a} say", f"what noise does {art} {a} make"],
             f"{cap(art)} {a} says {s}!", [f"how does {art} {a} sound"])
    babies = [("dog", "puppy"), ("cat", "kitten"), ("cow", "calf"), ("chicken", "chick"), ("sheep", "lamb"),
              ("lion", "cub"), ("bear", "cub"), ("horse", "foal"), ("frog", "tadpole"), ("duck", "duckling"),
              ("kangaroo", "joey"), ("goat", "kid")]
    for a, b in babies:
        fact([f"what is a baby {a} called", f"what do you call a baby {a}"],
             f"A baby {a} is called a {b}.", [f"what is the name for a baby {a}"])
    simple = [
        (["what is the biggest animal", "what is the largest animal", "what is the biggest animal in the world"],
         "The blue whale is the biggest animal in the world.", ["which animal is the biggest"]),
        (["what is the tallest animal", "which animal is the tallest"], "The giraffe is the tallest animal.",
         ["what animal has the longest neck"]),
        (["what is the fastest animal", "which animal is the fastest"], "The cheetah is the fastest animal on land.",
         ["what animal runs the fastest"]),
        (["what do cows give us", "what do we get from cows"], "Cows give us milk.", ["what comes from a cow"]),
        (["what do chickens give us", "what do we get from chickens"], "Chickens give us eggs.", ["what comes from a chicken"]),
        (["what do bees make", "what do bees give us"], "Bees make honey.", ["what comes from bees"]),
        (["what do sheep give us", "what do we get from sheep"], "Sheep give us wool to make warm clothes.",
         ["what comes from a sheep"]),
        (["where do fish live", "what do fish live in"], "Fish live in water, in rivers, lakes and the sea.",
         ["where can you find fish"]),
        (["where do birds live", "where do birds sleep"], "Birds live in nests in trees.", ["where is a bird's home"]),
        (["where do bees live"], "Bees live in a hive.", ["what is a bee's home"]),
        (["what do caterpillars turn into", "what does a caterpillar become"], "A caterpillar turns into a butterfly.",
         ["what happens to a caterpillar"]),
        (["what do pandas eat"], "Pandas eat bamboo.", ["what is a panda's favorite food"]),
        (["what do rabbits eat"], "Rabbits eat grass, hay and vegetables like carrots.", ["what food do bunnies like"]),
        (["are whales fish", "is a whale a fish"], "No, a whale is not a fish. It is a mammal that breathes air.",
         ["is a whale a mammal"]),
        (["where does rain come from", "why does it rain", "how does rain happen"],
         "Rain comes from clouds. Tiny water drops join together and fall down.", ["what makes rain"]),
        (["what is ice made of", "what is ice", "how is ice made"], "Ice is frozen water. Water turns to ice when it gets very cold.",
         ["how does water become ice"]),
        (["what is snow", "how is snow made"], "Snow is tiny pieces of ice that fall from cold clouds.",
         ["where does snow come from"]),
        (["what makes a rainbow", "how is a rainbow made", "why do rainbows happen"],
         "A rainbow happens when sunlight shines through raindrops and splits into colors.", ["where do rainbows come from"]),
        (["why is the sky blue", "why is the sky blue in the day", "what makes the sky blue"],
         "The sky is blue because sunlight bounces off the air, and blue light bounces the most.",
         ["how come the sky is blue"]),
        (["what makes thunder", "what is thunder"], "Thunder is the loud sound that lightning makes.",
         ["why is thunder so loud"]),
        (["what is wind", "what makes wind"], "Wind is moving air.", ["where does wind come from"]),
        (["what do plants need to grow", "how do plants grow"], "Plants need water, sunlight, air and soil to grow.",
         ["what helps a plant grow"]),
        (["why do leaves change color", "why do leaves turn orange", "why do leaves fall"],
         "In autumn leaves stop making green food, so their yellow, orange and red colors show.",
         ["why are leaves red in fall"]),
        (["why do we have to brush our teeth", "why should i brush my teeth", "why do we brush our teeth"],
         "We brush our teeth to keep them clean and stop holes called cavities.", ["what happens if i don't brush my teeth"]),
        (["why do we need to sleep", "why do we sleep", "why do i have to sleep"],
         "Sleep helps your body rest and grow, and helps your brain remember things.", ["what does sleep do"]),
        (["why should i eat vegetables", "why are vegetables good for you"],
         "Vegetables have vitamins that help you grow strong and stay healthy.", ["are vegetables healthy"]),
        (["why do we wash our hands", "why should i wash my hands"], "Washing hands gets rid of germs that can make us sick.",
         ["what does washing hands do"]),
        (["how many fingers do i have", "how many fingers do we have"], "You have ten fingers, five on each hand.",
         ["count my fingers"]),
        (["how many toes do i have", "how many toes do we have"], "You have ten toes, five on each foot.",
         ["how many toes are on my feet"]),
        (["what does the heart do", "what does my heart do"], "Your heart pumps blood all around your body.",
         ["why do we have a heart"]),
        (["how many bones are in my body", "how many bones do we have"], "A grown-up has two hundred and six bones.",
         ["how many bones does a person have"]),
        (["what do we breathe", "what do we need to breathe"], "We breathe air. Our lungs take in oxygen from the air.",
         ["why do we breathe"]),
        (["what is water made of"], "Water is made of tiny bits of hydrogen and oxygen.", ["what is in water"]),
        (["what is the biggest ocean"], "The Pacific Ocean is the biggest ocean.", ["which ocean is the largest"]),
        (["what is the tallest mountain", "what is the highest mountain"], "Mount Everest is the tallest mountain.",
         ["which mountain is the highest"]),
    ]
    for tr, a, ev in simple:
        fact(tr, a, ev)


def build():
    arithmetic()
    calendar()
    space()
    colors()
    shapes_letters_counting()
    animals_nature_body()


def styled(q: str, rng: random.Random) -> str:
    """Mostly STT style (as-is); sometimes typed style with capital + '?'."""
    r = rng.random()
    if r < 0.7:
        return q
    return cap(q) + "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="models_out/kid")
    ap.add_argument("--repeat", type=int, default=3, help="samples per training template")
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
    (out / "kid_train.txt").write_text("\n\n".join(train) + "\n", encoding="utf-8")
    (out / "kid_eval.jsonl").write_text("\n".join(json.dumps(e) for e in evals) + "\n", encoding="utf-8")
    print(f"facts {len(FACTS)}, train samples {len(train)}, eval questions {len(evals)}")


if __name__ == "__main__":
    main()
