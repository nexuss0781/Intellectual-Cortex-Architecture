# Stage 0 Transition Decision

## Decision

# PASS — eligible to transition to Stage 1, implementation halted pending user approval

Stage 0, **Substrate Stabilization**, has been implemented and evaluated end to end against the Stage 0 specification. All required regular-build tests, Stage 0 harness tests, sanitizer tests, workflow checks, and memory-probe artifacts passed for the final implementation. The repository is therefore technically eligible to begin Stage 1, but no Stage 1 code has been implemented. The next action is intentionally blocked until the project owner approves the transition.

## Implementation completed

The implementation corrected the UIN conductance convention so excitation depolarizes and inhibition hyperpolarizes; moved conductance decay before the refractory early-exit so conductances decay on every tick; strengthened invalid-range and invalid-synapse handling with typed exceptions; introduced a versioned `StateSchema` attached to `NeuronBlock`; made UIN overlay activation explicit; strengthened the membrane-direction regression test; registered all executable validations with CTest; replaced the stale `genesis_sim` workflow reference with the actual CMake target; added the Stage 0 harness; added reproducible memory probes; and added a versioned Stage 0 configuration manifest.

The harness also caught and corrected two validation defects during implementation. The first was a decay test failure caused by refractory ticks skipping conductance decay. The second was sanitizer-only nondeterminism caused by hashing a `std::vector<bool>` proxy rather than a logical boolean. The final harness writes valid four-column CSV metrics and includes an explicit state-schema test.

## Gate evaluation

| Gate | Required condition | Final evidence | Result |
|---|---|---|---|
| Core and UIN tests | 100% pass | `ctest.txt`: 13/13 tests passed | **PASS** |
| Stage 0 harness | 100% pass | `stage0_metrics.csv`: 11/11 harness tests passed | **PASS** |
| Physical excitation direction | 0 violations in 10,000 trials | `S0-PHY-01`: 10,000 randomized bounded cases; minimum depolarization `0.0288009643555 mV` | **PASS** |
| Physical inhibition direction | 0 violations in 10,000 trials | `S0-PHY-02`: 10,000 randomized bounded cases; minimum hyperpolarization `0.00226593017578 mV` | **PASS** |
| Membrane equation | Error ≤ `1e-5 mV` in 100 cases | `S0-PHY-03`: worst error `0` in final harness | **PASS** |
| Threshold boundary | Documented threshold behavior | `S0-PHY-04`: exact boundary spike passed | **PASS** |
| Refractory behavior | No illegal spike; exact timer recovery | `S0-PHY-05` and UIN tests passed | **PASS** |
| Conductance decay | Exponential decay within tolerance | `S0-PHY-06` and `ConductanceDecayMathematics` passed | **PASS** |
| Long-run boundedness | 1,000,000 stress ticks without non-finite/escaped state | `S0-PHY-07`: 1,000,000 ticks passed | **PASS** |
| Same-seed determinism | 0 mismatches in 3 repetitions | `S0-DET-01`: same hash across three runs | **PASS** |
| Seed separation | At least 95% distinct traces | `S0-DET-02`: 20/20 distinct-seed traces differed | **PASS** |
| State schema | UIN overlay matches versioned contract | `S0-SCHEMA-01` passed | **PASS** |
| Invalid-input safety | 0 silent failures | `S0-ERR-01`: invalid range, count, index, null synapse, and post ID all raised typed errors | **PASS** |
| Full workflow | Clean build and executable workflow exit 0 | `workflow.log`: completed successfully | **PASS** |
| Regular memory coverage | Four representative scales plus maximum profile recorded | `memory.csv`: 1K, 10K, 100K, 270K×5, and 270K×50 probes | **PASS** |
| Sanitizer safety | ASan/UBSan with zero errors | `sanitizer_ctest.txt`: 13/13 tests passed | **PASS** |
| Metric serialization | Machine-readable artifact | `stage0_metrics.csv`: 11 rows, 0 malformed rows | **PASS** |

## Resource observations

The direct resident-memory probes recorded the following final values:

| Configuration | Resident memory | Repository formula |
|---|---:|---:|
| 1,000 neurons × 5 synapses | 3,568 KB | 0.2720 MB |
| 10,000 neurons × 5 synapses | 5,704 KB | 2.7199 MB |
| 100,000 neurons × 5 synapses | 27,344 KB | 27.1988 MB |
| 270,000 neurons × 5 synapses | 68,528 KB | 73.4367 MB |
| 270,000 neurons × 50 synapses | 485,104 KB | 499.8436 MB |

The maximum configuration is therefore close to the nominal 500 MB formula and leaves little practical headroom for future episodic indexes, semantic pointers, replay queues, instrumentation, or serialization buffers. This is recorded as a design constraint for Stage 1 and is not treated as a Stage 0 failure.

## Scope boundaries and deferred work

Stage 0 proves substrate correctness, reproducibility, safety, and workflow integrity. It does not prove continual learning, persistent memory, replay, language acquisition, reasoning, grounding, or state-preserving dynamic reallocation. The existing meta-cognition controller still contains the documented future limitation that network reconfiguration recreates the engine instead of migrating learned state. That issue belongs to Stage 2 and must not be hidden by the Stage 0 pass.

The full-scale UIN tests still contain readiness checks that validate field presence and routing rather than higher cognition. Their pass is correctly interpreted here as substrate validation only.

## Required approval boundary

No Stage 1 implementation, learning-controller code, learning benchmark, or Stage 1 transition test has been started under this decision. Proceeding requires explicit project-owner approval. The exact approval request is:

> **Approve transition to Stage 1 — Local Learning Kernel.**

Until that approval is received, the repository remains at the Stage 0 decision boundary.

## Evidence files

The canonical artifact directory contains:

| Artifact | Purpose |
|---|---|
| `workflow.log` | Complete clean-build workflow output |
| `ctest.txt` | Regular 13-test CTest result |
| `sanitizer_ctest.txt` | ASan/UBSan 13-test result |
| `stage0_harness.txt` | Human-readable Stage 0 harness output |
| `stage0_metrics.csv` | Machine-readable 11-test metrics |
| `stage0_summary.txt` | Seed, count, failure, and deterministic hash summary |
| `memory.csv` | Resident-memory probes and formula estimates |
| `config.json` | Stage 0 configuration copy |
| `environment.txt` | Compiler, CMake, kernel, date, and source HEAD metadata |
| `manifest.sha256` | Evidence checksums |

## Reproduction command

From `Nexuss-Neural-Cognition/`:

```bash
BUILD_DIR="$PWD/build-stage0-canonical" \
ARTIFACT_DIR="$PWD/artifacts/stage-0-canonical" \
SEED=424242 \
./build_and_run.sh
```

The sanitizer command is:

```bash
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
ctest --test-dir build-stage0-asan --output-on-failure
```

## Final status

**Stage 0: PASS.**  
**Stage 1: NOT STARTED.**  
**Transition: WAITING FOR PROJECT-OWNER APPROVAL.**
