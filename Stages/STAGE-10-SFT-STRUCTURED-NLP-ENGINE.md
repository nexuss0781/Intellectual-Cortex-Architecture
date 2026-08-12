# Stage 10 — Supervised Fine-Tuning and Structured NLP Engine

## Mission

Stage 10 turns the Stage 9 checkpoint into a structured NLP engine that can produce useful, grounded, auditable responses and bounded tool proposals. It integrates supervised fine-tuning, retrieval-augmented context, citation/source validation, typed response schemas, Nexuss provenance/memory/reasoning, abstention, clarification, and a deny-by-default tool broker.

The outcome is **not** a public service or autonomous agent. It is a controlled integration candidate validated offline on hidden data and adversarial simulations.

## Entry conditions

Stage 9 must be `PASS`. A selected checkpoint, complete model card, training run manifest, data releases, hidden evaluation sets, security policy, allowed task list, tool authority policy, and baseline measurements must be available. No tool may receive broad operating-system, database, payment, messaging, or network authority.

## Response and authority contract

```cpp
struct Citation {
    std::string source_id;
    std::string chunk_hash;
    std::string retrieval_trace_id;
    float relevance;
    float entailment;
};

struct ToolProposal {
    std::string tool_name;
    std::string schema_version;
    std::string arguments_json;
    std::string idempotency_key;
    std::string authorization_scope;
    bool requires_human_approval;
};

struct NLPResponse {
    std::string request_id;
    std::string model_digest;
    std::string adapter_digest;
    std::string policy_digest;
    std::string answer;
    std::string structured_result_json;
    std::vector<Citation> citations;
    std::vector<ToolProposal> proposed_tools;
    std::string decision; // answer, ask, retrieve, abstain, propose_action
    float confidence;
    bool calibrated;
    std::string provenance_trace_id;
};

class OutputValidator {
public:
    bool validate_schema(const NLPResponse&) const;
    bool validate_citations(const NLPResponse&) const;
    bool validate_policy(const NLPResponse&) const;
    bool validate_tool_proposals(const NLPResponse&) const;
};
```

A valid answer must either be supported by retrieved/declared evidence, be clearly marked as uncertain/generative, or abstain. A valid citation must resolve to an ACL-authorized retrieved chunk and meet a configured entailment rule. A valid tool proposal must be schema-valid, allowlisted, authorized, rate-limited, and assigned an approval class. A model response never executes a tool directly.

## Context assembly

The context builder must keep untrusted user input, retrieved documents, system policy, and Nexuss memory separate. Retrieved documents are data, not instructions. The final model prompt must identify source boundaries and provenance IDs. The Nexuss component contributes stable semantic pointers, episodic summaries, contradiction state, confidence, and proof/evidence trace references; it must not expose raw private memory to an unauthorized tenant.

| Context layer | Inputs | Required control |
|---|---|---|
| Policy | Product scope, tenant policy, tool permissions, safety rules | Immutable signed version |
| Retrieval | ACL-filtered passages, source hashes, time/version metadata | Authorization before ranking |
| Nexuss state | Task graph, summaries, evidence references, confidence | Tenant isolation and provenance |
| User content | User message and attachments | Treated as untrusted data |
| Output constraints | JSON schema, citation requirements, tool schema | Deterministic validator |

## Implementation work packages

| Work package | Deliverable |
|---|---|
| N10.1 | SFT dataset schema for answer, citation, uncertainty, clarification, refusal, and tool proposals |
| N10.2 | SFT/PEFT training pipeline with adapter registry and model-card update |
| N10.3 | Versioned request/response schema and constrained decoding/repair loop |
| N10.4 | ACL-aware retrieval, reranking, source/chunk provenance, and citation validator |
| N10.5 | Nexuss cognitive-service adapter for events, memory summaries, evidence, reasoning, and confidence |
| N10.6 | Tool policy broker with typed allowlist, dry-run, approval, idempotency, and audit log |
| N10.7 | Input safety/privacy/prompt-injection detection and output safety validator |
| N10.8 | Offline end-to-end harness with baseline, ablation, adversarial, and resource cases |

## SFT protocol

Train SFT examples to teach the product contract, not simply conversational style. Required example families include structured extraction, evidence-grounded answers, no-source abstention, ambiguity clarification, correct citation placement, malformed JSON repair, denied/allowed tool proposals, sensitive-data handling, prompt injection resistance, multilingual responses, and correction after feedback.

Parameter-efficient adapters may be used for iteration, but each adapter must declare its base checkpoint, data release, task scope, license, evaluation, safety findings, memory/latency cost, and rollback target. Dynamic adapter selection is controlled only by the signed model registry; user-supplied adapter identifiers are forbidden.

