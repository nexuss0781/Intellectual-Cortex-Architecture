# Stage 13 Preparation Decision — Canary Governance and Independent Review Contracts

**Decision:** `PREPARATION_PASS_LIVE_CANARY_NOT_AUTHORIZED`

**Date:** 2026-08-12

**Entry:** Stage 12 candidate `model@2367994276643219537`

## Scope

Stage 13 preparation implements and exercises repository-local governance contracts for a possible future controlled canary. The contracts cover consent enrollment and withdrawal, cohort and traffic limits, permitted-task and excluded-domain boundaries, kill-switch and rollback behavior, risk-triggered and random sampling, minimized reviewer records, reviewer access auditing, blinded and multi-rater evaluation preparation, expert adjudication for high-impact safety cases, incident ownership and containment, red-team closure and retest, independent-review schemas, support-path readiness, and the rule that only reviewed incidents may produce regression candidates.

The preparation harness is an **offline control-contract test with bounded in-process simulation**. It did not enroll live users, route external traffic, open a public service, perform an external security or model review, or authorize training from pilot feedback. The evidence package therefore records readiness contracts and negative controls, not human pilot outcomes, production safety certification, or general-intelligence capability.

## Entry and governing boundary

| Contract | Recorded value |
|---|---|
| Entry stage | Stage 12 |
| Entry status | `PASS_OFFLINE_SHADOW_OPERATIONS_NONPRODUCTION` |
| Entry model | `model@2367994276643219537` |
| Adapter | `adapter@stage11-native-byte` |
| Policy | `policy@stage10-v1` |
| Preparation mode | `true` |
| Live canary started | `false` |
| External review executed | `false` |
| Stage 14 allowed | `false` |

The candidate cohort contract is advisory and bounded at **10 users maximum** and **1% traffic fraction**. Its permitted tasks are grounded document answering and clarification. Medical, legal, financial, and external-action domains are excluded, and consequential actions are disabled. Consent uses `consent-v1`; withdrawal is a hard boundary. Any future resume, expansion, or transition requires a governed approval rather than an automatic action.

## Measured evidence

| Evidence | Result |
|---|---:|
| Stage 13 preparation gates | **25/25 passed** |
| Normal repository CTest suite | **28/28 passed** |
| ASan/UBSan CTest suite | **28/28 passed**; status 0 and no sanitizer failure |
| Canonical workflow | **Completed successfully**; status 0 |
| Simulated cohort users | 1 |
| Simulated traffic fraction | 0.010000 |
| Review records in bounded simulation | 5 |
| Simulated quality rate | 1.000000 |
| Simulated grounding rate | 1.000000 |
| Unresolved severity-one incidents | 0 |
| Unresolved critical red-team findings | 0 |
| Live users or public traffic | 0 |
| External independent review executed | 0 |
| Stage 14 authorization | 0 |

The quality and grounding values above are **bounded simulation metrics produced by the preparation harness**. They are not estimates of live-user quality, not a statistical claim about human-level performance, and not evidence that the system is production-ready.

## Control and negative-control evidence

All 25 gates passed. The unit and integration gates verified the Stage 12 offline entry boundary, consent and cohort enrollment, excluded-domain denial, kill-switch and rollback blocking, bounded sampling, reviewer minimization and access auditing, incident completeness, blinded-rubric preparation, grounding and citation review contracts, expert closure requirements, privacy and secret-exposure controls, advisory-only action boundaries, red-team closure and retest, independent-assessment reproducibility, Stage 12 pause behavior, and support-path completeness.

The operations gates verified that cohort and traffic caps were not exceeded, pause and resume required explicit governed control, and only reviewed incidents could produce regression candidates. The negative controls denied non-consented and withdrawn users, rejected raw secrets and PII from the review ledger, rejected incomplete severity-one incidents, blocked unresolved critical red-team findings, and required expert adjudication for high-impact safety review. These are implementation and contract results; they are not proof that future live operation would be safe under all conditions.

## Decision and boundaries

Stage 13 preparation **passes its offline readiness objective**. The repository now contains executable contracts and evidence for a controlled-canary governance boundary, human-review workflow, red-team closure workflow, incident ledger, independent-review schema, and explicit stop conditions. This result does not authorize live canary traffic.

The following boundaries remain mandatory:

1. `live_canary_started=false` and no live cohort, public canary, or external deployment may be started under this decision.
2. `external_review_executed=false`; the preparation package does not substitute for independent external security, privacy, data-governance, or model-risk review.
3. `stage14_allowed=false`; Stage 14 production-scale continuous governance must not begin without a separate explicit user approval after review of this Stage 13 result.
4. No production, universal-safety, human-level, or general-intelligence claim is authorized by these tests.
5. No unreviewed incident or raw pilot feedback may write to training data, checkpoints, or promotion state.
6. Any future pilot must preserve consent, withdrawal, task/domain restrictions, reviewer privacy, expert escalation, kill-switch, rollback, incident ownership, red-team closure, and independent review as hard controls.

## Evidence index

- `config.json` — Stage 13 preparation configuration and explicit authorization boundaries.
- `stage13_gates.csv` — 25 preparation, operations, and negative-control gate outcomes.
- `stage13_metrics.csv` — bounded simulation, review, cohort, and control metrics.
- `stage13_cohort_manifest.json` — simulated cohort contract and cap record.
- `stage13_review_ledger.csv` — minimized review records and reviewer-access evidence.
- `stage13_independent_review.json` — independent-assessment schema and reproducibility contract.
- `stage13_red_team.csv` — red-team finding closure and retest records.
- `stage13_incidents.csv` — incident ownership, containment, and decision-state records.
- `stage13_summary.txt` — preparation result and explicit limitations.
- `normal_ctest.txt` — final 28-target normal CTest suite.
- `sanitizer_ctest.txt` — final 28-target ASan/UBSan CTest suite.
- `review_protocol.md` — human oversight, consent, incident, red-team, and independent-review protocol.
- `research_sources.md` — governance sources used to design the protocol; not independent assessment evidence.
- `manifest.sha256` — integrity manifest for the complete preparation package.

## References

[1]: https://www.nist.gov/itl/ai-risk-management "NIST AI Risk Management Framework"
[2]: https://doi.org/10.6028/NIST.AI.600-1 "NIST Generative AI Profile"
[3]: https://genai.owasp.org/llmrisk/llm01-prompt-injection/ "OWASP LLM01:2025 Prompt Injection"
