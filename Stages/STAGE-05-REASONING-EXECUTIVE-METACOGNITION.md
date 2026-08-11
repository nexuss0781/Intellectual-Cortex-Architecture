# Stage 5 — Reasoning, Executive Control, and Metacognition

## Mission

Stage 5 turns learned language and semantic structures into accountable reasoning and action selection. The engine must maintain a provenance-aware semantic graph, distinguish deduction from induction and abduction, represent small causal models, plan over options, estimate confidence, detect contradictions, and choose whether to answer, ask, investigate, simulate, or abstain.

The central requirement is not that the engine produces impressive explanations. It is that its explanations and actions are **traceable to evidence, internally consistent, testable against outcomes, and calibrated to uncertainty**.

## Entry conditions

Stage 4 must be `PASS`. The system must interpret and generate structured scenes, preserve provenance, compose semantic pointers, retain language knowledge, and abstain from unknown or unsafe commands. Stage 5 uses the language learner as an interface; it must not replace the underlying structured representations with unconstrained text generation.

## Cognitive data model

### Provenance-aware semantic graph

```cpp
struct EvidenceRef {
    uint64_t episode_id;
    uint64_t event_id;
    uint64_t source_id;
    float reliability;
};

struct BeliefNode {
    uint64_t node_id;
    uint32_t type_id;
    SparseCode pointer;
    float confidence;
    uint64_t revision;
};

struct BeliefEdge {
    uint64_t edge_id;
    uint64_t subject_id;
    uint64_t predicate_id;
    uint64_t object_id;
    float confidence;
    std::vector<EvidenceRef> evidence;
    uint64_t revision;
};
```

The graph must support typed relations, confidence, contradiction links, temporal validity, and evidence references. A proposition without evidence may exist as a hypothesis but must be marked provisional.

### Causal model

A causal model is a bounded structural graph with variables, value domains, mechanisms, and uncertainty. It must support observation, intervention, and counterfactual queries for small domains.

```cpp
struct CausalVariable {
    uint64_t id;
    uint32_t domain_type;
    std::vector<uint32_t> values;
};

struct CausalMechanism {
    uint64_t target;
    std::vector<uint64_t> parents;
    ConditionalTable or_function;
    float confidence;
};
```

If a mechanism cannot answer a query within its scope, it must return `UNKNOWN` or `OUT_OF_SCOPE`; it must not hallucinate a result.

### Executive option

```cpp
struct ExecutiveOption {
    uint64_t option_id;
    uint64_t goal_id;
    ActionCommand action;
    float expected_utility;
    float information_gain;
    float uncertainty;
    float resource_cost;
    float risk;
    uint32_t horizon;
};
```

## Reasoning contracts

### Deduction

Deduction operates over explicit rules and typed facts. Every derived proposition must include a proof trace consisting of premises, rule IDs, substitutions, and contradiction checks. A proof is valid only when all premises are present and the rule application is type-correct.

### Induction

Induction proposes a generalized relation or concept from multiple episodes. It must report support count, exceptions, confidence, and the hypothesis complexity penalty. A single observation cannot be promoted to a high-confidence universal rule unless the configuration explicitly permits it.

### Abduction

Abduction ranks explanations for observed outcomes using evidence fit, prior plausibility, complexity, and contradiction penalty. It must return alternatives when the posterior margin is small.

### Analogy

Analogy maps relational structure, not merely surface similarity. The harness must supply pairs with shared relational structure but different names and surface features, and pairs with high lexical overlap but incompatible structure.

### Causality and counterfactuals

The engine must distinguish `observe(X)` from `do(X=x)`. A counterfactual query must clone or branch the relevant model state and must not mutate the live belief state. Every answer must carry the intervention set, model revision, and uncertainty.

## Executive control

The executive policy selects among answering, asking, retrieving, replaying, simulating, acting, and abstaining. A baseline policy is:

