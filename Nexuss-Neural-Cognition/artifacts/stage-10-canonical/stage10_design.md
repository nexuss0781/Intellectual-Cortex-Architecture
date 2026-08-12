# Stage 10 Design — Structured NLP Engine and Grounded Application Path

## Scope and claims boundary

Stage 10 converts the Stage 9 checkpoint identity into a controlled structured-NLP application path. The implementation is an offline, deterministic integration candidate: it validates response contracts, retrieval, citation resolution and entailment, tenant isolation, Nexuss provenance, clarification and abstention, bounded tool proposals, supervised-adapter registry controls, and resource behavior.

The checked-in SFT examples and source documents are synthetic control fixtures. Stage 10 does not claim a production model, real-world language quality, autonomous action, or a public service. It does not execute tools, access a network, read raw private memory, or ingest external data.

## End-to-end application path

```text
request + tenant + untrusted user content
  → input boundary and prompt-injection detector
  → immutable policy manifest
  → ACL-filtered retrieval and source-version check
  → separate Nexuss context adapter
  → deterministic decision planner
  → typed answer / ask / retrieve / abstain / propose_action result
  → citation and entailment validator
  → tool-broker validation with human-approval class
  → provenance, audit, and response hash
```

Retrieved documents remain data, not instructions. User text, retrieved text, policy, and Nexuss context are held in separate fields. A tenant mismatch, stale source, malicious retrieval document, sensitive-data canary, unsupported language, or unauthorized tool request fails closed.

## Stable interfaces

`src/production/stage10_structured_nlp.h` defines `Citation`, `ToolProposal`, `NLPResponse`, `DocumentChunk`, `RetrievalTrace`, `NexussContext`, `StructuredRequest`, `PolicyManifest`, `AdapterManifest`, `OutputValidator`, `RetrievalIndex`, `ToolBroker`, `AdapterRegistry`, and `StructuredNLPEngine`.

The selected Stage 9 model digest and tokenizer digest are carried into every response. The adapter registry rejects user-supplied adapter IDs, base-model mismatches, missing data-release digests, incomplete safety records, and rollback targets. The engine is deterministic and rule-bounded for this stage; it is a structured application architecture, not an assertion that the small Stage 9 checkpoint can perform production NLP.

## Retrieval and citation protocol

Each source chunk has a tenant ACL, version, content hash, source ID, and malicious/stale flags. Retrieval authorizes before ranking. A citation must match an authorized chunk, exact chunk hash, retrieval trace ID, and configured entailment score. A source that is stale, cross-tenant, non-entailing, or malicious cannot support an answer. The response validator rejects citations that do not resolve.

## Nexuss integration

The context adapter accepts only stable IDs and summaries: semantic pointer ID, episodic summary, reasoning trace ID, evidence IDs, contradiction state, and confidence. It never exposes raw private memory. The provenance trace binds the Stage 9 model digest, adapter digest, policy digest, retrieval trace, tenant, and Nexuss trace.

## Tool authority

The tool broker contains a typed allowlist with `draft_report` as the only permitted dry-run proposal. It requires a schema version, scope, idempotency key, and human approval class. Payment, messaging, network, operating-system, database, unknown, and argument-injection proposals are denied. The response may propose a tool but never executes it. Replays preserve the same idempotency key and cannot create a second execution.

## Evaluation contract

The harness consumes `configs/stage10_scenarios.tsv` and `configs/stage10_sft_control.tsv`. It executes 27 gates: 8 unit, 8 integration, 3 operations, and 8 negative controls. Cases include grounded QA, clarification, retrieval request, abstention, multilingual output, ACL mismatch, stale source, conflicting sources, prompt injection, sensitive data, structured extraction, malformed-output repair, allowed tool proposal, denied payment, argument injection, unsupported language, hidden evaluation, and non-entailing evidence.

## Evidence contract

The canonical directory stores adapter/model cards, SFT and scenario manifests, retrieval index/version manifests, response and citation traces, tool-broker audit logs, ACL tests, structured results, multilingual/regression results, ablations, resource traces, restart/rollback comparisons, `stage10_metrics.csv`, normal/sanitizer logs, `decision.md`, and `manifest.sha256`.

## Transition boundary

A Stage 10 PASS authorizes only the controlled offline integration candidate. Stage 11 remains blocked until the user explicitly approves preference and safety post-training. Production training on real licensed data, deployment, autonomous execution, and public claims remain outside this decision.
