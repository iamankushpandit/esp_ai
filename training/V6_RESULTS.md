# v6 — full Braino parity, balanced

`training/model_8m_kid_v6.zip` — 19,702,528 params, unchanged architecture and
GPT-2 tokenizer, so it converts with the existing device flow.

v6 is v5 with three pipeline stages restored that v5's build had dropped.
Those were my build errors, not a cost of full parity.

## v4 → v5 → v6

| | v4 | v5 | **v6** |
|---|---|---|---|
| held-out exact | **64.2%** | 48.5% | 53.0% |
| weighted per-field | **63.3%** | 49.2% | 51.9% |
| refusal rate (want high) | — | 42.7% | **52.1%** |
| over-refusal (want zero) | — | 5.3% | **4.0%** |
| arithmetic, unseen pairs | 80.7% | 80.7% | **84.0%** |
| fractions & percentages | — | — | **17.5%** |
| facts wrong under BOTH wordings | 0.8% | **0.0%** | 0.8% |
| phrasing gap | +29.2 | +45.0 | **+53.3** |

v4 still leads on the headline, but v4 holds a much narrower fact set. v6
carries 18,602 facts from Gume's own tables plus 1,211 hand-written ones, and
answers the 16 games that had no data at all before.

## What the balancing fixed, and what it cost

Restored in v6: the spelling generator, floor-balancing (every field lifted to
at least 4,000 training samples), and contrastive refusal examples.

Recovered:

| field | v5 | v6 |
|---|---|---|
| letters & phonics | 16.7% | **66.7%** |
| spelling | 1.7% | **20.0%** |
| body | 90.0% | **100%** |
| colors & shapes | 61.7% | **76.7%** |
| space | 60.0% | **76.7%** |
| periodic table | 48.3% | **65.0%** |
| roman numerals | 66.7% | **76.7%** |
| vocabulary | 65.0% | **73.3%** |

Paid for by the big fields, which I under-weighted when I chose floor-only:

| field | v5 | v6 |
|---|---|---|
| arithmetic 2-digit | 66.7% | **46.7%** |
| money | 30.0% | **18.3%** |
| geography | 83.3% | **73.3%** |
| comparisons | 95.0% | **81.7%** |

Floor-balancing repeats small-category samples, and those gradient steps come
out of the large categories. Net weighted gain was +2.7 points — real, but far
smaller than the per-field swings look.

## The dominant problem, now unambiguous

| | v6 |
|---|---|
| answered correctly, wording it was TRAINED on | **98.3%** |
| answered correctly, wording HELD OUT | **45.0%** |
| facts wrong under BOTH wordings | **0.8%** |

Every fact is in the model. It is right 98.3% of the time when asked the way
it was taught and wrong more than half the time when a child rephrases. The
gap is +53.3 points and it is where the accuracy is going — not capacity, not
scope, not model size.

Balancing made this *worse* (+45.0 → +53.3), because repeating samples adds
exposure without adding variety. v7 raises phrasings per fact from 14 to 20.

## Known bad in v6

**Fractions and percentages: 17.5%.** Confidently wrong, and the derivation is
not self-consistent:

    can you work out half of seventy-four
      -> "Half means splitting into two equal groups. Twenty splits into ten
          and ten. So half of twenty is ten."          (want thirty-seven)

The training data is correct — it contains "a third of twelve is four". The
model confuses sparse, similar facts and invents the numbers, the same failure
as v4's division. v7 enumerates all 251 fraction/percent facts, each asserted
against its computed value.

Also weak: small arithmetic (5.0%), animals (13.3%), money (18.3%), time &
calendar (18.3%), word problems (38.3%).

## v5 or v6?

| ask about | use |
|---|---|
| two-digit adding, comparisons, capitals | **v5** |
| spelling, letters, space, shapes, roman numerals, elements | **v6** |
| chess and the other 15 games, jokes, greetings, science | either |
| fractions, percentages, money, time, animals | **neither** — wait for v7 |

Whichever you use, ask the exact wording. Both are ~98% on trained wordings
and ~45–52% on rephrasings.