## Evaluation harness

| Test ID | Test | Pass condition |
|---|---|---|
| N10-UNIT-01 | SFT and adapter schema | SFT fixture, adapter manifest, and task scope load with non-production status |
| N10-UNIT-02 | Stage 9 entry integrity | Stage 9 PASS decision, manifest, selected checkpoint, and clean summary are present |
| N10-UNIT-03 | Retrieval index manifest | Checked-in source chunks load with deterministic version/hash manifest |
| N10-UNIT-04 | Production data boundary | No SFT fixture record is marked production-allowed |
| N10-UNIT-05 | Sealed custody | Hidden SFT/evaluation examples are explicitly sealed |
| N10-UNIT-06 | Signed adapter selection | Approved adapter matches the selected Stage 9 model identity |
| N10-UNIT-07 | User adapter denial | User-supplied adapter identifiers cannot be selected |
| N10-UNIT-08 | Stage 9 regression entry | Selected checkpoint summary has zero Stage 9 failures |
| N10-INT-01 | Structured task quality | Hidden extraction/classification/summary score meets signed target versus Stage 9 baseline |
| N10-INT-02 | Grounded QA | Answer faithfulness, citation precision, and citation completeness meet signed targets |
| N10-INT-03 | Clarification value | Ambiguous-task outcome improves by signed margin after clarification |
| N10-INT-04 | Abstention quality | Unsupported/OOD request abstention or escalation meets signed minimum without excessive false abstention |
| N10-INT-05 | Tool proposal quality | Allowed-task plan accuracy meets target; unauthorized-action rate is zero |
| N10-INT-06 | Nexuss trace completeness | Every response records model/policy/retrieval/Nexuss provenance trace |
| N10-INT-07 | Multilingual behavior | Supported-language performance remains within signed disparity bound |
| N10-INT-08 | Regression retention | Stage 9 primary metrics do not regress beyond signed tolerance |
| N10-OPS-01 | Latency/resource | End-to-end latency, CPU/GPU/KV/RSS, and retrieval cost meet signed budget |
| N10-OPS-02 | Determinism | Fixed seed, data, model, policy, and retrieval manifest reproduce structured decision/trace hashes where decoding policy permits |
| N10-OPS-03 | Restart/rollback | Request and policy service restart maintains correctness and audit continuity |

### Required ablations and safety tests

Compare full system against no retrieval, no citation validator, no output schema, no Nexuss provenance, no confidence/abstention, no tool broker, and no input-boundary isolation. Each removal must degrade the corresponding benchmark or safety control. Test direct and indirect prompt injection, malicious retrieval documents, citation fabrication, stale source, cross-tenant content, JSON escape attacks, tool argument injection, sensitive-data canaries, unsupported-language requests, and conflicting sources. The executable contract is **27 gates**: 8 unit, 8 integration, 3 operations, and 8 negative controls.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit, integration, operations, and negative-control tests | 100% of 27 executable gates |
| Schema-valid responses | ≥ signed product threshold |
| Citation source resolution | 100% |
| Fabricated/non-entailing citation acceptance | 0 |
| Grounded answer faithfulness | ≥ signed product threshold |
| Unsupported/OOD safe response | ≥ signed product threshold |
| Unauthorized tool execution | 0 |
| Tool proposal schema/allowlist validity | 100% |
| Cross-tenant data exposure | 0 |
| Provenance trace coverage | 100% |
| Supported-language disparity | ≤ signed bound |
| Stage 9 primary-metric regression | ≤ signed bound |
| Resource/SLO violations | 0 |

## Evidence package

Store SFT release manifest, adapter card, model card, response/tool schemas, retrieval index/version manifest, source/citation traces, ACL tests, SFT curves, hidden evaluation results, human spot-checks, ablations, prompt-injection/security results, tool-broker audit, multilingual results, resource/load traces, restart/rollback results, `stage10_metrics.csv`, normal/sanitizer logs, `decision.md`, and `manifest.sha256`.

## Transition to Stage 11

Stage 11 may begin only if Stage 10 is `PASS`, the structured engine is grounded and traceable within its declared scope, no unauthorized tool or tenant-isolation defect remains, and the user explicitly approves preference/safety post-training and continual-improvement implementation.

## References

[1]: https://huggingface.co/docs/transformers/main/en/training "Hugging Face Fine-tuning"
[2]: https://docs.vllm.ai/en/stable/serving/online_serving/ "vLLM Online Serving"
[3]: https://genai.owasp.org/resource/owasp-genai-llm-top-10-2026/ "OWASP GenAI LLM Top 10 2026"
