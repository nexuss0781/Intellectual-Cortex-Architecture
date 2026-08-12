# Stage 12 — Production Serving, Shadow Traffic, and Operational Readiness

## Mission

Stage 12 deploys the Stage 11 model bundle behind a secure, observable, reversible serving stack and validates it under synthetic load, fault injection, security probes, and shadow traffic. It proves that the **system**, not merely the model checkpoint, can route requests, enforce tenant policy, retrieve authorized sources, validate outputs, record traces, withstand failure, and roll back.

The outcome is **not** a public production launch. Shadow traffic must not change a user-visible answer, execute a consequential action, or write unreviewed feedback into training.

## Entry conditions

Stage 11 must be `PASS`. An approved model/policy/calibration/retrieval/tool bundle, release authority, service-level objectives, incident response plan, data-residency constraints, infrastructure owner, security owner, and emergency rollback operator must be available. Development-only model-server endpoints and dynamic adapter controls are disabled in all production-like environments.

## Reference architecture

```text
client or replay traffic
  → API gateway (auth, quota, tenant, rate, idempotency)
  → request normalizer and policy pre-filter
  → ACL-aware retrieval + Nexuss cognitive service
  → model router + serving pool
  → output/citation/schema/tool validator
  → shadow comparator or response ledger
  → metrics, traces, immutable audit, incident detector
```

The serving layer may expose an OpenAI-compatible interface, but its production deployment must restrict management/development endpoints. vLLM documents compatible generation, embeddings, health, load, and Prometheus metrics, while warning that dynamic LoRA loading, arbitrary RPC, cache reset, and weight-control endpoints are development features that should not be used in production [1].

## Service contract

```cpp
struct RequestEnvelope {
    std::string request_id;
    std::string tenant_id;
    std::string user_or_service_id;
    std::string policy_digest;
    std::string model_route;
    std::string sensitivity;
    uint64_t timeout_ms;
    uint64_t token_budget;
    bool allow_external_action;
};

struct ServiceHealth {
    bool ready;
    bool model_loaded;
    bool retrieval_ready;
    bool policy_ready;
    double queue_depth;
    double p95_latency_ms;
    uint64_t error_count;
};

struct RollbackBundle {
    std::string model_digest;
    std::string adapter_digest;
    std::string tokenizer_digest;
    std::string policy_digest;
    std::string retrieval_index_digest;
    std::string tool_schema_digest;
};

class ServingControlPlane {
public:
    bool deploy_canary(const RollbackBundle&);
    bool route_shadow(const RequestEnvelope&);
    bool quarantine_tenant(const std::string& tenant_id);
    bool rollback(const RollbackBundle&);
    ServiceHealth health() const;
};
```

## Implementation work packages

| Work package | Deliverable |
|---|---|
| O12.1 | Signed container, SBOM, dependency lockfile, image scanning, and artifact registry |
| O12.2 | API gateway with authentication, authorization, quotas, rate limits, idempotency, request-size and token budgets |
| O12.3 | Model router, static model/adapter registry, warmup, readiness, health, and circuit breaker |
| O12.4 | Retrieval/Nexuss/tool/policy service isolation with mTLS, secrets management, and tenant boundaries |
| O12.5 | Metrics, traces, structured logs, audit ledger, privacy redaction, and retention controls |
| O12.6 | Load, soak, concurrency, queue, cancellation, timeout, and back-pressure harness |
| O12.7 | Fault-injection and disaster/rollback drills |
| O12.8 | Shadow traffic comparator, sampling policy, feedback quarantine, and operational decision package |

## Evaluation harness

