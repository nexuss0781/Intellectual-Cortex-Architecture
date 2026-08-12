# Stage 12 Decision — Secure Serving and Shadow Operations

**Decision:** `PASS_OFFLINE_SHADOW_OPERATIONS_NONPRODUCTION`

**Date:** 2026-08-12

**Entry:** Stage 11 candidate `model@2367994276643219537`

## Scope

Stage 12 implemented and exercised an offline serving control plane around the approved Stage 11 bundle. The control plane verifies signed bundle identity, authenticates and authorizes tenant requests, applies token/timeout/queue quotas, enforces static management endpoints, performs ACL-aware retrieval through the existing structured NLP engine, validates output/citations/tool proposals, emits linked audit records, compares shadow results without mutation, quarantines feedback, injects faults, load-sheds, and rolls back to the Stage 10 target.

This is a **repository-local validation harness**, not a deployed service. It does not open an external listener, use real user traffic, deploy a container, create a canary, or begin Stage 13. The only executed traffic is deterministic in-process replay and synthetic load.

## Approved bundle

| Component | Digest or policy |
|---|---|
| Model | `model@2367994276643219537` |
| Adapter | `adapter@stage11-native-byte` |
| Tokenizer | `tokenizer@byte-v1` |
| Policy | `policy@stage10-v1` |
| Retrieval index | `index@2581245035366428525` |
| Tool schema | `tool-schema-v1` |
| Rollback target | `model@2775430139297845034` |

Dynamic adapter loading, arbitrary RPC, weight-control endpoints, consequential external actions, and automatic feedback-to-training writes are disabled. The bundle requires authenticated identity, tenant scope, static policy, externalized secrets, and fail-closed behavior when model, retrieval, policy, or tool dependencies are unavailable.

## Measured evidence

| Evidence | Result |
|---|---:|
| Stage 12 control-plane gates | **22/22 passed** |
| Normal repository CTest suite | **27/27 passed** |
| ASan/UBSan CTest suite | **27/27 passed**; no diagnostics |
| Load requests | 512 |
| Accepted load requests | 512 |
| p50 latency | 0.004 ms |
| p95 latency | 0.006 ms; signed maximum 100 ms |
| p99 latency | 0.009 ms; signed maximum 200 ms |
| Throughput | 170,667 requests/s in-process synthetic replay |
| Quality/validator contract under load | 512/512 |
| Tenant isolation under load | 512/512 |
| Audit records | 537; linked audit digest emitted |
| Shadow user-visible mutations | 0 |
| Shadow feedback training writes | 0 |
| Normal peak RSS | 5,260 KB; signed maximum 500,000 KB |
| Rollback | Passed; Stage 10 model identity restored |

The measured latency and throughput are **in-process harness measurements** and must not be interpreted as network-service SLOs. They do not include TLS, network, serialization, container, GPU, queue, remote retrieval, or production infrastructure overhead.

## Security and failure evidence

The harness denied missing, expired, cross-tenant, and out-of-scope identities. It rejected token, timeout, and queue-budget violations. Development and management endpoints—including weight loading, adapter loading, cache reset, arbitrary RPC, debug RPC, and weight/adapter routes—were blocked. Secret/injection canaries did not appear in responses or audit identities.

Policy, retrieval, tool, model, queue, and expired-certificate faults failed closed. Prompt injection, consequential external-action, payment, dynamic-adapter, unsafe-bundle, and cross-tenant probes were denied. Shadow comparison emitted no user-visible mutation and no training write. A full bundle rollback restored the previous approved Stage 10 model identity and preserved audit continuity.

## Decision and boundaries

Stage 12 passes as an **offline serving and shadow-operations implementation**. It establishes evidence that the local control plane enforces the declared contracts; it does not prove production readiness.

`production_allowed=false`, `external_deployment=false`, and `stage13_allowed=false` remain explicit in the run manifest and configuration. No public service, shadow traffic from real users, canary cohort, independent human review, or Stage 13 operation has started.

Stage 13 may begin only after a separate explicit approval for controlled canary and independent review. No claim is made that the system is universally safe, production-ready, human-level, or capable of general intelligence.

## Evidence index

- `stage12_run_manifest.json` — bundle identity, boundaries, load, shadow, and audit digest.
- `stage12_gates.csv` — 22 serving, security, shadow, resilience, and operations gate outcomes.
- `stage12_metrics.csv` — load, quality, tenant, shadow, feedback, audit, and resource metrics.
- `stage12_load.csv` — synthetic load measurements.
- `stage12_audit.csv` — linked redacted audit records.
- `stage12_shadow.csv` — shadow comparison records.
- `stage12_faults.csv` — declared fault cases and outcomes.
- `stage12_endpoint_inventory.csv` — allowed and denied endpoint inventory.
- `normal_ctest.txt` — final 27-target normal suite.
- `sanitizer_ctest.txt` — final 27-target ASan/UBSan suite.
- `manifest.sha256` — immutable evidence manifest.

## References

[1]: https://docs.vllm.ai/en/stable/serving/online_serving/ "vLLM Online Serving"
[2]: https://csrc.nist.gov/pubs/sp/800/218/a/ipd "NIST AI Secure Software Development Profile"
[3]: https://genai.owasp.org/resource/owasp-genai-llm-top-10-2026/ "OWASP GenAI LLM Top 10"
