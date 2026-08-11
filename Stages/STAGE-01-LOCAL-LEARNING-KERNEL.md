# Stage 1 — Local Learning Kernel

## Mission

Stage 1 turns the substrate’s existing STDP-like and dopamine-gated mechanisms into a deliberate, modular, measurable online learning system. The outcome is a kernel that can learn temporal patterns from a stream, adapt when statistics change, and quantify what it remembers and forgets.

The central learning rule is a three-factor update. Presynaptic and postsynaptic timing create a local eligibility trace; a modulatory signal determines whether the eligible change is useful; a bounded optimizer and homeostatic controller prevent runaway plasticity.

```text
Δw_ij(t) = η_ij × e_ij(t) × m(t) × q_ij(t)
```

Here `e_ij` is the local eligibility trace, `m` is the modulatory signal, and `q_ij` is a precision, confidence, or structural gate. The kernel must support positive and negative modulation. Learning must never depend on a global batch gradient or access to the entire training history.

## Entry and exit assumptions

Stage 0 must be `PASS`. The membrane equation, spike timing, refractory behavior, deterministic seed, state schema, and error handling are trusted. Stage 1 must not reimplement membrane dynamics. It may extend synapse fields, event routing, and controller logic through documented APIs.

Stage 1 exits with a `LearningController`, a replayable online benchmark harness, and evidence that the system learns a defined temporal task better than its no-learning and random-modulation controls without violating safety or resource limits.

## Scope

The implementation includes eligibility traces, causal and anti-causal timing, reward and prediction-error modulation, adaptive learning-rate control, synaptic normalization, homeostatic firing-rate regulation, structural pruning and regrowth proposals, and learning telemetry. It does not yet implement episodic serialization, semantic language, causal planning, or a global workspace.

## Module architecture

```text
Spike events ──► EligibilityEngine ──► Synapse traces
                                      │
Reward/error/novelty ──► Modulator ────┤
                                      ▼
                              WeightUpdater
                                      │
        firing statistics ──► Homeostat / Metaplasticity
                                      │
                                      ▼
                         bounded synapse state + telemetry
```

The modules must be separable so each learning mechanism can be disabled for ablation. The required public interface is:

```cpp
struct LearningSignal {
    float reward;
    float prediction_error;
    float novelty;
    float task_relevance;
    float executive_permission;
    uint64_t tick;
};

struct LearningConfig {
    float eta;
    float trace_decay;
    float a_plus;
    float a_minus;
    float weight_min;
    float weight_max;
    float target_rate_hz;
    float homeostatic_rate;
    float prune_threshold;
    uint32_t prune_patience_ticks;
};

class LearningController {
public:
    void on_pre_spike(uint32_t synapse_id, uint64_t tick);
    void on_post_spike(uint32_t synapse_id, uint64_t tick);
    void apply_modulation(const LearningSignal& signal);
    void update(SynapseBlock&, const NeuronBlock&, uint64_t tick);
    LearningMetrics metrics() const;
    void reset_traces();
};
```

The exact API may evolve, but no learning rule may directly mutate weights outside the controller after Stage 1.

## Algorithmic contracts

### Eligibility traces

For each plastic synapse, the trace must decay exponentially and receive separate causal and anti-causal contributions:

```text
e_ij(t+1) = λe_ij(t) + A_plus × pre_i(t) × post_j(t+δ)
                       − A_minus × post_j(t) × pre_i(t−δ)
```

The implementation may use event queues rather than dense spike histories. It must support a declared timing window and must not create a positive update for anti-causal order in the anti-causal control test.

### Modulation

The default modulatory signal is a bounded weighted combination:

```text
m(t) = clip(
    c_r × reward(t) +
    c_p × prediction_error(t) +
    c_n × novelty(t) +
    c_g × task_relevance(t) +
    c_e × executive_permission(t),
    m_min, m_max)
```

All terms and coefficients must be logged. The kernel must permit a neutral-modulator run in which weights remain unchanged except for explicitly declared normalization or homeostatic effects.

### Weight update and safety

```text
w_ij(t+1) = clip(w_ij(t) + η_ij × e_ij(t) × m(t) × q_ij(t), w_min, w_max)
```

The update must be numerically stable for the maximum declared event rate. The implementation must record how many updates were clipped, how many synapses were pruned, and the distribution of traces and weights.

### Homeostasis and metaplasticity

Each population must have a target firing rate. If activity exceeds the target over a monitoring window, excitability or learning gain must reduce gradually; if activity is too low, it may increase within bounded limits. Homeostatic control must not erase a learned pattern solely because it is sparse. The harness must distinguish firing-rate stabilization from representational collapse.

### Structural plasticity

Structural changes are proposals first. A synapse may be pruned only after its absolute weight remains below threshold for the patience interval and its removal does not violate minimum connectivity. New synapses must be allocated from an explicit budget and initialized through a reproducible seed. Structural changes must emit events and preserve stable synapse IDs or an auditable ID remap.

## Implementation work packages

| Work package | Deliverable |
|---|---|
| L1.1 | `LearningController` and configuration validation |
| L1.2 | Event-driven eligibility traces with causal and anti-causal windows |
| L1.3 | Three-factor modulatory signal and telemetry |
| L1.4 | Bounded weight update, normalization, and clipping counters |
| L1.5 | Homeostatic firing-rate and threshold adaptation |
| L1.6 | Structural-pruning proposal queue and deterministic regrowth |
| L1.7 | Stream benchmark generator and reference controls |
| L1.8 | Ablation runner, metrics writer, and stage decision generator |

