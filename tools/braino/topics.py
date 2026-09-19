"""The Braino topic map: all 40 Gume games, as TOPICS rather than as questions.

Why topics and not questions. `tools/kid/gume_extract.py` mined quiz tables out
of the C++ source, which had two consequences:

  1. It covered 24 of the 40 games. A game with no lookup table produced
     nothing - Chess, Go, Maze, Memory, Sea Battle and eleven others
     contributed zero facts, even though a child asks plenty about chess.
  2. What it did produce were the games' own question strings. Training on
     those is what produced the measured failure: 85% correct on a trained
     wording, 25.8% on a held-out wording of the same fact. Fitting the
     question forms IS the failure mode.

So each game here contributes a SUBJECT AREA, and the generators are expected
to cover that area the way a curious 5-8 year old would wander through it -
including the questions the game never asks.

`breadth` is the instruction to the generator: what lies around the game that a
child would reasonably ask about. `game_asks` records what the game itself
tests, so we keep that coverage without being limited to it.
"""

# (game class, topic, what the game tests, what else a child would ask)
TOPICS = [
    # ---- number and arithmetic ----------------------------------------
    ("Math", "arithmetic", "add/subtract over 5 difficulty levels",
     "why carrying works, what plus and minus mean, doubles, near-doubles, "
     "checking an answer, zero and its behaviour, word problems in real settings"),
    ("Multiplication", "multiplication", "times tables 1-12",
     "what multiplication IS (repeated addition), arrays and groups, "
     "why order does not matter, skip counting, square numbers"),
    ("NumberLine", "number line", "hop left and right from a start point",
     "before/after, between, ordering, negative direction, distance between "
     "two numbers, number line as a tool for adding"),
    ("Counting", "counting", "count objects and tap the number",
     "counting on and back, counting by 2s/5s/10s, estimating, "
     "one-to-one correspondence, zero, ordinal position"),
    ("ObjectAdd", "adding objects", "add/take away shapes",
     "grouping, sharing equally, what is left over, comparing amounts"),
    ("Sort", "ordering numbers", "order numbers up or down",
     "bigger/smaller/equal, between, ascending and descending, "
     "comparing two-digit numbers by tens then ones"),
    ("Sequence", "calendar & patterns", "days and months in order",
     "days of the week, months, seasons, yesterday/tomorrow, "
     "repeating patterns, what comes next, growing patterns"),
    ("Fraction", "fractions", "naming slices, comparing fractions",
     "halves/thirds/quarters in real life, equal shares, "
     "numerator and denominator in kid words, fractions of a set, "
     "when two fractions are the same"),
    ("PercentCircle", "percentages", "percent <-> fraction, p% of n",
     "what percent means (out of a hundred), 50%/25%/10% as everyday amounts, "
     "percent in shops and batteries"),
    ("Roman", "roman numerals", "number <-> numeral to 3999",
     "where Roman numerals are still seen (clocks, books, films), "
     "the subtraction rule, why there is no zero"),
    ("Money", "money", "coin values, change, combinations",
     "what money is for, saving, cost comparison, "
     "notes as well as coins, making the same amount different ways"),
    ("Microku", "sudoku & logic", "mini sudoku boards",
     "what a puzzle grid is, reasoning by elimination, "
     "rows/columns/boxes, why guessing less beats guessing more"),

    # ---- language -----------------------------------------------------
    ("Trace", "handwriting & spelling", "trace letters, numbers, words",
     "letter names and sounds, uppercase/lowercase, spelling common words, "
     "vowels and consonants, how many letters in a word"),
    ("Cursive", "cursive writing", "trace joined-up letters",
     "what cursive is, why joined writing exists, print vs cursive"),
    ("GreWords", "vocabulary", "250 GRE words: meaning, part of speech",
     "AGE-APPROPRIATE vocabulary instead: common words a 5-8 year old meets, "
     "synonyms and opposites, nouns/verbs/adjectives in kid terms. "
     "The GRE list itself is far above this age - keep the SKILL, drop the list"),

    # ---- science and the world ----------------------------------------
    ("Space", "space", "41 quiz questions on planets and the atmosphere",
     "planets and their order, the Moon and its phases, stars, the Sun, "
     "day and night, seasons, gravity, rockets and astronauts, comets"),
    ("Elements", "chemistry", "all 118 elements: symbol, protons, category",
     "MUCH narrower: the handful a child meets - oxygen, hydrogen, carbon, "
     "gold, iron, helium - plus what an atom is, solid/liquid/gas. "
     "118 elements is not age-appropriate coverage"),
    ("ColorMix", "colors", "mixing two colours",
     "primary and secondary colours, rainbows, why leaves change, "
     "light vs paint mixing, shades and brightness"),
    ("ShapeColor", "shapes", "side counts, concave/convex",
     "2D and 3D shapes, sides/corners/faces, shapes in the world, "
     "symmetry, what makes a square not a rectangle"),
    ("Time", "telling time", "clock hands to 5 minutes",
     "reading a clock, o'clock/half past/quarter past, am and pm, "
     "days/hours/minutes/seconds, how long things take, calendars"),

    # ---- geography ----------------------------------------------------
    ("Flag", "world geography", "195 countries: capital, continent, flag",
     "continents and oceans, famous landmarks, where animals live, "
     "maps and directions, a SMALLER set of countries a child would meet"),
    ("States", "US geography", "50 states and capitals",
     "what a state is, big landmarks, rivers and mountains, "
     "the state the child lives in"),
    ("StateFlag", "flags", "state flag recognition",
     "what a flag is and why countries have them, common flag colours "
     "and what they stand for"),
    ("StateMap", "maps", "state outline recognition",
     "what a map is, north/south/east/west, globes, "
     "why maps have keys and scales"),

    # ---- music, body, and play ----------------------------------------
    ("Piano", "music", "note names, octaves, sharps",
     "instruments and their families, loud/soft, fast/slow, rhythm and beat, "
     "singing, what makes sound"),
    ("FingerCount", "counting on hands", "show N on fingers",
     "hands and fingers, tally marks, body parts, "
     "counting in fives because of hands"),
    ("Dice", "dice & chance", "sides, totals, opposite faces",
     "what luck and chance mean, likely vs unlikely, "
     "fair and unfair, taking turns"),
    ("CoinFlip", "chance", "heads or tails",
     "fifty-fifty, prediction, why past flips do not change the next one "
     "(kept very simple), fair ways to choose"),

    # ---- strategy games: covered by NO existing extraction -------------
    ("Chess", "chess", "play chess against a friend or the computer",
     "how each piece moves, the board, check and checkmate, "
     "the aim of the game, why chess is old and where it came from"),
    ("Go", "go", "play Go",
     "what the game is, stones and territory, that it is very old, "
     "how it differs from chess"),
    ("Backgammon", "backgammon", "race 15 checkers home",
     "how the race works, dice in board games, "
     "games that mix luck and skill"),
    ("Ludo", "ludo", "race four tokens home",
     "taking turns, how to win, similar games around the world"),
    ("TicTacToe", "tic tac toe", "Xs and Os on 3x3",
     "how to win, why it often ends in a draw, "
     "simple strategy a child can hold"),
    ("SeaBattle", "battleship", "hunt hidden ships",
     "grids and coordinates, guessing strategies, ships and boats"),
    ("Maze", "mazes", "drag the dot to the exit",
     "what a maze is, finding a path, left/right turns, "
     "famous mazes and labyrinths"),
    ("Memory", "memory game", "flip cards to find pairs",
     "what memory is, tips for remembering, pairs and matching, "
     "concentration"),
    ("SlidingPuzzle", "sliding puzzle", "slide tiles into order",
     "how the puzzle works, ordering, patience and planning"),
    ("WhackAMole", "reaction game", "tap the mole",
     "reaction time, fast and slow, hand-eye coordination, "
     "moles and burrowing animals"),
    ("OddOneOut", "odd one out", "find the one that differs",
     "sorting and classifying, what things have in common, "
     "categories, same and different"),
    ("Cinnamon", "cinnamon game", "arcade-style play",
     "spices and where they come from, cinnamon as bark, "
     "tastes and smells"),
]

