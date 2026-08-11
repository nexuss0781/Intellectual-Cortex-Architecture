# Stage 2 — Persistent Memory and Selective Replay

## Mission

Stage 2 makes the system capable of remembering experiences across time, process restarts, new learning, and resource reallocation. It implements fast episodic capture, compact hippocampal indexing, selective replay, slow semantic consolidation hooks, and a versioned persistent `BrainState` format.

The stage is successful only if learned behavior survives a save/load cycle and remains substantially intact after learning new material. A fresh network with the same neuron count is not an acceptable substitute for preserved memory.

Research on hippocampal memory indexing and generative replay motivates storing compact cues that can select or reconstruct relevant prior experiences rather than replaying undirected noise.[1] The implementation should use this as an engineering hypothesis and test it against no-replay, random-replay, and full-replay controls.

## Entry conditions

Stage 1 must be `PASS`. The learning controller must expose bounded, deterministic updates, stable neuron and synapse identifiers, and telemetry for prediction error, novelty, reward, and activity. Stage 2 may add memory-specific plasticity but must not create an untracked alternative learning path.

## Memory model

The memory subsystem has four layers with different lifetimes:

| Layer | Content | Lifetime | Required representation |
|---|---|---:|---|
| Working memory | Active task context, pointers, unresolved predictions | Ticks to seconds | Bounded ring or attractor slots |
| Episodic memory | Individual experiences and temporal context | Seconds to indefinite | Append-only records with stable IDs |
| Replay index | Compact cues for selecting episodes | Indefinite | Sparse key, context, error, utility, and checksum |
| Semantic consolidation queue | Candidate stable concepts and relations | Until consolidated | Aggregated pointer and evidence statistics |

The hippocampal index must not be confused with the episode payload. An index is a retrieval cue. An episode contains the event sequence, context, outcomes, and provenance needed for replay or audit.

## Persistent data contracts

```cpp
struct EventHeader {
    uint64_t event_id;
    uint64_t tick;
    uint32_t source_module;
    uint32_t event_type;
    uint64_t payload_id;
    uint64_t provenance_id;
};

struct EpisodeRecord {
    uint64_t episode_id;
    uint64_t start_tick;
    uint64_t end_tick;
    uint64_t context_pointer_id;
    uint64_t event_offset;
    uint32_t event_count;
    float novelty;
    float prediction_error;
    float reward;
    float future_utility;
    uint32_t checksum;
};

struct ReplayIndex {
    uint64_t episode_id;
    uint64_t cue_hash;
    uint64_t context_hash;
    float retrieval_strength;
    float replay_priority;
    uint32_t access_count;
    uint32_t flags;
};

struct BrainStateHeader {
    uint32_t format_version;
    uint32_t state_schema_version;
    uint64_t seed;
    uint64_t tick;
    uint64_t manifest_hash;
    uint32_t endian_marker;
    uint32_t section_count;
};
```

The exact binary format may use a packed binary file with checksums or a self-describing container, but every section must have a version, byte length, checksum, and schema identifier. The loader must reject truncated, corrupted, incompatible, or unknown-required sections. Optional sections may be skipped only when the manifest declares that behavior.

## State-preserving allocation

The current controller rebuilds a `BioEngine` with new random connectivity during reallocation. Stage 2 replaces that behavior with a stable arena or segmented allocator. New capacity is acquired in blocks; existing neuron and synapse records remain at stable addresses or stable IDs. If compaction is required, the operation produces an old-to-new ID map and migrates all references, topology indexes, traces, episodes, pointers, and replay cues before publishing the new state.

Reallocation is transactional:

```text
prepare → reserve → copy/migrate → rebuild indexes → validate → commit
                                      └──────────────► rollback on failure
```

A failed migration must leave the previous state usable. The system must never publish a partially migrated brain.

## Episode capture

An episode begins when a task, context, or novelty boundary is detected and ends at a declared boundary such as goal completion, timeout, context switch, or maximum duration. The capture policy records only typed events and compact state summaries; it must not copy the entire neuron array for every tick.

The capture score should be explicit:

```text
capture_score =
    a × novelty +
    b × prediction_error +
    c × |reward| +
    d × future_utility +
    e × uncertainty_reduction
```

The score determines whether the episode is retained, compressed, or discarded. The decision and all terms must be logged so that memory selection can be audited.

## Selective replay and consolidation

During replay, the hippocampal index selects episodes by priority, current query similarity, prediction error, reward, age, and rehearsal count. The default policy must prevent a small number of high-reward episodes from monopolizing replay. Use quotas or temperature-controlled priority sampling.

Replay must support at least three modes: exact event replay, compressed pointer replay, and generative reconstruction from an index cue. The third mode may initially call a deterministic reconstruction module; it must not be represented as a learned generative model until its reconstruction error is measured.

Consolidation sends replayed patterns to slower cortical populations with a lower learning rate. The system must distinguish replay-induced learning from live-input learning in telemetry. Sleep or idle cycles may change neuromodulatory settings, but every state transition must be explicit and reproducible.

## Semantic consolidation hook

Stage 2 does not require a full semantic graph, but it must expose a consolidation interface that Stage 3 and Stage 4 can consume:

```cpp
struct ConsolidationCandidate {
    uint64_t pointer_id;
    uint64_t context_id;
    uint32_t relation_type;
    float evidence;
    float confidence;
    uint32_t supporting_episode_count;
};

class MemorySystem {
public:
    uint64_t begin_episode(const EventHeader& context);
    void append_event(uint64_t episode_id, const EventHeader& event);
    void close_episode(uint64_t episode_id, float outcome);
    std::vector<ReplayIndex> select_replay(const ReplayQuery&, size_t budget);
    ReplayReport replay(const std::vector<ReplayIndex>&);
    std::vector<ConsolidationCandidate> emit_candidates();
    void save(const std::filesystem::path&);
    void load(const std::filesystem::path&);
};
```

## Evaluation harness

The Stage 2 harness must use a sequence of controlled tasks and process-level restart tests. It must compare at least four policies: no memory, episodic memory without replay, random replay, and selective indexed replay.

| Test ID | Test | Pass condition |
|---|---|---|
| M2-UNIT-01 | Episode boundaries | Start, close, timeout, and context-switch boundaries produce exactly the declared records |
| M2-UNIT-02 | Event ordering | Events reload in timestamp and sequence order with no duplication or loss |
| M2-UNIT-03 | Checksum rejection | One-byte corruption is detected and load fails without modifying live state |
| M2-UNIT-04 | Version rejection | Incompatible required schema is rejected with an actionable error |
| M2-UNIT-05 | Stable IDs | Save/load and growth preserve all referenced neuron, synapse, episode, and pointer IDs |
| M2-UNIT-06 | Transaction rollback | Forced migration failure leaves the pre-migration state hash unchanged |
| M2-UNIT-07 | Replay budget | Replay never exceeds event, time, or memory budget |
| M2-UNIT-08 | Priority fairness | Priority replay does not starve lower-priority episodes beyond the configured starvation bound |
| M2-INT-01 | Save/load continuation | A saved brain and uninterrupted brain produce equivalent continuation metrics for 10,000 ticks |
| M2-INT-02 | Episodic cue recall | Partial cue retrieves the correct episode among at least 100 distractors with recall ≥ 90% |
| M2-INT-03 | Pattern completion | Corrupted or partial episode cues recover the target event sequence with ≥ 85% sequence accuracy |
| M2-INT-04 | Continual retention | After learning 10 later tasks, early-task score retains ≥ 85% of pre-interference score with selective replay |
| M2-INT-05 | Consolidation benefit | Replay improves retention over no-replay by at least 10 percentage points or reduces sample count for equal retention by ≥ 20% |
| M2-INT-06 | Growth preservation | Growing or shrinking within the budget changes retained-task score by no more than 5 percentage points |
| M2-OPS-01 | Restart integrity | Five save/load cycles produce no checksum, ID, or metric drift beyond declared tolerance |
| M2-OPS-02 | Memory budget | Persistent indexes, event records, and replay queues stay within the Stage 2 budget |
| M2-OPS-03 | Replay overhead | Replay overhead and latency are recorded and remain within the configured idle-cycle budget |

### Continual-learning protocol

The benchmark must train task A, checkpoint, train tasks B through J, and test A through J after every task. It must report average accuracy, backward transfer, forward transfer, forgetting, episodic recall, semantic-candidate confidence, and memory occupancy. The task order and input manifest must be hashed.

The harness must include a reallocation protocol: learn A, force growth, learn B, force shrink, save, reload, and test A and B. A controller that meets RAM targets by resetting the brain fails this stage.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit and integration tests | 100% |
| Corruption and version rejection | 100% of injected faults detected |
| Save/load continuation divergence | ≤ 5% on declared metrics |
| Partial-cue episode recall | ≥ 90% |
| Pattern completion | ≥ 85% sequence accuracy |
| Early-task retention after later learning | ≥ 85% of baseline |
| Selective replay benefit | ≥ 10 percentage points or ≥ 20% sample efficiency |
| Growth/shrink performance loss | ≤ 5 percentage points |
| Migration rollback corruption | 0 cases |
| Memory and replay budget violations | 0 |

Stage 2 fails if load silently repairs corrupted data, if replay only helps because it replays the entire history, if reallocation changes all synapse IDs without a migration map, or if retention is measured only on a memorized lookup table with no interference.

## Evidence package

Artifacts must include the state format specification, example files, checksums, migration maps, rollback logs, episode-selection traces, replay-policy ablations, task-by-task retention curves, memory occupancy, and a decision file. Include at least one deliberately corrupted state and one deliberately failed migration as evidence that safety paths were exercised.

## Transition to Stage 3

Stage 3 may begin once the system can preserve and selectively reactivate experiences across process restarts and resource changes. Stage 3 consumes replay and consolidation candidates to build prediction, attention, precision, and global-workspace behavior.

## References

[1]: https://aclanthology.org/2023.eacl-main.65/ "Generative Replay Inspired by Hippocampal Memory Indexing for Continual Language Learning"

[2]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/Nexuss-Neural-Cognition/src/meta_cognition.cpp "Nexuss resource controller and reallocation implementation"
