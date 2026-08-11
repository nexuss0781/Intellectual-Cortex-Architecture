# Stage 3 — Predictive Processing and Global Workspace

## Mission

Stage 3 turns isolated neural populations and memories into a coordinated cognitive workspace. The system must maintain predictions at multiple timescales, compute normalized prediction errors, select information using bottom-up surprise and top-down goals, modulate precision, and broadcast a bounded coalition to modules that need global access.

The result is not consciousness. It is an experimentally defined information-routing mechanism that can be evaluated on prediction, selection, ambiguity resolution, and reportable broadcast. Any later claim about awareness or self-modeling must not be inferred from this stage alone.

## Entry conditions

Stage 2 must be `PASS`. Episodic records, replay, consolidation candidates, stable IDs, and state persistence must work. The Stage 3 implementation must use the Stage 2 event and pointer contracts, and its predictions must be serializable as part of `BrainState`.

## Functional architecture

```text
                         ┌─────────────────────┐
                         │ Executive goal/query │
                         └──────────┬──────────┘
                                    ▼
Input populations ──► local predictors ──► precision weighting
       │                    │                    │
       │                    ▼                    ▼
       └──────────────► prediction errors ──► coalition competition
                                                     │
                                                     ▼
                                          workspace ignition/broadcast
                                                     │
                   ┌────────────────────────────────┼─────────────────────┐
                   ▼                                ▼                     ▼
              memory query                      planner             language module
```

Each population must expose a local prediction, an observed code, a prediction error, a precision estimate, a salience score, a goal-bias score, and a workspace eligibility score. A module may not broadcast arbitrary internal state; it must submit a typed workspace proposal.

## Mathematical contracts

### Hierarchical prediction

For level `l`, the predictor estimates the lower-level code:

```text
μ_l(t+1) = f_l(z_l(t), context_l(t), goal(t))
e_l(t) = z_(l−1)(t) − μ_l(t)
```

The error must be normalized by an adaptive scale to avoid a large-dimensional population dominating selection:

```text
ê_l(t) = e_l(t) / (σ_l(t) + ε)
```

A precision-weighted error is:

```text
p_l(t) = clip(exp(−log_variance_l(t)), p_min, p_max)
q_l(t) = p_l(t) × ê_l(t)
```

The implementation may use a rate code, sparse pointer, or event code, but it must declare the dimensionality and norm used by every prediction and error.

### Surprise and salience

Surprise must be computed from an observable discrepancy, not from an arbitrary activity threshold. A practical approximation is:

```text
surprise(x_t) = ||ê_t||_2^2 + κ × novelty_t
```

The salience proposal is:

```text
salience_i =
    a × surprise_i +
    b × goal_bias_i +
    c × uncertainty_i +
    d × expected_information_gain_i −
    e × cost_i
```

All terms must be logged. The weights may be learned later, but Stage 3 requires a deterministic baseline.

### Competition and ignition

At each workspace cycle, proposals compete under a declared capacity `K`. The coalition must satisfy a minimum coherence threshold and a maximum broadcast budget. Ignition occurs only when the coalition score exceeds `Θ_ignite` for `n` consecutive ticks or a documented event-triggered equivalent. A proposal that loses competition must not appear in the broadcast trace.

The broadcast record is:

```cpp
struct WorkspaceBroadcast {
    uint64_t broadcast_id;
    uint64_t tick;
    uint32_t winner_count;
    float coalition_score;
    float ignition_margin;
    uint64_t goal_id;
    std::vector<uint64_t> proposal_ids;
};
```

### Precision control

Precision is a gain on prediction error, not a synonym for confidence. The system must maintain both. A precision update should be bounded and slow enough to avoid oscillation:

```text
π(t+1) = clip(π(t) + α × (π_target(t) − π(t)), π_min, π_max)
```

Confidence is an executive estimate of correctness, evaluated later. Stage 3 must test that increasing precision increases the effect of an error on selection, while confidence may remain low when evidence is ambiguous.

## Implementation work packages

| Work package | Deliverable |
|---|---|
| W3.1 | Typed prediction, error, surprise, and precision events |
| W3.2 | Local predictor interface with multi-timescale context |
| W3.3 | Error normalization and adaptive variance estimates |
| W3.4 | Goal bias and information-gain proposal scoring |
| W3.5 | Bounded competition and workspace coalition selection |
| W3.6 | Ignition state machine and broadcast log |
| W3.7 | Precision controller separated from confidence monitor |
| W3.8 | Ambiguity, distractor, and goal-switch evaluation harness |

## Required interfaces

```cpp
struct PredictionProposal {
    uint64_t proposal_id;
    uint64_t source_population;
    uint64_t context_id;
    SparseCode prediction;
    SparseCode observation;
    float normalized_error;
    float precision;
    float surprise;
    float goal_bias;
    float expected_information_gain;
    float cost;
};

class PredictivePopulation {
public:
    PredictionProposal predict(const CognitiveContext&);
    void observe(const SparseCode&);
    void learn(const LearningSignal&);
    void set_precision(float);
};

class GlobalWorkspace {
public:
    void submit(PredictionProposal);
    std::optional<WorkspaceBroadcast> step(uint64_t tick);
    WorkspaceState state() const;
};
```

