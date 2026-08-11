# Stage 4 — Grounded Compositional Language Acquisition

## Mission

Stage 4 gives the brain engine a language-learning loop rather than a static vocabulary or an external chatbot wrapper. The system must learn forms from streams, predict sequences, bind words to roles and contexts, acquire reusable constructions, understand novel combinations of familiar elements, and produce an observable response through a decoder or action interface.

The first target is a controlled but genuine language environment. The system should learn from text paired with entities, actions, scenes, or structured consequences. Unrestricted language is not the first gate because it would make it impossible to distinguish memorization, external prior knowledge, and compositional learning.

## Entry conditions

Stage 3 must be `PASS`. The system must provide bounded workspace broadcasts, predictive error signals, precision, goal context, stable pointers, persistent episodes, and replay. Stage 4 uses the Stage 3 workspace and must preserve the Stage 2 episode records that support language replay.

## Scope and non-goals

Stage 4 includes token or phoneme event encoding, lexical identity and context learning, role-filler binding, temporal and discourse context, construction induction, semantic-pointer cleanup, interactive correction, and a small decoder. It does not claim human-level language, broad world knowledge, or unrestricted dialogue. It does not require a large transformer to be embedded in the spiking kernel.

## Language architecture

```text
Text / phoneme / speech features
              │
              ▼
       Token-event encoder
              │
              ▼
   lexical pointers + position/context
              │
              ▼
       predictive workspace
              │
      role-filler binder
       │       │       │
       ▼       ▼       ▼
   lexicon  constructions  episode index
              │
              ▼
       semantic scene / command graph
              │
              ▼
       decoder or action policy
```

The language layer must make the distinction between form, meaning, context, and consequence explicit. A token occurrence is not the same object as a lexical type. A lexical type is not the same object as a grounded concept. A sentence form is not the same object as the situation it describes.

## Representation contracts

### Token and lexical events

Each input unit produces a typed event with a stable occurrence ID, sequence position, speaker or channel ID, and optional acoustic or visual features. The lexical memory maintains type-level statistics and instance-level episodes.

```cpp
struct TokenEvent {
    uint64_t occurrence_id;
    uint64_t lexical_type_id;
    uint64_t episode_id;
    uint32_t position;
    uint32_t channel;
    SparseCode form_code;
    uint64_t context_pointer_id;
};
```

The initial encoder may use characters, subwords, phonemes, or controlled word tokens, but the configuration must state which one is used. The tokenizer, vocabulary, and corpus manifest must be versioned and hashed.

### Semantic pointers

Implement a typed vector-symbolic layer with deterministic operations:

```text
bind(role, filler)          → structured pointer
bundle(p1, p2, ..., pn)     → superposed pointer
permute(pointer, k)         → temporal/role transform
unbind(structure, role)     → recovered filler estimate
cleanup(noisy_pointer)      → nearest known pointer
similarity(p, q)            → normalized score
```

Every pointer has a dimension, sparsity or norm convention, type ID, generation, and checksum. The engine must reject binding incompatible pointer types unless the operation is explicitly polymorphic. The initial implementation should support 2,048-dimensional sparse or binary pointers because that matches the existing Phase I direction, but the dimension must remain configurable for capacity experiments.

### Role-filler and sentence structure

A controlled sentence is represented as a structured scene rather than a bag of words:

```text
Sentence = bundle(
    bind(SUBJECT, entity),
    bind(VERB, action),
    bind(OBJECT, entity),
    bind(LOCATION, place),
    bind(TIME, temporal_context),
    bind(MODALITY, modality)
)
```

Missing roles must remain missing; the system must not fill them with a default entity without reporting uncertainty. Repeated roles, negation, tense, and argument order must be represented explicitly in the task configuration.

## Learning algorithms

### Lexical learning

A new form creates a provisional lexical entry with low confidence. Repeated contexts, grounded co-occurrence, prediction success, and corrective feedback increase evidence. Conflicting contexts must split or preserve senses rather than overwrite the entry. Every lexical update records supporting episodes.

### Construction learning