```text
option_score =
    expected_utility +
    λ_info × information_gain −
    λ_risk × risk −
    λ_cost × resource_cost −
    λ_unc × uncertainty
```

The system must be able to choose a clarifying question when the expected reduction in uncertainty is greater than the cost of acting on a low-confidence interpretation. The policy must expose the selected option and all score terms.

### Confidence and calibration

Confidence is a prediction about correctness, not a direct copy of activation, precision, or reward. The monitor receives the evidence available before the outcome and predicts whether a reasoning result or action will be correct.

The harness must evaluate reliability diagrams, expected calibration error, Brier score, selective accuracy, and abstention coverage. A system that is accurate but confidently wrong on novel inputs fails the metacognitive gate.

### Contradiction management

Contradictory beliefs must be retained with provenance and revision rather than silently overwritten. The executive policy may choose to seek evidence, prefer a more reliable source, maintain both context-indexed beliefs, or abstain. The decision must be logged.

## Implementation work packages

| Work package | Deliverable |
|---|---|
| R5.1 | Provenance-aware semantic graph and belief revision |
| R5.2 | Deductive proof engine with typed proof traces |
| R5.3 | Inductive hypothesis generator with complexity penalty |
| R5.4 | Abductive explanation ranker with alternatives |
| R5.5 | Structure-mapping analogy engine |
| R5.6 | Bounded causal model with observation/intervention/counterfactual API |
| R5.7 | Executive option manager and information-gain policy |
| R5.8 | Confidence monitor, calibration metrics, and abstention policy |
| R5.9 | Reasoning, contradiction, planning, and metacognition harness |

## Required interfaces

```cpp
class ReasoningEngine {
public:
    ProofResult deduce(const Query&, const BeliefGraph&);
    std::vector<Hypothesis> induce(const EpisodeSet&);
    std::vector<Explanation> explain(const Observation&, const BeliefGraph&);
    AnalogyResult map_structure(const SourceDomain&, const TargetDomain&);
};

class CausalEngine {
public:
    QueryResult observe(const Query&);
    QueryResult intervene(const Intervention&);
    QueryResult counterfactual(const WorldState&, const Intervention&);
};

class ExecutiveController {
public:
    std::vector<ExecutiveOption> generate_options(const Goal&, const CognitiveState&);
    ExecutiveDecision select(const std::vector<ExecutiveOption>&);
    void record_outcome(const ExecutiveDecision&, const Outcome&);
};

class MetacognitiveMonitor {
public:
    ConfidenceEstimate estimate(const Result&, const EvidenceContext&);
    CalibrationReport evaluate(const OutcomeLog&);
    bool should_abstain(const ConfidenceEstimate&, const RiskProfile&);
};
```

## Evaluation harness

The harness must contain deterministic micro-worlds with known rules, causal mechanisms, rewards, costs, and answer keys. It must include novel combinations, conflicting evidence, out-of-scope queries, and adversarially plausible but wrong explanations.

