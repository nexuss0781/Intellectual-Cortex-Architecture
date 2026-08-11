# Stage 1 Transition Decision

## Decision

# PASS — Stage 1 Local Learning Kernel is complete and eligible for Stage 2

Stage 1 has been implemented, integrated into `BioEngine`, exercised by a deterministic continual-stream harness, and validated under both optimized and ASan/UBSan builds. All 17 Stage 1 harness tests are green, all 14 registered CTest targets are green, the required ablation matrix is present, and the final learning controller is bounded, deterministic, instrumented, and compatible with the Stage 0 substrate.

**Stage 2 implementation has not started.** The repository is stopped at this transition boundary as requested.

## What was implemented

The Stage 1 implementation adds a public `LearningController` with validated configuration, signed eligibility traces, causal and anti-causal timing windows, bounded three-factor updates, reward/prediction-error/novelty/task/executive modulation, precision and population plasticity scaling, homeostatic firing-rate gain, optional weight normalization, structural-pruning proposals, stable synapse IDs, disabled-slot pruning, and learning telemetry.

`BioEngine` now owns the controller and routes learning through it. Existing membrane dynamics are not reimplemented. BioEngine lifecycle initialization creates controller state; dopamine is translated into the default reward signal; explicit `LearningSignal` values are accepted; pre- and postsynaptic events are routed to the controller; direct weight mutation in the old engine path has been replaced by controller updates; topology baking preserves all Stage 1 synapse overlay fields; and the public metrics accessor exposes validated learning telemetry.

The implementation also adds `SparseCodebook`, a deterministic top-k sparse prototype module used by the pattern-separation integration task. It is intentionally a Stage 1 representation primitive, not a semantic-language system.

## Gate evaluation

| Gate | Required threshold | Final evidence | Result |
|---|---:|---|---|
| Unit and integration tests | 100% | `stage1_metrics.csv`: 17/17 pass | **PASS** |
| Full project CTest | 100% | `stage-0-revalidated/ctest.txt`: 14/14 pass | **PASS** |
| Sanitizer CTest | 100% | `sanitizer_ctest.txt`: 14/14 pass | **PASS** |
| Causal STDP | Positive and ≥5× anti-causal | `L1-UNIT-01`: causal trace `0.19801`; anti-causal `-0.019801` | **PASS** |
| Anti-causal LTD | Non-positive anti-causal update | `L1-UNIT-02`: `-0.019801` | **PASS** |
| Eligibility decay | Relative error ≤ `1e-5` for 1,000 ticks | `L1-UNIT-03`: worst error `1.96e-7` | **PASS** |
| Neutral modulation | No unnormalized weight change | `L1-UNIT-04` passed exactly | **PASS** |
| Reward selectivity | ≥20 percentage-point improvement | `L1-UNIT-05`: 45.238 percentage points | **PASS** |
| Bounds and finite state | No safety violation | `L1-UNIT-06`: 0 controller bound violations | **PASS** |
| Homeostasis | Within ±25% of target | `L1-UNIT-07`: 49.99995 Hz for 50 Hz target | **PASS** |
| Structural patience | No early proposal; proposal after patience | `L1-UNIT-08`: 4 qualifying proposals after 5 ticks; stable IDs preserved | **PASS** |
| Normalization | Configured outgoing mass bound | `L1-UNIT-09`: outgoing total ≤1.0 | **PASS** |
| Temporal association | Held-out target ≥75%; false prediction ≤20% | `L1-INT-01`: target probability 0.952381 | **PASS** |
| Nonstationary adaptation | New distribution ≥70% within 5,000 events | `L1-INT-02`: 0.952381 | **PASS** |
| Retention | Post-switch retention ≥80% | `L1-INT-03`: retention ratio 1.0 | **PASS** |
| Sparse separation | Noisy-code accuracy ≥85% | `L1-INT-04`: accuracy 1.0 | **PASS** |
| Determinism | 0 mismatches in 3 same-seed runs | `L1-OPS-01`: identical hash `7.11492111304e18` | **PASS** |
| Learning overhead | ≤2.5× matched maintenance path | `L1-OPS-02`: 1.91035× optimized final run; 2.09809× ASan run | **PASS** |
| Auxiliary memory | Measured below 512 MB | `L1-OPS-03`: approximately 66,584 KB incremental RSS | **PASS** |
| Ablation isolation | Required configurations present | `ablation.csv`: six required rows; reward-gated selectivity 39.9196 points vs 0 controls | **PASS** |
| Evidence telemetry | Curves, rates, distributions, counters | `task_curves.csv`, `firing_rate_trace.csv`, `weight_trace_histogram.csv`, `event_counts.csv` | **PASS** |