The construction learner searches for recurring role-patterns across episodes. It proposes a construction when a form-context pattern recurs above the support threshold and predicts a role structure better than a memorized sentence baseline. Each construction contains a form signature, role bindings, optional slots, temporal constraints, evidence count, and confidence.

A minimum description-length or penalized likelihood criterion should prevent a new construction from being created for every observed sentence:

```text
score(construction) = fit_gain − complexity_penalty − contradiction_penalty
```

The threshold and all evidence must be logged. A construction is not promoted to semantic memory until it survives replay and held-out validation.

### Sequence prediction

The language population predicts the next token or event pointer. Learning uses Stage 1 eligibility traces and Stage 3 prediction error. The language harness must compare the learned model with a frequency-only and context-free baseline.

### Decoder

The first decoder may be a compact recurrent or table-backed module that maps semantic scene pointers to token sequences or action commands. Its interface must be explicit so that later work can replace it with a spiking decoder. The decoder must report whether it produced a response from a learned construction, a memorized episode, or a fallback policy.

## Grounded learning protocol

The core curriculum consists of named entities, simple actions, locations, quantities, temporal changes, and corrective feedback. Each linguistic example is paired with a scene or executable command. The system receives a sentence, predicts a scene or action, receives success/error feedback, and stores the episode.

The curriculum must include novel compositions. For example, if the engine learns `red cube`, `blue sphere`, `move`, and `left`, the test should include a combination such as `move the red sphere left` only when each component and construction supports it. The exact vocabulary and split must be declared so that the test cannot accidentally leak the answer.

## Implementation work packages

| Work package | Deliverable |
|---|---|
| L4.1 | Versioned token/phoneme event encoder |
| L4.2 | Lexical type/instance memory with evidence and senses |
| L4.3 | Typed semantic-pointer operations and cleanup memory |
| L4.4 | Role-filler binder and structured scene representation |
| L4.5 | Construction induction with complexity penalty |
| L4.6 | Language prediction population and correction loop |
| L4.7 | Decoder/action interface with provenance labels |
| L4.8 | Controlled grounded-language corpus and split manifest |
| L4.9 | Compositionality, retention, ambiguity, and transfer harness |

## Required interfaces

```cpp
struct GroundedExample {
    uint64_t example_id;
    std::vector<TokenEvent> tokens;
    SceneGraph scene;
    ActionCommand optional_action;
    uint64_t episode_id;
};

struct LanguageHypothesis {
    uint64_t hypothesis_id;
    SparseCode semantic_pointer;
    SceneGraph predicted_scene;
    float confidence;
    enum class Provenance { CONSTRUCTION, EPISODIC, LEXICAL, FALLBACK } provenance;
};

class LanguageLearner {
public:
    void observe(const GroundedExample&);
    LanguageHypothesis interpret(const std::vector<TokenEvent>&, const CognitiveContext&);
    std::vector<TokenEvent> generate(const SceneGraph&, const CognitiveContext&);
    void correct(const Feedback&);
    LanguageMetrics metrics() const;
};
```

## Evaluation harness

The harness must use disjoint train, development, and test manifests. The test split must contain unseen combinations of familiar forms and concepts. A separate transfer split must introduce new names or a new domain with the same relational structure.