## Evaluation harness

Stage 3 uses controlled streams with known latent variables, distractors, ambiguity, and changing goals. The input manifest must specify the latent sequence, distractor schedule, goal schedule, and expected selected content.

| Test ID | Test | Pass condition |
|---|---|---|
| W3-UNIT-01 | Prediction error | Error equals observation minus prediction under sparse, dense, zero, and sign-reversed inputs within tolerance |
| W3-UNIT-02 | Error normalization | Population scale changes do not change normalized error ordering beyond declared tolerance |
| W3-UNIT-03 | Precision gain | Doubling precision increases downstream error influence by 1.8–2.2× in an isolated test |
| W3-UNIT-04 | Precision bounds | Precision never leaves `[p_min,p_max]` or produces NaN |
| W3-UNIT-05 | Proposal scoring | Scores reproduce the declared equation for 10,000 random proposals |
| W3-UNIT-06 | Capacity | Workspace broadcasts no more than `K` proposals per cycle |
| W3-UNIT-07 | Ignition hysteresis | Borderline coalitions do not chatter across ignition states beyond the configured hysteresis bound |
| W3-UNIT-08 | Broadcast isolation | Non-winning proposals do not appear in the broadcast payload |
| W3-UNIT-09 | Goal override | A top-down goal can select a lower-salience target when the declared goal bias exceeds threshold |
| W3-INT-01 | Prediction learning | Held-out next-event prediction improves over the untrained baseline by ≥ 20 percentage points or ≥ 30% relative error reduction |
| W3-INT-02 | Distractor rejection | Goal-relevant target selection ≥ 85% with distractors occupying at least 70% of input events |
| W3-INT-03 | Ambiguity resolution | Context-conditioned interpretation ≥ 80% and exceeds context-free control by ≥ 15 percentage points |
| W3-INT-04 | Goal switch | After a goal change, selected content changes within 20 workspace cycles without uncontrolled global activation |
| W3-INT-05 | Novelty response | Novel events produce higher salience than matched familiar events in ≥ 90% of paired trials |
| W3-INT-06 | Broadcast utility | A downstream task using workspace broadcasts beats a local-only control by ≥ 15 percentage points |
| W3-OPS-01 | Broadcast budget | Average and peak broadcast payload stay within configured byte/event budget |
| W3-OPS-02 | Determinism | Same seed and input manifest produce identical proposal and broadcast hashes |
| W3-OPS-03 | State persistence | Save/load preserves predictor, precision, workspace, and context state within declared tolerance |

### Negative controls and ablations

Run bottom-up-only, top-down-only, no-precision, no-normalization, no-workspace, and full configurations. The full configuration must improve goal-directed selection over bottom-up-only and improve ambiguity resolution over a context-free predictor. If the workspace broadcasts everything, it fails regardless of downstream accuracy because it violates the information-bottleneck contract.

### Confidence separation test

Stage 3 must include an explicit paired test in which precision is high but the evidence is ambiguous. Precision may increase error influence, but the future confidence module must not be allowed to label the result certain merely because precision is high. This prevents a common conflation between attention gain and epistemic confidence.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit and integration tests | 100% |
| Goal-directed distractor rejection | ≥ 85% |
| Context-conditioned ambiguity accuracy | ≥ 80% |
| Context advantage over context-free baseline | ≥ 15 percentage points |
| Novelty paired-trial ordering | ≥ 90% |
| Goal-switch response | ≤ 20 cycles |
| Broadcast capacity violations | 0 |
| Unintended broadcast items | 0 |
| Same-seed trace mismatch | 0 of 3 |
| State persistence divergence | ≤ 5% on declared metrics |

Stage 3 fails if the model obtains accuracy by broadcasting all proposals, if precision cannot be distinguished from confidence, if top-down goals cannot override distractors, or if prediction errors are not normalized across populations.

## Evidence package

Store proposal traces, normalized-error distributions, precision trajectories, workspace broadcasts, ignition-state transitions, distractor and ambiguity results, ablations, memory/throughput measurements, and the stage decision. Include a visualization or machine-readable trace showing that the broadcast is selective.

## Transition to Stage 4

Stage 4 may begin only when the system can learn and use predictions, select goal-relevant information, and broadcast a bounded cognitive context. The language module will use this workspace to bind tokens, roles, temporal context, and grounded concepts.

## References

[1]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/README.md "Intellectual Cortex Architecture overview"

[2]: https://proceedings.neurips.cc/paper_files/paper/2022/hash/26f5a4e26c13d1e0a47f46790c999361-Abstract-Conference.html "Lifelong Neural Predictive Coding"
