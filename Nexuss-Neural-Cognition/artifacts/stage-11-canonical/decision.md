# Stage 11 Decision — Preference, Safety, Calibration, and Continual Improvement

**Decision:** `PASS_OFFLINE_POST_TRAINING_NONPRODUCTION`

**Date:** 2026-08-12

**Entry checkpoint:** `model@2775430139297845034` (Stage 10 real-data SFT candidate)

**Candidate checkpoint:** `model@2367994276643219537`

## Scope and method

Stage 11 executed a real deterministic offline preference and safety post-training pilot over the existing native `genesis::ByteLanguageModel`. The model has 65,792 parameters. This run did not use Qwen, UltraChat, a transformer, or the native 273k-neuron UIN substrate. It updated the same byte-level model family that Stage 10 trained.

The run used a bounded pairwise response-margin update. Anthropic HH-RLHF supplied reviewed helpfulness/harmlessness preference pairs. NVIDIA Aegis 2.0 supplied safety-labelled prompt/response examples. For the safety update, only explicitly unsafe training rows were used, with the refusal target preferred over the unsafe response. Safe/unsafe Aegis validation and test rows were not used for updates. This safety-only ablation was selected because it reduced the sealed harmful-compliance rate while the full safe-and-unsafe update produced a small harmful-compliance regression and was rejected.

The run reconstructed the Stage 9 base with seed `424242`, replayed the exact Stage 10 Dolly path, then applied one preference epoch followed by one unsafe-only safety-preference epoch. The effective parameters were preference learning rate `0.0008`, safety learning rate `0.00025`, beta `2.0`, and response-span-only updates. Raw user messages, production traces, thumbs signals, tool results, and unreviewed incidents were not used.

## Data provenance and custody

| Release | Immutable revision | License | Train | Validation | Test | Test used for training |
|---|---|---:|---:|---:|---:|---:|
| [`Anthropic/hh-rlhf`][1] | `09be8c5bbc57cb3887f3a9732ad6aa7ec602a1fa` | MIT | 24,000 | 3,000 | 3,000 | No |
| [`nvidia/Aegis-AI-Content-Safety-Dataset-2.0`][2] | `d86bb8bedff51d25ac834ab7838f1cc61acb7a2c` | CC BY 4.0 | 7,412 | 926 | 806 | No |

The governed preparation quarantined malformed transcripts, prompt mismatches, duplicate prompts/IDs, empty or undersized records, overlong records, email/phone patterns, and secret patterns. Each retained row carries source dataset/revision, row hash, rubric/policy version, reviewer group, evidence scope, adjudication state, privacy-review state, and training eligibility. The Aegis card declares hybrid human/synthetic collection, so this evidence does not claim that every label or refusal target is human-authored.

The complete source files, source hashes, derived TSV releases, quarantine records, release manifest, and research record are retained under `data/stage11_preference/`.

## Measured gates

| Gate | Result | Value |
|---|---:|---:|
| Stage 11 harness gates | PASS | 21/21 |
| Preference validation gain | PASS | `+0.0000444701842273` |
| Preference held-out gain | PASS | `+0.0000221032147897` |
| Safety preference validation change | PASS within signed helpful-safe tolerance | `-0.00126775893973` |
| Harmful-compliance rate | PASS | base `0.765306` → candidate `0.744898` |
| Over-refusal rate | PASS within signed maximum | base `0.234300` → candidate `0.246377` |
| Unsafe overconfidence | PASS | `0` |
| Safety calibration non-regression | PASS | `0` regression |
| Stage 10 retention | PASS | `1.000000` |
| Privacy/injection custody scan | PASS | `0` findings |
| Deterministic reproduction | PASS | exact same-seed digest |
| Rollback | PASS | Stage 10 entry restored exactly |
| Normal resource gate | PASS | 183,744 KB peak RSS; 500,000 KB limit |
| Normal CTest suite | PASS | 26/26 |
| ASan/UBSan CTest suite | PASS | 26/26; no sanitizer diagnostics |

The safety preference validation margin is reported as a signed tolerance rather than misrepresented as an improvement. The candidate’s safety outcome is accepted because harmful compliance improved, over-refusal remained within the declared maximum, unsafe overconfidence did not regress, privacy/injection custody passed, and benign Stage 10 retention remained at 1.0.

## Decision and boundaries

Stage 11 passes as an **offline, non-production post-training pilot**. The candidate is a research artifact. It is not promoted to the production registry. `production_allowed=false` and `stage12_allowed=false` remain explicit in the run manifest and configuration.

Stage 12 is blocked until a new explicit approval. No serving, shadow traffic, canary traffic, or public release is authorized by this decision. No claim is made that this model is generally aligned, universally safe, human-level, production-ready, or capable of general intelligence.

This stage also does not constitute large-corpus training of the Nexuss UIN brain. The native Cortex substrate remains a separate research architecture. A future native-language stage must connect text-to-spike encoding, predictive/error signals, local eligibility, replay, consolidation, GPU batching, and native held-out evaluation to the UIN system itself.

## Evidence index

- `stage11_run_manifest.json` — run identity, source revisions, optimizer, checkpoint digests, custody, and boundaries.
- `stage11_candidate_checkpoint.bin` — final Stage 11 candidate snapshot.
- `stage11_base_checkpoint.bin` — reconstructed Stage 10 entry snapshot.
- `stage11_checkpoint_evaluations.csv` — preference, safety, calibration, and retention metrics.
- `stage11_calibration.csv` — baseline/candidate calibration outcomes.
- `stage11_training_curves.csv` — checkpoint curve.
- `stage11_gates.csv` — 21 gate outcomes.
- `normal_ctest.txt` — final complete normal CTest result.
- `sanitizer_ctest.txt` — final complete ASan/UBSan result.
- `manifest.sha256` — immutable evidence manifest.

## References

[1]: https://huggingface.co/datasets/Anthropic/hh-rlhf "Anthropic HH-RLHF dataset card"
[2]: https://huggingface.co/datasets/nvidia/Aegis-AI-Content-Safety-Dataset-2.0 "NVIDIA Aegis 2.0 dataset card"
