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
SEED="${SEED:-424242}"

printf '%b\n' "${BOLD}[Nexuss] Clean reproducible validation${NC}"
printf '  root: %s\n  build: %s\n  artifacts: %s\n  seed: %s\n' "$ROOT_DIR" "$BUILD_DIR" "$ARTIFACT_DIR" "$SEED"

rm -rf "$BUILD_DIR" "$STAGE1_ARTIFACT_DIR" "$STAGE2_ARTIFACT_DIR"
mkdir -p "$ARTIFACT_DIR" "$STAGE1_ARTIFACT_DIR" "$STAGE2_ARTIFACT_DIR"

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

printf '%s\n' 'record_type,scale_neurons,synapses,rss_kb,formula_mb' > "$ARTIFACT_DIR/memory.csv"
for scale in 1000 10000 100000 270000; do
    "$BUILD_DIR/stage0_harness" --memory-probe --memory-scale "$scale" --synapses-per-neuron 5 >> "$ARTIFACT_DIR/memory.csv"
done
"$BUILD_DIR/stage0_harness" --memory-probe --memory-scale 270000 --synapses-per-neuron 50 >> "$ARTIFACT_DIR/memory.csv"

printf '%b\n' "${GREEN}[Nexuss] Workflow completed successfully${NC}"
