#!/usr/bin/env bash
# Fine-tune TinyTalk 2 (8M) on kid + Gume Q&A on an Apple Silicon Mac (MPS).
#
#   unzip story_kid_bundle.zip && cd story_kid_bundle && bash tools/kid/mac_train.sh
#
# Produces models_out/kid/model_8m_kidgume/ (HF checkpoint) and train log.
# Copy that folder back to the Windows project; it is converted to the device
# format there (tools/kid/convert_to_device.ps1 flow).
set -euo pipefail
cd "$(dirname "$0")/../.."

EPOCHS="${EPOCHS:-4}"
BATCH="${BATCH:-64}"

if [ ! -d .venv ]; then
  python3 -m venv .venv
  .venv/bin/pip install -q --upgrade pip
  .venv/bin/pip install -q torch transformers safetensors
fi
.venv/bin/python -c "import torch; print('torch', torch.__version__, 'mps', torch.backends.mps.is_available())"

# Base TinyTalk 2 checkpoint + original chat/story corpus (replay data).
if [ ! -d .refs/cardputer-ai ]; then
  mkdir -p .refs
  git clone --depth 1 https://github.com/therezor/cardputer-ai.git .refs/cardputer-ai
fi

mkdir -p models_out/kid
PYTORCH_ENABLE_MPS_FALLBACK=1 .venv/bin/python tools/kid/finetune_kid.py \
  --train models_out/kid/kid_train.txt models_out/kid/gume_train.txt \
  --eval models_out/kid/kid_eval.jsonl models_out/kid/gume_eval.jsonl \
  --replay 30000 --epochs "$EPOCHS" --batch-size "$BATCH" --eval-n 400 \
  --out models_out/kid/model_8m_kidgume 2>&1 | tee models_out/kid/train_mac_log.txt

echo
echo "Done. Copy models_out/kid/model_8m_kidgume/ and models_out/kid/train_mac_log.txt back."
