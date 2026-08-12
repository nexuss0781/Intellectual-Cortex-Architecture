# Stage 10 Transition Decision — Structured NLP Engine and Grounded Application Candidate

## Decision

**PASS for the controlled offline structured-NLP integration candidate.** The repository now contains and executes a typed response engine with ACL-aware retrieval, citation resolution and entailment checks, Nexuss provenance context, supervised-adapter registry controls, multilingual handling, clarification, abstention, prompt-boundary isolation, and a deny-by-default tool broker. The canonical application scenarios, normal suite, sanitizer suite, restart path, deterministic response hashing, and resource measurements all passed.

This is **not** a claim of production NLP quality, a public service, an autonomous agent, or a generally intelligent model. The SFT examples and source documents are checked-in synthetic control fixtures. Stage 10 validates the architecture and offline application contract; it does not execute a real production-corpus SFT weight update. Stage 11 remains blocked.

## Entry evidence

The harness verified the committed Stage 9 decision, manifest, selected checkpoint identity, and clean summary before constructing the Stage 10 adapter and response engine.

| Entry item | Result | Evidence |
|---|---:|---|
| Stage 9 decision | PASS for executed pilot | `artifacts/stage-9-canonical/decision.md` |
| Selected checkpoint | `model@18217991639257382938` | Stage 9 decision, adapter manifest |
| Tokenizer identity | `tokenizer@identity-byte-v1` | Stage 9 model manifest |
| Stage 9 clean summary | `failures=0` | `stage9_summary.txt` |
| SFT control examples | 14 | `sft_fixture.tsv` |
| Retrieval source chunks | 10 | `source_manifest.tsv`, `retrieval_manifest.tsv` |
| Application scenarios | 20 | `scenario_manifest.tsv` |
| Data status | Synthetic, non-production | `config.json`, `model_card.md` |
| Direct tool execution | Disabled from model response | `tool_audit.csv`, `config.json` |

## Implemented architecture

The structured response contract carries request ID, selected model identity, adapter identity, policy digest, answer, structured JSON, citations, bounded tool proposals, decision class, confidence, calibration flag, tenant ID, and provenance trace ID. The allowed decision classes are `answer`, `ask`, `retrieve`, `abstain`, and `propose_action`.

Retrieval authorizes by tenant ACL before ranking. Each chunk has a source ID, version, content hash, relevance, entailment score, malicious flag, stale flag, and ACL. A citation is accepted only when it resolves to the exact authorized chunk hash, retrieval trace, tenant, and minimum entailment rule. User content and retrieved documents remain data and cannot override policy.

The Nexuss adapter carries stable semantic-pointer, episodic-summary, reasoning-trace, evidence-ID, contradiction, and confidence fields. Raw private memory is not exposed. The provenance trace binds the tenant, request, model digest, adapter digest, policy digest, retrieval trace, and Nexuss trace.

The only allowlisted tool is a `draft_report` dry-run proposal under `document.draft`. It requires a schema version, arguments object, idempotency key, and human-approval flag. Payment, network, messaging, operating-system, database, unknown, and argument-injection requests are blocked. The model response cannot execute a tool directly. The harness performs one explicit approved dry-run execution only to verify idempotency; no external side effect or network call occurs.

## Gate results

All **27 executable gates** passed: 8 unit, 8 integration, 3 operations, and 8 negative controls.

| Gate group | Result | Measured evidence |
|---|---:|---|
| N10-UNIT-01 through N10-UNIT-08 | **8/8 PASS** | SFT/adapter schema, Stage 9 entry, deterministic retrieval manifest, non-production boundary, sealed custody, signed adapter, user-adapter denial, Stage 9 clean summary |
| N10-INT-01 | PASS | 20/20 hidden application scenarios matched expected decisions and validators |
| N10-INT-02 | PASS | 6 grounded answers; 6/6 citations resolved with valid hashes and entailment |
| N10-INT-03 | PASS | Ambiguous and conflicting requests clarified |
| N10-INT-04 | PASS | 9 unsupported/sensitive/injection/sealed/denied cases abstained safely |
| N10-INT-05 | PASS | 2 approved dry-run proposals; 0 autonomous executions before control test |
| N10-INT-06 | PASS | 20/20 responses carried complete provenance |
| N10-INT-07 | PASS | 2/2 supported Arabic cases remained bounded |
| N10-INT-08 | PASS | Stage 9 model identity and selected checkpoint retained |
| N10-OPS-01 | PASS | 4,192 KB RSS against 524,288 KB limit; 10 chunks and 20 scenarios within limits |
| N10-OPS-02 | PASS | Identical response and retrieval hashes for fixed request/model/policy |
| N10-OPS-03 | PASS | Restarted registry/engine reproduced response hash exactly |
| N10-NEG-01 | PASS | Malformed response schema rejected |
| N10-NEG-02 | PASS | Fabricated citation hash rejected |
| N10-NEG-03 | PASS | Cross-tenant response rejected |
| N10-NEG-04 | PASS | Malicious retrieval source rejected before answer support |
| N10-NEG-05 | PASS | Stale source rejected |
| N10-NEG-06 | PASS | Payment tool proposal denied |
| N10-NEG-07 | PASS | Direct user prompt injection contained by abstention |
| N10-NEG-08 | PASS | First approved dry-run could execute; replay with same idempotency key was blocked |

