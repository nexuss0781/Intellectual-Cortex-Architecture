# Stage 11 Reviewed Preference and Safety Data Protocol

**Status:** Preparation-only control document. This protocol authorizes neither post-training nor model promotion.

## Purpose

Stage 11 may improve a Stage 10 candidate only through an approved offline data release. A raw user message, reaction signal, incident report, tool result, support ticket, or production trace is **not** training data. Such material may enter a review queue only under the governing consent, privacy, retention, and access policies. It becomes eligible for a training release only after the steps below are completed.

```text
candidate source
→ consent and privacy screening
→ de-identification/minimization review
→ annotation using versioned rubric
→ multi-reviewer assessment
→ disagreement record and adjudication
→ provenance and rights verification
→ approved immutable dataset release
→ sealed offline evaluation
→ explicit user approval for post-training
```

## Annotation objects

| Object | Required fields | Admission requirement |
|---|---|---|
| Preference pair | Prompt hash; chosen/rejected hashes; task; rubric version; evidence scope; reviewer group; disagreement; policy; source release; limitations | Reviewed, adjudicated, privacy-reviewed, and marked eligible by the approved release owner |
| Safety case | Risk category; expected decision; severity; policy; evidence scope; reviewer group; source release | Reviewed, adjudicated, privacy-reviewed, taxonomy-valid, and marked eligible by the approved release owner |
| Regression candidate | Incident ID; severity; owner; root-cause state; test linkage; source release | Reviewed and formally approved before it can become a training or evaluation item |
| Calibration outcome | Sample ID; model confidence; correctness; abstention status; unsafe status | Measured from a sealed, versioned evaluation release only |

The allowed safety taxonomy is: `harmful_compliance`, `safe_transformation`, `correct_refusal`, `clarification`, `escalation`, `over_refusal`, `privacy_risk`, `injection_risk`, `unauthorized_agency`, `misinformation`, and `unsupported_claim`. The allowed action classes are: `answer`, `transform`, `ask`, `abstain`, `refuse`, and `escalate`.

## Reviewer calibration and adjudication

Before reviewing a release, each reviewer group must complete the current calibration set under the current rubric. The release record must include the reviewer-calibration digest, reviewer group, known disagreements, and adjudication outcome. A single preference label without a rubric, reviewer group, and adjudication state cannot enter an approved training release.

Adjudication must explain why the selected response is preferred or why the expected safety action applies. Style-only preference, unsupported factuality judgments, and unstated policy interpretation are insufficient. High-severity safety cases require the designated safety reviewer or escalation owner.

## Privacy, rights, and provenance controls

The release must preserve source-release identity, rights basis, policy version, evidence scope, privacy-review digest, dataset-card digest, annotation-rubric version, sealed-evaluation digest, and approver identity. The training path accepts only a release whose status is `APPROVED_FOR_TRAINING` and whose provenance is complete. Material carrying personal data, secrets, unresolved rights restrictions, or unapproved sensitive content must remain quarantined.

## Feedback and promotion boundary

No raw feedback source can directly update model weights, adapters, safety classifiers, confidence thresholds, prompts, policies, or retrieval configuration. No candidate is promoted automatically. A later offline post-training run requires a reviewed preference release, a reviewed safety release, a sealed evaluation release, a rollback bundle, a signed resource budget, and explicit user approval.

## Required future evidence

A completed Stage 11 release must include the rubric, calibration report, reviewer agreement/adjudication report, approved dataset card, sealed evaluation manifest, post-training manifest, candidate/baseline comparison, calibration data, safety and over-refusal analysis, privacy/injection/agency results, regression ledger, retention/replay analysis, rollback evidence, and immutable artifact hashes.

## Explicit non-claims

This protocol does not establish that Nexuss is aligned, universally safe, authorized for autonomous action, or approved for production operation. It does not change the Stage 10 non-production boundary, start Stage 12, or train the native Nexuss Cortex substrate.
