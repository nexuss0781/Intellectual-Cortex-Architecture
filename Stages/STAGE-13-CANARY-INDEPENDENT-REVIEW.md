# Stage 13 — Controlled Canary, Human Evaluation, and Independent Review

## Mission

Stage 13 evaluates the Stage 12 operational system with a deliberately small, consented user cohort, strict scope, active monitoring, human review, red teaming, and independent security/model assessment. It converts offline and shadow evidence into pilot evidence without allowing unrestricted rollout or uncontrolled action authority.

The outcome is **not** universal production readiness, a guarantee of safety, or a claim of general intelligence. It is a documented go/no-go decision for the narrow pilot scope.

## Entry conditions

Stage 12 must be `PASS`. The model/policy/tool/retrieval bundle, pilot product charter, user consent language, privacy notice, onboarding process, support path, incident playbook, on-call rotation, monitoring dashboards, rollback mechanism, safety reviewers, and external-review scope must be approved. Pilot actions remain advisory or require human approval according to the Stage 7 risk policy.

## Pilot contract

```cpp
struct CanaryCohort {
    std::string cohort_id;
    uint32_t maximum_users;
    std::vector<std::string> permitted_tasks;
    std::vector<std::string> excluded_domains;
    std::string consent_version;
    double traffic_fraction;
    bool consequential_actions_disabled;
};

struct HumanReviewRecord {
    std::string review_id;
    std::string request_hash;
    std::string model_bundle_digest;
    std::string rubric_version;
    std::string reviewer_id_or_group;
    std::string outcome; // acceptable, incorrect, unsafe, privacy, policy, escalation
    uint32_t severity;
    bool adjudicated;
};

struct ReleaseDecision {
    std::string decision; // continue, pause, rollback, expand, terminate
    std::string rationale;
    std::vector<std::string> evidence_digests;
    std::vector<std::string> approvers;
};

class CanaryController {
public:
    bool enroll(const CanaryCohort&);
    bool pause(const std::string& rationale);
    bool rollback(const std::string& incident_id);
    ReleaseDecision decide() const;
};
```

## Human evaluation design

Human evaluation must be blinded where possible, rubric-based, multi-rater, and stratified by task, language, domain, severity, and uncertainty. Domain experts must review high-impact categories. The system must measure agreement, disagreement, adjudication, false acceptance, false rejection, correctness, grounding, citation quality, usefulness, tone, safety, privacy, and appropriate abstention.

| Review layer | Required evidence |
|---|---|
| User consent and onboarding | Consent receipt, scope explanation, feedback channel, opt-out path |
| Live sample review | Random and risk-triggered sampled requests with redaction |
| Expert review | Domain-specific assessment for high-impact or specialized tasks |
| Safety review | Harmful content, privacy, injection, agency, misinformation, and overreliance review |
| Adversarial review | Internal and external red-team campaigns against actual deployed configuration |
| Independent review | Reproducibility, security, data governance, and claims-boundary assessment |
| Incident review | Severity, containment, root cause, regression candidate, and disclosure decision |

## Implementation work packages

| Work package | Deliverable |
|---|---|
| C13.1 | Cohort eligibility, consent, privacy notice, onboarding, support, and opt-out controls |
| C13.2 | Canary traffic policy, kill switch, pause/rollback, and traffic expansion controls |
| C13.3 | Human-review queue, rubrics, sampling, rater calibration, adjudication, and audit ledger |
| C13.4 | Safety and incident command center with severity, containment, communications, and regression workflow |
| C13.5 | External security assessment and model/system evaluation charter |
| C13.6 | Red-team campaign for prompt injection, sensitive data, tool authority, misinformation, and denial-of-service |
| C13.7 | Claims review and transparent pilot system card |
| C13.8 | Canary metrics, decision harness, and formal release package |

## Evaluation harness

