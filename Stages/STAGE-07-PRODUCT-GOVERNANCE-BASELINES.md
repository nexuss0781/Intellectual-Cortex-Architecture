# Stage 7 — Product Governance, Risk Boundary, and Independent Baselines

## Mission

Stage 7 converts the Stage 0–6 research artifact into a governed product program. It freezes the first production use case, prohibited uses, risk tier, supported languages, data jurisdictions, human-oversight model, evaluation plan, independent baselines, and release-accountability chain. It creates the operating contract that prevents later training or deployment work from becoming an uncontrolled capability claim.

The outcome is **not** a production launch, model release, or proof of general intelligence. It is a signed, reproducible, testable product and governance baseline.

## Entry conditions

Stage 6 must be `PASS` with its formal decision, canonical artifacts, source code, and verified evidence manifest available. The team must designate a product owner, ML lead, data-governance lead, security lead, trust-and-safety lead, cognitive-systems lead, and release authority. No training or user-facing deployment may begin before these roles and escalation paths are recorded.

## Product boundary

The first product must be an auditable knowledge and workflow intelligence platform operating in advisory mode by default. Consequential actions, including account changes, payments, messaging, production database mutation, code execution, or unrestricted network access, are prohibited unless a later stage grants typed, allowlisted, policy-approved authority.

| Product class | Permitted in Stage 7 baseline | Prohibited in Stage 7 baseline |
|---|---|---|
| Language | Retrieval-grounded QA, extraction, summarization, classification, translation assistance | Unqualified factual authority or hidden reasoning claims |
| Workflow | Plans, checklists, structured drafts, tool proposals | Autonomous execution of consequential external actions |
| Memory | Consent-bound, provenance-bearing records | Silent retention or training use of private conversations |
| Learning | Offline, reviewed, versioned experiments | Online self-modification from raw user traffic |
| Claims | “Research substrate” and “evaluated product prototype” | “AGI,” “superintelligence,” “human-like language learning,” “safe by construction,” or “hallucination-free” |

## Required governance objects

```text
ProductCharter
  - intended users, supported tasks, languages, and domains
  - prohibited uses and high-impact exclusions
  - risk classification and escalation rules
  - service-level objectives and data-residency requirements

ReleaseAuthority
  - named accountable owners and approval quorum
  - severity classification, stop-ship policy, rollback authority
  - exception process with expiry and audit trail

ClaimsRegistry
  - approved claim text, evaluated scope, evidence links
  - expiry date, owner, and disallowed wording

RiskRegister
  - threat, likelihood, impact, mitigation, residual risk
  - validation evidence, owner, and review date
```

Every public or internal capability statement must resolve to a `ClaimsRegistry` item. A claim without evidence scope, owner, and expiry is rejected.

## Interfaces

```cpp
struct ProductScope {
    uint64_t scope_id;
    std::vector<std::string> allowed_tasks;
    std::vector<std::string> prohibited_uses;
    std::vector<std::string> supported_languages;
    uint32_t risk_tier;
    bool advisory_only;
};

struct ReleaseBundle {
    std::string model_digest;
    std::string tokenizer_digest;
    std::string adapter_digest;
    std::string policy_digest;
    std::string dataset_manifest_digest;
    std::string evaluation_manifest_digest;
    std::string code_commit;
};

struct ClaimRecord {
    std::string claim_id;
    std::string text;
    std::string evidence_manifest;
    std::string evaluated_scope;
    std::string owner;
    uint64_t expiry_unix;
};

class GovernanceRegistry {
public:
    bool approve_scope(const ProductScope&, const std::vector<std::string>& approvers);
    bool approve_release(const ReleaseBundle&, const std::vector<std::string>& approvers);
    bool permit_claim(const ClaimRecord&) const;
    std::vector<std::string> blocking_risks() const;
};
```

## Implementation work packages

| Work package | Deliverable |
|---|---|
| P7.1 | Versioned product charter and prohibited-use policy |
| P7.2 | Risk register mapped to NIST AI RMF and OWASP GenAI threats |
| P7.3 | Claims registry, approved language, and claims-review workflow |
| P7.4 | Model, dataset, policy, tool, and release bundle registries |
| P7.5 | Independent baseline selection protocol: one licensed/open-weight, one small local, one external API or equivalent independent reference |
| P7.6 | Benchmark charter, hidden-test custody, and contamination policy |
| P7.7 | Approval quorum, stop-ship, rollback, incident, and exception procedures |
| P7.8 | Stage 7 deterministic governance harness and evidence package |