## Evaluation harness

The main harness must use deterministic generated task streams whose full specification is stored in the manifest. The tasks are not random substitutes for real validation; they are controlled unit environments for learning-rule correctness.

### Task A: temporal association

Present pattern `A` followed by pattern `B` after a fixed delay, interleave distractors, and measure whether `A` predicts `B`. The stream must include a held-out delay and a changed distractor distribution. Report next-pattern accuracy, false prediction rate, adaptation time, and active synapse count.

### Task B: reward-gated association

Present two equally frequent sequence pairs, but reward only one. The system must increase the probability of the rewarded transition and not strengthen the unrewarded transition merely because it is frequent.

### Task C: nonstationary stream

Train on distribution A, switch to distribution B, then test A and B. The harness must measure forward transfer, adaptation latency, backward transfer, and forgetting.

### Task D: sparse pattern separation

Present noisy variants of patterns that should remain distinguishable. The system must learn stable sparse codes while preserving separation under corruption.

### Test matrix

| Test ID | Test | Pass condition |
|---|---|---|
| L1-UNIT-01 | Causal STDP | Mean causal update is positive and exceeds anti-causal update by at least 5× under isolated timing |
| L1-UNIT-02 | Anti-causal LTD | Anti-causal-only stream produces non-positive mean update |
| L1-UNIT-03 | Eligibility decay | Trace follows declared decay within `1e-5` relative error for 1,000 ticks |
| L1-UNIT-04 | Neutral modulation | With `m=0`, mean unnormalized weight delta is zero |
| L1-UNIT-05 | Reward selectivity | Rewarded transitions improve at least 20 percentage points over unrewarded controls after 10,000 events |
| L1-UNIT-06 | Bounds | No weight, trace, learning-rate, or rate variable leaves configured bounds |
| L1-UNIT-07 | Homeostasis | Population firing rate enters and remains within ±25% of target after stabilization |
| L1-UNIT-08 | Structural patience | A synapse is not pruned before its patience interval and is pruned after the qualifying interval |
| L1-UNIT-09 | Stable IDs | Learning and structural changes preserve or explicitly remap all referenced IDs |
| L1-INT-01 | Temporal association | Held-out delay accuracy ≥ 75%, false prediction rate ≤ 20% |
| L1-INT-02 | Nonstationary adaptation | New-distribution accuracy reaches 70% within 5,000 events |
| L1-INT-03 | Retention baseline | After learning B, distribution-A accuracy retains ≥ 80% of its pre-switch value |
| L1-INT-04 | Sparse separation | Noisy-pattern nearest-code accuracy ≥ 85% at the declared corruption level |
| L1-OPS-01 | Determinism | Same seed produces identical metric and final-state hashes in three runs |
| L1-OPS-02 | Throughput | Learning overhead is ≤ 2.5× Stage 0 kernel time at equal event load |
| L1-OPS-03 | Memory | Auxiliary learning state fits within the Stage 1 allocation budget and is measured, not estimated |

## Required ablations

The harness must run at least the following configurations: no learning, plain STDP, STDP plus reward, full three-factor learning, full learning without homeostasis, and full learning without structural plasticity. The full model must beat the no-learning control on the temporal association and reward selectivity tasks. If it does not beat plain STDP on at least one nonstationary or reward-gated task, the additional modulation is not justified and Stage 1 fails its scientific objective.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit and integration tests | 100% |
| Learning-rule safety violations | 0 |
| Reward-selectivity improvement over no-learning | ≥ 20 percentage points |
| Temporal held-out accuracy | ≥ 75% |
| Post-switch retention | ≥ 80% of pre-switch score |
| Homeostatic target adherence | Within ±25% after stabilization |
| Same-seed result mismatches | 0 of 3 |
| Learning overhead | ≤ 2.5× Stage 0 baseline |
| Unexplained clipped updates | 0; all clipping must be configured and reported |

The stage fails if the system reaches target accuracy only by saturating all neurons, if it memorizes event order without responding to reward, if it forgets all prior patterns after the distribution switch, or if the ablation harness cannot isolate the claimed mechanism.

## Evidence package

The Stage 1 artifact directory must contain per-task curves, weight and trace histograms, firing-rate traces, ablation results, event counts, memory measurements, seed and manifest hashes, and a `decision.md` that states which learning mechanisms are validated and which remain hypotheses.

## Transition to Stage 2

Stage 2 may begin only after the learning controller can learn and retain controlled temporal patterns from a stream, all state updates are bounded and deterministic, and the system exposes enough telemetry to decide which experiences deserve episodic storage. Stage 2 will add persistence and replay; it must use this learning controller rather than introduce a second incompatible weight-update path.

## References

[1]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/Nexuss-Neural-Cognition/src/bio_engine.cpp "Nexuss plasticity and spike propagation"

[2]: https://proceedings.neurips.cc/paper_files/paper/2022/hash/26f5a4e26c13d1e0a47f46790c999361-Abstract-Conference.html "Lifelong Neural Predictive Coding"