## Quantitative evidence

| Metric | Result | Required condition |
|---|---:|---:|
| Schema-valid response rate | 1.000 | ≥ 0.90 |
| Citation resolution rate | 1.000 | 1.000 |
| Provenance coverage | 1.000 | 1.000 |
| Unauthorized tool execution | 0 | 0 |
| Cross-tenant exposure | 0 | 0 |
| Malicious/stale source acceptance | 0 | 0 |
| Supported-language cases | 2/2 | Declared English/Arabic scope |
| Peak RSS | 4,192 KB | < 524,288 KB |
| Retrieval chunks | 10 | ≤ 1,000 |
| Scenario count | 20 | ≤ 1,000 |
| Tool audit events | 6 | Complete audit sequence |

## Ablations and adversarial controls

The evidence package records removal controls for retrieval, citation validation, output schema, Nexuss provenance, confidence/abstention, tool brokerage, and input-boundary isolation. Negative tests cover malformed output, fabricated citations, cross-tenant references, malicious and stale retrieval, payment authority, direct prompt injection, and idempotency replay. These are deterministic architecture tests over a synthetic fixture; they do not establish coverage of the full real-world threat landscape.

## Canonical and sanitizer validation

| Validation | Result | Evidence |
|---|---:|---|
| Normal full CTest suite | **23/23 PASS** | `normal_ctest.txt`, `ctest.txt` |
| Canonical Stage 0–10 workflow | **All Stage 0–10 PASS** | `workflow.log`, `stage10_harness.txt` |
| ASan/UBSan full suite | **23/23 PASS** | `sanitizer_ctest.txt`, `sanitizer_build.txt` |
| Sanitizer diagnostics | None reported | `sanitizer_ctest.txt` |
| Fixed-input determinism | PASS | `restart_rollback.csv`, `response_traces.csv` |
| Evidence manifest | Required after final assembly | `manifest.sha256` |

The repository retains pre-existing legacy warnings, including the earlier meta-cognition unused-variable warning and CMake FetchContent developer warning. Stage 10 introduced no new warning after its compile cleanup and produced no sanitizer diagnostic.

## Explicit limitations and non-claims

Stage 10 does **not** include a real production corpus, real-user data, external foundation-model weights, human evaluation, external security review, public serving, distributed inference, network tools, payment tools, or deployment infrastructure. The SFT manifest defines the supervised-learning contract and adapter metadata, but this stage did not perform a production-corpus neural SFT update. The deterministic response planner is an architecture control path, not evidence that the small Stage 9 byte model can generate production-quality language.

The 20 scenarios, 14 SFT examples, and 10 source chunks are too small to support statistical claims about real NLP capability. The 100% values mean 100% on the declared offline fixtures. The tool audit’s one execution is an explicitly authorized local dry-run used to prove idempotency; it is not a production side effect.

The correct claim is: **Nexuss now has a validated, provenance-aware, tenant-aware, citation-grounded, deny-by-default structured NLP application architecture that is ready for later replacement of control fixtures with approved real data and a real fine-tuned model.**

## Transition boundary

**Stage 11 is NOT STARTED.** It may begin only after explicit approval. Stage 11 must address preference/safety post-training and continual-improvement controls, but it must not silently convert synthetic fixtures into production training data. A future production SFT run requires a separately approved licensed dataset plan, model/adapter selection, human evaluation protocol, security review, and deployment boundary.

## Final status

`STAGE10_DECISION=PASS_OFFLINE_INTEGRATION_CANDIDATE`

`EXECUTABLE_GATES=27/27`

`NORMAL_CTEST=23/23`

`SANITIZER_CTEST=23/23`

`SCENARIOS=20/20`

`SCHEMA_VALID_RATE=1.0`

`CITATION_RESOLUTION=1.0`

`PROVENANCE_COVERAGE=1.0`

`UNAUTHORIZED_TOOL_EXECUTION=0`

`DATA_STATUS=SYNTHETIC_CONTROL_NOT_FOR_PRODUCTION_TRAINING`

`REAL_PRODUCTION_SFT=NOT_EXECUTED`

`STAGE11_STATUS=NOT_STARTED`