| Test ID | Test | Pass condition |
|---|---|---|
| L4-UNIT-01 | Pointer binding | Bind/unbind recovers the filler with similarity ≥ 0.95 under clean conditions |
| L4-UNIT-02 | Pointer noise cleanup | Cleanup recovers the correct pointer at the declared corruption level in ≥ 95% of trials |
| L4-UNIT-03 | Type safety | Invalid role/filler operations are rejected or explicitly coerced; no silent aliasing |
| L4-UNIT-04 | Position handling | Permutation or positional encoding preserves order and distinguishes swaps |
| L4-UNIT-05 | Lexical evidence | Evidence counts, senses, and provenance survive save/load exactly |
| L4-UNIT-06 | Missing-role uncertainty | Missing roles remain unknown and are not silently filled |
| L4-UNIT-07 | Construction penalty | Memorized one-off sentences do not create promoted constructions |
| L4-UNIT-08 | Decoder provenance | Every output identifies learned construction, episode, lexical, or fallback source |
| L4-INT-01 | Token prediction | Held-out next-token or next-event accuracy beats a unigram/context-free baseline by ≥ 15 percentage points |
| L4-INT-02 | Lexical grounding | New word-to-entity mapping reaches ≥ 85% referent accuracy after the declared exposure budget |
| L4-INT-03 | Role binding | Subject/action/object role accuracy ≥ 85% on held-out sentences |
| L4-INT-04 | Compositional generalization | Unseen combinations of familiar components achieve ≥ 70% scene or action accuracy |
| L4-INT-05 | Construction transfer | A learned construction transfers to at least one held-out vocabulary set with ≥ 60% accuracy |
| L4-INT-06 | Ambiguous reference | Context-conditioned reference resolution beats context-free control by ≥ 15 percentage points |
| L4-INT-07 | Correction | After explicit negative feedback, the error rate on the corrected pattern decreases by ≥ 30% without reducing unrelated accuracy by > 10% |
| L4-INT-08 | Continual language retention | After learning 10 new batches, early lexical and construction accuracy retains ≥ 85% of baseline |
| L4-INT-09 | Grounded command | Correct executable action selection ≥ 80% and unsafe/unknown command abstention ≥ 95% |
| L4-OPS-01 | Replay benefit | Selective replay improves early-language retention over no-replay by ≥ 10 percentage points |
| L4-OPS-02 | Memory budget | Vocabulary, pointers, constructions, episodes, and decoder fit in declared Stage 4 auxiliary budget |
| L4-OPS-03 | Determinism | Fixed seed, tokenizer, and manifest produce identical outputs and state hashes |
| L4-OPS-04 | No leakage | Test vocabulary combinations and answer labels are absent from training manifests |

### Required baselines and ablations

The harness must compare a frequency-only language model, a context-free lexical mapper, an episodic-nearest-neighbor system, a semantic-pointer system without construction induction, and the full system. The full system must demonstrate a measurable advantage on novel combinations or transfer, not merely on memorized examples.

The harness must also run ablations without workspace context, without replay, without role binding, and without corrective feedback. If removing role binding does not reduce role accuracy, the role representation is not doing meaningful work. If removing construction induction does not reduce compositional generalization, the construction learner is not validated.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit and integration tests | 100% |
| Pointer clean recovery | ≥ 95% |
| Held-out token prediction improvement | ≥ 15 percentage points over context-free baseline |
| Grounded lexical accuracy | ≥ 85% |
| Role accuracy | ≥ 85% |
| Novel compositional accuracy | ≥ 70% |
| Construction transfer | ≥ 60% |
| Correction error reduction | ≥ 30% |
| Early-language retention | ≥ 85% of baseline |
| Unsafe/unknown-command abstention | ≥ 95% |
| Manifest leakage | 0 detected cases |

Stage 4 fails if the engine only retrieves complete memorized sentences, if the decoder cannot identify provenance, if unknown words are confidently mapped to arbitrary known concepts, if role order is ignored, or if the test split contains leaked combinations.

## Evidence package

Store tokenizer and corpus hashes, vocabulary growth curves, pointer capacity results, construction proposals, evidence tables, prediction curves, scene/action confusion matrices, compositional split definitions, ablations, correction traces, replay results, memory use, and the stage decision.

## Transition to Stage 5

Stage 5 may begin when the engine can learn grounded lexical mappings, compose known elements into unseen structures, use context to resolve ambiguity, retain earlier language after new learning, and abstain on unknown or unsafe commands. The reasoning stage will consume structured scenes, semantic memory, confidence, and action outcomes.

## References

[1]: https://aclanthology.org/2023.eacl-main.65/ "Generative Replay Inspired by Hippocampal Memory Indexing for Continual Language Learning"

[2]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/SPEC/phase-1/unit-4_subcomponent-1.1_Pyramidal-SDR-Generators-(PSG).md "ICA sparse pointer and encoding contract"

[3]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/README.md "Intellectual Cortex Architecture overview"
