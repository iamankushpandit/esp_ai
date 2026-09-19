#!/bin/zsh
# A/B on TinyStories-33M: direct vs worked-steps arithmetic answers.
# Same datasets as the 8M A/B so the comparison is like-for-like.
# --replay 0: the cardputer chat corpus belongs to the 8M base, not this one.
set -e
R=/Users/ankushpandit/esp_ai/training/story_kid_bundle
S=/private/tmp/claude-501/-Users-ankushpandit-esp-ai/9d6c0eed-639d-4555-bef2-601662424f8b/scratchpad
T=$S/t33
PY=$R/.venv/bin/python
BASE=$T/base_33m

# Never share the GPU: that halves throughput and invalidates timings.
while pgrep -f "[f]inetune_kid" >/dev/null; do sleep 30; done
echo "=== GPU free at $(date) ==="

for ST in direct steps; do
  while pgrep -f "[f]inetune_kid" >/dev/null; do sleep 30; done
  echo "=== 33M arm: $ST  start $(date +%s) ==="
  PYTORCH_ENABLE_MPS_FALLBACK=1 $PY /Users/ankushpandit/esp_ai/tools/kid33/finetune_kid33.py \
    --train $S/ab/math_${ST}_train.txt \
    --eval  $S/ab/math_${ST}_eval.jsonl \
    --base  $BASE \
    --out   $T/model_33m_$ST \
    --replay 0 --epochs 8 --batch-size 64 --eval-n 200 --save-every-epoch
  echo "=== 33M arm: $ST  done $(date +%s) ==="
done
echo "=== A/B training complete $(date) ==="
