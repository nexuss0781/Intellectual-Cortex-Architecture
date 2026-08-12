# Stage 9 Design — Executable NLP Baseline and Continued-Training Pilot

## Honest scope

The sandbox has six CPU threads, approximately 3.8 GiB RAM, 32 GiB available storage, and no NVIDIA accelerator. No licensed production corpus or external base-model weights have been supplied. Stage 9 therefore executes a real **CPU training pilot** over a checked-in synthetic control corpus derived from the Stage 8 sandbox policy domain. The corpus is explicitly `not_for_production_training`.

This is still actual training: a trainable byte-level language model performs gradient updates over the control corpus, saves and restores optimizer/model state, evaluates held-out data, and is compared with independent non-neural reference implementations. The result validates the training pipeline and resource accounting. It does not claim production model quality.

## Actual training path

```text
Stage 8 PASS entry
  → model/data/license due-diligence manifests
  → signed train/development/sealed split check
  → immutable base-model initialization
  → byte-level tokenizer/vocabulary manifest
  → CPU gradient training pilot
  → checkpoint + optimizer-state digest
  → held-out domain/general/safety/language evaluation
  → independent reference comparison
  → restart and rollback comparison
  → multi-objective checkpoint selection
```

The primary candidate is a compact byte-level softmax language model with a 256×256 trainable transition matrix and bias vector. It is intentionally small enough to train on CPU and large enough to execute real cross-entropy gradient updates. The independent arms are a count-based byte-bigram model and a frequency/lookup reference with separate implementations. All arms use the same task manifest and do not read sealed evaluation data.

## Stable interfaces

The implementation adds `src/production/stage9_training.h` with:

- `ModelIdentity`, `TrainingRunManifest`, `CheckpointEvaluation`, `TrainingExample`, and `TrainingRegistry`.
- `ByteLanguageModel` with deterministic initialization, SGD training, evaluation, snapshot/restore, tokenizer digest, checkpoint digest, and memory accounting.
- `IndependentBigramReference` and `IndependentFrequencyReference` with separate scoring paths.
- `Stage9Evaluator` for domain score, general retention, safety-policy retention, language coverage, calibration/abstention, latency, and compute traces.

## Data boundary

`configs/stage9_training_control.tsv` is an approved synthetic control fixture with explicit source, license, intended-use, and production-status fields. The harness fails if a record lacks the required rights fields, if any record is marked sealed, or if the production flag is misrepresented. The two Stage 8 sealed evaluation records remain out of training and are only represented by custody checks.

## Training protocol

The pilot uses a fixed seed, fixed 256-byte vocabulary, SGD, learning rate, fixed sequence budget, 12 epochs, checkpoint every 4 epochs, and a deterministic training order. The training mixture contains domain examples plus a small general-control mixture; ablations remove one component at a time. The model is selected by domain gain subject to general-retention, safety, language, calibration, latency, and memory constraints. Lowest training loss alone cannot promote a checkpoint.

The restart test trains an uninterrupted reference and compares it with a run interrupted after six epochs, restored from a checkpoint including optimizer state, and continued for six epochs. The accepted divergence is zero under the deterministic CPU policy. A corrupted checkpoint, tokenizer mismatch, sealed-read attempt, contamination marker, and unsafe candidate are all fail-closed controls.

## Evidence contract

The harness writes:

```text
model_due_diligence.md
base_model_card.md
model_manifest.json
training_data_manifest.tsv
run_manifest.json
training_curves.csv
checkpoint_ledger.csv
checkpoint_evaluations.csv
benchmark_manifest.tsv
benchmark_results.csv
confidence_intervals.csv
general_retention.csv
safety_report.csv
language_coverage.csv
calibration.csv
resource_trace.csv
restart_comparison.csv
rollback_evidence.csv
ablations.csv
supply_chain_report.csv
stage9_metrics.csv
```

The canonical workflow additionally writes configuration, normal/sanitizer builds and tests, environment metadata, formal decision, and SHA-256 manifest.

## Non-claims and transition

A Stage 9 PASS means the actual training pilot, checkpoint protocol, comparison arms, and safety/resource controls executed as declared. It does not mean the model is production-ready, broadly capable, or trained on real licensed data. Stage 10 remains blocked until the user explicitly approves supervised fine-tuning and structured NLP-engine implementation.