| Test ID | Test | Pass condition |
|---|---|---|
| O12-UNIT-01 | Artifact integrity | Container, model, adapter, tokenizer, policy, retrieval, and tool digests match approved bundle |
| O12-UNIT-02 | AuthN/AuthZ | Missing, expired, cross-tenant, or out-of-scope identity is denied |
| O12-UNIT-03 | Rate/token limit | Requests beyond signed quota/token budget are throttled without service instability |
| O12-UNIT-04 | Endpoint exposure | Development, weight-update, arbitrary-RPC, and unsafe management endpoints are unreachable |
| O12-UNIT-05 | Secret isolation | Secret canaries never appear in logs, metrics, traces, or model output |
| O12-UNIT-06 | Audit trace | Request route, model/policy bundle, retrieval, validator, and decision emit linked audit record |
| O12-UNIT-07 | Feedback quarantine | Shadow/user feedback cannot enter training release directly |
| O12-INT-01 | Load SLO | p50/p95/p99, TTFT, throughput, error, timeout, and queue metrics meet signed service tier |
| O12-INT-02 | Quality under load | Stage 11 quality/safety/citation metrics remain within signed tolerance at target concurrency |
| O12-INT-03 | Tenant isolation | Concurrent tenants cannot access each other’s retrieval, memory, logs, or quotas |
| O12-INT-04 | Policy fail-closed | Policy/retrieval/tool-validator outage fails safe by refusal, fallback, or advisory degradation |
| O12-INT-05 | Fault recovery | Instance, network, dependency, and index faults recover within signed RTO/RPO/SLO |
| O12-INT-06 | Rollback | Full bundle rollback restores prior behavior and audit continuity within signed time |
| O12-INT-07 | Shadow parity | Shadow results are compared without user-visible mutation; material regressions create alerts |
| O12-INT-08 | Security probes | Injection, output handling, DoS, supply-chain, and data-exfiltration probes meet policy gate |
| O12-OPS-01 | Soak stability | Signed-duration soak has no memory leak, unbounded queue, or error-rate violation |
| O12-OPS-02 | Capacity evidence | Scale-out/load-shedding behavior matches capacity plan and cost budget |
| O12-OPS-03 | Observability | 100% sampled requests satisfy trace/audit completeness target without privacy leak |

### Required fault and attack cases

Inject model-server crash, GPU out-of-memory, malformed token stream, retrieval timeout, policy-service outage, expired certificate, registry digest mismatch, dependency compromise signal, queue overload, tenant flood, prompt injection, indirect injection through retrieved documents, malicious JSON/tool arguments, secrets in prompts, stale index, and rollback during active traffic. The system must fail closed when authorization, validation, or evidence dependencies are unavailable.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit/integration/operations tests | 100% |
| Unauthorized endpoint/tenant/tool access | 0 successful cases |
| Development-only endpoint exposure | 0 exposed endpoints |
| Audit/trace bundle completeness | 100% of sampled requests |
| Secret/PII leakage in approved logs/traces | 0 critical leaks |
| Quality/safety/citation regression at target load | ≤ signed tolerance |
| p95/p99/TTFT/throughput/error/timeout | Meet signed service tier |
| Memory leak/unbounded queue during soak | 0 |
| Full bundle rollback success | 100% within signed RTO |
| Fault recovery and fail-safe behavior | 100% declared cases |
| Shadow traffic user-visible mutation | 0 |
| Unreviewed feedback sent to training | 0 items |

## Evidence package

Store infrastructure manifests, signed bundle/SBOM, endpoint inventory, security scan, configuration digests, load/soak traces, latency/throughput/cost benchmarks, quality-under-load report, fault-injection results, rollback drill logs, shadow comparison report, tenant-isolation tests, audit completeness report, incident runbooks, capacity plan, `stage12_metrics.csv`, normal/sanitizer/service-security logs, `decision.md`, and `manifest.sha256`.

## Transition to Stage 13

Stage 13 may begin only after Stage 12 is `PASS`, shadow traffic has demonstrated no user-visible mutation, all critical security and operational findings are closed, full-bundle rollback is proven, and the user explicitly approves a controlled pilot with human and independent review.

## References

[1]: https://docs.vllm.ai/en/stable/serving/online_serving/ "vLLM Online Serving"
[2]: https://mlcommons.org/2026/04/mlperf-inference-v6-0-results/ "MLPerf Inference v6.0"
[3]: https://csrc.nist.gov/pubs/sp/800/218/a/ipd "NIST AI Secure Software Development Profile"
[4]: https://genai.owasp.org/resource/owasp-genai-llm-top-10-2026/ "OWASP GenAI LLM Top 10 2026"
