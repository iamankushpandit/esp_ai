# Gume (Braino!) questions -> kid chat training pairs

`tools/kid/gume_extract.py` reads `.refs/Gume/src/games/*` and writes
`models_out/kid/gume_train.txt` (TinyTalk `User:/Bot:` format) and
`models_out/kid/gume_eval.jsonl` (held-out phrasings). It also writes
`gume_sources.json`, which holds the per-game counts and the file:line of every pattern it parsed.

```
tools/.venv/Scripts/python.exe tools/kid/gume_extract.py
    [--arith-phrasings 3] [--word-problems 1200] [--sort-pairs 300] [--sort-sets 100]
    [--repeat 1] [--eval-per-fact 1] [--seed 11]
```

How the script works:

- **Tables are parsed from the C++ source** with regexes: elements, countries, US states, GRE words,
  the space quiz, colour mixes, shape facts, coin values, fraction denominators, percent tables,
  multiplication level tables, Math operand ranges, name/thing lists, trace/cursive words and Sea
  Battle ship lengths. If Gume changes one of these tables, rerun the script. If a pattern stops
  matching, the script stops with an error instead of emitting stale data.
- **Generators are mirrored.** Arduino `random(lo, hi)` is `lo..hi-1`, and the script applies the same rule.
- **Dedupe against `gen_kid_data.py`.** The script imports that file, runs `build()`, and drops every question
  it already owns (2,210 phrasings). A fact with no phrasings left is dropped entirely. As a result, only the
  Gume ranges beyond 0-20 add/sub and beyond 10x10 multiplication are emitted.
- **Ambiguous questions** are dropped from every fact that uses them, for example when one question maps to two
  answers. The only case left is "what is the capital of georgia". The qualified forms "the state of georgia" and
  "the country of georgia" are kept.
- **Eval phrasings** are checked against every training question, including gen_kid's, so they are truly held out.
  The hand-written rule facts get a generic held-out form: "i want to know ...".
- Questions are lower case with no punctuation except apostrophes, and numbers are spelled out as speech
  ("twenty one"). Answers are ASCII with numbers as words ("twenty-one"). The script warns if any digit or
  non-ASCII character leaks into the output. None did.

## Per-game table

Paths are relative to `.refs/Gume/`. Counts are from the default run: facts / train pairs / eval pairs.

