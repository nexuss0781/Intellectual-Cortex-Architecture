# Stage 7 Design — Executable Governance and Independent-Baseline Product Path

## Design decision

Stage 7 will be implemented as a dependency-light C++17 governance and benchmark service inside the existing Nexuss repository. It will execute an actual end-to-end advisory workflow over a signed scenario manifest rather than merely checking that fields or functions exist.

The product path is deliberately transparent. It will use three independently implemented deterministic baseline adapters over the same real task scenarios:

1. `nexuss_advisory_v1`: the governed product path with policy, evidence, provenance, scope, and fail-closed claims checks.
2. `local_reference_v1`: an independent lexical/reference implementation with no Nexuss state access.
3. `external_equivalent_v1`: an independent adapter that models the contract of an external API reference without claiming that a real external provider was contacted.

This is enough to validate the **baseline protocol and governance machinery**. It is not a claim that a production foundation model has been benchmarked. Stage 9 will require actual model selection and continued-pretraining evidence.

## Real application workflow

Each signed scenario contains a tenant, language, task type, user request, authorized context documents, expected decision class, expected structured fields, and a risk category. The workflow is:

```text
scenario manifest
  → scope and authorization check
  → policy pre-filter
  → source selection with content hashes
  → baseline adapter inference
  → structured response validation
  → citation/source validation
  → provenance trace creation
  → baseline metric and latency recording
  → immutable audit event
```

The benchmark scenarios cover grounded question answering, extraction, summarization, classification, translation assistance, unsupported/OOD abstention, prohibited action refusal, cross-tenant denial, and expired-claim rejection. Each scenario has positive, negative, and boundary variants. A scenario is not considered passed because the executable returned; its decision, evidence, structured result, provenance, and policy outcome must match the declared contract.

## Governance state machine

```text
UNINITIALIZED
  → SCOPE_APPROVED
  → EVIDENCE_REGISTERED
  → BASELINES_REGISTERED
  → RELEASE_APPROVED
  → CLAIMS_PERMITTED

Any missing digest, missing role, unresolved severity-1 risk,
expired exception, prohibited claim, or invalid evidence returns BLOCKED.
```

The registry uses stable IDs, canonical serialization, deterministic hashes, immutable audit events, required approval roles, risk blockers, and explicit release bundles. It does not infer approval from a passing score.

## Stable interfaces

The implementation adds `src/production/stage7_governance.h` with:

- `ProductScope`, `ReleaseBundle`, `ClaimRecord`, `RiskRecord`, `ApprovalRecord`, `BaselineSpec`, `Scenario`, `ScenarioResult`, and `AuditEvent`.
- `GovernanceRegistry` for scope, evidence, baseline, release, risk, exception, claims, approval, and audit operations.
- `Stage7Application` for policy-filtered scenario execution and provenance-bearing responses.
- `BaselineRunner` for the three independent adapters, identical scenario manifests, and measured output traces.
- deterministic `stage7_hash_string`, `stage7_mix`, canonical field serialization, and no global mutable RNG.

## Failure-injection plan

The harness loads `configs/stage7_scenarios.tsv`, verifies the immutable Stage 6 entry evidence at `artifacts/stage-6-canonical`, and actively exercises missing scopes, incomplete claims, fabricated evidence, unknown licenses, missing security approval, expired exceptions, hidden-test leakage flags, unresolved severity-1 risk, cross-tenant context, prohibited actions, and altered release digests. Every negative case must fail closed and emit an audit event. The executable contract is 22 gates: 7 unit, 5 integration, 3 operations, and 7 negative controls.

## Resource and safety policy

The management-plane registry and offline benchmark must remain below a signed 64 MB RSS budget. The harness must report peak RSS and elapsed time per baseline. No network call, external side effect, user-data retention, shell execution, or arbitrary tool invocation is permitted. The external-equivalent baseline is clearly labelled as a protocol adapter, not a contacted external model.

## Evidence contract

The harness writes:

```text
product_charter.md
prohibited_uses.md
risk_register.csv
claims_registry.csv
baseline_manifest.json
baseline_results.csv
benchmark_charter.md
approval_matrix.csv
rollback_plan.md
scope_manifest.json
stage7_metrics.csv
audit_trace.jsonl
resource_trace.csv
scenario_results.csv
negative_controls.csv
```

The canonical workflow additionally writes build, CTest, normal harness, sanitizer, configuration, environment, formal decision, and SHA-256 manifest files. The formal decision must state the exact number of gates, all outcomes, the transparent baseline limitation, and `Stage 8 NOT STARTED`.

## Transition boundary

Stage 7 PASS authorizes only Stage 8 design/implementation after explicit user approval. It does not authorize data ingestion, model training, user-facing deployment, external API claims, tool execution, or production launch.
