# Stage 11 — Preference, Safety, Calibration, and Continual Improvement

## Mission

Stage 11 improves the Stage 10 NLP engine using reviewed preference data, safety-specific post-training, calibrated uncertainty, regression learning, and a controlled feedback loop. It establishes how the system learns from failures without turning raw production traffic into automatic self-modification.

The outcome is **not** proof that the system is safe in all contexts, aligned in a human-like sense, or authorized for high-impact autonomy. It is a bounded post-training release whose gains and regressions are independently measured.

## Entry conditions

Stage 10 must be `PASS`. The product policy, data factory, sealed evaluation set, SFT model card, tool authority matrix, incident taxonomy, annotation rubric, reviewer calibration protocol, and rollback path must be available. All feedback used for training must originate from an approved dataset release.

## Learning boundary

```text
Production event
  → privacy/safety filter
  → sampling and consent policy
  → review queue
  → annotation and adjudication
  → regression candidate
  → sealed offline evaluation
  → post-training approval
  → canary only after later deployment stage
```

No model, adapter, safety classifier, confidence threshold, or policy is updated directly from a user message, thumbs-up/down signal, tool result, or incident report. Automatic collection for audit is allowed only according to consent, retention, privacy, and access policy. Automatic promotion is prohibited.

## Post-training contract

```cpp
struct PreferenceExample {
    std::string example_id;
    std::string prompt_hash;
    std::string chosen_hash;
    std::string rejected_hash;
    std::string rubric_version;
    std::vector<std::string> reasons;
    std::string reviewer_group;
    bool adjudicated;
};

struct SafetyExample {
    std::string example_id;
    std::string risk_category;
    std::string expected_decision; // answer, transform, ask, abstain, refuse, escalate
    std::string policy_version;
    std::string evidence_scope;
    uint32_t severity;
};

struct CalibrationReport {
    double expected_calibration_error;
    double brier_score;
    double selective_accuracy;
    double coverage;
    double unsafe_overconfidence_rate;
};

class ImprovementGate {
public:
    bool accept_preference_set(const std::vector<PreferenceExample>&) const;
    bool accept_safety_set(const std::vector<SafetyExample>&) const;
    bool promote_candidate(const CalibrationReport&, const std::string& evaluation_manifest) const;
};
```

Preference labels must retain the task, rubric, evidence context, rater/reviewer group, disagreement, adjudication, policy version, and known limitations. Safety labels must distinguish harmful compliance, safe transformation, correct refusal, clarification, escalation, over-refusal, privacy risk, injection risk, unauthorized agency, and misinformation/unsupported-claim behavior.

## Implementation work packages

| Work package | Deliverable |
|---|---|
| S11.1 | Preference and safety annotation schemas, rubrics, rater calibration, and adjudication workflow |
| S11.2 | Data release for reviewed preference pairs and safety cases with provenance and privacy controls |
| S11.3 | Offline SFT-to-preference post-training pipeline using DPO/KTO or another approved method |
| S11.4 | Safety-focused post-training, policy classifier, and refusal/clarification/abstention evaluation |
| S11.5 | Confidence calibration, selective prediction, and uncertainty reporting |
| S11.6 | Incident-to-regression pipeline with severity, owner, root cause, and test linkage |
| S11.7 | Replay/retention protocol to prevent catastrophic forgetting of Stage 10 functions |
| S11.8 | Red-team harness, data-poisoning checks, and rollback/ablation evidence |

## Post-training progression

Begin with offline supervised/pairwise preference optimization. TRL supports SFT, DPO, KTO, reward modeling, online methods, and distillation [1]. Online reinforcement learning or reward modeling may be introduced only after the project can demonstrate reliable automatic outcome labels, reward-model robustness, bounded policy updates, and reward-hacking tests. A reward score is never the sole release criterion.

| Step | Allowed method | Required evidence before use |
|---|---|---|
| P1 | Preference data curation | Rater calibration, adjudication, provenance, privacy review |
| P2 | Offline DPO/KTO-style optimization | Hidden quality/safety evaluation and rollback plan |
| P3 | Safety-specific post-training | Severe-risk suite, refusal quality, over-refusal, privacy, injection tests |
| P4 | Distillation | Teacher/student evaluation parity and resource benefit |
| P5 | Reward/process model | Inter-rater agreement, adversarial reward-hacking test, held-out validation |
| P6 | Online/agentic RL | Automatically verifiable sandbox outcome and independent safety approval |

