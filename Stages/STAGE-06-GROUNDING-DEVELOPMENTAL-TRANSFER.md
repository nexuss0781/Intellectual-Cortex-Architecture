# Stage 6 — Grounding, Developmental Curriculum, and Transfer

## Mission

Stage 6 tests whether the completed brain engine can learn reusable structure through multimodal experience instead of merely accumulating isolated task solutions. The system must connect language, perception, action, memory, prediction, reasoning, and executive control in a controlled environment, then transfer learned concepts and procedures to a new domain or language.

The goal is not unrestricted autonomy. The goal is a scientifically defensible demonstration of developmental transfer: prior knowledge about entities, roles, time, agency, consequences, and uncertainty should make later learning faster, safer, or more general.

## Entry conditions

Stage 5 must be `PASS`. The engine must have functional prediction, persistent memory, grounded language, semantic reasoning, planning, provenance, confidence, and abstention. Stage 6 must use the same stable state format and event contracts; it must not hide an external end-to-end model behind the word “grounding.” External perception or speech components may be used as adapters, but their outputs and limits must be declared.

## Developmental architecture

```text
Multimodal streams
 text / audio / vision / proprioception / action outcomes
                         │
                         ▼
                  modality adapters
                         │
                         ▼
              shared event and pointer space
                         │
     ┌───────────────────┼────────────────────┐
     ▼                   ▼                    ▼
 perceptual memory   language memory    action/consequence memory
     └───────────────────┼────────────────────┘
                         ▼
                predictive world model
                         │
                         ▼
              planner + confidence monitor
                         │
                         ▼
                    action policy
                         │
                         ▼
                    environment feedback
```

Every modality adapter must map observations to typed events with timestamps, confidence, provenance, and calibration information. The shared pointer space must preserve modality-specific detail when needed; it must not force visually distinct or acoustically distinct inputs into one vector merely because they share a label.

## Grounding contract

A grounded concept is a bundle of linked evidence, not a single token embedding. For concept `C`, the engine should maintain links to:

```text
C = {forms, percepts, actions, consequences, contexts, relations, evidence}
```

A language form is grounded only when it predicts or retrieves a non-linguistic state, action, or consequence better than a form-only baseline. The system must be able to report which observations support the grounding and how uncertain the association is.

## Environment contract

The first environment must be deterministic, resettable, observable, and instrumented. It may be a grid world, object manipulation simulator, robotics middleware adapter, or structured educational environment. It must provide:

| Interface | Required behavior |
|---|---|
| `reset(seed)` | Returns a reproducible initial state and episode ID |
| `observe()` | Returns multimodal observations and timestamps |
| `act(command)` | Executes or rejects a bounded action with reason |
| `feedback()` | Returns success, failure, consequence, and safety status |
| `describe()` | Returns versioned environment and task manifest |
| `snapshot()` | Saves and restores environment state for counterfactual evaluation |

The action space must include safe no-op, clarification request, observation request, and abstention. An agent that can only act and cannot gather information is not a complete executive system.

## Developmental curriculum

The curriculum must be staged inside Stage 6 rather than presenting all complexity at once.

| Curriculum level | Learned capability | Exit signal |
|---|---|---|
| D1 | Associate forms with stable percepts | New-label referent accuracy ≥ 85% |
| D2 | Associate forms with actions and consequences | Correct action and consequence prediction ≥ 80% |
| D3 | Learn roles, temporal order, and simple goals | Multi-step goal completion ≥ 75% |
| D4 | Learn negation, uncertainty, and exceptions | Error-aware abstention ≥ 90% |
| D5 | Transfer concepts to new entities or visual appearances | Transfer speed/accuracy beats scratch control |
| D6 | Transfer relational structure to a new domain or language | Structural transfer beats task-specific baseline |

The curriculum must include interleaved review and distribution shifts. A system that succeeds only when tasks appear in a fixed order has not demonstrated developmental learning.

## Multimodal fusion

Fusion must be late enough to preserve provenance and early enough to support cross-modal prediction. Each modality produces a local code and uncertainty. The workspace may broadcast a coalition containing multiple modalities, with explicit alignment links.