| Test ID | Test | Pass condition |
|---|---|---|
| R5-UNIT-01 | Proof trace | Every derived fact contains valid premises and rule substitutions |
| R5-UNIT-02 | Invalid deduction rejection | Type-invalid or unsupported rule applications are rejected |
| R5-UNIT-03 | Hypothesis complexity | More complex hypotheses do not win when fit is equal |
| R5-UNIT-04 | Abduction alternatives | Close posterior explanations return multiple candidates rather than false certainty |
| R5-UNIT-05 | Analogy structure | Relationally matching domains map correctly despite renamed entities |
| R5-UNIT-06 | Surface distractor | Lexically similar but structurally incompatible domains are not mapped as strong analogies |
| R5-UNIT-07 | Intervention isolation | `do(X=x)` does not mutate the observational live state |
| R5-UNIT-08 | Counterfactual provenance | Result contains model revision, intervention, and evidence references |
| R5-UNIT-09 | Belief contradiction | Conflicting evidence remains inspectable and revisioned |
| R5-UNIT-10 | Confidence independence | Confidence does not equal precision or raw activation in the isolated test |
| R5-INT-01 | Deductive reasoning | Held-out theorem validity ≥ 95% with zero unsupported proof traces |
| R5-INT-02 | Inductive generalization | Generalization accuracy ≥ 80% with exception reporting ≥ 90% |
| R5-INT-03 | Abductive explanation | Top explanation accuracy ≥ 75%; alternative coverage ≥ 90% when posterior margin is small |
| R5-INT-04 | Analogy transfer | Structural analogy accuracy ≥ 80% and surface-distractor false-positive rate ≤ 15% |
| R5-INT-05 | Causal intervention | Correct intervention outcome ≥ 85% on in-scope models |
| R5-INT-06 | Counterfactual | Correct counterfactual answer ≥ 80%; live-state mutation count 0 |
| R5-INT-07 | Planning | Goal completion ≥ 80% under a fixed horizon and resource budget |
| R5-INT-08 | Clarification | Asking a question improves final task accuracy by ≥ 15 percentage points on ambiguous trials |
| R5-INT-09 | Abstention | Out-of-scope or unsafe queries are abstained from ≥ 95% of the time |
| R5-INT-10 | Calibration | Expected calibration error ≤ 0.10 and Brier score beats an uncalibrated baseline |
| R5-INT-11 | Contradiction handling | Correct evidence-seeking or abstention decision ≥ 80% on conflicting cases |
| R5-INT-12 | Continual retention | Earlier reasoning skills retain ≥ 85% accuracy after later domain learning |
| R5-OPS-01 | Trace completeness | 100% of answers and actions have provenance and score traces |
| R5-OPS-02 | Determinism | Same seed and scenario manifest produce identical decisions and proof hashes |
| R5-OPS-03 | Resource limits | Planning and counterfactual branches respect node, depth, time, and memory budgets |

### Required ablations

Compare the full system against versions without provenance, without causal intervention, without clarification, without calibration, without replay, and without structural analogy. A reasoning module is validated only if removing its claimed mechanism degrades the corresponding benchmark while leaving unrelated controls interpretable.

### Safety-critical negative tests

The harness must include unsafe action requests, incomplete instructions, contradictory policies, and maliciously plausible evidence. The correct response may be refusal, clarification, or evidence gathering. A fluent but unsupported action counts as a failure.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit and integration tests | 100% |
| Deductive validity | ≥ 95% |
| Inductive generalization | ≥ 80% |
| Causal intervention accuracy | ≥ 85% |
| Counterfactual accuracy | ≥ 80% |
| Planning success | ≥ 80% |
| Clarification benefit | ≥ 15 percentage points |
| Unsafe/out-of-scope abstention | ≥ 95% |
| Expected calibration error | ≤ 0.10 |
| Unsupported proof/action traces | 0 |
| Live-state mutation during counterfactuals | 0 |
| Earlier-skill retention | ≥ 85% |

Stage 5 fails if the engine gives a correct answer without a valid proof or evidence trace, treats correlation as intervention, reports high confidence on unsupported claims, cannot abstain, or improves only by memorizing the micro-world answer table.

## Evidence package

Store belief-graph snapshots, proof traces, hypothesis tables, analogy mappings, causal model revisions, intervention branches, executive option scores, calibration plots/data, abstention decisions, contradiction logs, ablations, task-retention curves, resource traces, and the transition decision.

## Transition to Stage 6

Stage 6 may begin once reasoning, planning, uncertainty, and evidence management are demonstrably functional in controlled environments. Stage 6 will connect these abilities to multimodal experience and a developmental curriculum, testing whether the system transfers structure rather than merely accumulating task-specific knowledge.

## References

[1]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/README.md "Intellectual Cortex Architecture overview"

[2]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/SPEC/phase-1/unit-4_subcomponent-1.1_Pyramidal-SDR-Generators-(PSG).md "ICA sparse and compositional substrate direction"