## Ablation interpretation

The ablation table demonstrates that the added modulation is functionally relevant rather than decorative. No-learning and plain-STDP controls produce zero reward selectivity in the controlled task. Reward-gated STDP and the full three-factor configuration produce approximately 39.92 percentage points of selectivity. Disabling homeostasis or structural plasticity does not remove the reward effect in this small task, which is expected: those mechanisms are safety and long-horizon control layers, while reward selectivity is primarily established by the modulatory factor.

| Configuration | Selectivity improvement |
|---|---:|
| No learning | 0.0 percentage points |
| Plain STDP | 0.0 percentage points |
| STDP plus reward | 39.9196 percentage points |
| Full three-factor learning | 39.9196 percentage points |
| Full learning without homeostasis | 39.9196 percentage points |
| Full learning without structural plasticity | 39.9196 percentage points |

## Correctness decisions

The controller uses a two-sided event rule. A presynaptic event records local timing and creates a positive eligibility contribution; if the postsynaptic event occurred first within the timing window, the anti-causal depression term is applied. When the postsynaptic event subsequently arrives after a recent presynaptic event, a causal contribution is added and the synapse becomes eligible for the modulatory update. Eligibility then decays exponentially and is bounded.

The controller does not allow global reward alone to update every active synapse. A causal gate is required, preventing an unrelated outgoing synapse from saturating merely because another transition received reward. This is a critical distinction between an actual local learning kernel and a global reward multiplier.

Structural plasticity is proposal-first. It never compacts arrays or changes existing IDs. A qualifying synapse is marked disabled only when its proposal is explicitly applied, preserving stable references for later episodic indexing and audit logs.

## Resource observations

The Stage 1 auxiliary state is measured, not estimated. The final 1,000,000-synapse and 270,000-neuron probe recorded approximately 66.6 MB incremental RSS in the optimized run. The ASan run recorded approximately 68.1 MB, remaining within the 512 MB Stage 1 auxiliary budget. The maximum Stage 0 substrate profile remains approximately 485 MB resident at 270K neurons and 13.5M synapses, so the combined Stage 0 plus future Stage 1/Stage 2 memory budget must be managed carefully.

## Limitations that remain explicit

Stage 1 does not yet provide durable episodic serialization, replay scheduling, semantic language, predictive workspace broadcast, causal reasoning, or embodied grounding. The sparse codebook is a controlled pattern-separation primitive, not a language model. Structural changes are stable disabled-slot proposals, not a full allocator or regrowth system. These are deliberate Stage 2 and later responsibilities, not hidden claims of the current stage.

The temporal and nonstationary tasks are controlled learning-rule environments. They prove local credit assignment, reward selectivity, bounded adaptation, retention, and deterministic telemetry. They do not prove human-level learning, language acquisition, or general intelligence.

## Evidence package

| Artifact | Purpose |
|---|---|
| `workflow.log` | Complete clean-build workflow output |
| `stage-0-revalidated/ctest.txt` | Regular 14-target project result, including Stage 0 and Stage 1 harnesses |
| `sanitizer_ctest.txt` | ASan/UBSan 14-target result |
| `stage1_harness.txt` | Human-readable Stage 1 harness output |
| `stage1_metrics.csv` | Machine-readable 17-test metrics |
| `stage1_summary.txt` | Seed, count, failure, and deterministic hash summary |
| `ablation.csv` | Required learning-mechanism ablations |
| `task_curves.csv` | Temporal and nonstationary learning curves |
| `firing_rate_trace.csv` | Homeostatic rate and gain trace |
| `weight_trace_histogram.csv` | Weight and eligibility distributions |
| `event_counts.csv` | Pre, post, causal, anti-causal, update, clipping, proposal, and bound counters |
| `config.json` | Versioned Stage 1 configuration |
| `environment.txt` | Compiler, CMake, kernel, date, and source metadata |
| `manifest.sha256` | Evidence checksums |

## Transition boundary

Stage 2 may begin only by consuming this `LearningController` and its telemetry. Stage 2 must not introduce a second incompatible weight-update path. The first Stage 2 task should be persistent state and replay integration, with state-preserving allocation and replay decisions built around the validated Stage 1 event and metrics interfaces.

This decision does **not** authorize silently skipping the Stage 2 harness. It authorizes only the transition from the validated Stage 1 learning kernel to the next explicitly gated stage.

## Final status

**Stage 0: PASS.**  
**Stage 1: PASS.**  
**Stage 2: NOT STARTED.**  
**Current boundary: STOPPED BEFORE STAGE 2.**