## Baseline protocol

The baseline protocol must compare at least three independently versioned systems under identical prompts, retrieval conditions, temperature policy, language set, and task budget. Baseline selection cannot use leaderboard rank alone. It must record license, model digest, cost, latency, context limit, known safety limitations, and reproducible invocation configuration.

The benchmark charter must define product metrics before model selection. Primary metrics include task accuracy or rubric score, citation/source validity, schema validity, abstention quality, privacy leakage, prompt-injection resilience, fairness/language coverage, latency, throughput, and cost. The final hidden test set is never used to choose prompts, hyperparameters, adapters, or policies.

## Evaluation harness

The Stage 7 harness must be deterministic over a signed `scope_manifest.json` and must include positive, negative, and boundary cases.

| Test ID | Test | Pass condition |
|---|---|---|
| P7-UNIT-01 | Scope completeness | Every permitted task, prohibited use, language, risk tier, and data jurisdiction is populated |
| P7-UNIT-02 | Claims registry | Every proposed claim links to an evidence manifest, scope, owner, and expiry |
| P7-UNIT-03 | Prohibited claim rejection | AGI, human-equivalence, hallucination-free, and unsupported memory claims are rejected |
| P7-UNIT-04 | Release bundle integrity | Model, tokenizer, adapter, policy, data, eval, and code digests are present and immutable |
| P7-UNIT-05 | Approval quorum | Missing required owner approval blocks release |
| P7-UNIT-06 | Exception expiry | Expired exception automatically blocks release |
| P7-INT-01 | Baseline parity | All three baselines run the same signed benchmark manifest |
| P7-INT-02 | Baseline provenance | License, digest, invocation parameters, cost, and latency are recorded for 100% of baselines |
| P7-INT-03 | Risk coverage | Every NIST/OWASP threat selected by product scope has mitigation, owner, and test reference |
| P7-INT-04 | Stop-ship | Severity-1 finding blocks release regardless of aggregate score |
| P7-INT-05 | Rollback readiness | Release bundle can resolve previous approved bundle deterministically |
| P7-OPS-01 | Determinism | Same manifests produce identical registry hashes |
| P7-OPS-02 | Auditability | 100% of decisions emit immutable audit events |
| P7-OPS-03 | Resource bound | Registry and harness stay within the declared management-plane resource budget |

### Required negative controls

The harness must include seven named negative-control gates: incomplete scope, claim with no evidence, fabricated evidence/unknown license, hidden-test leakage, expired safety exception or missing approval, missing release digest, and cross-tenant context. All must fail closed. The complete executable gate count is **22**: 7 unit, 5 integration, 3 operations, and 7 negative-control gates.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit, integration, operations, and negative-control tests | 100% of 22 executable gates |
| Scope and claims completeness | 100% |
| Prohibited unsupported claims accepted | 0 |
| Release bundles with missing digest | 0 |
| Selected risks with no owner/mitigation/test | 0 |
| Hidden-test leakage approvals | 0 |
| Severity-1 release exceptions | 0 |
| Baseline provenance coverage | 100% |
| Approval/audit decision coverage | 100% |
| Deterministic registry replay | 100% |

## Evidence package

Store `product_charter.md`, `prohibited_uses.md`, `risk_register.csv`, `claims_registry.csv`, `baseline_manifest.json`, `baseline_results.csv`, `benchmark_charter.md`, `approval_matrix.csv`, `rollback_plan.md`, `scope_manifest.json`, `stage7_metrics.csv`, `audit_trace.jsonl`, `resource_trace.csv`, normal and sanitizer test logs, `decision.md`, and `manifest.sha256`.

## Transition to Stage 8

Stage 8 may begin only if Stage 7 is `PASS`, the first product scope is signed, the independent baselines are reproducible, and no severity-1 governance or claims defect remains open. Stage 8 creates the data factory and evaluation-integrity controls; it must not ingest training data before explicit user approval.

## References

[1]: https://www.nist.gov/itl/ai-risk-management-framework "NIST AI Risk Management Framework"
[2]: https://doi.org/10.6028/NIST.AI.600-1 "NIST Generative AI Profile"
[3]: https://genai.owasp.org/resource/owasp-genai-llm-top-10-2026/ "OWASP GenAI LLM Top 10 2026"