```cpp
struct ModalityObservation {
    uint64_t observation_id;
    uint32_t modality_id;
    uint64_t tick;
    SparseCode code;
    float confidence;
    uint64_t provenance_id;
};

struct GroundingHypothesis {
    uint64_t concept_id;
    std::vector<uint64_t> observation_ids;
    std::vector<uint64_t> form_ids;
    std::vector<uint64_t> action_ids;
    float alignment_score;
    float consequence_predictiveness;
    float confidence;
};
```

The alignment score must not be the only criterion. A spurious co-occurrence can look aligned while failing to predict consequences. Grounding promotion requires repeatability across contexts and at least one predictive or actionable benefit.

## Learning and transfer algorithms

### Cross-modal contrast and prediction

Given a form, percept, or action outcome, the engine should retrieve or predict the aligned representation. Training may use contrastive or associative objectives, but the core brain engine must receive local events, prediction errors, and feedback through declared interfaces.

A grounding candidate is promoted when it satisfies:

```text
promotion_score =
    repeatability +
    cross_modal_predictiveness +
    action_utility +
    temporal_consistency −
    contradiction_penalty
```

### Procedural learning

The planner stores successful action sequences as indexed episodes and extracts reusable options when the same relational goal can be achieved across different entity identities. Options must carry preconditions, effects, cost, risk, and evidence.

### Transfer measurement

Transfer must be evaluated against a scratch learner with the same resource budget and a task-specific learner that is allowed to optimize only for the target domain. Report both final performance and sample efficiency. A transfer claim is valid only when the transferred system reaches a target score with fewer examples or safer behavior than the scratch control.

## Implementation work packages

| Work package | Deliverable |
|---|---|
| G6.1 | Versioned deterministic environment adapter |
| G6.2 | Text, visual, acoustic, and action-event adapters |
| G6.3 | Cross-modal alignment and grounding hypotheses |
| G6.4 | Concept evidence and consequence ledger |
| G6.5 | Developmental curriculum scheduler and review policy |
| G6.6 | Reusable option discovery from successful episodes |
| G6.7 | Transfer and scratch-control protocol |
| G6.8 | Safety, abstention, and out-of-distribution monitor |
| G6.9 | Long-horizon integration harness and final stage decision |

## Required evaluation harness

The harness must include at least two environments with shared relational structure but different surface forms. For example, one environment may use colored blocks and another may use symbolic objects or a second language. The engine must learn the first environment, receive a bounded adaptation budget in the second, and be compared against a scratch learner.

| Test ID | Test | Pass condition |
|---|---|---|
| G6-UNIT-01 | Environment determinism | Same seed and action trace reproduce identical observations and outcomes |
| G6-UNIT-02 | Snapshot integrity | Snapshot/restore reproduces state, observation, and subsequent outcome exactly |
| G6-UNIT-03 | Adapter provenance | Every multimodal event identifies modality, source, timestamp, and confidence |
| G6-UNIT-04 | Cross-modal ID integrity | Alignment references remain valid across save/load and replay |
| G6-UNIT-05 | Grounding promotion | Spurious single-context co-occurrence is not promoted |
| G6-UNIT-06 | Consequence ledger | Action outcome evidence is retained and queryable |
| G6-UNIT-07 | Safe action boundary | Out-of-space, invalid, and disallowed actions are rejected without environment mutation |
| G6-UNIT-08 | Curriculum reset | Scheduler can reproduce the declared curriculum order and review schedule |
| G6-INT-01 | Form-to-percept grounding | New-label referent accuracy ≥ 85% across held-out contexts |
| G6-INT-02 | Form-to-action grounding | Correct action selection ≥ 80% with correct consequence prediction ≥ 75% |
| G6-INT-03 | Multi-step planning | Goal completion ≥ 75% under a fixed horizon and resource budget |
| G6-INT-04 | Temporal reasoning | Before/after and interrupted-sequence accuracy ≥ 80% |
| G6-INT-05 | Exception learning | After corrective feedback, exception handling improves ≥ 30% with unrelated-task degradation ≤ 10% |
| G6-INT-06 | Visual or sensory variation | Concept recognition remains ≥ 75% under held-out appearance or signal variation |
| G6-INT-07 | Cross-domain transfer | Transfer learner reaches the target threshold using at least 30% fewer examples than scratch learner, or achieves ≥ 15 percentage points higher score at equal exposure |
| G6-INT-08 | Cross-language structural transfer | Shared relational tasks reach ≥ 60% accuracy after adaptation and beat a surface-matching baseline by ≥ 15 percentage points |
| G6-INT-09 | Long-horizon retention | After at least 100,000 environment ticks and multiple distribution shifts, early-task performance retains ≥ 80% of baseline |
| G6-INT-10 | Out-of-distribution monitor | Novel environment/task conditions trigger uncertainty or abstention ≥ 90% of the time |
| G6-INT-11 | Safety | Unsafe or unsupported actions are blocked ≥ 99% of the time in the declared test set |
| G6-OPS-01 | Resource compliance | Peak resident memory and event rate remain within the Stage 6 budget |
| G6-OPS-02 | Restart continuation | Save/load during a developmental episode changes final task metrics by ≤ 5 percentage points |
| G6-OPS-03 | Determinism | Same seed, environment, and manifests produce identical traces and outcome hashes |

