# Stage 11 preference-data research findings

## Anthropic HH-RLHF

Source: https://huggingface.co/datasets/Anthropic/hh-rlhf

Observed dataset-card metadata on 2026-08-12: MIT license; human-feedback tag; two splits (`train`, approximately 161k rows; `test`, approximately 8.55k rows); columns `chosen` and `rejected`; dataset summary identifies human preference data about helpfulness and harmlessness from Anthropic's helpful-and-harmless assistant work. The card visibly contains harmful, privacy-sensitive, discriminatory, and offensive prompts, including unsafe rejected responses, so governance filtering and a separate safety evaluation custody path are mandatory.

Use decision: suitable as a helpfulness/harmlessness preference source, but not acceptable for blind direct training ingestion. The Stage 11 preparation pipeline must normalize the dialogue, scan/quarantine secret/PII patterns, preserve source/release hashes, keep the published test split untouched, record the MIT source license, and make clear that the source's human preference labels are not a universal safety certification.

## PKU-SafeRLHF

Source: https://huggingface.co/datasets/PKU-Alignment/PKU-SafeRLHF

Observed dataset-card metadata on 2026-08-12: CC BY-NC 4.0 license; human-feedback and safety tags; default subset approximately 82.1k rows with train approximately 73.9k and test approximately 8.21k; fields include prompt, two responses, safety booleans, harm-category dictionaries, severity levels, better-response ID, safer-response ID, and response SHA-256 fields. The card warns that data may be offensive or harmful, is intended for research, and expresses views that do not represent the PKU-Alignment team.

Use decision: suitable as a safety-preference evaluation/training source only under a non-commercial research boundary, with harmful-content handling, quarantine/privacy scanning, source-license recording, and untouched test custody. Its paired labels distinguish helpfulness (`better_response_id`) from safety (`safer_response_id`), so both must be retained rather than collapsed into a single preference signal.

## NVIDIA Aegis-AI-Content-Safety-Dataset-2.0

Source: https://huggingface.co/datasets/nvidia/Aegis-AI-Content-Safety-Dataset-2.0

Observed dataset-card metadata on 2026-08-12: CC BY 4.0 license; English; approximately 30k train, 1.45k validation, and 1.96k test rows; text-classification/content-safety corpus. The card describes hybrid collection: human overall content-safety annotations with synthetic LLM augmentations where needed; it reports 25,007 user prompts/interactions plus refusal data, and states that the held-out test set is not augmented with synthetic refusal data. The card presents it as an LLM content-safety moderation resource rather than a general helpfulness preference set.

Use decision: selected as the Stage 11 safety classifier/evaluation release because it has a permissive CC BY 4.0 declaration, explicit train/validation/test structure, human overall safety labels, a declared safety purpose, and a test set that is not augmented with synthetic refusal data. It will not be treated as purely human-authored preference data; the hybrid human/synthetic composition will remain in the manifest and claims boundary.

## Stage 11 source selection

The Stage 11 preparation will use Anthropic HH-RLHF as the helpfulness/harmlessness preference source and NVIDIA Aegis 2.0 as the safety classification/evaluation source. PKU-SafeRLHF remains a research-only optional comparison because its dataset card declares CC BY-NC 4.0; it will not be silently included in a candidate intended for broader deployment. Both selected releases require immutable revision pinning, governance filtering, source hashes, separate validation/test custody, and explicit non-production claims.

## Immutable source revisions

Hugging Face API metadata returned the following source revisions:

| Dataset | Revision | Last modified | License |
|---|---|---|---|
| `Anthropic/hh-rlhf` | `09be8c5bbc57cb3887f3a9732ad6aa7ec602a1fa` | 2023-05-26 | MIT |
| `nvidia/Aegis-AI-Content-Safety-Dataset-2.0` | `d86bb8bedff51d25ac834ab7838f1cc61acb7a2c` | 2025-06-09 | CC BY 4.0 |
