#!/usr/bin/env bash

# Nexuss reproducible validation workflow.
# Stage 0 requires a clean build, complete CTest coverage, demo smoke tests,
# and an explicit artifact directory. The script fails on the first error.
set -euo pipefail

BOLD='\033[1m'
GREEN='\033[0;32m'
NC='\033[0m'

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
ARTIFACT_DIR="${ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-0}"
STAGE1_ARTIFACT_DIR="${STAGE1_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-1}"
STAGE2_ARTIFACT_DIR="${STAGE2_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-2}"
STAGE3_ARTIFACT_DIR="${STAGE3_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-3}"
STAGE4_ARTIFACT_DIR="${STAGE4_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-4}"
STAGE5_ARTIFACT_DIR="${STAGE5_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-5}"
STAGE6_ARTIFACT_DIR="${STAGE6_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-6}"
STAGE7_ARTIFACT_DIR="${STAGE7_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-7}"
STAGE6_ENTRY_ARTIFACT_DIR="${STAGE6_ENTRY_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-6-canonical}"
STAGE8_ARTIFACT_DIR="${STAGE8_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-8}"
STAGE7_ENTRY_ARTIFACT_DIR="${STAGE7_ENTRY_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-7-canonical}"
STAGE9_ARTIFACT_DIR="${STAGE9_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-9}"
STAGE8_ENTRY_ARTIFACT_DIR="${STAGE8_ENTRY_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-8-canonical}"
STAGE10_ARTIFACT_DIR="${STAGE10_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-10}"
STAGE9_ENTRY_ARTIFACT_DIR="${STAGE9_ENTRY_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-9-canonical}"
STAGE11_PREPARATION_ARTIFACT_DIR="${STAGE11_PREPARATION_ARTIFACT_DIR:-${ROOT_DIR}/artifacts/stage-11-preparation}"
SEED="${SEED:-424242}"

printf '%b\n' "${BOLD}[Nexuss] Clean reproducible validation${NC}"
printf '  root: %s\n  build: %s\n  artifacts: %s\n  seed: %s\n' "$ROOT_DIR" "$BUILD_DIR" "$ARTIFACT_DIR" "$SEED"

rm -rf "$BUILD_DIR" "$STAGE1_ARTIFACT_DIR" "$STAGE2_ARTIFACT_DIR" "$STAGE3_ARTIFACT_DIR" "$STAGE4_ARTIFACT_DIR" "$STAGE5_ARTIFACT_DIR" "$STAGE6_ARTIFACT_DIR" "$STAGE7_ARTIFACT_DIR" "$STAGE8_ARTIFACT_DIR" "$STAGE9_ARTIFACT_DIR" "$STAGE10_ARTIFACT_DIR" "$STAGE11_PREPARATION_ARTIFACT_DIR"
mkdir -p "$ARTIFACT_DIR" "$STAGE1_ARTIFACT_DIR" "$STAGE2_ARTIFACT_DIR" "$STAGE3_ARTIFACT_DIR" "$STAGE4_ARTIFACT_DIR" "$STAGE5_ARTIFACT_DIR" "$STAGE6_ARTIFACT_DIR" "$STAGE7_ARTIFACT_DIR" "$STAGE8_ARTIFACT_DIR" "$STAGE9_ARTIFACT_DIR" "$STAGE10_ARTIFACT_DIR" "$STAGE11_PREPARATION_ARTIFACT_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-RelWithDebInfo}"
cmake --build "$BUILD_DIR" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

printf '%b\n' "${BOLD}[Nexuss] Running all registered CTest targets${NC}"
ctest --test-dir "$BUILD_DIR" --output-on-failure | tee "$ARTIFACT_DIR/ctest.txt"

