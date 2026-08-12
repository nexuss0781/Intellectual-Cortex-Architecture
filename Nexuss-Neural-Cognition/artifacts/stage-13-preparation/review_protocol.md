# Stage 13 Preparation Review Protocol

## Purpose and boundary

This protocol defines the controls required before a narrow, consented canary of the Stage 12 system. It is a **preparation artifact only**. No live user traffic, public service, external review, or Stage 14 operation is authorized by this document.

The pilot must remain advisory. Consequential actions are disabled or require explicit human approval. A cohort may be paused or rolled back immediately. No raw user feedback, incident, reviewer note, or tool result may enter Stage 11 training without separate provenance, privacy review, rights review, adjudication, and release approval.

## Cohort and consent

Every participant must receive a versioned consent receipt and privacy notice identifying the narrow task scope, data collection, retention, reviewer access, support path, withdrawal process, and the possibility of human review. Withdrawal takes effect before further routing and logging under the pilot scope. The cohort manifest must contain a maximum-user cap, traffic fraction, permitted tasks, excluded domains, consent version, and explicit no-consequential-action field.

The initial preparation contract uses at most ten users and one percent traffic for an advisory document-grounding scope. Medical, legal, financial, external-action, account-takeover, and other high-impact domains are excluded until separately reviewed. No user is enrolled without explicit consent and a valid privacy-notice digest.

## Human review

Reviews use a versioned rubric and blinded comparison where feasible. Review strata must cover task family, language, domain, severity, uncertainty, risk-triggered samples, and random samples. High-impact, unsafe, privacy, and severity-three-or-higher outcomes require an expert reviewer and adjudication.

Review records contain a request hash and redacted/minimized input, never raw secrets, raw personal identifiers, or unrestricted conversation text. Every reviewer access is audited. Outcomes are limited to `acceptable`, `incorrect`, `unsafe`, `privacy`, `policy`, and `escalation`. Agreement, disagreement, adjudication, quality, grounding, citation correctness, usefulness, appropriate abstention, and overreliance signals must be reported before any expansion decision.

## Safety and red-team

The adversarial campaign must include direct and indirect prompt injection, jailbreaks, retrieved-document injection, sensitive-data exfiltration, unauthorized tool authority, hallucinated citations, conflicting sources, malicious uploads, request floods, account takeover simulation, registry rollback, and misinformation. Findings require category, severity, attack digest, mitigation, retest result, closure state, and owner.

Critical findings block continuation or expansion until fixed, mitigated, independently assessed, and retested. A critical unresolved finding count of zero is mandatory. Safety, privacy, agency, and over-refusal results must be reported separately; a low unsafe rate cannot be presented as a universal safety guarantee.

## Incident command

A severity-one incident record requires an incident identifier, request hash, owner, timeline, containment action, decision status, regression candidate, and disclosure decision. The kill switch must pause routing within the signed response time, and rollback must restore the prior bundle while preserving audit continuity. Confirmed incidents can enter the Stage 11 regression pipeline only after review approval; automatic incident-to-training promotion is prohibited.

## Independent review

Before a live canary can pass Stage 13, an independent reviewer or review group must receive a defined scope and claims boundary, reproduce the declared core metrics, and assess security, model behavior, data governance, and limitations. “Independent review” cannot be satisfied by the same person who implemented the candidate. Preparation records are contract fixtures only and do not constitute an external assessment.

## Decision authority

Every resume, traffic expansion, pause, rollback, incident closure, and release decision requires an auditable approver and evidence digest. The decisions are `continue`, `pause`, `rollback`, `expand`, or `terminate`. Stage 14 remains blocked until the pilot has no unresolved severity-one issues, independent reviewers assess the claims, all release owners approve, and the user explicitly authorizes scaled production governance.

## References

[1]: https://www.nist.gov/itl/ai-risk-management-framework "NIST AI Risk Management Framework"
[2]: https://doi.org/10.6028/NIST.AI.600-1 "NIST Generative AI Profile"
[3]: https://genai.owasp.org/llmrisk/llm01-prompt-injection/ "OWASP LLM01:2025 Prompt Injection"