# Areas the games imply but never test directly, and that a child on this
# device will certainly ask about.
EXTRA_TOPICS = [
    ("animals", "pets, farm, wild, babies, habitats, what they eat, "
                "how they move, biggest and smallest"),
    ("the body", "parts of the body, the five senses, bones and muscles, "
                 "teeth, staying healthy, sleeping and eating well"),
    ("plants & nature", "how plants grow, seeds, trees, flowers, "
                        "what plants need, fruit and vegetables"),
    ("weather", "rain, snow, wind, clouds, storms, rainbows, "
                "hot and cold, what to wear"),
    ("community & safety", "jobs people do, emergency numbers, road safety, "
                           "asking an adult, being kind, manners"),
    ("feelings", "naming feelings, what to do when upset, "
                 "friendship, sharing, saying sorry"),
    ("jokes & fun", "kid-safe jokes, riddles, silly questions, tongue twisters"),
    ("about me", "who the device is, what it can do, greetings, "
                 "please and thank you"),
]


def all_topics():
    """-> [(topic, game_asks, breadth)] over games and extras."""
    out = [(t, asks, breadth) for _game, t, asks, breadth in TOPICS]
    out += [(t, "", breadth) for t, breadth in EXTRA_TOPICS]
    return out


if __name__ == "__main__":
    games = {g for g, *_ in TOPICS}
    print(f"{len(TOPICS)} games mapped, {len(games)} distinct")
    print(f"{len(EXTRA_TOPICS)} extra topics")
    print(f"{len(all_topics())} topics total\n")
    for t, asks, breadth in all_topics():
        print(f"{t:22s} {breadth[:70]}")