printf '%b\n' "${BOLD}[Nexuss] Running executable smoke tests${NC}"
"$BUILD_DIR/nexuss_sim" | tee "$ARTIFACT_DIR/nexuss_sim.txt"
"$BUILD_DIR/meta_cognition_demo" | tee "$ARTIFACT_DIR/meta_cognition_demo.txt"
"$BUILD_DIR/test_meta_cognition_v2" | tee "$ARTIFACT_DIR/meta_cognition_v2.txt"
"$BUILD_DIR/stage0_harness" --seed "$SEED" --artifact-dir "$ARTIFACT_DIR" | tee "$ARTIFACT_DIR/stage0_harness.txt"
"$BUILD_DIR/stage1_harness" --seed "$SEED" --artifact-dir "$STAGE1_ARTIFACT_DIR" | tee "$STAGE1_ARTIFACT_DIR/stage1_harness.txt"
"$BUILD_DIR/stage2_harness" --seed "$SEED" --artifact-dir "$STAGE2_ARTIFACT_DIR" | tee "$STAGE2_ARTIFACT_DIR/stage2_harness.txt"
"$BUILD_DIR/stage3_harness" --seed "$SEED" --artifact-dir "$STAGE3_ARTIFACT_DIR" | tee "$STAGE3_ARTIFACT_DIR/stage3_harness.txt"
"$BUILD_DIR/stage4_harness" --seed "$SEED" --artifact-dir "$STAGE4_ARTIFACT_DIR" | tee "$STAGE4_ARTIFACT_DIR/stage4_harness.txt"
"$BUILD_DIR/stage5_harness" --seed "$SEED" --artifact-dir "$STAGE5_ARTIFACT_DIR" | tee "$STAGE5_ARTIFACT_DIR/stage5_harness.txt"
"$BUILD_DIR/stage6_harness" --seed "$SEED" --artifact-dir "$STAGE6_ARTIFACT_DIR" | tee "$STAGE6_ARTIFACT_DIR/stage6_harness.txt"
"$BUILD_DIR/stage7_harness" --seed "$SEED" --artifact-dir "$STAGE7_ARTIFACT_DIR" --repo-root "$ROOT_DIR" --entry-evidence-dir "$STAGE6_ENTRY_ARTIFACT_DIR" | tee "$STAGE7_ARTIFACT_DIR/stage7_harness.txt"
"$BUILD_DIR/stage8_harness" --seed "$SEED" --artifact-dir "$STAGE8_ARTIFACT_DIR" --repo-root "$ROOT_DIR" --entry-evidence-dir "$STAGE7_ENTRY_ARTIFACT_DIR" | tee "$STAGE8_ARTIFACT_DIR/stage8_harness.txt"
"$BUILD_DIR/stage9_harness" --seed "$SEED" --artifact-dir "$STAGE9_ARTIFACT_DIR" --repo-root "$ROOT_DIR" --entry-evidence-dir "$STAGE8_ENTRY_ARTIFACT_DIR" | tee "$STAGE9_ARTIFACT_DIR/stage9_harness.txt"
"$BUILD_DIR/stage10_harness" --seed "$SEED" --artifact-dir "$STAGE10_ARTIFACT_DIR" --repo-root "$ROOT_DIR" --entry-evidence-dir "$STAGE9_ENTRY_ARTIFACT_DIR" | tee "$STAGE10_ARTIFACT_DIR/stage10_harness.txt"
"$BUILD_DIR/stage10_sft_harness" --seed "$SEED" --artifact-dir "$STAGE10_ARTIFACT_DIR" --repo-root "$ROOT_DIR" | tee "$STAGE10_ARTIFACT_DIR/stage10_sft_harness.txt"
"$BUILD_DIR/stage11_preparation_harness" --seed "$SEED" --artifact-dir "$STAGE11_PREPARATION_ARTIFACT_DIR" --repo-root "$ROOT_DIR" | tee "$STAGE11_PREPARATION_ARTIFACT_DIR/stage11_preparation_harness.txt"
( cd "$STAGE11_PREPARATION_ARTIFACT_DIR" && find . -type f ! -name 'manifest.sha256' ! -name 'artifact_hashes.fnv64' -print0 | sort -z | xargs -0 sha256sum > manifest.sha256 )

printf '%s\n' 'record_type,scale_neurons,synapses,rss_kb,formula_mb' > "$ARTIFACT_DIR/memory.csv"
for scale in 1000 10000 100000 270000; do
    "$BUILD_DIR/stage0_harness" --memory-probe --memory-scale "$scale" --synapses-per-neuron 5 >> "$ARTIFACT_DIR/memory.csv"
done
"$BUILD_DIR/stage0_harness" --memory-probe --memory-scale 270000 --synapses-per-neuron 50 >> "$ARTIFACT_DIR/memory.csv"

printf '%b\n' "${GREEN}[Nexuss] Workflow completed successfully${NC}"
