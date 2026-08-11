# Stage 3 Transition Decision

## Decision

# PASS — Stage 3 Predictive Workspace is complete and eligible for Stage 4

Stage 3 converts the validated Stage 2 memory/replay substrate into a selective predictive workspace. It now provides typed sparse codes, local prediction, normalized prediction error, adaptive error scales, precision-weighted error, salience scoring, goal bias, information-gain scoring, bounded competition, ignition hysteresis, selective broadcast, confidence separation, consolidation-candidate attachment, deterministic traces, and explicit predictive-workspace persistence.

**Stage 4 implementation has not started.** The repository is stopped at this transition boundary.

## Implemented architecture

`PredictivePopulation` maintains a declared code dimensionality, a default prediction, context-conditioned transition predictions, adaptive per-dimension error scales, bounded precision, the previous and pending observed codes, the latest cognitive context, and proposal sequencing. For each observation it computes

```text
normalized_error = sqrt(mean((observation - prediction)^2 / (scale + epsilon)^2))
precision_weighted_error = precision × normalized_error
surprise = mean(raw_error^2) + novelty_weight × normalized_error
```

Precision is updated with a bounded slow controller:

```text
precision_next = clip(precision + alpha × (target - precision), p_min, p_max)
```

Confidence remains a separate proposal field derived from precision-weighted error. High precision therefore increases the influence of ambiguous evidence without automatically making the evidence certain.

The deterministic baseline salience equation is

```text
salience = surprise + goal_bias + 0.25 × uncertainty
           + 0.50 × expected_information_gain - 0.10 × cost
```

`GlobalWorkspace` sorts proposals deterministically by salience and proposal ID, selects no more than `K`, requires both a coalition threshold and coherence threshold, ignites after the configured consecutive-tick requirement, and uses a lower hysteresis threshold to prevent chatter. Only winning proposal IDs are emitted in a `WorkspaceBroadcast`. The byte budget is checked before broadcast publication.

The predictive state format is explicit and independent of compiler struct padding. It persists population configuration-relevant state, prediction transitions, adaptive scales, precision, context IDs and goal code, previous/pending observations, proposal sequencing, workspace configuration, ignition state, broadcast sequencing, and consolidation-candidate fields. The Stage 3 persistence gate compares a state hash that includes configuration, context, predictor state, workspace state, and candidate state.

## Gate evaluation

| Gate | Required threshold | Final evidence | Result |
|---|---:|---|---|
| Stage 3 harness | 100% | `stage3_metrics.csv`: 19/19 pass | **PASS** |
| Full project CTest | 100% | `workflow.log`: 16/16 pass | **PASS** |
| ASan/UBSan CTest | 100% | `sanitizer_ctest.txt`: 16/16 pass | **PASS** |
| Prediction error equation | Exact within declared epsilon tolerance | `W3-UNIT-01`: sparse, zero, and sign-reversed cases pass | **PASS** |
| Error normalization | Scale-invariant ordering | `W3-UNIT-02`: normalized error difference approximately 1e-5 | **PASS** |
| Precision gain | 1.8–2.2× for doubled precision | `W3-UNIT-03`: exactly 2.0× | **PASS** |
| Precision bounds | No NaN/out-of-range values | `W3-UNIT-04`: final bounded precision 10.0 | **PASS** |
| Proposal scoring | 10,000/10,000 equation matches | `W3-UNIT-05`: maximum score error 0 | **PASS** |
| Workspace capacity | No more than `K` winners | `W3-UNIT-06`: 3 winners at capacity 3 | **PASS** |
| Ignition hysteresis | No borderline chatter | `W3-UNIT-07`: borderline coalition remains stable and later extinguishes | **PASS** |
| Broadcast isolation | Zero losing proposals in payload | `W3-UNIT-08`: two winners, losing proposal excluded | **PASS** |
| Goal override | Top-down goal selects lower bottom-up target | `W3-UNIT-09`: goal-biased proposal wins using declared equation | **PASS** |
| Precision/confidence separation | High precision does not imply certainty | `W3-UNIT-10`: precision 8.0, ambiguity confidence approximately 0.111 | **PASS** |
| Prediction learning | ≥20 percentage-point improvement | `W3-INT-01`: baseline-to-trained error improvement 0.5263 | **PASS** |
| Distractor rejection | ≥85% with 70% distractors | `W3-INT-02`: 100% goal-directed selection | **PASS** |
| Ambiguity resolution | ≥80% and ≥15-point context advantage | `W3-INT-03`: 100% contextual accuracy, 50-point advantage | **PASS** |
| Goal switch | ≤20 workspace cycles | `W3-INT-04`: response within 1 cycle | **PASS** |
| Novelty response | ≥90% paired ordering | `W3-INT-05`: 100% novel-over-familiar ordering | **PASS** |
| Broadcast utility | ≥15-point downstream improvement | `W3-INT-06`: 100-point improvement over local-only control | **PASS** |
| Broadcast budget | Zero capacity violations | `W3-OPS-01`: 88-byte payload within 512-byte budget | **PASS** |
| Determinism | 0 mismatches across 3 same-seed traces | `W3-OPS-02`: identical trace hash `744627057891` | **PASS** |
| State persistence | ≤5% declared divergence | `W3-OPS-03`: predictor, context, workspace, and candidate state hash equivalent | **PASS** |