| Test ID | Test | Pass condition |
|---|---|---|
| C13-UNIT-01 | Consent gate | Non-consented or withdrawn user cannot enter pilot traffic/logging scope |
| C13-UNIT-02 | Scope enforcement | Excluded task/domain is refused, rerouted, or escalated per policy |
| C13-UNIT-03 | Kill switch | Pause/rollback blocks canary routing within signed response time |
| C13-UNIT-04 | Sampling integrity | Random and risk-triggered samples preserve cohort/task stratification |
| C13-UNIT-05 | Reviewer privacy | Reviewer sees redacted/minimized data and all access is audited |
| C13-UNIT-06 | Incident completeness | Severity-1 record requires containment, owner, timeline, and decision status |
| C13-INT-01 | Human quality | Blinded human quality meets signed target versus approved baseline |
| C13-INT-02 | Grounding/citation | Human reviewers confirm evidence/citation quality meets signed threshold |
| C13-INT-03 | Safety | Critical unsafe behavior has zero unresolved cases; overall severe rate meets threshold |
| C13-INT-04 | Privacy | No unresolved critical PII/secret/tenant exposure in pilot scope |
| C13-INT-05 | Abstention/overreliance | Model abstains/escalates appropriately; user signals do not show unacceptable automation bias risk |
| C13-INT-06 | Red team | All critical red-team findings are fixed, mitigated, or stop pilot; retest required |
| C13-INT-07 | Independent assessment | External review can reproduce declared key metrics and validate claims boundary |
| C13-INT-08 | Operational stability | SLOs and rollback behavior remain within Stage 12 limits during pilot |
| C13-INT-09 | Support/incident response | High-severity drill meets signed detection, containment, and communication times |
| C13-OPS-01 | Canary limit | Traffic and cohort never exceed signed cap without approval |
| C13-OPS-02 | Decision audit | 100% traffic expansions, pauses, incidents, and releases have approval/audit evidence |
| C13-OPS-03 | Regression conversion | Confirmed incidents enter Stage 11 regression pipeline only after review approval |

### Required adversarial cases

Run direct and indirect prompt injection against live-equivalent retrieval, tool and data-exfiltration attempts, jailbreaks, sensitive personal data, unsupported medical/legal/financial decisions, hallucinated citations, conflicting sources, malicious uploads, high-volume request floods, account takeover simulation, model-registry rollback, and incident misinformation. Test both over-refusal and unsafe acceptance.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit/integration/operations tests | 100% |
| Consent/scope violations | 0 |
| Canary traffic/cohort cap violations | 0 |
| Unresolved severity-1 safety/privacy/security incidents | 0 |
| Human quality and grounding | Meet signed pilot thresholds with confidence intervals |
| Critical citation fabrication accepted by reviewers | 0 |
| Red-team critical findings before expansion | 0 unresolved |
| Independent review reproducibility | 100% of declared core metrics reproduced or limitation documented |
| Kill switch/rollback success | 100% within signed time |
| Pilot SLO/security/tenant-isolation regression | ≤ signed tolerance |
| Incident decision/audit completeness | 100% |
| Unreviewed incident-to-training promotion | 0 |

## Evidence package

Store cohort/consent manifests, pilot policy, traffic cap configuration, system card, human-review rubrics/calibration/agreement/adjudication reports, sampled evaluation ledger, domain-expert reports, safety/privacy/agency reports, red-team scope/results/retests, external review reports, incident logs and drills, kill-switch/rollback evidence, pilot SLO dashboards, claims registry update, `stage13_metrics.csv`, `decision.md`, and `manifest.sha256`.

## Transition to Stage 14

Stage 14 may begin only if Stage 13 is `PASS`, the pilot scope has no unresolved severity-1 issue, independent reviewers have assessed the declared claims, all release owners approve, and the user explicitly approves scaled production operation and continuous governance.

## References

[1]: https://doi.org/10.6028/NIST.AI.600-1 "NIST Generative AI Profile"
[2]: https://www.nist.gov/itl/ai-risk-management-framework "NIST AI Risk Management Framework"
[3]: https://genai.owasp.org/resource/owasp-genai-llm-top-10-2026/ "OWASP GenAI LLM Top 10 2026"