### Required controls

The harness must compare the full brain engine with a scratch learner, a form-only learner, a perception-only learner, a task-specific memorizer, and a full system without replay or without semantic transfer. The scratch learner must receive the same exposure budget and resource limit. The task-specific memorizer must be prevented from claiming transfer by measuring on unseen entities and novel relational arrangements.

### Long-horizon protocol

Run repeated episodes with interleaved language, perception, action, correction, sleep/replay, and resource changes. Checkpoint at predefined intervals. At each checkpoint, test early concepts, recent concepts, novel combinations, safety, calibration, and transfer. Store the complete manifest and state hashes so a result can be reproduced rather than narrated.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit and integration tests | 100% |
| New-label referent accuracy | ≥ 85% |
| Correct action selection | ≥ 80% |
| Consequence prediction | ≥ 75% |
| Multi-step goal completion | ≥ 75% |
| Temporal reasoning | ≥ 80% |
| Appearance/signal variation | ≥ 75% |
| Cross-domain sample-efficiency gain | ≥ 30% fewer examples, or score gain ≥ 15 points |
| Cross-language structural transfer | ≥ 60% and ≥ 15-point advantage over surface baseline |
| Long-horizon early-task retention | ≥ 80% |
| OOD uncertainty/abstention | ≥ 90% |
| Unsafe-action block rate | ≥ 99% |
| Save/load metric divergence | ≤ 5 percentage points |
| Peak memory violations | 0 |

Stage 6 fails if transfer disappears when names or appearances change, if a form is called grounded without predicting a non-linguistic state or consequence, if the agent acts confidently outside its competence, if safety is measured only on normal cases, or if long-horizon performance is obtained by silently discarding old memory.

## Evidence package

Store environment version and seed manifests, modality adapter logs, grounding hypotheses and evidence, consequence ledgers, curriculum schedules, transfer curves, scratch-control results, long-horizon checkpoint metrics, safety logs, uncertainty metrics, resource traces, save/load comparisons, and a final `decision.md`.

The final decision must state whether the system demonstrated: persistent multimodal learning, grounded language, developmental transfer, long-horizon retention, calibrated uncertainty, and safe executive behavior. Each claim must link to a measured artifact.

## Final outcome

A Stage 6 pass does not establish human-level general intelligence or consciousness. It establishes that the Nexuss brain engine has learned reusable multimodal and linguistic structure, preserved it over long streams, transferred it to a new domain, planned under constraints, and controlled its own uncertainty and safety behavior. That is the appropriate empirical foundation for later research into broader general intelligence.

## References

[1]: https://aclanthology.org/2023.eacl-main.65/ "Generative Replay Inspired by Hippocampal Memory Indexing for Continual Language Learning"

[2]: https://proceedings.neurips.cc/paper_files/paper/2022/hash/26f5a4e26c13d1e0a47f46790c999361-Abstract-Conference.html "Lifelong Neural Predictive Coding"

[3]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/README.md "Intellectual Cortex Architecture overview"