## Evaluation harness

| Test ID | Test | Pass condition |
|---|---|---|
| S11-UNIT-01 | Preference provenance | Every pair has source, rubric, reviewer group, policy, and adjudication state |
| S11-UNIT-02 | Preference order control | Pair-order and style-only perturbations do not reverse approved label without reason |
| S11-UNIT-03 | Safety taxonomy | Every safety item maps to a risk category, decision class, severity, and policy |
| S11-UNIT-04 | Feedback boundary | Raw production event cannot enter training release without review approval |
| S11-UNIT-05 | Regression linkage | Confirmed incident creates immutable regression candidate with owner/status |
| S11-UNIT-06 | Calibration math | ECE/Brier/selective metrics reproduce from fixed outcome log |
| S11-UNIT-07 | Rollback | Candidate rejection restores prior approved model/policy bundle |
| S11-INT-01 | Preference quality | Candidate exceeds Stage 10 model by signed margin on blinded pairwise quality |
| S11-INT-02 | Helpful-safe balance | Safety gain does not reduce benign-task quality beyond signed tolerance |
| S11-INT-03 | Harmful compliance | Severe unsafe-compliance rate meets signed risk threshold |
| S11-INT-04 | Over-refusal | Benign-task refusal remains below signed maximum |
| S11-INT-05 | Privacy/secrets | Sensitive-data disclosure suite shows no newly introduced critical leak |
| S11-INT-06 | Injection/agency | Prompt-injection and unauthorized-tool proposal rate does not regress |
| S11-INT-07 | Calibration | ECE, Brier, selective accuracy, and unsafe-overconfidence meet signed gates |
| S11-INT-08 | Retention | Stage 10 structured/grounded task metrics retain signed minimum |
| S11-INT-09 | Regression learning | Fixed historical incident suite improves or stays non-regressive |
| S11-INT-10 | Language fairness | Supported-language performance disparity remains within signed bound |
| S11-OPS-01 | Training resource | Post-training fits signed compute, memory, storage, and cost budget |
| S11-OPS-02 | Determinism | Fixed data/config/seed reproduces candidate evaluation traces under declared policy |
| S11-OPS-03 | Model bundle integrity | Weight, adapter, policy, calibration, and regression-suite digests are complete |

### Required ablations and adversarial tests

Compare full post-training against no preference data, no safety data, no calibration, no replay/retention, no incident regression set, no adjudication, and no privacy filter. Use adversarial reward-hacking examples, biased rater subsets, poisoned preference pairs, unsafe-but-helpful prompts, benign-but-sensitive prompts, model-extraction attempts, prompt injection, data-exfiltration canaries, and multilingual edge cases.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit/integration/operations tests | 100% |
| Blinded preference-quality improvement | ≥ signed margin with confidence interval |
| Critical harmful-compliance failures in release scope | 0 unresolved |
| Over-refusal rate | ≤ signed maximum |
| New critical privacy/secret disclosure | 0 |
| Injection/unauthorized-agency regression | ≤ signed maximum |
| Calibration and unsafe-overconfidence | Meet signed product thresholds |
| Stage 10 retention | ≥ signed floor |
| Incident regression suite | No severity-1 regression |
| Training from unreviewed production feedback | 0 items |
| Model/policy/calibration bundle completeness | 100% |
| Resource violations | 0 |

## Evidence package

Store annotation rubrics, rater calibration report, agreement/adjudication report, preference/safety dataset cards, post-training manifests, model/adapter cards, hidden quality/safety results, calibration curves/data, refusal/over-refusal analysis, privacy/injection/agency results, incident regression ledger, retention/replay results, ablations, red-team report, resource traces, rollback evidence, `stage11_metrics.csv`, normal/sanitizer logs where applicable, `decision.md`, and `manifest.sha256`.

## Transition to Stage 12

Stage 12 may begin only if Stage 11 is `PASS`, the candidate has measured benefit without unacceptable safety, privacy, calibration, or retention regression, all severity-1 findings are resolved, and the user explicitly approves production-serving and shadow-operations implementation.

## References

[1]: https://huggingface.co/docs/trl/index "Hugging Face TRL"
[2]: https://doi.org/10.6028/NIST.AI.600-1 "NIST Generative AI Profile"
[3]: https://genai.owasp.org/resource/owasp-genai-llm-top-10-2026/ "OWASP GenAI LLM Top 10 2026"
