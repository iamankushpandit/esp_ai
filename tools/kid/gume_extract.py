"""Turn every question the Braino! (Gume) games can ask into spoken-style Q/A
training pairs for the kid chat model.

Reads the Gume sources directly (.refs/Gume/src/games/*.cpp) wherever the
content is a table, so the output follows the game when the game changes, and
mirrors the generator rules (operand ranges, level tables) where the content is
procedural. Nothing in .refs/Gume is modified.

Output (same format as tools/kid/gen_kid_data.py):
    models_out/kid/gume_train.txt   blank-line separated
        User: what is the chemical symbol for gold
        Bot: The symbol for gold is Au.<|endoftext|>
    models_out/kid/gume_eval.jsonl  {"q":..., "a":..., "topic":...} held out

Questions are written the way the speech recognizer outputs text: lower case,
no punctuation, numbers as words. Answers are short, ASCII, numbers as words.

Anything gen_kid_data.py already trains (basic arithmetic 0-20, times tables to
10x10, days/months, planets, colours, shapes, alphabet ...) is removed here by
importing that script and dropping every question it already owns.

    tools/.venv/Scripts/python.exe tools/kid/gume_extract.py
    tools/.venv/Scripts/python.exe tools/kid/gume_extract.py --arith-phrasings 2 --word-problems 500
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import random
import re
import sys
import unicodedata
from collections import OrderedDict, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GUME = ROOT / ".refs" / "Gume"
GAMES = GUME / "src" / "games"

# ------------------------------------------------------------------ numbers
ONES = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
        "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
        "eighteen", "nineteen"]
TENS = ["", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"]
ORD = {1: "first", 2: "second", 3: "third", 4: "fourth", 5: "fifth", 6: "sixth", 7: "seventh",
       8: "eighth", 9: "ninth", 10: "tenth", 12: "twelfth", 20: "twentieth"}


def w(n: int) -> str:
    """Number in words, British 'and' style used by gen_kid_data: 'one hundred and five'."""
    if n < 0:
        return "minus " + w(-n)
    if n < 20:
        return ONES[n]
    if n < 100:
        t, o = divmod(n, 10)
        return TENS[t] + ("" if o == 0 else "-" + ONES[o])
    if n < 1000:
        h, r = divmod(n, 100)
        return ONES[h] + " hundred" + ("" if r == 0 else " and " + w(r))
    if n < 1_000_000:
        th, r = divmod(n, 1000)
        if r == 0:
            return w(th) + " thousand"
        return w(th) + " thousand" + (" and " if r < 100 else " ") + w(r)
    m, r = divmod(n, 1_000_000)
    return w(m) + " million" + ("" if r == 0 else " " + w(r))


def spoken(n: int) -> str:
    """How the STT writes a number: no hyphens."""
    return w(n).replace("-", " ")


def cap(s: str) -> str:
    return s[:1].upper() + s[1:] if s else s


def ascii_fold(s: str) -> str:
    s = s.replace("’", "'").replace("‘", "'").replace("“", '"').replace("”", '"')
    s = s.replace("—", ", ").replace("–", "-")
    s = "".join(c for c in unicodedata.normalize("NFKD", s) if not unicodedata.combining(c))
    return s.encode("ascii", "ignore").decode("ascii")


def numbers_to_words(s: str) -> str:
    """Spell out the digits that turn up in game text ('88 days', '-90C', '78%', '9.5')."""
    s = re.sub(r"-(\d+)C\b", lambda m: "minus " + w(int(m.group(1))) + " degrees Celsius", s)
    s = re.sub(r"(\d+)%", lambda m: w(int(m.group(1))) + " percent", s)
    s = re.sub(r"(\d+)\.5\b", lambda m: w(int(m.group(1))) + " and a half", s)
    s = re.sub(r"(\d+) km\b", lambda m: w(int(m.group(1))) + " kilometers", s)
    s = re.sub(r"\d+", lambda m: w(int(m.group(0))), s)
    return s


def norm_q(q: str) -> str:
    """Speech-recognizer style: lower case, numbers as words, no punctuation but apostrophes."""
    q = ascii_fold(q)
    q = numbers_to_words(q)
    q = q.lower().replace("-", " ")
    q = re.sub(r"[^a-z' ]+", " ", q)
    q = re.sub(r"\s+", " ", q).strip()
    return q


def norm_a(a: str) -> str:
    a = ascii_fold(a)
    a = numbers_to_words(a)
    a = re.sub(r"\s+", " ", a).strip()
    a = a.replace("..", ".")
    return a


def art(word: str) -> str:
    return "an" if word[:1].lower() in "aeiou" else "a"


def letters(sym: str) -> str:
    """How a child would say a symbol / numeral letter by letter: 'Au' -> 'a u'."""
    return " ".join(sym.lower())


# ------------------------------------------------------------------ source readers
def read(name: str) -> str:
    return (GAMES / name).read_text(encoding="utf-8", errors="replace")


SOURCES: dict[str, list[str]] = defaultdict(list)


def line_of(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def grab(game: str, fname: str, pattern: str, flags=re.DOTALL):
    """First regex match in a Gume source, recording file:line for the docs."""
    text = read(fname)
    m = re.search(pattern, text, flags)
    if not m:
        raise SystemExit(f"[gume_extract] pattern not found in {fname}: {pattern[:60]}")
    SOURCES[game].append(f"src/games/{fname}:{line_of(text, m.start())}")
    return m, text


def str_array(game: str, fname: str, name: str) -> list[str]:
    m, _ = grab(game, fname, name + r"\s*\[\]\s*=\s*\{(.*?)\};")
    return re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))


def int_array(game: str, fname: str, name: str) -> list[int]:
    m, _ = grab(game, fname, name + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};")
    return [int(x) for x in re.findall(r"\d+", m.group(1))]


# ------------------------------------------------------------------ fact store
class Store:
    def __init__(self):
        self.facts = []          # (game, [train q], answer, [eval q])

    def add(self, game, train, answer, evalq=()):
        tq = list(OrderedDict.fromkeys(norm_q(q) for q in train if q))
        if not evalq and tq:     # hand-written rule facts: hold out a generic rewording of the first
            evalq = ["i want to know " + tq[0]]
        eq = [norm_q(q) for q in evalq if q]
        eq = [q for q in OrderedDict.fromkeys(eq) if q not in tq]
        self.facts.append([game, tq, norm_a(answer), eq])


S = Store()


# ================================================================== MATH
def math_game(a):
    G = "math"
    names = str_array(G, "MathGame.cpp", r"const char\* const NAMES")
    things = str_array(G, "MathGame.cpp", r"const char\* const THINGS")
    m, _ = grab(G, "MathGame.cpp", r"void MathGame::newQuestion\(\)(.*?)\n\}")
    ranges = re.findall(r"left_ = random\((\d+), (\d+)\);\s*right_ = random\((\d+), (\d+)\);", m.group(1))
    # Arduino random(lo, hi) is lo..hi-1.
    levels = [((int(l0), int(l1) - 1), (int(r0), int(r1) - 1)) for l0, l1, r0, r1 in ranges]
    adds, subs = set(), set()
    for i, ((l0, l1), (r0, r1)) in enumerate(levels):
        for x in range(l0, l1 + 1):
            for y in range(r0, r1 + 1):
                adds.add((x, y))
                if i > 0:                                   # level 1 is addition only
                    subs.add((max(x, y), min(x, y)))
    P = a.arith_phrasings
    for x, y in sorted(adds):
        X, Y = spoken(x), spoken(y)
        tr = [f"what is {X} plus {Y}", f"what's {X} plus {Y}", f"{X} plus {Y}",
              f"how much is {X} plus {Y}", f"what is {X} add {Y}", f"can you add {X} and {Y}"][:P]
        S.add(G, tr, f"{cap(w(x))} plus {w(y)} is {w(x + y)}.",
              [f"what do you get when you add {X} and {Y}"])
    for x, y in sorted(subs):
        X, Y = spoken(x), spoken(y)
        tr = [f"what is {X} minus {Y}", f"what's {X} minus {Y}", f"{X} minus {Y}",
              f"what is {X} take away {Y}", f"how much is {X} minus {Y}", f"what is {X} subtract {Y}"][:P]
        S.add(G, tr, f"{cap(w(x))} minus {w(y)} is {w(x - y)}.",
              [f"if you take {Y} away from {X} what is left"])

    # Word problems: the game asks about a third of level 2-5 sums as a story.
    rng = random.Random(a.seed + 1)
    singular = {"cherries": "cherry", "leaves": "leaf", "pencils": "pencil"}

    def one(thing, n):
        return singular.get(thing, thing[:-1]) if n == 1 else thing

    story_add = [(x, y) for (x, y) in adds if x >= 2]
    story_sub = sorted(subs)
    n_add = a.word_problems // 2
    picks = [("+", p) for p in rng.sample(sorted(story_add), min(n_add, len(story_add)))]
    picks += [("-", p) for p in rng.sample(story_sub, min(a.word_problems - n_add, len(story_sub)))]
    for op, (x, y) in picks:
        who = rng.choice(names)
        other = rng.choice([n for n in names if n != who])
        thing = rng.choice(things)
        X, Y = w(x), w(y)
        t = rng.randrange(3)
        if op == "+":
            s = x + y
            if t == 0:
                tr = [f"{who} has {X} {thing} and finds {Y} more how many now",
                      f"{who} has {X} {thing} and finds {Y} more how many does {who} have now",
                      f"if {who} has {X} {thing} and finds {Y} more how many {thing} are there"]
                ev = [f"{who} had {X} {thing} then found {Y} more how many altogether"]
                ans = f"{who} has {w(s)} {one(thing, s)} now."
            elif t == 1:
                tr = [f"there are {X} {thing} in a box {Y} more go in how many in all",
                      f"a box has {X} {thing} and {Y} more go in how many are in the box",
                      f"there are {X} {thing} in a box and you put in {Y} more how many in all"]
                ev = [f"if a box holds {X} {thing} and {Y} more are added how many is that"]
                ans = f"There are {w(s)} {one(thing, s)} in all."
            else:
                tr = [f"{who} picks {X} {thing} on monday and {Y} on tuesday how many",
                      f"{who} picked {X} {thing} on monday and {Y} on tuesday how many altogether",
                      f"if {who} picks {X} {thing} on monday and {Y} on tuesday how many is that"]
                ev = [f"how many {thing} did {who} pick with {X} on monday and {Y} on tuesday"]
                ans = f"{who} picked {w(s)} {one(thing, s)} in all."
        else:
            d = x - y
            if t == 0:
                tr = [f"{who} has {X} {thing} and gives {Y} away how many are left",
                      f"if {who} has {X} {thing} and gives away {Y} how many are left",
                      f"{who} had {X} {thing} and gave {Y} away how many does {who} have"]
                ev = [f"{who} gives {Y} of {X} {thing} away how many are still there"]
                ans = f"{who} has {w(d)} {one(thing, d)} left."
            elif t == 1:
                tr = [f"{X} {thing} are on the table {Y} are taken away how many stay",
                      f"there are {X} {thing} on the table and {Y} are taken away how many stay",
                      f"if {Y} of the {X} {thing} on the table are taken how many are left"]
                ev = [f"{X} {thing} sit on a table and someone takes {Y} how many remain"]
                ans = f"{cap(w(d))} {one(thing, d)} stay on the table."
            else:
                tr = [f"{who} has {X} {thing} {other} has {Y} how many more has {who}",
                      f"{who} has {X} {thing} and {other} has {Y} how many more does {who} have",
                      f"if {who} has {X} {thing} and {other} has {Y} how many more has {who} got"]
                ev = [f"how many more {thing} does {who} have with {X} than {other} with {Y}"]
                ans = f"{who} has {w(d)} more {one(thing, d)} than {other}."
        S.add(G, tr, ans, ev)


# ================================================================== MULTIPLY
def multiply_game(a):
    G = "multiply"
    tables = set()
    for lv in range(1, 6):
        tables.update(int_array(G, "MultiplicationGame.cpp", rf"constexpr uint8_t LEVEL{lv}_TABLES"))
    m, _ = grab(G, "MultiplicationGame.cpp", r"random\(1, currentLevel <= 2 \? (\d+) : (\d+)\)")
    maxf = int(m.group(2)) - 1
    for t in sorted(tables):
        for f in range(1, maxf + 1):
            for x, y in ((t, f), (f, t)):
                X, Y = spoken(x), spoken(y)
                S.add(G, [f"what is {X} times {Y}", f"what's {X} times {Y}", f"{X} times {Y}",
                          f"what is {X} multiplied by {Y}"],
                      f"{cap(w(x))} times {w(y)} is {w(x * y)}.",
                      [f"can you multiply {X} by {Y}"])


# ================================================================== COLOR MIX
def colormix_game(a):
    G = "colormix"
    m, text = grab(G, "ColorMixGame.cpp", r"ColorMixDefinition MIXES\[\] = \{(.*?)\};")
    mixes = re.findall(r'\{"([^"]+)", "([^"]+)", "([^"]+)"', m.group(1))
    for x, y, c in mixes:
        for p, q in ((x, y), (y, x)):
            S.add(G, [f"what color do you get when you mix {p} and {q}", f"what does {p} and {q} make",
                      f"what color is {p} and {q} mixed", f"what do you get if you mix {p} and {q}"],
                  f"{cap(p)} and {q} make {c}.", [f"if i mix {p} paint with {q} paint what color do i get"])
        S.add(G, [f"what two colors make {c}", f"how do you make {c}", f"what colors mix to make {c}",
                  f"how do i make {c} paint"],
              f"Mix {x} and {y} to make {c}.", [f"which colors do i mix to get {c}"])


# ================================================================== ELEMENTS
def elements_game(a):
    G = "elements"
    m, _ = grab(G, "ElementDataTable.cpp", r"ELEMENT_CATEGORY_NAMES\[ELEMENT_CATEGORY_COUNT\] = \{(.*?)\};")
    cats = re.findall(r'"([^"]+)"', m.group(1))
    m, _ = grab(G, "ElementDataTable.cpp", r"ELEMENT_STATE_NAMES\[ELEMENT_STATE_COUNT\] = \{(.*?)\};")
    states = [s.lower() for s in re.findall(r'"([^"]+)"', m.group(1))]
    m, _ = grab(G, "ElementDataTable.cpp", r"const ElementFact ELEMENTS\[ELEMENT_COUNT\] = \{(.*?)\n\};")
    rows = re.findall(r'\{ "(\w+)", "([^"]+)", "((?:[^"\\]|\\.)*)", (\d+), (\d+), (\d+), (\d+), (\d+), (\d+) \}',
                      m.group(1))
    grab(G, "ElementsQuiz.cpp", r"case QType::SymbolToName:")
    catword = {"Alkaline earth": "alkaline earth metal", "Metal": "metal"}
    for sym, name, fact, z, col, row, cat, state, tier in rows:
        z, cat, state, tier = int(z), int(cat), int(state), int(tier)
        n = name.lower()
        sp = letters(sym)
        category = catword.get(cats[cat], cats[cat].lower())
        S.add(G, [f"what is the chemical symbol for {n}", f"what's the symbol for {n}",
                  f"what is the symbol for {n}", f"how do you write {n} on the periodic table"],
              f"The symbol for {n} is {sym}.", [f"what letters stand for {n}"])
        S.add(G, [f"which element is {sp}", f"what element has the symbol {sp}",
                  f"what does {sp} stand for on the periodic table"],
              f"{sym} is the symbol for {n}.", [f"what element uses the letters {sp}"])
        pr = "proton" if z == 1 else "protons"
        S.add(G, [f"how many protons does {n} have", f"what is the atomic number of {n}",
                  f"what number element is {n}", f"how many protons are in {n}"],
              f"{name} has {w(z)} {pr}, so its atomic number is {w(z)}.", [f"what's {n}'s atomic number"])
        S.add(G, [f"which element is number {spoken(z)}", f"what is element number {spoken(z)}",
                  f"what element has {spoken(z)} {pr}"],
              f"Element number {w(z)} is {n}.", [f"which element has atomic number {spoken(z)}"])
        fl = fact.rstrip(".")
        S.add(G, [f"tell me about {n}", f"what is {n}", f"tell me a fact about {n}"],
              f"{name} is {art(category)} {category}. {fact}", [f"what do you know about {n}"])
        S.add(G, [f"what element is this {fl}", f"guess the element {fl}", f"{fl} which element is it"],
              f"That is {n}.", [f"which element matches this clue {fl}"])
        S.add(G, [f"what kind of element is {n}", f"what type of element is {n}",
                  f"what group does {n} belong to"],
              f"{name} is {art(category)} {category}.", [f"is {n} a metal or not"])
        if tier <= 2:        # the game only asks state for tier 1-2 (ElementsQuiz.cpp)
            S.add(G, [f"is {n} a solid liquid or gas", f"what state is {n} at room temperature",
                      f"is {n} a solid or a gas", f"is {n} a liquid"],
                  f"{name} is {art(states[state])} {states[state]} at room temperature.",
                  [f"at room temperature is {n} a solid a liquid or a gas"])
    S.add(G, ["how many elements are there", "how many elements are on the periodic table",
              "how many elements are in the periodic table"],
          "There are one hundred and eighteen elements on the periodic table.",
          ["how many elements do we know about"])
    S.add(G, ["what is the periodic table", "what is a periodic table", "what does the periodic table show"],
          "The periodic table is a chart of all the elements, in order of how many protons they have.",
          ["tell me about the periodic table"])
    S.add(G, ["what is an element", "what are elements", "what is a chemical element"],
          "An element is a pure substance made of just one kind of atom, like gold or oxygen.",
          ["what does element mean"])
    S.add(G, ["what is a proton", "what are protons"],
          "A proton is a tiny particle in the middle of every atom. Each element has its own number of them.",
          ["tell me about protons"])


# ================================================================== FLAGS / COUNTRIES
COUNTRY_NAMES = {
    "AF": "Afghanistan", "AL": "Albania", "DZ": "Algeria", "AD": "Andorra", "AO": "Angola",
    "AG": "Antigua and Barbuda", "AR": "Argentina", "AM": "Armenia", "AU": "Australia", "AT": "Austria",
    "AZ": "Azerbaijan", "BS": "the Bahamas", "BH": "Bahrain", "BD": "Bangladesh", "BB": "Barbados",
    "BY": "Belarus", "BE": "Belgium", "BZ": "Belize", "BJ": "Benin", "BT": "Bhutan", "BO": "Bolivia",
    "BA": "Bosnia and Herzegovina", "BW": "Botswana", "BR": "Brazil", "BN": "Brunei", "BG": "Bulgaria",
    "BF": "Burkina Faso", "BI": "Burundi", "CV": "Cape Verde", "KH": "Cambodia", "CM": "Cameroon",
    "CA": "Canada", "CF": "the Central African Republic", "TD": "Chad", "CL": "Chile", "CN": "China",
    "CO": "Colombia", "KM": "Comoros", "CG": "the Republic of the Congo",
    "CD": "the Democratic Republic of the Congo", "CR": "Costa Rica", "CI": "Ivory Coast", "HR": "Croatia",
    "CU": "Cuba", "CY": "Cyprus", "CZ": "the Czech Republic", "DK": "Denmark", "DJ": "Djibouti",
    "DM": "Dominica", "DO": "the Dominican Republic", "EC": "Ecuador", "EG": "Egypt", "SV": "El Salvador",
    "GQ": "Equatorial Guinea", "ER": "Eritrea", "EE": "Estonia", "SZ": "Eswatini", "ET": "Ethiopia",
    "FJ": "Fiji", "FI": "Finland", "FR": "France", "GA": "Gabon", "GM": "the Gambia", "GE": "Georgia",
    "DE": "Germany", "GH": "Ghana", "GR": "Greece", "GD": "Grenada", "GT": "Guatemala", "GN": "Guinea",
    "GW": "Guinea-Bissau", "GY": "Guyana", "HT": "Haiti", "HN": "Honduras", "HU": "Hungary",
    "IS": "Iceland", "IN": "India", "ID": "Indonesia", "IR": "Iran", "IQ": "Iraq", "IE": "Ireland",
    "IL": "Israel", "IT": "Italy", "JM": "Jamaica", "JP": "Japan", "JO": "Jordan", "KZ": "Kazakhstan",
    "KE": "Kenya", "KI": "Kiribati", "KP": "North Korea", "KR": "South Korea", "KW": "Kuwait",
    "KG": "Kyrgyzstan", "LA": "Laos", "LV": "Latvia", "LB": "Lebanon", "LS": "Lesotho", "LR": "Liberia",
    "LY": "Libya", "LI": "Liechtenstein", "LT": "Lithuania", "LU": "Luxembourg", "MG": "Madagascar",
    "MW": "Malawi", "MY": "Malaysia", "MV": "the Maldives", "ML": "Mali", "MT": "Malta",
    "MH": "the Marshall Islands", "MR": "Mauritania", "MU": "Mauritius", "MX": "Mexico",
    "FM": "Micronesia", "MD": "Moldova", "MC": "Monaco", "MN": "Mongolia", "ME": "Montenegro",
    "MA": "Morocco", "MZ": "Mozambique", "MM": "Myanmar", "NA": "Namibia", "NR": "Nauru", "NP": "Nepal",
    "NL": "the Netherlands", "NZ": "New Zealand", "NI": "Nicaragua", "NE": "Niger", "NG": "Nigeria",
    "MK": "North Macedonia", "NO": "Norway", "OM": "Oman", "PK": "Pakistan", "PW": "Palau",
    "PA": "Panama", "PG": "Papua New Guinea", "PY": "Paraguay", "PE": "Peru", "PH": "the Philippines",
    "PL": "Poland", "PT": "Portugal", "QA": "Qatar", "RO": "Romania", "RU": "Russia", "RW": "Rwanda",
    "KN": "Saint Kitts and Nevis", "LC": "Saint Lucia", "VC": "Saint Vincent and the Grenadines",
    "WS": "Samoa", "SM": "San Marino", "ST": "Sao Tome and Principe", "SA": "Saudi Arabia",
    "SN": "Senegal", "RS": "Serbia", "SC": "Seychelles", "SL": "Sierra Leone", "SG": "Singapore",
    "SK": "Slovakia", "SI": "Slovenia", "SB": "the Solomon Islands", "SO": "Somalia",
    "ZA": "South Africa", "SS": "South Sudan", "ES": "Spain", "LK": "Sri Lanka", "SD": "Sudan",
    "SR": "Suriname", "SE": "Sweden", "CH": "Switzerland", "SY": "Syria", "TJ": "Tajikistan",
    "TZ": "Tanzania", "TH": "Thailand", "TL": "East Timor", "TG": "Togo", "TO": "Tonga",
    "TT": "Trinidad and Tobago", "TN": "Tunisia", "TR": "Turkey", "TM": "Turkmenistan", "TV": "Tuvalu",
    "UG": "Uganda", "UA": "Ukraine", "AE": "the United Arab Emirates", "GB": "the United Kingdom",
    "US": "the United States", "UY": "Uruguay", "UZ": "Uzbekistan", "VU": "Vanuatu", "VE": "Venezuela",
    "VN": "Vietnam", "YE": "Yemen", "ZM": "Zambia", "ZW": "Zimbabwe", "VA": "Vatican City",
    "PS": "Palestine",
}
COUNTRY_ALIASES = {"US": ["america", "the usa"], "GB": ["britain", "the uk"], "CZ": ["czechia"],
                   "AE": ["the uae"], "NL": ["holland"], "CI": ["the ivory coast"]}
# Name collisions with a US state: qualify both sides, the plain form is dropped as ambiguous.
COUNTRY_QUALIFIED = {"GE": "the country of georgia"}

# Flag descriptions for the tier 1 + 2 countries (the 62 the game's Easy/Medium pools draw from).
# The flag artwork lives in the external map-n-flag library, so these are written by hand.
FLAGS = {
    "US": "red and white stripes with a blue corner full of white stars",
    "GB": "blue with red and white crosses on top of each other, called the Union Jack",
    "FR": "three up-and-down stripes of blue, white and red",
    "DE": "three flat stripes of black, red and gold",
    "IT": "three up-and-down stripes of green, white and red",
    "ES": "red, then a wide yellow stripe, then red, with a coat of arms on the yellow",
    "CA": "red and white with a big red maple leaf in the middle",
    "MX": "green, white and red up-and-down stripes with an eagle on a cactus in the middle",
    "BR": "green with a yellow diamond and a blue starry globe in the middle",
    "AR": "light blue, white and light blue stripes with a sun in the middle",
    "CN": "red with five yellow stars, one big and four small",
    "JP": "white with a red circle in the middle",
    "KR": "white with a red and blue circle in the middle and four black bar patterns",
    "IN": "orange, white and green stripes with a blue wheel in the middle",
    "RU": "three flat stripes of white, blue and red",
    "AU": "blue with the Union Jack in the corner and white stars",
    "NZ": "blue with the Union Jack in the corner and four red stars",
    "EG": "red, white and black stripes with a golden eagle in the middle",
    "ZA": "a green sideways Y shape with red, blue, black, yellow and white",
    "NG": "three up-and-down stripes of green, white and green",
    "KE": "black, red and green stripes with thin white lines and a shield with two spears",
    "GR": "blue and white stripes with a white cross in the blue corner",
    "PT": "green and red with a coat of arms where the colors meet",
    "NL": "three flat stripes of red, white and blue",
    "CH": "a red square with a white cross in the middle",
    "SE": "blue with a yellow cross",
    "NO": "red with a blue cross outlined in white",
    "IE": "three up-and-down stripes of green, white and orange",
    "TR": "red with a white crescent moon and a white star",
    "TH": "red, white, a wide blue stripe, white and red",
    "BE": "three up-and-down stripes of black, yellow and red",
    "AT": "red, white and red flat stripes",
    "DK": "red with a white cross",
    "FI": "white with a blue cross",
    "PL": "white on top and red on the bottom",
    "UA": "blue on top and yellow on the bottom",
    "CL": "white and red with a blue square and a white star in the corner",
    "CO": "a wide yellow stripe on top, then blue, then red",
    "PE": "three up-and-down stripes of red, white and red",
    "ID": "red on top and white on the bottom",
    "VN": "red with a big yellow star in the middle",
    "PH": "blue and red with a white triangle holding a golden sun and three stars",
    "MY": "red and white stripes with a blue corner holding a yellow moon and star",
    "SG": "red on top and white on the bottom, with a white moon and five stars",
    "PK": "dark green with a white moon and star, and a white stripe on one side",
    "BD": "green with a big red circle",
    "MA": "red with a green five-pointed star in the middle",
    "ET": "green, yellow and red stripes with a blue circle and a yellow star",
    "GH": "red, gold and green stripes with a black star in the middle",
    "SA": "green with white Arabic writing and a white sword",
    "AE": "green, white and black stripes with a red band on one side",
    "IL": "white with two blue stripes and a blue Star of David",
    "IQ": "red, white and black stripes with green writing in the middle",
    "IR": "green, white and red stripes with a red emblem in the middle",
    "QA": "maroon with a white zigzag band on one side",
    "CU": "blue and white stripes with a red triangle and a white star",
    "JM": "a gold X with green triangles at the top and bottom and black ones at the sides",
    "IS": "blue with a red cross outlined in white",
    "HR": "red, white and blue stripes with a red and white checkered shield",
    "CZ": "white on top and red on the bottom, with a blue triangle on the side",
    "HU": "three flat stripes of red, white and green",
    "RO": "three up-and-down stripes of blue, yellow and red",
}
CONTINENT_FULL = {"N. America": "North America", "S. America": "South America"}


def flags_game(a):
    G = "flags"
    m, _ = grab(G, "CountryDataTable.cpp", r"CONTINENT_NAMES\[CONTINENT_COUNT\] = \{(.*?)\};")
    conts = [CONTINENT_FULL.get(c, c) for c in re.findall(r'"([^"]+)"', m.group(1))]
    m, _ = grab(G, "CountryDataTable.cpp", r"const CountryFact COUNTRY_FACTS\[\] = \{(.*?)\n\};")
    rows = re.findall(r'\{ "([A-Z]{2})", "([^"]+)", (\d+), (\d+) \}', m.group(1))
    grab(G, "FlagGame.cpp", r"Bonus! Capital of %s\?")
    for iso, capital, cont, tier in rows:
        name = COUNTRY_NAMES.get(iso)
        if name is None:
            print(f"[gume_extract] WARNING: no English name for {iso}, skipped", file=sys.stderr)
            continue
        refs = [COUNTRY_QUALIFIED.get(iso, name.lower())] + COUNTRY_ALIASES.get(iso, [])
        if iso in COUNTRY_QUALIFIED:
            refs.append(name.lower())      # still emitted; the collision pass drops it if a state shares it
        c0 = refs[0]
        capq = capital.lower().replace(".", "")
        tr = [f"what is the capital of {c0}", f"what's the capital city of {c0}",
              f"what city is the capital of {c0}", f"tell me the capital of {c0}"]
        tr += [f"what is the capital of {r}" for r in refs[1:]]
        S.add(G, tr, f"The capital of {name} is {capital}.", [f"which city is the capital of {c0}"])
        if capq != c0:
            S.add(G, [f"{capq} is the capital of which country", f"which country has {capq} as its capital",
                      f"what country is {capq} the capital of"],
                  f"{capital} is the capital of {name}.", [f"where is {capq} the capital of"])
        cn = conts[int(cont)]
        S.add(G, [f"what continent is {c0} in", f"which continent is {c0} on", f"where is {c0}",
                  f"what continent is {c0} on"],
              f"{cap(name)} is in {cn}.", [f"on which continent can you find {c0}"])
        if iso in FLAGS:
            S.add(G, [f"what does the flag of {c0} look like", f"what colors are on the flag of {c0}",
                      f"describe the flag of {c0}", f"what is the flag of {c0} like"]
                  + [f"what does the flag of {r} look like" for r in refs[1:]],
                  f"The flag of {name} is {FLAGS[iso]}.", [f"tell me about the flag of {c0}"])
    S.add(G, ["how many countries are there", "how many countries are in the world",
              "how many countries are there in the world"],
          "There are about one hundred and ninety-five countries in the world.",
          ["how many countries does the world have"])
    S.add(G, ["how many continents are there", "what are the continents", "name the continents"],
          "There are seven continents: Africa, Antarctica, Asia, Australia, Europe, North America and South America.",
          ["list the continents"])


# ================================================================== US STATES
STATE_QUALIFIED = {"Georgia": "the state of georgia", "Washington": "washington state"}


def states_game(a):
    G = "states"
    m, _ = grab(G, "StateData.cpp", r"const StateFact STATE_FACTS\[\] = \{(.*?)\n\};")
    rows = re.findall(r'\{ "([A-Z]{2})", "([^"]+)", "([^"]+)", (\d+) \}', m.group(1))
    grab(G, "StatesGame.cpp", r"What is its capital\?")
    SOURCES["stateflags/statemaps"].append("src/games/StateFlagGame.cpp (Bonus! Capital of ...)")
    for code, name, capital, tier in rows:
        s0 = STATE_QUALIFIED.get(name, name.lower())
        refs = [s0, name.lower()] if name in STATE_QUALIFIED else [s0]
        tr = [f"what is the capital of {s0}", f"what's the capital of {s0}",
              f"what city is the capital of {s0}", f"what is the state capital of {s0}"]
        tr += [f"what is the capital of {r}" for r in refs[1:]]
        S.add(G, tr, f"The capital of {name} is {capital}.", [f"which city is {s0}'s capital"])
        cq = capital.lower()
        S.add(G, [f"{cq} is the capital of which state", f"which state has {cq} as its capital",
                  f"what state is {cq} the capital of"],
              f"{capital} is the capital of {name}.", [f"which state's capital is {cq}"])
    S.add(G, ["how many states are there", "how many states are in america",
              "how many states does the united states have"],
          "The United States has fifty states.", ["how many states are in the usa"])
    S.add(G, ["what is the biggest state", "which state is the biggest", "what is the largest state"],
          "Alaska is the biggest state.", ["which us state is the largest"])


# ================================================================== GRE WORDS
POS = {"adj.": "an adjective", "n.": "a noun", "v.": "a verb", "adv.": "an adverb"}


def gre_game(a):
    G = "grewords"
    m, _ = grab(G, "GreWordTable.cpp", r"const GreWord GRE_WORDS\[\] = \{(.*?)\n\};")
    rows = re.findall(r'\{"([^"]+)", "([^"]+)", "([^"]+)", "((?:[^"\\]|\\.)*)"\}', m.group(1))
    for word, pos, gloss, ex in rows:
        ex = ex.replace('\\"', '"')
        gl = ("to " + gloss) if pos == "v." and not gloss.startswith("to ") else gloss
        S.add(G, [f"what does {word} mean", f"what is the meaning of {word}", f"define {word}",
                  f"what's the meaning of the word {word}"],
              f"{cap(word)} means {gl}. For example: {ex}", [f"can you tell me what {word} means"])
        S.add(G, [f"use {word} in a sentence", f"give me a sentence with {word}",
                  f"can you say a sentence using {word}"],
              ex, [f"how do you use the word {word}"])
        S.add(G, [f"is {word} a noun or a verb", f"what part of speech is {word}",
                  f"what kind of word is {word}"],
              f"{cap(word)} is {POS.get(pos, 'a word')}.", [f"is {word} an adjective"])


# ================================================================== SPACE QUIZ
def space_game(a):
    G = "space"
    m, _ = grab(G, "SpaceGame.cpp", r"const SpaceQuestion QUESTIONS\[\] = \{(.*?)\n\};")
    rows = re.findall(r'\{(\d), "([^"]+)",\s*\{"([^"]+)", "([^"]+)", "([^"]+)", "([^"]+)"\},\s*"([^"]+)"\}',
                      m.group(1))
    for tier, q, ans, _o1, _o2, _o3, expl in rows:
        base = q.rstrip("?")
        low = base[0].lower() + base[1:]
        S.add(G, [base, f"do you know {low}", f"can you tell me {low}", f"tell me {low}"],
              f"{ans}. {expl}", [f"quick question {low}"])


# ================================================================== FRACTIONS
FRAC_NAME = {2: ("half", "halves"), 3: ("third", "thirds"), 4: ("quarter", "quarters"),
             5: ("fifth", "fifths"), 6: ("sixth", "sixths"), 8: ("eighth", "eighths")}


def frac(n, d, fourth=False):
    if fourth and d == 4:
        s, p = "fourth", "fourths"
    else:
        s, p = FRAC_NAME[d]
    return f"{w(n)} {s if n == 1 else p}"


def fractions_game(a):
    G = "fractions"
    denoms = int_array(G, "FractionGame.cpp", r"constexpr uint8_t DENOMS_L5")
    fr = [(n, d) for d in denoms for n in range(1, d)]
    for n, d in fr:
        tr = [f"what fraction is {spoken(n)} out of {spoken(d)} slices",
              f"if a pizza has {spoken(d)} slices and i eat {spoken(n)} what fraction did i eat",
              f"how do you say {spoken(n)} out of {spoken(d)} as a fraction"]
        if d == 4:
            tr.append(f"what is {spoken(n)} out of four as a fraction in fourths")
        ans = f"{cap(w(n))} out of {w(d)} equal parts is {frac(n, d)}."
        if d == 4:
            ans = f"{cap(w(n))} out of four equal parts is {frac(n, d)}, also called {frac(n, d, True)}."
        S.add(G, tr, ans, [f"what do you call {spoken(n)} of {spoken(d)} equal parts"])
    for d in denoms:
        s, p = FRAC_NAME[d]
        S.add(G, [f"how many {p} make a whole", f"how many {p} are in a whole", f"how many {p} make one"],
              f"{cap(w(d))} {p} make a whole.", [f"how many {p} fill up a whole pie"])
    for i, (n1, d1) in enumerate(fr):
        for n2, d2 in fr[i + 1:]:
            f1, f2 = frac(n1, d1), frac(n2, d2)
            v1, v2 = n1 * d2, n2 * d1
            if v1 == v2:
                ans = f"{cap(f1)} and {f2} are the same size."
            else:
                big, small = (f1, f2) if v1 > v2 else (f2, f1)
                ans = f"{cap(big)} is bigger than {small}."
            S.add(G, [f"which is bigger {f1} or {f2}", f"what is bigger {f1} or {f2}",
                      f"which is more {f1} or {f2}"],
                  ans, [f"which fraction is larger {f1} or {f2}"])


# ================================================================== PERCENT
PCT_FRAC = {10: "one tenth", 20: "one fifth", 25: "one quarter", 30: "three tenths", 40: "two fifths",
            50: "one half", 60: "three fifths", 70: "seven tenths", 75: "three quarters",
            80: "four fifths", 90: "nine tenths", 100: "the whole thing"}


def percent_game(a):
    G = "percent"
    grab(G, "PercentCircleGame.cpp", r"static constexpr uint8_t L3\[\]")
    bases = int_array(G, "PercentCircleGame.cpp", r"static constexpr uint8_t BASE_NUMBERS")
    pcts = int_array(G, "PercentCircleGame.cpp", r"static constexpr uint8_t PERCENTS")
    for p, f in PCT_FRAC.items():
        P = spoken(p)
        S.add(G, [f"what is {P} percent as a fraction", f"how much of a circle is {P} percent",
                  f"if i shade {P} percent of a circle how much is shaded"],
              f"{cap(w(p))} percent is {f}.", [f"what fraction is {P} percent"])
    for p, f in ((25, "a quarter"), (50, "half"), (75, "three quarters"), (100, "the whole")):
        S.add(G, [f"what percent is {f}", f"what percent of a circle is {f}", f"how many percent is {f}"],
              f"{cap(f)} is {w(p)} percent.", [f"{f} is what percent"])
    S.add(G, ["what does percent mean", "what is a percent", "what is percent"],
          "Percent means out of one hundred. Fifty percent is fifty out of a hundred, which is half.",
          ["what does the word percent mean"])
    for b in bases:
        for p in pcts:
            v2 = b * p                       # hundredths
            whole, rem = divmod(v2, 100)
            if rem == 0:
                val = w(whole)
            elif rem == 50:
                val = f"{w(whole)} and a half"
            else:
                val = f"{w(whole)} point {w(rem)}"
            S.add(G, [f"what is {spoken(p)} percent of {spoken(b)}", f"what's {spoken(p)} percent of {spoken(b)}",
                      f"how much is {spoken(p)} percent of {spoken(b)}"],
                  f"{cap(w(p))} percent of {w(b)} is {val}.", [f"find {spoken(p)} percent of {spoken(b)}"])


# ================================================================== MONEY
COIN = {1: ("penny", "pennies"), 5: ("nickel", "nickels"), 10: ("dime", "dimes"),
        25: ("quarter", "quarters"), 50: ("half dollar", "half dollars")}


def money(c: int) -> str:
    d, r = divmod(c, 100)
    if d == 0:
        return f"{w(r)} cent" + ("" if r == 1 else "s")
    s = "one dollar" if d == 1 else f"{w(d)} dollars"
    return s + ("" if r == 0 else f" and {w(r)} cent" + ("" if r == 1 else "s"))


def money_q(c: int) -> str:
    d, r = divmod(c, 100)
    if d == 0:
        return f"{spoken(r)} {'cent' if r == 1 else 'cents'}"
    s = "one dollar" if d == 1 else f"{spoken(d)} dollars"
    return s + ("" if r == 0 else f" and {spoken(r)} {'cent' if r == 1 else 'cents'}")


def coins_q(k, v):
    s, p = COIN[v]
    return f"{'a' if k == 1 else spoken(k)} {s if k == 1 else p}"


def money_game(a):
    G = "money"
    values = int_array(G, "MoneyGame.cpp", r"constexpr uint8_t COIN_VALUES")
    m, _ = grab(G, "MoneyGame.cpp", r"const uint16_t payChoices\[\] = \{([^}]*)\}")
    pays = [int(x) for x in re.findall(r"\d+", m.group(1))]
    limit = 150
    for v in values:
        s, p = COIN[v]
        S.add(G, [f"how many cents is a {s} worth", f"how much is a {s}", f"what is a {s} worth",
                  f"how much money is a {s}"],
              f"A {s} is worth {money(v)}.", [f"what's the value of a {s}"])
        S.add(G, [f"which coin is worth {spoken(v)} {'cent' if v == 1 else 'cents'}",
                  f"what coin is {spoken(v)} {'cent' if v == 1 else 'cents'}",
                  f"what do you call a {spoken(v)} cent coin"],
              f"A {s} is worth {money(v)}.", [f"name the coin worth {spoken(v)} cents"])
        if v > 1:
            S.add(G, [f"how many pennies make a {s}", f"how many pennies are in a {s}",
                      f"how many cents make a {s}"],
                  f"{cap(w(v))} pennies make a {s}.", [f"how many pennies equal a {s}"])
        S.add(G, [f"how many {p} make a dollar", f"how many {p} are in a dollar",
                  f"how many {p} make one dollar"],
              f"{cap(w(100 // v))} {p if 100 // v != 1 else s} make a dollar.", [f"how many {p} equal a dollar"])
        for k in range(2, 9):
            if k * v > limit:
                break
            S.add(G, [f"how much are {spoken(k)} {p}", f"what do {spoken(k)} {p} make",
                      f"how many cents is {spoken(k)} {p}"],
                  f"{cap(w(k))} {p} make {money(k * v)}.", [f"what is the total of {spoken(k)} {p}"])
    for i, v1 in enumerate(values):
        for v2 in values[i + 1:]:
            for k1 in range(1, 4):
                for k2 in range(1, 4):
                    t = k1 * v1 + k2 * v2
                    if t > limit:
                        continue
                    a1, a2 = coins_q(k2, v2), coins_q(k1, v1)
                    S.add(G, [f"how much is {a1} and {a2}", f"what do {a1} and {a2} make",
                              f"how many cents is {a1} and {a2}"],
                          f"{cap(a1)} and {a2} make {money(t)}.",
                          [f"if i have {a1} and {a2} how much money is that"])
    for paid in pays:
        for price in range(1, paid):
            Pd, Pr = money_q(paid), money_q(price)
            S.add(G, [f"if something costs {Pr} and i pay {Pd} how much change do i get",
                      f"i pay {Pd} for a toy that costs {Pr} how much change",
                      f"what is my change from {Pd} if it costs {Pr}"],
                  f"Your change is {money(paid - price)}.",
                  [f"how much change from {Pd} when the price is {Pr}"])
    S.add(G, ["how many cents are in a dollar", "how many cents make a dollar", "how many cents is one dollar"],
          "One hundred cents make a dollar.", ["how many cents equal one dollar"])


# ================================================================== ROMAN
def to_roman(n: int, pieces) -> str:
    out = ""
    for v, s in pieces:
        while n >= v:
            out += s
            n -= v
    return out


def roman_game(a):
    G = "roman"
    m, _ = grab(G, "RomanGame.cpp", r"const RomanPiece PIECES\[\] = \{(.*?)\};")
    pieces = [(int(v), s) for v, s in re.findall(r'\{(\d+), "([A-Z]+)"\}', m.group(1))]
    ceil = int_array(G, "RomanGame.cpp", r"static const uint16_t CEILING")
    m, _ = grab(G, "RomanGame.cpp", r"\{\"I=1   V=5   X=10\",")
    nums = list(range(1, ceil[3] + 1)) + [n for n in range(ceil[3] + 1, ceil[4] + 1) if n % 50 == 0]
    nums += [ceil[4]]
    for n in sorted(set(nums)):
        r = to_roman(n, pieces)
        N = spoken(n)
        S.add(G, [f"how do you write {N} in roman numerals", f"what is {N} in roman numerals",
                  f"{N} in roman numerals"],
              f"{cap(w(n))} in Roman numerals is {r}.", [f"write {N} as a roman numeral"])
        rs = letters(r)
        tr = [f"what number is the roman numeral {rs}", f"what does roman numeral {rs} mean",
              f"what number is {rs} in roman numerals"]
        if len(r) > 1 and n <= 40:
            tr.append(f"what number is the roman numeral {r.lower()}")
        S.add(G, tr, f"{r} is {w(n)}.", [f"what's the value of the roman numeral {rs}"])
    for v, s in [(1, "I"), (5, "V"), (10, "X"), (50, "L"), (100, "C"), (500, "D"), (1000, "M")]:
        S.add(G, [f"what does {s.lower()} mean in roman numerals", f"what is {s.lower()} worth in roman numerals",
                  f"how much is the roman numeral {s.lower()}"],
              f"In Roman numerals, {s} means {w(v)}.", [f"what is the value of {s.lower()} in roman numerals"])
    tips = [
        (["how do roman numerals work", "how do you read roman numerals", "how do i read roman numerals"],
         "Add the letters up, biggest first. A smaller letter in front of a bigger one is taken away, so IV is four."),
        (["why is four i v", "why is four written as i v", "why is nine i x"],
         "A smaller letter before a bigger one is taken away. IV is one before five, and IX is one before ten."),
        (["can you write four i's in a row", "can you have four of the same roman numeral in a row",
          "is i i i i four"],
         "No. Never write four of the same letter in a row. Four is IV, not IIII."),
        (["which roman numerals can go in front to subtract", "what letters can you subtract in roman numerals",
          "which letters are put before to take away"],
         "Only I, X and C are ever put in front to take away."),
        (["what is the biggest roman numeral in the game", "what is the biggest number in roman numerals"],
         "With the usual letters the biggest is MMMCMXCIX, which is three thousand nine hundred and ninety-nine."),
        (["what are roman numerals", "who used roman numerals", "what is a roman numeral"],
         "Roman numerals are the letters the ancient Romans used for numbers, like I, V, X, L, C, D and M."),
    ]
    for tr, ans in tips:
        S.add(G, tr, ans, [])


# ================================================================== TIME
def hour_after(h):
    return 1 if h == 12 else h + 1


def time_words(h, mnt):
    if mnt == 0:
        return f"{w(h)} o'clock"
    if mnt == 15:
        return f"quarter past {w(h)}"
    if mnt == 30:
        return f"half past {w(h)}"
    if mnt == 45:
        return f"quarter to {w(hour_after(h))}"
    if mnt < 30:
        return f"{w(mnt)} minutes past {w(h)}" if mnt != 1 else f"one minute past {w(h)}"
    return f"{w(60 - mnt)} minutes to {w(hour_after(h))}"


def digital(h, mnt):
    if mnt == 0:
        return f"{w(h)} o'clock"
    if mnt < 10:
        return f"{w(h)} oh {w(mnt)}"
    return f"{w(h)} {w(mnt)}"


def time_game(a):
    G = "time"
    grab(G, "TimeGame.cpp", r"minute = random\(12\) \* 5;")
    for h in range(1, 13):
        for mnt in range(0, 60, 5):
            big = 12 if mnt == 0 else mnt // 5
            tw, dg = time_words(h, mnt), digital(h, mnt)
            ans = f"It is {tw}." if tw == dg else f"It is {tw}, or {dg}."
            S.add(G, [f"what time is it when the little hand is on {spoken(h)} and the big hand is on {spoken(big)}",
                      f"the short hand is on {spoken(h)} and the long hand is on {spoken(big)} what time is it",
                      f"if the hour hand points to {spoken(h)} and the minute hand points to {spoken(big)} what time is it"],
                  ans,
                  [f"big hand on {spoken(big)} and little hand on {spoken(h)} what time is that"])
            if mnt in (15, 30, 45):
                S.add(G, [f"what is another way to say {norm_q(dg)}", f"how else can you say {norm_q(dg)}",
                          f"is {norm_q(dg)} quarter past or half past"],
                      f"{cap(dg)} is also called {tw}.", [f"what's {norm_q(dg)} in words"])
    facts = [
        (["which hand shows the hour", "what does the little hand on a clock show", "what does the short hand show"],
         "The short hand is the hour hand. It shows the hour."),
        (["which hand shows the minutes", "what does the big hand on a clock show", "what does the long hand show"],
         "The long hand is the minute hand. It shows the minutes."),
        (["what does quarter past mean", "how many minutes is quarter past", "what is quarter past"],
         "Quarter past means fifteen minutes after the hour."),
        (["what does half past mean", "how many minutes is half past", "what is half past"],
         "Half past means thirty minutes after the hour."),
        (["what does quarter to mean", "how many minutes is quarter to", "what is quarter to"],
         "Quarter to means fifteen minutes before the next hour."),
        (["how many numbers are on a clock", "what numbers are on a clock face"],
         "A clock has the numbers one to twelve around its face."),
        (["how many minutes is each number on a clock", "how many minutes between the numbers on a clock"],
         "Each number on the clock is five minutes for the big hand."),
        (["what does o'clock mean", "what time is it when the big hand is on twelve"],
         "O'clock means the big hand is on twelve, right at the start of the hour."),
    ]
    for tr, ans in facts:
        S.add(G, tr, ans, [])


# ================================================================== SMALL ARITHMETIC GAMES
def numberline_game(a):
    G = "numberline"
    grab(G, "NumberLineGame.cpp", r"n1_\s*=\s*static_cast<uint8_t>\(1 \+ random\(5\)\)")
    for n1 in range(1, 6):
        for n2 in range(1, 5):
            S.add(G, [f"a frog starts at {spoken(n1)} and jumps {spoken(n2)} to the right where does it land",
                      f"on a number line if i start at {spoken(n1)} and hop {spoken(n2)} forward where am i",
                      f"start at {spoken(n1)} on the number line and jump {spoken(n2)} right"],
                  f"You land on {w(n1 + n2)}.",
                  [f"if the frog is on {spoken(n1)} and hops {spoken(n2)} spaces right where is it"])
            if n1 > n2:
                S.add(G, [f"a frog starts at {spoken(n1)} and jumps {spoken(n2)} to the left where does it land",
                          f"on a number line if i start at {spoken(n1)} and hop {spoken(n2)} back where am i",
                          f"start at {spoken(n1)} on the number line and jump {spoken(n2)} left"],
                      f"You land on {w(n1 - n2)}.",
                      [f"if the frog is on {spoken(n1)} and hops {spoken(n2)} spaces left where is it"])
    S.add(G, ["what is a number line", "how does a number line work"],
          "A number line shows numbers in order. Jumping right adds, jumping left takes away.",
          ["tell me about number lines"])


def shapearith_game(a):
    G = "shapearith"
    grab(G, "ObjectAddGame.cpp", r"n1_ = static_cast<uint8_t>\(1 \+ random\(4\)\);")
    shapes = ["circles", "squares", "triangles", "stars"]
    k = 0
    for n1 in range(1, 5):
        for n2 in range(1, 5):
            s = shapes[k % 4]
            k += 1
            S.add(G, [f"i have {spoken(n1)} {s} and add {spoken(n2)} more how many {s}",
                      f"{spoken(n1)} {s} plus {spoken(n2)} {s} is how many",
                      f"if there are {spoken(n1)} {s} and {spoken(n2)} more come how many {s} are there"],
                  f"There are {w(n1 + n2)} {s}.", [f"add {spoken(n2)} {s} to {spoken(n1)} {s} how many now"])
    for n2 in range(1, 5):
        for n1 in range(n2 + 1, n2 + 5):
            s = shapes[k % 4]
            k += 1
            d = n1 - n2
            one = s[:-1] if d == 1 else s
            S.add(G, [f"i have {spoken(n1)} {s} and take away {spoken(n2)} how many are left",
                      f"{spoken(n1)} {s} take away {spoken(n2)} {s} how many left",
                      f"if there are {spoken(n1)} {s} and {spoken(n2)} go away how many stay"],
                  f"{cap(w(d))} {one} {'is' if d == 1 else 'are'} left.",
                  [f"take {spoken(n2)} {s} from {spoken(n1)} {s} how many remain"])


def fingers_game(a):
    G = "fingers"
    grab(G, "FingerCountGame.cpp", r"target_ = static_cast<uint8_t>\(1 \+ random\(10\)\);")
    for n in range(1, 11):
        N = spoken(n)
        if n <= 5:
            ans = f"Hold up {w(n)} {'finger' if n == 1 else 'fingers'} on one hand."
        elif n < 10:
            ans = f"Hold up five fingers on one hand and {w(n - 5)} on the other."
        else:
            ans = "Hold up all ten fingers, five on each hand."
        S.add(G, [f"how do i show {N} on my fingers", f"show me {N} {'finger' if n == 1 else 'fingers'}",
                  f"how do i count {N} on my hands"],
              ans, [f"how can i make {N} with my fingers"])
        if n > 5:
            S.add(G, [f"five fingers and {spoken(n - 5)} more fingers is how many",
                      f"one whole hand and {spoken(n - 5)} fingers is how many",
                      f"how many fingers is one hand and {spoken(n - 5)} more"],
                  f"That is {w(n)} fingers.", [f"a full hand plus {spoken(n - 5)} fingers makes what"])


def sort_game(a):
    G = "sort"
    m, _ = grab(G, "SortGame.cpp", r"maxValue = score_ < 6 \? (\d+) : \(score_ < 12 \? (\d+) : (\d+)\)")
    top = int(m.group(3))
    rng = random.Random(a.seed + 7)
    seen = set()
    while len(seen) < a.sort_pairs:
        x, y = rng.randint(1, top), rng.randint(1, top)
        if x == y or max(x, y) <= 20 or (x, y) in seen:
            continue
        seen.add((x, y))
        big, small = max(x, y), min(x, y)
        S.add(G, [f"which is bigger {spoken(x)} or {spoken(y)}", f"what is bigger {spoken(x)} or {spoken(y)}",
                  f"which number is more {spoken(x)} or {spoken(y)}"],
              f"{cap(w(big))} is bigger than {w(small)}.", [f"which is larger {spoken(x)} or {spoken(y)}"])
    for _ in range(a.sort_sets):
        k = rng.randint(4, 6)
        nums = rng.sample(range(1, top + 1), k)
        Q = " ".join(spoken(n) for n in nums)
        up = sorted(nums)
        S.add(G, [f"put these in order from smallest to biggest {Q}",
                  f"sort these numbers smallest first {Q}",
                  f"can you put {Q} in order from smallest to largest"],
              cap(", ".join(w(n) for n in up[:-1]) + " and " + w(up[-1]) + "."),
              [f"order these from least to greatest {Q}"])
        S.add(G, [f"put these in order from biggest to smallest {Q}",
                  f"sort these numbers biggest first {Q}",
                  f"can you put {Q} in order from largest to smallest"],
              cap(", ".join(w(n) for n in up[::-1][:-1]) + " and " + w(up[0]) + "."),
              [f"order these from greatest to least {Q}"])


# ================================================================== SHAPES
def shapes_game(a):
    G = "shapecolor"
    m, _ = grab(G, "ShapeColorGame.cpp", r"const ShapeFact SHAPE_FACTS\[\] = \{(.*?)\};")
    rows = re.findall(r'\{"(\w+)",\s*(\d+), (true|false)\}', m.group(1))
    special = {
        "star": "A star has ten sides and five points.",
        "oval": "An oval is a stretched circle. It has no corners and no straight sides.",
        "diamond": "A diamond has four equal sides, like a square standing on its corner.",
        "trapezium": "A trapezium has four sides, and two of them are parallel.",
        "cross": "A cross, shaped like a plus sign, has twelve sides.",
        "arrow": "An arrow shape has seven sides.",
        "pinwheel": "The pinwheel shape has eight sides.",
        "heptagon": "A heptagon has seven sides.",
    }
    for name, sides, concave in rows:
        if name in special:
            S.add(G, [f"how many sides does {art(name)} {name} have", f"how many sides has {art(name)} {name}",
                      f"how many corners does {art(name)} {name} have"],
                  special[name], [f"count the sides of {art(name)} {name}"])
        kind = "concave" if concave == "true" else "convex"
        extra = " It has corners that point inward." if kind == "concave" else " None of its corners point inward."
        if int(sides) == 0:
            extra = " It is round with no corners at all."
        S.add(G, [f"is {art(name)} {name} concave or convex", f"is {art(name)} {name} convex",
                  f"is {art(name)} {name} concave"],
              f"{cap(art(name))} {name} is {kind}.{extra}", [f"would you call {art(name)} {name} concave"])
    S.add(G, ["what shape has seven sides", "which shape has seven sides", "what do you call a shape with seven sides"],
          "A heptagon has seven sides.", ["name a shape with seven sides"])
    S.add(G, ["what does concave mean", "what is a concave shape", "what is concave"],
          "A concave shape has at least one corner that points inward, like a star.",
          ["tell me what concave means"])
    S.add(G, ["what does convex mean", "what is a convex shape", "what is convex"],
          "A convex shape has all its corners pointing outward, like a square or a circle.",
          ["tell me what convex means"])
    S.add(G, ["what is a trapezoid", "what is a trapezium", "is a trapezium the same as a trapezoid"],
          "A trapezium, also called a trapezoid, is a four-sided shape with two parallel sides.",
          ["tell me about trapeziums"])


# ================================================================== SPELLING (Trace / Cursive)
def spelling_game(a):
    G = "trace/cursive"
    words = str_array(G, "TraceGame.cpp", r"const char\* const TRACE_WORDS")
    words += str_array(G, "CursiveGlyphData.cpp", r"const char\* const CURSIVE_WORDS")
    for word in sorted(set(words)):
        sp = ", ".join(c.upper() for c in word)
        S.add(G, [f"how do you spell {word}", f"spell {word}", f"how do i spell {word}",
                  f"can you spell {word} for me"],
              f"{cap(word)} is spelled {sp}.", [f"what letters are in the word {word}"])
        S.add(G, [f"how many letters are in {word}", f"how many letters does {word} have",
                  f"how many letters in the word {word}"],
              f"{cap(word)} has {w(len(word))} letters.", [f"count the letters in {word}"])
    S.add(G, ["what is cursive", "what is cursive writing", "what does cursive mean"],
          "Cursive is joined-up writing, where the letters connect to each other.",
          ["tell me about cursive"])


# ================================================================== RULE / FACT GAMES
def rules_games(a):
    grab("piano", "PianoGame.cpp", r'"C", "D", "E", "F", "G", "A", "B", "C"')
    grab("dice", "DiceGame.cpp", r"Throw one, two or three dice\.")
    grab("coinflip", "CoinFlipGame.cpp", r"best of 1, 3 or 5")
    grab("tictactoe", "TicTacToeGame.cpp", r"3x3 board")
    grab("chess", "ChessRules.cpp", r"Promotion is automatic and always a queen")
    grab("go", "GoRules.h", r"The rules as played")
    grab("ludo", "LudoRules.h", r"The rules, as played here")
    grab("backgammon", "BackgammonRules.h", r"The rules, as played here")
    m, _ = grab("seabattle", "SeaBattleGame.cpp", r"SHIP_LEN\[SeaBattleGame::SHIP_COUNT\] = \{([^}]*)\}")
    ships = [int(x) for x in re.findall(r"\d+", m.group(1))]
    grab("microku", "MicrokuGame.cpp", r"constexpr StageDef STAGES\[\]")
    grab("cinnamon", "CinnamonGame.cpp", r"Watch the pattern, then repeat it\.")
    grab("memory", "MemoryGame.cpp", r"Flip cards to find matching pairs\.")
    grab("slide", "SlidingPuzzleGame.cpp", r"Slide tiles into order\.")
    notes = ["c", "d", "e", "f", "g", "a", "b"]
    R = {
        "piano": [
            (["what are the music notes", "what are the notes on a piano", "name the music notes"],
             "The white keys are C, D, E, F, G, A and B, and then C again."),
            (["how many notes are in an octave", "how many white keys are in an octave"],
             "An octave has eight white keys, from C up to the next C."),
            (["what are the black keys on a piano", "what are the black keys called"],
             "The black keys are sharps and flats, like C sharp and F sharp."),
            (["how many black keys are in an octave", "how many black keys are in one octave"],
             "There are five black keys in each octave, in groups of two and three."),
            (["what note comes after g", "what comes after g in music"],
             "After G comes A. The music alphabet starts again after G."),
            (["what is an octave", "what does octave mean"],
             "An octave is the jump from one note to the next note with the same name, like C to C."),
            (["what is middle c", "where is middle c"],
             "Middle C is the C near the middle of the piano, a good place to start playing."),
            (["what is a sharp in music", "what does sharp mean in music"],
             "A sharp note is a little bit higher. It is often a black key."),
        ] + [([f"what note comes after {n} on the piano", f"which note is after {n}", f"what white key is after {n}"],
              f"After {n.upper()} comes {notes[(i + 1) % 7].upper()}.") for i, n in enumerate(notes) if n != "g"],
        "dice": [
            (["how many sides does a dice have", "how many faces does a die have", "how many sides on a dice"],
             "A dice has six sides, with one to six dots."),
            (["what is the biggest number on a dice", "what's the highest number on a dice"],
             "The biggest number on a dice is six."),
            (["what is the most you can roll with two dice", "what's the biggest total with two dice",
              "highest roll with two dice"],
             "The most you can roll with two dice is twelve."),
            (["what is the most you can roll with three dice", "what's the biggest total with three dice",
              "highest roll with three dice"],
             "The most you can roll with three dice is eighteen."),
            (["what is the smallest you can roll with two dice", "lowest roll with two dice"],
             "The smallest you can roll with two dice is two."),
            (["what is the smallest you can roll with three dice", "lowest roll with three dice"],
             "The smallest you can roll with three dice is three."),
            (["how many dots are on a dice", "how many dots are on a whole dice", "how many spots on a dice"],
             "A dice has twenty-one dots altogether."),
            (["what is the most common total with two dice", "which total comes up most with two dice"],
             "Seven comes up the most with two dice."),
        ] + [([f"what number is opposite {spoken(n)} on a dice", f"what is on the other side of {spoken(n)} on a dice",
               f"what's across from {spoken(n)} on a dice"],
              f"{cap(w(7 - n))} is opposite {w(n)}. Opposite sides always add up to seven.") for n in range(1, 7)],
        "coinflip": [
            (["what are the two sides of a coin", "what are the sides of a coin called"],
             "The two sides of a coin are heads and tails."),
            (["what are the chances of heads", "what is the chance a coin lands on heads", "is heads or tails more likely"],
             "Heads and tails are equally likely. Each has a one in two chance."),
            (["what does best of three mean", "how do you win best of three"],
             "Best of three means the first to win two flips wins."),
            (["what does best of five mean", "how do you win best of five"],
             "Best of five means the first to win three flips wins."),
            (["can a coin land on heads twice in a row", "if i flip heads is tails next"],
             "Yes. Every flip is new, so heads can come up again. The coin does not remember."),
        ],
        "tictactoe": [
            (["how do you play tic tac toe", "what are the rules of tic tac toe", "how do you win tic tac toe"],
             "Take turns putting X or O on a three by three grid. Get three in a row to win."),
            (["who goes first in tic tac toe", "does x or o go first"],
             "X goes first in tic tac toe."),
            (["how many squares are in tic tac toe", "how many boxes on a tic tac toe board"],
             "A tic tac toe board has nine squares."),
            (["what happens if nobody wins tic tac toe", "what is a draw in tic tac toe"],
             "If the board fills up with no three in a row, it is a draw."),
        ],
        "chess": [
            (["how does a knight move in chess", "how does the horse move in chess", "how does a knight move"],
             "A knight moves in an L shape: two squares one way and one square to the side. It can jump over pieces."),
            (["how does a bishop move in chess", "how does a bishop move", "where can a bishop go"],
             "A bishop moves diagonally, as far as it likes."),
            (["how does a rook move in chess", "how does the castle move in chess", "how does a rook move"],
             "A rook moves in straight lines, up, down or sideways, as far as it likes."),
            (["how does a queen move in chess", "how does the queen move", "where can the queen go in chess"],
             "The queen moves any number of squares in a straight line or diagonally."),
            (["how does a king move in chess", "how does the king move", "how far can the king move"],
             "The king moves one square in any direction."),
            (["how does a pawn move in chess", "how does a pawn move", "how do pawns capture"],
             "A pawn moves forward one square, or two on its first move, and captures diagonally."),
            (["what happens when a pawn reaches the end", "what is pawn promotion", "what happens to a pawn at the other side"],
             "A pawn that reaches the far side turns into a queen."),
            (["what is checkmate", "how do you win at chess", "how do you win chess"],
             "Checkmate is when the king is attacked and cannot escape. That wins the game."),
            (["what is check in chess", "what does check mean in chess"],
             "Check means the king is being attacked and must get out of danger."),
            (["what is stalemate", "what is a stalemate in chess"],
             "Stalemate is when a player has no legal move but is not in check. The game is a draw."),
            (["what is castling", "how do you castle in chess"],
             "Castling moves the king two squares toward a rook, and the rook jumps to the other side of the king."),
            (["what is en passant", "what does en passant mean"],
             "En passant is a special pawn capture of a pawn that just moved two squares past it."),
            (["who moves first in chess", "does white or black go first in chess"],
             "White always moves first in chess."),
            (["how many pieces does each player have in chess", "how many chess pieces are there"],
             "Each player starts with sixteen pieces, thirty-two in all."),
            (["how many squares are on a chess board", "how big is a chess board"],
             "A chess board has sixty-four squares, eight by eight."),
            (["what is the most powerful chess piece", "which chess piece is the strongest"],
             "The queen is the most powerful piece in chess."),
            (["how many pawns does each player have", "how many pawns are in chess"],
             "Each player has eight pawns."),
        ],
        "go": [
            (["how do you play go", "what are the rules of go", "what is the game go"],
             "Players take turns placing black and white stones. Surround the other player's stones to capture them."),
            (["how do you capture in go", "how do you take stones in go"],
             "Surround a stone or group on every side so it has no empty points next to it, and it is captured."),
            (["who goes first in go", "does black or white go first in go"],
             "Black plays first in go."),
            (["how big is a go board", "how many lines are on a go board"],
             "A small go board is nine by nine. The full size board is nineteen by nineteen."),
            (["what is komi in go", "what does komi mean"],
             "Komi is extra points given to White for going second. In this game it is five and a half."),
            (["what is ko in go", "what is the ko rule"],
             "The ko rule says you cannot take back a single stone right away if it just captured a single stone."),
            (["how do you win go", "how do you win at go"],
             "In capture go the first to capture enough stones wins. In the full game, whoever surrounds more space wins."),
            (["what is a liberty in go", "what are liberties in go"],
             "A liberty is an empty point next to a stone. A stone with no liberties is captured."),
        ],
        "ludo": [
            (["how do you play ludo", "what are the rules of ludo", "how do you win ludo"],
             "Roll the dice and race all four of your tokens around the board and home. The first player home wins."),
            (["how do you get a token out in ludo", "what do you need to start in ludo", "what number gets you out in ludo"],
             "You need a six to move a token out of the yard."),
            (["what happens if you roll a six in ludo", "do you get another turn for a six in ludo"],
             "A six gives you another roll. But three sixes in a row ends your turn."),
            (["what happens when you land on someone in ludo", "how do you capture in ludo"],
             "Land on a lone token of another color and it goes back to its yard. You get another roll too."),
            (["what are the safe squares in ludo", "where are you safe in ludo"],
             "The start squares and the star squares are safe. Nobody can be captured there."),
            (["what is a block in ludo", "how do you make a block in ludo"],
             "Two tokens of the same color on one square make a block. Other tokens cannot land on it or pass it."),
            (["how many tokens do you have in ludo", "how many pieces in ludo"],
             "Each player has four tokens."),
            (["do you need an exact roll to get home in ludo", "how do you get home in ludo"],
             "Yes, you need the exact number to reach home."),
        ],
        "backgammon": [
            (["how do you play backgammon", "what are the rules of backgammon", "how do you win backgammon"],
             "Roll two dice and move your fifteen checkers around the board. The first to bear them all off wins."),
            (["how many checkers are in backgammon", "how many pieces does each player have in backgammon"],
             "Each player has fifteen checkers."),
            (["what happens if you roll doubles in backgammon", "what are doubles in backgammon"],
             "Doubles are played four times, so a double three moves three, four times."),
            (["what is a blot in backgammon", "how do you hit in backgammon"],
             "A blot is a single checker alone on a point. Land on it to hit it to the bar."),
            (["what is the bar in backgammon", "what happens to a checker on the bar"],
             "A hit checker goes on the bar and must come back in before any other checker can move."),
            (["what is bearing off", "what does bearing off mean in backgammon"],
             "Bearing off is taking your checkers off the board once all fifteen are in your home board."),
            (["what is a gammon", "what is a gammon in backgammon"],
             "A gammon is a win where the loser has not borne off any checkers. It counts double."),
            (["how many points are on a backgammon board", "how many triangles on a backgammon board"],
             "A backgammon board has twenty-four points."),
        ],
        "seabattle": [
            (["how do you play sea battle", "how do you play battleships", "what are the rules of battleships"],
             "Take turns guessing squares to find the other player's hidden ships. Sink all their ships to win."),
            (["how big is the sea battle grid", "how many squares in sea battle"],
             f"The sea battle grid is {w(8)} by {w(8)}, sixty-four squares."),
            (["how many ships are in sea battle", "how many ships do you get in sea battle"],
             f"Each fleet has {w(len(ships))} ships, {', '.join(w(s) for s in ships[:-1])} and {w(ships[-1])} squares long."),
            (["how do you sink a ship", "when is a ship sunk in sea battle"],
             "A ship is sunk when every square of it has been hit."),
        ],
        "microku": [
            (["what is sudoku", "how do you play sudoku", "how do you play microku"],
             "Fill the grid so every row, every column and every box has each number just once."),
            (["what sizes are microku puzzles", "how big are microku puzzles"],
             "Microku has two by two, four by four and six by six puzzles."),
        ],
        "cinnamon": [
            (["how do you play cinnamon says", "what is cinnamon says"],
             "Watch the pads light up in a pattern, then tap them in the same order. Each round adds one more."),
        ],
        "memory": [
            (["how do you play memory", "how do you play the matching game", "what is memory match"],
             "Turn over two cards at a time and try to find the matching pairs. Remember where each card was."),
        ],
        "slide": [
            (["how do you solve a slide puzzle", "how do you play the sliding puzzle"],
             "Slide the tiles into the empty space, one at a time, until the numbers are in order."),
        ],
    }
    for game, items in R.items():
        for tr, ans in items:
            S.add(game, tr, ans, [])


# ================================================================== pipeline
def gen_kid_questions() -> set[str]:
    """Every question gen_kid_data.py already trains or evaluates."""
    p = ROOT / "tools" / "kid" / "gen_kid_data.py"
    spec = importlib.util.spec_from_file_location("gen_kid_data", p)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    mod.FACTS.clear()
    mod.build()
    qs = set()
    for tq, eq, _ in mod.FACTS:
        qs.update(norm_q(q) for q in tq)
        qs.update(norm_q(q) for q in eq)
    return qs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "models_out" / "kid"))
    ap.add_argument("--seed", type=int, default=11)
    ap.add_argument("--repeat", type=int, default=1, help="copies of each training phrasing")
    ap.add_argument("--arith-phrasings", type=int, default=3,
                    help="training phrasings per extended add/subtract fact (Math levels 3-5)")
    ap.add_argument("--word-problems", type=int, default=1200, help="Math word problems to sample")
    ap.add_argument("--sort-pairs", type=int, default=300)
    ap.add_argument("--sort-sets", type=int, default=100)
    ap.add_argument("--eval-per-fact", type=int, default=1)
    a = ap.parse_args()
    if not GAMES.is_dir():
        raise SystemExit(f"Gume sources not found at {GAMES}")

    for fn in (math_game, multiply_game, colormix_game, elements_game, flags_game, states_game,
               gre_game, space_game, fractions_game, percent_game, money_game, roman_game, time_game,
               numberline_game, shapearith_game, fingers_game, sort_game, shapes_game, spelling_game,
               rules_games):
        fn(a)

    # 1. drop anything gen_kid_data.py already owns
    kidq = gen_kid_questions()
    dropped_kid = 0
    for f in S.facts:
        before = len(f[1])
        f[1] = [q for q in f[1] if q not in kidq]
        f[3] = [q for q in f[3] if q not in kidq]
        dropped_kid += before - len(f[1])

    # 2. a question that maps to two different answers is ambiguous: drop it everywhere
    owners = defaultdict(set)
    for f in S.facts:
        for q in f[1] + f[3]:
            owners[q].add(f[2])
    ambiguous = {q for q, ans in owners.items() if len(ans) > 1}
    for f in S.facts:
        f[1] = [q for q in f[1] if q not in ambiguous]
        f[3] = [q for q in f[3] if q not in ambiguous]

    # 3. eval phrasings must be held out from ALL training text
    trainq = {q for f in S.facts for q in f[1]}
    for f in S.facts:
        f[3] = [q for q in f[3] if q not in trainq][: a.eval_per_fact]

    facts = [f for f in S.facts if f[1]]
    # merge identical (question-set, answer) duplicates
    seen, uniq = set(), []
    for f in facts:
        key = (f[2], tuple(f[1]))
        if key not in seen:
            seen.add(key)
            uniq.append(f)
    facts = uniq

    rng = random.Random(a.seed)
    train, evals = [], []
    per_game = defaultdict(lambda: [0, 0, 0])
    for game, tq, ans, eq in facts:
        per_game[game][0] += 1
        for q in tq:
            for _ in range(a.repeat):
                train.append(f"User: {q}\nBot: {ans}<|endoftext|>")
                per_game[game][1] += 1
        for q in eq:
            # Carry the true game label. eval_by_category.py otherwise has to
            # guess the field from words in the text, and it guesses badly:
            # "what do you know about carbon" was being counted as an ANIMALS
            # question because the answer contains the word "animal", and
            # "how would you use the word bucolic" likewise because its answer
            # mentions sheep. Two of the three fields that looked permanently
            # broken turned out to be mislabelled buckets rather than weak
            # fields.
            evals.append({"q": q, "a": ans, "topic": game})
            per_game[game][2] += 1
    rng.shuffle(train)

    bad = [t for t in train if re.search(r"\d", t) or not t.isascii()]
    if bad:
        print(f"[gume_extract] WARNING: {len(bad)} samples contain digits or non-ASCII, e.g. {bad[0]!r}")

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    # newline="\n" on write_text is Python 3.10+, and the training venv is 3.9.
    # open(newline="\n") does the same job on both.
    with (out / "gume_train.txt").open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n\n".join(train) + "\n")
    with (out / "gume_eval.jsonl").open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(json.dumps(e) for e in evals) + "\n")
    (out / "gume_sources.json").write_text(json.dumps(
        {g: {"sources": SOURCES.get(g, []), "facts": c[0], "train": c[1], "eval": c[2]}
         for g, c in per_game.items()}, indent=1), encoding="utf-8")

    print(f"{'game':<16}{'facts':>8}{'train':>9}{'eval':>8}")
    for g in sorted(per_game, key=lambda g: -per_game[g][1]):
        c = per_game[g]
        print(f"{g:<16}{c[0]:>8}{c[1]:>9}{c[2]:>8}")
    tf = sum(c[0] for c in per_game.values())
    print(f"{'TOTAL':<16}{tf:>8}{len(train):>9}{len(evals):>8}")
    print(f"dropped {dropped_kid} phrasings already in gen_kid_data.py, {len(ambiguous)} ambiguous questions")
    print(f"wrote {out / 'gume_train.txt'} and {out / 'gume_eval.jsonl'}")


if __name__ == "__main__":
    main()
