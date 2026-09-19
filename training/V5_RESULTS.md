# v5 — full Braino parity, device-ready

`training/model_8m_kid_v5.zip` — 19,702,528 params, unchanged architecture and
GPT-2 tokenizer, so it converts with the existing device flow.

**Read this before testing: v5 scores LOWER overall than v4 (48.5% vs 64.2%).
It is pushed because it covers things v4 cannot answer at all, and because it
adds the refusal behaviour — not because it is better across the board.**

## What v5 is

Scope was taken from the Gume source, not from judgement. Measured in
`src/games/`:

  * 195 countries, 118 elements, 250 GRE words, 50 US states
  * Roman numerals to 3999
  * add/subtract, operands 1–99; times tables 1–12
  * **no division anywhere** — zero division questions exist in Gume, so v4's
    broken division was out of scope rather than a bug

18,602 facts, extracted from Gume's own data tables so the answers are ground
truth rather than invented. Plus 1,211 hand-written facts covering the 16
games that have no lookup table — chess, go, mazes, battleship and the rest,
which contributed **zero words** to every previous model.

## Numbers

| | v4 | **v5** |
|---|---|---|
| held-out exact | **64.2%** | 48.5% |
| weighted per-field | **63.3%** | 49.2% |
| arithmetic, unseen operand pairs | 80.7% | 80.7% |
| facts wrong under BOTH wordings | 0.8% | **0.0%** |
| phrasing gap | +29.2 pts | +45.0 pts |

The 0.0% is the important one. With 18,602 facts in a 6.8M-parameter
transformer body, **every fact is retrievable** — 99.2% correct under a
wording it was trained on. Nothing needs cutting for capacity. Every loss is
phrasing coverage, which is a data problem, not a model-size problem.

## Out-of-scope refusal

One canonical answer, so it is maximally learnable:

> I am not clever enough to answer that yet. I hope my makers teach me about
> that one day!

| | v5 | target |
|---|---|---|
| refuses out-of-scope questions | 42.7% | high |
| refuses things it was taught | 5.3% | zero |

Both need work. It still invents answers for near-misses — Braino teaches what
rain is, so "will it be sunny **tomorrow**" sits right beside a topic it knows
and it answered with a confident invention. It also wrongly refused real chess
questions. A model cannot know what it does not know; the behaviour is trained
and measured, never detected.

## Known broken in v5 — fixed in v6

Three pipeline stages that v4 had were missing from v5's build. These are
build errors, not consequences of full parity:

| field | v4 | v5 | cause |
|---|---|---|---|
| spelling | 60.0% | **1.7%** | spelling generator not run: 142 samples vs 1,424 |
| arithmetic small | 75.0% | **1.7%** | no floor balancing |
| time & calendar | 26.7% | **10.0%** | no floor balancing |
| animals | 53.3% | **18.3%** | no floor balancing |

2-digit arithmetic had 24,572 training samples against spelling's 142 — 173:1.
Nearly every gradient step taught arithmetic. v6 restores the spelling
generator and floor-balancing (every field lifted to ≥ 4,000 samples).

## What works well

| field | exact |
|---|---|
| comparisons | 95.0% |
| geography | 83.3% |
| part of speech | 80.0% |
| science & nature | 75.0% |
| arithmetic 2-digit | 66.7% |
| roman numerals | 66.7% |
| vocabulary | 65.0% |

Spot checks from the packaged model:

    what is the capital of japan  -> The capital of Japan is Tokyo.
    what is the symbol for gold   -> The symbol for gold is Au.
    how does a knight move        -> A knight moves in an L shape: two squares
                                     one way and one square to the side. It can
                                     jump over pieces.
    what is twenty three plus fourteen
                                  -> Three plus four is seven. Twenty plus ten
                                     is thirty. Thirty plus seven is
                                     thirty-seven. So twenty-three plus
                                     fourteen is thirty-seven.
    what is my name               -> I am not clever enough to answer that yet...

## Suggested testing focus

Worth exercising: the 16 games that never had data — chess, go, backgammon,
ludo, tic tac toe, battleship, mazes, memory, sliding puzzle, odd one out.
v4 could not answer any of them.

Expect weakness in: spelling, small arithmetic, time and calendar, animals
(all above), and out-of-scope questions that sit close to a taught topic.
