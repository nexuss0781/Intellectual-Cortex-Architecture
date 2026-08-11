# Stage 4 Transition Decision

## Decision

# PASS — Stage 4 Grounded Language Acquisition is complete and eligible for Stage 5

Stage 4 adds a genuine controlled language-learning loop above the validated Stage 3 predictive workspace. It learns token forms from streams, maintains type-level lexical evidence and concept senses, binds role fillers into structured scenes, induces reusable constructions with a complexity penalty, supports context-conditioned interpretation, provides provenance-aware decoding, accepts corrective feedback, and abstains on unknown or unauthorized commands.

**Stage 5 implementation has not started.** The repository is stopped at this transition boundary.

## Implemented architecture

`TokenEncoder` uses a declared and versioned `whitespace-lowercase-v1` tokenizer. Every token occurrence has an occurrence ID, lexical type ID, episode ID, sequence position, channel, form code, context pointer, and surface form. The vocabulary and corpus manifest are hashed and persisted.

`SemanticPointerAlgebra` implements typed 2,048-dimensional bipolar pointers with deterministic atomic generation, role-filler binding, bundling, permutation, unbinding, cleanup, similarity, metadata, generation counters, and checksums. Binding rejects non-role pointers and dimension mismatches unless an explicit polymorphic flag is supplied. Scene identity is based on role/filler content, not ephemeral example IDs.

`LanguageLearner` separates lexical forms, lexical senses, grounded concepts, token occurrences, structured scenes, constructions, episodic examples, and executable actions. Each lexical sense records concept identity, type, evidence, contradictions, and supporting episodes. Missing scene roles remain explicitly missing.

Construction induction records a form signature, category sequence, role sequence, exemplar lexical IDs, support, contradiction count, fit gain, complexity penalty, and confidence. The baseline promotion rule requires repeated support, positive penalized score, and a contradiction ratio below the promotion limit. One-off sentences are retained as evidence but are not promoted.

Interpretation uses the following precedence: context-conditioned scene, corrective override, promoted construction, exact episodic example, lexical fallback, and explicit abstention for unknown forms. Decoder responses identify `CONSTRUCTION`, `EPISODIC`, or `FALLBACK` provenance. Grounded commands require a known safe action and abstain on unauthorized, unsafe, or unknown commands.

## Gate evaluation

| Gate | Required threshold | Final evidence | Result |
|---|---:|---|---|
| Stage 4 harness | 100% | `stage4_metrics.csv`: 21/21 pass | **PASS** |
| Full project CTest | 100% | `workflow.log`: 17/17 pass | **PASS** |
| ASan/UBSan CTest | 100% | `sanitizer_ctest.txt`: 17/17 pass | **PASS** |
| Pointer clean recovery | Similarity ≥0.95 | `L4-UNIT-01`: similarity 1.0 | **PASS** |
| Pointer cleanup | ≥95% at declared corruption | `L4-UNIT-02`: 100/100 | **PASS** |
| Pointer type safety | No silent invalid aliasing | `L4-UNIT-03`: invalid role and dimension cases rejected | **PASS** |
| Position handling | Swapped order distinguishable | `L4-UNIT-04`: similarity 0.2715 | **PASS** |
| Lexical evidence persistence | Exact after save/load | `L4-UNIT-05`: evidence and state hash preserved | **PASS** |
| Missing-role uncertainty | No silent role filling | `L4-UNIT-06`: four required roles remain explicitly missing | **PASS** |
| Construction penalty | One-off not promoted | `L4-UNIT-07`: one-off remains unpromoted | **PASS** |
| Decoder provenance | Every output labelled | `L4-UNIT-08`: construction and fallback provenance observed | **PASS** |
| Token prediction advantage | ≥15 points over baseline | `L4-INT-01`: 0.50 advantage | **PASS** |
| Lexical grounding | ≥85% referent accuracy | `L4-INT-02`: 100% | **PASS** |
| Role binding | ≥85% held-out role accuracy | `L4-INT-03`: 100% | **PASS** |
| Compositional generalization | ≥70% unseen familiar combinations | `L4-INT-04`: 100% | **PASS** |
| Construction transfer | ≥60% held-out vocabulary transfer | `L4-INT-05`: 100% | **PASS** |
| Ambiguous reference | ≥15-point context advantage | `L4-INT-06`: 50-point advantage | **PASS** |
| Correction | ≥30% targeted error reduction | `L4-INT-07`: target corrected with unrelated pattern retained | **PASS** |
| Continual retention | ≥85% early baseline after 10 batches | `L4-INT-08`: full replay retained, no-replay control lost | **PASS** |
| Grounded command safety | ≥80% safe action, ≥95% abstention | `L4-INT-09`: safe action 100%, unsafe/unknown abstention 100% | **PASS** |
| Replay benefit | ≥10-point retention advantage | `L4-OPS-01`: 1.0 controlled benefit | **PASS** |
| Auxiliary memory budget | ≤2 MB declared budget | `L4-OPS-02`: 144,192 estimated bytes | **PASS** |
| Determinism | Exact same-seed state | `L4-OPS-03`: identical state hashes | **PASS** |
| Manifest leakage | Zero train/test intersection | `L4-OPS-04`: zero leaked compositions | **PASS** |

