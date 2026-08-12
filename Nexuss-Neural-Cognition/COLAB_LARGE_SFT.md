# Colab Large-Corpus SFT Runbook

This package runs a real GPU supervised fine-tuning experiment over a larger public instruction corpus. The intended user experience is one command after cloning the repository:

```bash
cd Nexuss-Neural-Cognition
bash run.sh
```

The launcher installs the Python dependencies, downloads the selected public dataset from Hugging Face, creates governed train/validation/test JSONL files, trains a QLoRA adapter, evaluates validation and the untouched published test split, generates fixed prompts, verifies artifact hashes, and prints the result locations.

## Selected corpus and model

The default corpus is [`HuggingFaceH4/ultrachat_200k`][1]. The dataset card reports a MIT license and four published splits: `train_sft`, `test_sft`, `train_gen`, and `test_gen`. The default training path uses `train_sft` for training, takes a deterministic 5% hash-sorted tail of `train_sft` as validation, and keeps the published `test_sft` split untouched for final testing. This gives a real training set, a validation set, and a final test set without using test examples for gradient updates.

The default base model is [`Qwen/Qwen2.5-1.5B-Instruct`][2], whose model card declares Apache-2.0 licensing. The default run uses 4-bit NF4 loading with LoRA adapters rather than updating all base weights. This is chosen for ordinary Colab T4-class memory. A larger model can be selected through `MODEL_ID`, but the user must then adjust memory and sequence settings based on the actual GPU.

| Component | Default |
|---|---|
| Dataset | `HuggingFaceH4/ultrachat_200k` |
| Dataset license | MIT, subject to the source card and any upstream content obligations |
| Dataset revision | `8049631c405ae6576f93f445c6b8166f76f5505a` |
| Base model | `Qwen/Qwen2.5-1.5B-Instruct` |
| Model license | Apache-2.0 |
| Training split | `train_sft` after governance filtering and deterministic 95/5 train/validation split |
| Final test split | Published `test_sft`, filtered only by the same deterministic governance checks |
| Sequence length | 2,048 tokens |
| LoRA rank/alpha | 16 / 32 |
| Epochs | 1 |
| Learning rate | `2e-4` |
| Effective batch | 16 examples by default: 2 per device × 8 accumulation steps |
| Seed | `424242` |
| Production release | Disabled |

## Colab execution

Create a Colab notebook with a GPU runtime, clone the selected repository, and run the following cells. The first cell is intentionally explicit about the GPU requirement.

```python
!nvidia-smi
!git clone https://github.com/nexuss0781/Intellectual-Cortex-Architecture.git
%cd Intellectual-Cortex-Architecture/Nexuss-Neural-Cognition
!bash run.sh
```

The run may take a substantial amount of time because it processes the complete governed corpus rather than a smoke-test subset. The script does not silently reduce the corpus. It uses the full split unless an explicit `MAX_TRAIN_EXAMPLES`, `MAX_VALIDATION_EXAMPLES`, or `MAX_TEST_EXAMPLES` override is supplied.

If a Colab session disconnects after checkpoints have been written, reconnect to the same runtime storage and run:

```bash
RESUME=1 bash run.sh
```

The trainer searches `artifacts/colab-large-sft/run/checkpoints/` for the highest numbered checkpoint and resumes from it. To force a new governed dataset release, use `FORCE_DATA=1 bash run.sh`. This is normally unnecessary because the prepared release is hash-pinned by its manifest.

## Optional resource controls

The defaults target a T4-class GPU. These overrides are supported without editing source code:

```bash
# Faster bounded pilot before the full run; this is not the final large-corpus result.
MAX_TRAIN_EXAMPLES=20000 MAX_VALIDATION_EXAMPLES=2000 MAX_TEST_EXAMPLES=2000 bash run.sh

# Lower memory use on a constrained GPU.
MAX_SEQ_LENGTH=1024 bash run.sh

# Use a larger model only when the selected GPU can hold it.
MODEL_ID=Qwen/Qwen2.5-3B-Instruct MAX_SEQ_LENGTH=2048 bash run.sh
```

A bounded pilot must be labeled as a pilot in any report. The final large-corpus result should be run with the default zero-valued maximum-example controls.

## Governance and evaluation contract

The preparation script normalizes only `system`, `user`, and `assistant` messages, requires at least one user message and an assistant final message, deduplicates by `prompt_id`, retains source row hashes, and quarantines detected email, phone, secret, malformed, over-short, over-long, and duplicate records. It writes `release_manifest.json` and `quarantine.jsonl` in the data artifact directory.

The test split is loaded only after training data preparation and is never passed to the training dataset. The verifier checks train/validation/test prompt-ID disjointness and records `heldout_used_for_training=false`. It also verifies that the run has finite validation and test losses, a LoRA adapter directory, generation probes, and self-consistent artifact SHA-256 hashes.

The evaluation package reports validation loss/perplexity and test loss/perplexity. It also emits deterministic greedy-generation probes for knowledge, arithmetic reasoning, writing, uncertainty handling, code, prompt-boundary, and tool-boundary behavior. These probes are diagnostic controls, not a safety certification or human-quality evaluation.

## Output artifacts

After a successful run, inspect:

| Path | Meaning |
|---|---|
| `artifacts/colab-large-sft/data/release_manifest.json` | Dataset identity, source split counts, derived split counts, quarantine counts, and split hashes |
| `artifacts/colab-large-sft/data/train.jsonl` | Governed training release |
| `artifacts/colab-large-sft/data/validation.jsonl` | Deterministic validation release derived from `train_sft` |
| `artifacts/colab-large-sft/data/test.jsonl` | Governed copy of the published `test_sft` evaluation release |
| `artifacts/colab-large-sft/data/quarantine.jsonl` | Excluded-row reasons and source identifiers |
| `artifacts/colab-large-sft/run/adapter/` | LoRA adapter and tokenizer files |
| `artifacts/colab-large-sft/run/checkpoints/` | Resumable trainer checkpoints |
| `artifacts/colab-large-sft/run/run_summary.json` | Resolved device, training metrics, validation/test metrics, probes, and limitations |
| `artifacts/colab-large-sft/run/generation_probes.jsonl` | Fixed greedy generations and simple safety-pattern scan |
| `artifacts/colab-large-sft/run/verification.json` | Final pass/fail contract checks |
| `artifacts/colab-large-sft/run/artifact_manifest.json` | SHA-256 hashes for run artifacts |

To preserve results beyond the Colab runtime, copy the complete `artifacts/colab-large-sft/` directory to Google Drive or download it as an archive after the final verification passes.

## Interpretation boundary

A lower test perplexity or a positive generation-probe outcome is evidence only for this model, dataset, prompt format, and run configuration. It is not evidence of general intelligence, broad reasoning, factual reliability, harmlessness, or production readiness. Before any deployment claim, the next work must include human evaluation, contamination review, broader independent benchmarks, adversarial testing, privacy review, license review, model-card documentation, and an explicit release gate.

This package does **not** start Stage 11 and does not change the Stage 10 production boundary. The resulting adapter is a research artifact until the measured run is reviewed and approved.

## References

[1]: https://huggingface.co/datasets/HuggingFaceH4/ultrachat_200k "UltraChat 200k dataset card"
[2]: https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct "Qwen2.5-1.5B-Instruct model card"
