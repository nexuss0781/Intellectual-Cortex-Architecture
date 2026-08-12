# Production NLP Stage Program

## Purpose

Stages 0–6 established a **research substrate** for deterministic learning, memory, prediction, language, reasoning, metacognition, and controlled developmental transfer. Those stages do not establish production readiness, human-level intelligence, or general intelligence. This program defines the separate, end-to-end engineering stages required to turn the substrate into a governed, trainable, fine-tunable, secure, and auditable NLP product.

Every stage below is independently implemented, tested, archived, and approved before the next stage starts. A passing internal benchmark is not a marketing claim. Claims become permissible only within the evaluated use case, data boundary, deployment configuration, and published limits.

> **Production rule:** Model outputs are untrusted data until they pass authorization, schema, safety, evidence, and policy checks. Consequential actions require a typed allowlist and the approval level specified by the active policy.

## Stage sequence

| Stage | File | Primary outcome | Depends on |
|---:|---|---|---|
| 7 | [`STAGE-07-PRODUCT-GOVERNANCE-BASELINES.md`](STAGE-07-PRODUCT-GOVERNANCE-BASELINES.md) | Frozen product scope, governance, risk boundary, and independent baselines | Stage 6 PASS |
| 8 | [`STAGE-08-DATA-FACTORY-EVALUATION-INTEGRITY.md`](STAGE-08-DATA-FACTORY-EVALUATION-INTEGRITY.md) | Governed data pipeline and hidden, contamination-resistant evaluation suite | Stage 7 PASS |
| 9 | [`STAGE-09-NLP-BASELINE-CONTINUED-PRETRAINING.md`](STAGE-09-NLP-BASELINE-CONTINUED-PRETRAINING.md) | Reproducible base-model selection and measured continued-pretraining capability | Stage 8 PASS |
| 10 | [`STAGE-10-SFT-STRUCTURED-NLP-ENGINE.md`](STAGE-10-SFT-STRUCTURED-NLP-ENGINE.md) | Fine-tuned structured NLP engine with retrieval, citations, and Nexuss integration | Stage 9 PASS |
| 11 | [`STAGE-11-PREFERENCE-SAFETY-CONTINUAL-IMPROVEMENT.md`](STAGE-11-PREFERENCE-SAFETY-CONTINUAL-IMPROVEMENT.md) | Preference/safety post-training, calibration, regression learning, and bounded continual improvement | Stage 10 PASS |
| 12 | [`STAGE-12-PRODUCTION-SERVING-SHADOW-OPERATIONS.md`](STAGE-12-PRODUCTION-SERVING-SHADOW-OPERATIONS.md) | Secure model serving, observability, load evidence, rollback, and shadow traffic | Stage 11 PASS |
| 13 | [`STAGE-13-CANARY-INDEPENDENT-REVIEW.md`](STAGE-13-CANARY-INDEPENDENT-REVIEW.md) | Controlled pilot, human evaluation, red team, external review, and release decision | Stage 12 PASS |
| 14 | [`STAGE-14-PRODUCTION-SCALE-CONTINUOUS-GOVERNANCE.md`](STAGE-14-PRODUCTION-SCALE-CONTINUOUS-GOVERNANCE.md) | Scaled operation with continuous governance, retraining discipline, and auditability | Stage 13 PASS |

## Shared production invariants

| Invariant | Required behavior |
|---|---|
| Claims discipline | State evaluated scope, model version, data boundary, and limitations. Do not claim AGI, human equivalence, consciousness, or real-world grounding without independent evidence. |
| Data lineage | Every train, validation, test, preference, safety, and production-feedback item has source, rights/permission, hash, retention, jurisdiction, and version metadata. |
| Evaluation integrity | Final test sets are hidden, source/time separated, contamination-scanned, and never used for training or tuning. |
| Reproducibility | Every train/eval/deploy run records model, tokenizer, adapter, data, code, seeds, policies, environment, and artifact hashes. |
| Security | Apply secure development, threat modeling, supply-chain integrity, tenant isolation, input/output validation, and least privilege. |
| Tool authority | Language models cannot invoke arbitrary commands or network actions. Every tool call is schema-valid, allowlisted, authorized, rate-limited, auditable, and reversible where possible. |
| Human oversight | Consequential decisions require the approval and review level specified by the risk policy. |
| Privacy | Production feedback does not enter training automatically. It follows observe → redact → sample → review → label → evaluate → approve → train. |
| Resource accounting | Report substrate memory separately from model-weight, KV-cache, retrieval, telemetry, and serving memory. The Stage 0 500 MB substrate envelope is not a blanket deployment-memory claim. |
| Incident learning | Every confirmed incident produces a signed record, containment action, owner, root-cause analysis, and permanent regression candidate. |

## Required implementation pattern

Each stage must include the following sections in its independent specification:

1. Mission and explicit non-claims.
2. Entry conditions and hard transition boundary.
3. Architecture and stable interfaces.
4. Implementation work packages and configuration.
5. Deterministic and independent evaluation harnesses.
6. Positive, negative, boundary, ablation, and adversarial cases.
7. Quantitative pass/fail gates with no unbounded qualitative substitute.
8. Resource, privacy, security, and incident controls.
9. Versioned evidence package, SHA-256 manifest, and formal `decision.md`.
10. User approval requirement before the next stage begins.

## Shared evidence layout

```text
artifacts/stage-N-canonical/
├── config.json
├── scenario_manifest.json
├── environment.txt
├── build.txt
├── ctest.txt
├── sanitizer_ctest.txt
├── stageN_metrics.csv
├── benchmark.csv
├── ablations.csv
├── resource_trace.csv
├── security_results.csv
├── decision.md
└── manifest.sha256
```

Stages add domain-specific artifacts such as dataset cards, model cards, training manifests, evaluation ledgers, reviewer reports, load traces, incident drills, and approval records. A missing required artifact is a gate failure.

## Reference framework

The program uses the NIST AI RMF lifecycle functions—Govern, Map, Measure, and Manage—as governance structure [1]. It uses NIST’s Generative AI Profile for lifecycle risks including confabulation, privacy, bias, information integrity, information security, intellectual property, and supply-chain integration [2]. It uses the NIST AI-focused SSDF profile for secure development [3], the OWASP GenAI security project for application-threat coverage [4], HELM for broad reproducible model evaluation [5], MLPerf and internal workload tests for measurable serving performance [6], and model/dataset-card practices for documentation and lineage [7] [8].

## References

[1]: https://www.nist.gov/itl/ai-risk-management-framework "NIST AI Risk Management Framework"
[2]: https://doi.org/10.6028/NIST.AI.600-1 "NIST AI 600-1: Generative AI Profile"
[3]: https://csrc.nist.gov/pubs/sp/800/218/a/ipd "NIST SP 800-218A"
[4]: https://genai.owasp.org/resource/owasp-genai-llm-top-10-2026/ "OWASP GenAI LLM Top 10 2026"
[5]: https://crfm.stanford.edu/helm/ "Stanford HELM"
[6]: https://mlcommons.org/2026/04/mlperf-inference-v6-0-results/ "MLCommons MLPerf Inference v6.0"
[7]: https://huggingface.co/docs/hub/en/datasets-cards "Hugging Face Dataset Cards"
[8]: https://huggingface.co/docs/hub/en/model-cards "Hugging Face Model Cards"
