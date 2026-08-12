# Stage 6 Design — Grounding and Developmental Transfer

## Design objective

Stage 6 will provide a deterministic, resettable two-domain developmental world in which the same relational structure is expressed through different surface forms. The engine will learn form-to-percept, form-to-action, consequence, temporal, role, exception, uncertainty, and safety associations in Domain A, then adapt to Domain B with a bounded number of examples. The full engine will be compared with scratch, form-only, perception-only, task-specific memorizer, no-replay, and no-semantic-transfer controls.

The implementation is a transparent symbolic/sparse developmental substrate. It does not hide a speech, vision, or end-to-end foundation model behind a grounding API. Each adapter emits a typed observation carrying modality, tick, confidence, provenance, and a stable code. The engine maintains links between forms, percepts, actions, consequences, contexts, relations, and evidence.

## Component boundaries

| Component | Responsibility | Required invariant |
|---|---|---|
| `DevelopmentalEnvironment` | Reset, observe, act, feedback, describe, snapshot/restore | Invalid or unsafe actions never mutate state |
| `ModalityAdapter` | Convert text, visual, acoustic, and action outcomes to typed events | Every event has source, timestamp, confidence, and provenance |
| `SharedPointerSpace` | Assign stable concept/form/percept/action identifiers | IDs survive save/load and replay |
| `GroundingLedger` | Accumulate evidence, consequence prediction, repeatability, contradiction, promotion | Single-context co-occurrence cannot promote |
| `CurriculumScheduler` | Reproduce D1–D6 order with interleaved review and shifts | Same seed yields same schedule |
| `OptionLibrary` | Extract successful relational procedures | Options contain preconditions, effects, cost, risk, evidence |
| `DevelopmentalEngine` | Learn, retrieve, plan, act, abstain, checkpoint, restore | Full engine can be compared to explicit controls |
| `SafetyMonitor` | Detect invalid, unsafe, novel, and unsupported conditions | Unsafe block rate is measured on adversarial cases |

## Environment model

The deterministic environment is a small grid-world with two surface domains. Domain A uses color/object forms such as `red`, `blue`, `square`, and `circle`; Domain B uses symbolic forms such as `ka`, `mi`, `ta`, and `zo`. Both expose the same relational task: identify a movable target, move it to a goal, and avoid a hazard. Visual observations encode shape/color or symbol/texture separately so surface names and appearance can change without changing relational structure. Acoustic observations are deterministic token hashes; text observations carry the label; action outcomes provide consequence evidence.

The environment exposes `reset(seed)`, `observe()`, `act(command)`, `feedback()`, `describe()`, `snapshot()`, and `restore(snapshot)`. Commands include `NO_OP`, `OBSERVE`, `CLARIFY`, `MOVE`, and `ABSTAIN`. A move outside the grid, a move into a hazard, an unknown action, or a move without a grounded target is rejected without state mutation. A snapshot contains the complete state and can be hashed for exact restart verification.

## Grounding and learning algorithm

A concept record is keyed by a stable relational role rather than a surface label. Evidence updates are local and deterministic. A candidate is promotable only after observations occur in at least two contexts, with repeatability, cross-modal predictiveness, action utility, temporal consistency, and contradiction penalty recorded separately. Consequence prediction is computed from action-outcome evidence rather than form co-occurrence alone. A promoted option is indexed by relational goal and carries preconditions, effects, cost, risk, and evidence IDs.

The full engine uses replay of prior episode summaries and semantic-transfer mappings from Domain A. The scratch learner starts with no records. The form-only control maps labels to labels but cannot use visual, action, consequence, or relational evidence. The perception-only control ignores language forms. The memorizer keys on Domain A surface identities and is evaluated on unseen identities and Domain B. The no-replay control discards previous episodes at the domain boundary. The no-semantic-transfer control retains raw records but disables relational mapping.

## Curriculum and evaluation protocol

The scheduler emits deterministic episodes with interleaved review and distribution shifts. D1 measures new-label referent accuracy. D2 measures action selection and consequence prediction. D3 tests multi-step movement and temporal order. D4 supplies negation, exceptions, uncertainty, and unsafe requests. D5 changes appearance and entity identity. D6 changes the surface domain and evaluates relational transfer against scratch and surface matching.

The long-horizon protocol executes at least 100,000 environment ticks through compact episodes, with periodic replay, checkpoints, distribution shifts, and early-task probes. Metrics are collected at fixed checkpoints. Restart continuation saves during a developmental episode, restores into a fresh engine/environment pair, and compares final metrics and outcome hashes.

## Gate-to-artifact mapping

| Gate family | Evidence |
|---|---|
| Unit determinism/snapshot/provenance/ID/safety | `stage6_metrics.csv`, `environment_trace.csv`, `snapshot_hashes.csv`, `adapter_events.csv` |
| Grounding and consequence | `grounding_hypotheses.csv`, `consequence_ledger.csv`, `curriculum_schedule.csv` |
| Developmental thresholds | `developmental_metrics.csv`, `transfer_curves.csv`, `control_results.csv` |
| Long horizon and restart | `long_horizon_checkpoints.csv`, `restart_comparison.csv`, `state_hashes.csv` |
| Safety and OOD | `safety_log.csv`, `uncertainty.csv` |
| Resources and reproducibility | `resource_trace.csv`, `scenario_manifest.json`, `ctest.txt`, `sanitizer_ctest.txt`, `manifest.sha256` |

## Acceptance rule

All 22 executable Stage 6 gates (8 unit, 11 integration, and 3 operations) must pass in the deterministic harness, with the required control, safety, and long-horizon evidence artifacts present and consistent. The formal decision must explicitly state whether persistent multimodal learning, grounded language, developmental transfer, long-horizon retention, calibrated uncertainty, and safe executive behavior were demonstrated. It must state that Stage 7 is not started and await explicit user approval.