| Game (app id) | Question type(s) generated | Source (file:line) | Facts / train / eval | Skipped and why |
|---|---|---|---|---|
| Math (`math`) | Add/sub over the Gume level ranges: L1 1-9+1-9, L2 2-12 +/- 1-9, L3 10-29 +/- 1-9, L4 10-49 +/- 10-49, L5 10-99 +/- 10-99 (subtraction swaps to stay non-negative). Also 1,200 sampled word problems using the game's 6 templates, 24 names and 16 things. | `src/games/MathGame.cpp:64` (NAMES), `:71` (THINGS), `:142` (word templates), `:194` (level ranges) | 13,425 / 40,275 / 13,425 | Pairs already in gen_kid (0-20, sum <= 30). Word problems are sampled because the full cross product of numbers x names x things x templates is millions of pairs. |
| Multiply (`multiply`) | a x b for tables 1-12 x factors 1-12 (level 5), both orders | `src/games/MultiplicationGame.cpp:19-23` (LEVELn_TABLES), `:107` (factor range) | 44 / 176 / 44 | 0-10 x 0-10 is already in gen_kid. The game has no division. |
| Color Mix (`colormix`) | Mix X+Y -> colour (only blue+white=light blue is new), and "what two colours make Z" for all 6 mixes | `src/games/ColorMixGame.cpp:14` | 8 / 32 / 8 | The other 5 forward mixes are in gen_kid. |
| Elements (`elements`) | For all 118 elements: name->symbol, symbol (spelled "a u")->name, protons/atomic number, number->element, "tell me about X" (category + the game's fact line), clue->element (the FactToElement quiz), category. State at room temperature only for tier 1-2, as the game does. Plus 4 general facts. | `src/games/ElementDataTable.cpp:20` (categories), `:33` (states), `:40` (ELEMENTS), `src/games/ElementsQuiz.cpp:129` (question types), `:53` (state tier rule) | 893 / 2,977 / 893 | "Find it in the table" (tap a cell) is visual. Table row/column positions are not voiced. |
| Flags (`flags`) | For all 195 countries: capital, capital->country, continent. Flag descriptions for the 62 tier 1+2 countries. Plus a count of countries and the continents. | `src/games/CountryDataTable.cpp:24` (continents), `:34` (COUNTRY_FACTS), `src/games/FlagGame.cpp:376` (capital bonus) | 644 / 2,399 / 644 | Country names and flag artwork live in the external `map-n-flag` library, which is not in the repo. English names are hard-coded in the script for the 195 ISO codes. Flag descriptions are hand-written for tier 1+2 only, because tier 3 flags (for example Kiribati or Eswatini) were too error-prone to describe from memory. |
| US States (`states`), State Flags (`stateflags`), State Maps (`statemaps`) | For all 50 states: capital, capital->state. Plus a state count and the biggest state. | `src/games/StateData.cpp:15`, `src/games/StatesGame.cpp:96,232`, `src/games/StateFlagGame.cpp:257`, `src/games/StateMapGame.cpp:233` | 102 / 357 / 102 | State flag and outline recognition is purely visual. Only their capital bonus rounds are voiced. |
| GRE Words (`grewords`) | For all 250 words: meaning (gloss + example), "use X in a sentence", part of speech | `src/games/GreWordTable.cpp:25` | 750 / 2,500 / 750 | None. The vocabulary is far above age 5-8, but you asked for all entries. |
| Space (`space`) | All 41 quiz questions. The answer is the correct option plus the game's explanation, with numbers spelled out ("-90C" becomes "minus ninety degrees Celsius"). | `src/games/SpaceGame.cpp:57` | 41 / 159 / 41 | Plain phrasings that match gen_kid space questions were dropped. The reworded ones are kept. |
| Fractions (`fractions`) | Naming: n out of d slices for d in {2,3,4,5,6,8}. "How many halves/thirds... make a whole". All 231 "which is bigger" pairs (equal pairs answer "same size"). | `src/games/FractionGame.cpp:22` (DENOMS_L5), `:406` (prompts) | 259 / 780 / 259 | Pie-picking is visual. It is covered by the naming facts. |
| Percent (`percent`) | Percent <-> fraction for 10..100. "What percent is half". The "p% of n" mode: all 25 base x percent combinations. "What does percent mean". | `src/games/PercentCircleGame.cpp:88,92` (L2/L3), `:144-145` (BASE_NUMBERS/PERCENTS) | 42 / 126 / 42 | "Shade p%" is a visual drag. The 5% steps have no kid-friendly fraction. **Game bug:** `:149`/`:312` computes `(base*pct)/100` in integer maths, so 25% of 10 shows 2, 75% of 10 shows 7, and 25%/75% of 50 show 12/37. The training data uses the exact values ("two and a half"). |
| Money (`money`) | Value of each US coin (1/5/10/25/50c) both ways. Pennies per coin. Coins per dollar. k of one coin (k=2-8, <= $1.50). Two-coin-type combinations (1-3 of each). Make-change for every price below each pay amount of 25c/50c/$1/$1.50 (step 1, level 5). | `src/games/MoneyGame.cpp:14` (COIN_VALUES), `:236` (payChoices) | 446 / 1,343 / 446 | Random mixed groups of 3-8 coins are combinatorial, so only the systematic combinations are covered. "Make this amount" by tapping coins is interactive. |
| Roman (`roman`) | Number->numeral and numeral (spelled "x i v")->number for 1-399 (levels 1-4), 400-3999 in steps of 50, plus 3999. Symbol values and the game's 5 teaching tips. | `src/games/RomanGame.cpp:44` (PIECES), `:154` (tips), `:194` (CEILING) | 957 / 2,907 / 957 | 400-3999 is sampled every 50 because the full range is 3,600 more numbers. Raise it in `roman_game()` if wanted. |
| Time (`time`) | "Little hand on H, big hand on N" for every 5-minute time (levels 1-4, 144). The answer gives the "quarter past" form and the digital form. "Another way to say three fifteen". Clock-hand facts. | `src/games/TimeGame.cpp:144-156` | 188 / 561 / 188 | Level 5 (any minute) cannot be described by which number the big hand is on. |
| Number Line (`numberline`) | Frog starts at 1-5 and jumps 1-4 right or left | `src/games/NumberLineGame.cpp:67-73` | 31 / 92 / 31 | None. |
| Shape Arith (`shapearith`) | "I have n circles/squares/triangles/stars and add or take away m" (1-4 +/- 1-4, subtraction n > m) | `src/games/ObjectAddGame.cpp:101-105` | 32 / 96 / 32 | None. |
| Fingers (`fingers`) | "How do I show N on my fingers" (1-10). "A whole hand and k more" | `src/games/FingerCountGame.cpp:113` | 15 / 45 / 15 | Counting drawn fingers is visual. |
| Sorting (`sort`) | 300 sampled "which is bigger" pairs up to 99 (at least one number > 20). 100 sampled 4-6 number sets, each sorted both ways. | `src/games/SortGame.cpp:65,70` | 500 / 1,500 / 500 | All pairs up to 99 would be about 4,800 facts, so they are sampled. |
| Shapes (`shapecolor`) | Side counts for star, oval, diamond, trapezium, cross, arrow, pinwheel, heptagon. Concave/convex for all 15 shapes. Definitions of concave, convex and trapezium. | `src/games/ShapeColorGame.cpp:47` (SHAPE_FACTS) | 27 / 81 / 27 | Side counts for circle, square, triangle, rectangle, pentagon, hexagon and octagon are in gen_kid. Outline matching is visual. |
| Trace (`trace`), Cursive (`cursive`) | Spelling and letter count for the union of trace words (54) and cursive words (49). "What is cursive". | `src/games/TraceGame.cpp:50` (TRACE_WORDS), `src/games/CursiveGlyphData.cpp:466` (CURSIVE_WORDS) | 143 / 500 / 143 | Letter and digit tracing is a motor skill. The alphabet order is in gen_kid. |
| Piano (`piano`) | Note names, octave, black keys and sharps, which note follows which | `src/games/PianoGame.cpp:51` | 14 / 35 / 14 | Playing is audio and touch. |
| Dice (`dice`) | Sides, max/min totals for 1-3 dice, total dots, opposite faces, most common total with 2 dice | `src/games/DiceGame.cpp:21` | 14 / 38 / 14 | Rolls are random, so there is nothing to answer. |
| Coin Flip (`coinflip`) | Heads/tails, chances, what "best of 3/5" means, independence | `src/games/CoinFlipGame.cpp:23` | 5 / 11 / 5 | Same as Dice. |
| Tic-Tac-Toe (`tictactoe`) | Rules, who goes first, 9 squares, draw | `src/games/TicTacToeGame.cpp:26` | 4 / 9 / 4 | Move-by-move play. |
| Chess (`chess`) | How each piece moves, promotion (always to a queen, as in the game), check, checkmate, stalemate, castling, en passant, board/piece counts | `src/games/ChessRules.cpp:271`, `src/games/ChessRules.h` | 17 / 42 / 17 | Move-by-move play and the AI. |
| Go (`go`) | Rules, capture, liberties, ko, komi (5.5 as played here), board sizes, how to win | `src/games/GoRules.h:24` | 8 / 17 / 8 | Move-by-move play. |
| Ludo (`ludo`) | Rules as played: need a 6 to leave the yard, a 6 or a capture gives another roll, three 6s end the turn, safe squares, blocks, exact roll to reach home | `src/games/LudoRules.h:32` | 8 / 18 / 8 | Move-by-move play. |
| Backgammon (`backgammon`) | Rules as played: 15 checkers, doubles played four times, blot, bar, bearing off, gammon, 24 points | `src/games/BackgammonRules.h:32` | 8 / 17 / 8 | Move-by-move play. |
| Sea Battle (`seabattle`) | Rules, 8x8 grid, fleet of 4 ships (4, 3, 2 and 2 long), sinking | `src/games/SeaBattleGame.cpp:46` (SHIP_LEN) | 4 / 9 / 4 | Guessing play. |
| Microku (`microku`) | Sudoku rules and puzzle sizes | `src/games/MicrokuGame.cpp:69` | 2 / 5 / 2 | The 3 fixed puzzles are visual grids. |
| Cinnamon Says (`cinnamon`) | How to play | `src/games/CinnamonGame.cpp:33`, `:78` | 1 / 2 / 1 | The pattern is random and visual. |
| Memory (`memory`) | How to play | `src/games/MemoryGame.cpp:32`, `data/sd/games/memory/default.json` | 1 / 3 / 1 | Card positions are visual. |
| Slide Puzzle (`slide`) | How to play | `src/games/SlidingPuzzleGame.cpp:24` | 1 / 2 / 1 | The tile state is visual. |
| Counting (`counting`) | Nothing generated | `src/games/CountingGame.cpp:61`, `data/sd/games/counting/default.json` (1-12) | 0 | "Count the drawn objects" is visual. Counting 1-20 and next/previous number are in gen_kid. |
| Calendar (`calendar`) | Nothing generated | `src/games/SequenceGame.cpp:33` | 0 | Day and month before/after is entirely covered by gen_kid. |
| Odd One Out (`oddone`) | Nothing generated | `src/games/OddOneOutGame.cpp:78` | 0 | A purely visual difference in colour, shape, size or rotation. |
| Maze (`maze`) | Nothing generated | `src/games/MazeGame.cpp:305` | 0 | Dragging, with nothing to ask. |
| Whack A Mole (`whack`) | Nothing generated | `src/games/WhackAMoleGame.cpp:98` | 0 | A reflex game. |
| System apps (Settings, Wi-Fi, Profiles, Scores, About, System Info, Nearby) | Nothing generated | `src/games/*App.cpp` | 0 | Device UI, not educational content. |
| **Total** | | | **18,634 / 57,114 / 18,634** | |

## Size note for an 8M-parameter model

- About **5,200 distinct non-arithmetic facts**. Elements account for 893 of them, flags 644, GRE 750, roman
  957, sort 500 and money 446. That is already a lot to memorise for about 8M parameters. The main kid set
  (`gen_kid_data.py`) is roughly 2,000 facts, so this multiplies the knowledge load about 2.5x. Expect partial
  recall on the long tail: tier 3 elements, tier 3 capitals and GRE glosses.
- About **12,200 extended arithmetic facts** (two-digit add/sub up to 99+99), which is 70% of all training pairs.
  A tiny LM will not reliably memorise that many sums or learn carrying from them. Two options:
  1. Answer "X plus/minus/times Y" with a deterministic parser and calculator in firmware, and train the model
     only on a sample. Use `--arith-phrasings 1` or change the ranges in `math_game()`.
  2. Keep the sums but mix `gume_train.txt` at a lower weight than `kid_train.txt`, so the arithmetic does not
     crowd out everything else.
- GRE vocabulary (for example "abscond" or "obsequious") is out of scope for 5-8 year olds and costs 2,500 pairs.
  Consider dropping it (comment out `gre_game` in `main()`) if capacity is tight.
- Numbers of 100 and above are written "one hundred and five". Check that the STT also emits "and". If it
  outputs "one hundred five", the questions will not match the training text.