## Evaluation protocol

The harness uses a declared input manifest containing the seed, latent alternating sequence, distractor ratio, goal schedule, and workspace capacity. Proposal traces log tick, proposal ID, surprise, precision, precision-weighted error, goal bias, information gain, salience, and confidence. Workspace traces log broadcast ID, tick, winner count, coalition score, ignition margin, goal ID, and only selected proposal IDs. Precision trajectories, ablations, and the input manifest are retained as independent artifacts.

The distractor protocol presents seven high-surprise distractors against one goal-biased target. The target wins using the same salience equation implemented by the workspace, not a manually supplied winner score. The downstream utility control presents the same target without top-down goal bias; the full workspace selects it in all trials while the local-only control selects it in none.

The ambiguity protocol compares context-conditioned goal selection with a context-free tie control. Contextual selection reaches 100%; the context-free control is 50%, producing a 50-point advantage. This verifies that the workspace can use context to resolve identical bottom-up evidence.

## Persistence and safety

Predictive state persistence is transaction-like at the object boundary: the loader reconstructs populations, workspace state, and consolidation candidates in temporary objects and publishes them only after the complete stream is valid. State serialization avoids raw `WorkspaceConfig`, `WorkspaceState`, and `ConsolidationCandidate` layout writes. Dimension bounds, transition-count bounds, candidate-count bounds, precision bounds, finite-value checks, workspace threshold checks, and magic/version checks are enforced.

The exact final source passes the complete 16-target ASan/UBSan suite. Stage 0, Stage 1, and Stage 2 are re-run by the canonical workflow and remain green.

## Ablations

The artifact package includes bottom-up-only, top-down-only, no-precision, no-normalization, no-workspace, and full configurations. The full configuration is the only one allowed to claim the combined workspace behavior. A system that broadcasts all proposals would fail the Stage 3 contract even if downstream accuracy improved; the broadcast trace proves the current implementation emits only bounded winning coalitions.

## Resource observations

The Stage 3 workspace uses compact proposal vectors and a bounded broadcast payload. The measured fixture payload is 88 bytes for three winner IDs plus the fixed broadcast record. This is deliberately separate from the Stage 0 substrate maximum of approximately 485 MB for 270K neurons and 13.5M synapses. Future stages must preserve the selective information bottleneck and must not attach unbounded language or semantic metadata to every neuron.

## Explicit limitations

Stage 3 is an experimentally defined information-routing mechanism, not evidence of consciousness, subjective awareness, or general intelligence. The predictor currently uses compact rate-code vectors and context-conditioned moving-average transitions. The persistence format is a predictive-workspace state container, not yet merged into the Stage 2 `BrainState` section registry. The generative and semantic capabilities required for language acquisition are not implemented here. Confidence is represented and tested as a separate field, but calibration against real-world correctness belongs to Stage 5.

The 100% goal-directed and workspace-utility values are controlled benchmark results. They show that the architecture can select the declared target under the supplied task structure; they do not establish robustness to naturalistic distributions or adversarial ambiguity.

## Evidence package

| Artifact | Purpose |
|---|---|
| `workflow.log` | Complete clean Stage 3 workflow output |
| `stage3_harness.txt` | Human-readable Stage 3 output |
| `stage3_metrics.csv` | Machine-readable 19-gate result set |
| `stage3_summary.txt` | Seed, count, failure count, and trace hash |
| `proposal_trace.csv` | Per-proposal predictive and salience telemetry |
| `workspace_broadcasts.csv` | Selective broadcast trace |
| `precision_trajectories.csv` | Precision-controller trajectory |
| `ablation.csv` | Required ablation matrix |
| `input_manifest.json` | Declared latent stream, distractors, and goals |
| `predictive_workspace.bin` | Persisted predictive/workspace state |
| `sanitizer_ctest.txt` | ASan/UBSan 16-target result |
| `config.json` | Versioned Stage 3 configuration |
| `environment.txt` | Toolchain and source metadata |
| `manifest.sha256` | Evidence checksums |

## Transition boundary

Stage 4 may begin only by consuming the Stage 3 `PredictionProposal`, `PredictivePopulation`, `GlobalWorkspace`, `WorkspaceBroadcast`, and typed Stage 2 replay/consolidation contracts. The language module must bind tokens, roles, temporal context, and grounded concepts through bounded workspace broadcasts rather than bypassing selection with an unrestricted side channel.

Stage 4 must add compositional semantic pointers, construction/grammar learning, grounded commands, and language-specific evaluation. It must preserve the Stage 3 distinction between precision and confidence, the broadcast capacity bound, same-seed determinism, and predictive-state persistence.

## Final status

**Stage 0: PASS.**  
**Stage 1: PASS.**  
**Stage 2: PASS.**  
**Stage 3: PASS.**  
**Stage 4: NOT STARTED.**  
**Current boundary: STOPPED BEFORE STAGE 4.**
