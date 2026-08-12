#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
export PYTHONUNBUFFERED=1

CONFIG="${CONFIG:-config.json}"
OUTPUT_ROOT="${OUTPUT_ROOT:-artifacts/colab-large-sft}"
DATA_DIR="${DATA_DIR:-${OUTPUT_ROOT}/data}"
RUN_DIR="${RUN_DIR:-${OUTPUT_ROOT}/run}"
FORCE_DATA="${FORCE_DATA:-0}"
RESUME="${RESUME:-0}"

if [[ "${SKIP_INSTALL:-0}" != "1" ]]; then
  python3 -m pip install -q -U -r requirements.txt
fi

if [[ "$FORCE_DATA" == "1" ]]; then
  python3 prepare_dataset.py --config "$CONFIG" --output-dir "$DATA_DIR" --force
else
  python3 prepare_dataset.py --config "$CONFIG" --output-dir "$DATA_DIR"
fi

python3 verify_artifacts.py --config "$CONFIG" --data-dir "$DATA_DIR" --run-dir "$RUN_DIR"

if [[ "$RESUME" == "1" ]]; then
  export RESUME_FROM_CHECKPOINT=auto
fi

python3 train_eval.py --config "$CONFIG" --data-dir "$DATA_DIR" --output-dir "$RUN_DIR"
python3 verify_artifacts.py --config "$CONFIG" --data-dir "$DATA_DIR" --run-dir "$RUN_DIR" --require-run

printf '\nCOLAB_LARGE_SFT=PASS\n'
printf 'DATA_DIR=%s\n' "$DATA_DIR"
printf 'RUN_DIR=%s\n' "$RUN_DIR"
printf 'SUMMARY=%s/run_summary.json\n' "$RUN_DIR"
printf 'VERIFICATION=%s/verification.json\n' "$RUN_DIR"
printf 'ADAPTER=%s/adapter\n' "$RUN_DIR"
