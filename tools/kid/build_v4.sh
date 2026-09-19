#!/bin/zsh
# Build the v4 training mix from everything measured so far.
#
#   1. phrasing   3.3 -> ~13 wordings per fact. diag_phrasing.py measured an
#                 85% / 26% split between trained and held-out wordings, so
#                 this is the single biggest lever.
#   2. spelling   71 distinct words -> 356, and "f u n" instead of "F, U, N"
#                 (35% fewer tokens, and the commas taught nothing).
#   3. arithmetic regenerated in whichever answer style won the A/B, with
#                 systematic coverage and held-out operand PAIRS.
#   4. balance    arithmetic was 83% of all samples and spelling 0.3%. Every
#                 category is resampled toward an equal share.
#
# Usage: tools/kid/build_v4.sh <direct|steps> <out-dir>
set -e
STYLE=${1:?need math style: direct or steps}
OUT=${2:?need output dir}

cd "$(dirname "$0")/../.."
R=training/story_kid_bundle
PY=$R/.venv/bin/python
K=$R/models_out/kid

mkdir -p "$OUT"

echo "=== 1/4 spelling (356 words, space-separated letters) ==="
$PY tools/kid/gen_spelling_data.py --out "$OUT"

echo "=== 2/4 arithmetic, style=$STYLE, held-out operand pairs ==="
$PY tools/kid/gen_math_data.py --style "$STYLE" --out "$OUT" --suffix v4

echo "=== 3/4 phrasing augmentation on knowledge + jokes ==="
# One call PER SOURCE. balance_data classifies partly by source filename
# (joke_*, school_*), so augmenting everything into one file erased those
# categories - they all collapsed into "other", which ballooned 16k -> 51k.
# Keeping the names keeps the categories.
for src in kid gume school joke spelling; do
  case $src in
    spelling) T="$OUT/spelling_train.txt"; E="$OUT/spelling_eval.jsonl" ;;
    joke)     T=$K/joke_train.txt;         E=$K/joke_eval.jsonl ;;
    *)        T=$K/${src}_train_knowledge.txt; E=$K/${src}_eval_knowledge.jsonl ;;
  esac
  $PY tools/kid/augment_phrasings.py --train "$T" --eval "$E" \
      --out-dir "$OUT" --prefix "${src}_aug" --per-fact 14
done

echo "=== 4/4 category balance ==="
# --floor-only: augmentation already equalised exposure per FACT, so this only
# lifts categories that are small AND hard (spelling, jokes) rather than
# shrinking the fact-rich ones.
$PY tools/kid/balance_data.py "$OUT"/*_aug_train.txt "$OUT/math_v4_train.txt" \
    --target 3000 --floor-only --out "$OUT/v4_train.txt"

# The eval deliberately stays UNbalanced: held-out wordings for knowledge,
# held-out operand pairs for arithmetic.
cat "$OUT"/*_aug_eval.jsonl "$OUT/math_v4_eval.jsonl" > "$OUT/v4_eval.jsonl"

echo
echo "train: $OUT/v4_train.txt  ($(grep -c '^User:' "$OUT/v4_train.txt") samples)"
echo "eval:  $OUT/v4_eval.jsonl ($(wc -l < "$OUT/v4_eval.jsonl") questions)"