## Evaluation protocol

The harness declares disjoint train and test compositions. Training contains `red move cube`, `blue move sphere`, and `green move triangle`; testing includes `red move sphere`, `blue move triangle`, and `green move cube`. The intersection is checked mechanically and must remain empty.

The compositional test exposes familiar lexical components and a familiar construction in different combinations. The transfer test exposes a held-out color and shape through isolated examples, then evaluates a novel construction using those elements. The contextual-reference test presents identical token forms under two context pointers mapped to different subjects and compares against a context-free control.

The correction test first establishes an incorrect object interpretation, applies explicit negative feedback with a corrected scene, and verifies that the target changes while an unrelated sentence remains correct. The grounded-command test checks a safe `open door` action, an authorization-required `delete door` action, and an unknown `erase door` command.

## Replay and continual retention

The language retention protocol learns an early pattern, presents ten later batches, then compares a full replay learner with a control whose unconsolidated episodic, construction, contextual, and corrective state is cleared. The replay learner re-observes the early grounded example and retains the early interpretation; the no-replay control does not. This verifies the Stage 2 replay boundary is useful for language retention without claiming broad lifelong language competence.

## Persistence and safety

The language state stores tokenizer metadata, vocabulary, lexical entries, typed semantic pointers, lexical senses, construction proposals, evidence counts, contradiction counts, complexity penalties, confidence, and promotion state. Vocabulary serialization is canonicalized by sorted form order, and persisted pointer checksums are validated during load. The unit persistence gate verifies exact lexical evidence and state-hash preservation.

The exact final source passes the complete 17-target ASan/UBSan suite. Stage 0 through Stage 3 are re-run by the canonical workflow and remain green.

## Ablations and baselines

The artifact package compares frequency-only, context-free lexical, episodic-nearest-neighbor, semantic-pointer-without-construction, and full configurations. It also records the required construction, role-binding, workspace/replay, and correction controls. The full system is the only configuration allowed to claim novel compositional transfer because it combines lexical grounding, role structure, and reusable construction evidence.

## Resource observations

The Stage 4 language fixture reports 144,192 estimated auxiliary bytes after 300 lexical examples and remains below the declared 2 MB language budget. The budget is separate from the approximately 485 MB Stage 0 maximum substrate configuration. Future stages must continue to use compact lexical indexes, bounded constructions, sparse pointers, and replay references rather than duplicating full neuron state per token.

## Explicit limitations

Stage 4 is a controlled grounded-language engine, not human-level language, broad world knowledge, unrestricted dialogue, or evidence of general intelligence. The tokenizer is intentionally small and word-level. The decoder is compact and table-backed. The semantic-pointer layer is bipolar and deterministic rather than a complete learned cortical language representation. The construction learner is a controlled recurrence learner and does not yet model natural grammar, morphology, phonology, pragmatics, or long-range discourse.

The 100% compositional and transfer scores are measured on declared controlled splits. They demonstrate that the implementation composes familiar role-bearing elements and transfers a construction in the test environment; they do not establish open-domain language generalization.

## Evidence package

| Artifact | Purpose |
|---|---|
| `workflow.log` | Complete clean Stage 4 workflow output |
| `stage4_harness.txt` | Human-readable Stage 4 output |
| `stage4_metrics.csv` | Machine-readable 21-gate results |
| `stage4_summary.txt` | Seed, count, and failure count |
| `tokenizer_manifest.txt` | Tokenizer mode and vocabulary hash |
| `corpus_manifest.json` | Train/test/transfer split declaration |
| `lexical_growth.csv` | Vocabulary and memory growth curve |
| `construction_proposals.csv` | Construction support, penalty, and promotion trace |
| `prediction_curves.csv` | Learned prediction versus baseline trace |
| `scene_results.csv` | Scene accuracy and provenance trace |
| `correction_trace.csv` | Correction response trace |
| `replay_results.csv` | Replay versus no-replay retention result |
| `ablations.csv` | Required baseline and ablation matrix |
| `memory.csv` | Auxiliary memory estimate and budget |
| `language_state_unit.bin` | Persistence fixture |
| `sanitizer_ctest.txt` | ASan/UBSan 17-target result |
| `config.json` | Versioned Stage 4 configuration |
| `environment.txt` | Toolchain and source metadata |
| `manifest.sha256` | Evidence checksums |

## Transition boundary

Stage 5 may begin only by consuming typed `SceneGraph`, `SemanticPointer`, `LanguageHypothesis`, `Construction`, `ActionCommand`, and provenance-bearing `GeneratedResponse` objects. Reasoning must preserve the distinction between lexical, episodic, constructional, and fallback provenance. Executive control must treat unknown and unauthorized command abstention as a first-class safety outcome.

Stage 5 must add semantic reasoning, causal models, hierarchical planning, calibrated confidence, abstention beyond lexical unknowns, and executive strategy control. It must not replace the Stage 4 learner with an external chatbot wrapper or bypass the workspace and replay contracts.

## Final status

**Stage 0: PASS.**  
**Stage 1: PASS.**  
**Stage 2: PASS.**  
**Stage 3: PASS.**  
**Stage 4: PASS.**  
**Stage 5: NOT STARTED.**  
**Current boundary: STOPPED BEFORE STAGE 5.**
