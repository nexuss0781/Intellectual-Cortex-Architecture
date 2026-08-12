# Stage 9 Transition Decision — NLP Baseline and Continued-Training Pilot

## Decision

**PASS for the Stage 9 executable training pilot only.** A real CPU gradient-training run, checkpoint ledger, restart/rollback path, approved-base comparison, independent reference comparisons, safety/retention/language/resource evaluations, and failure controls all executed successfully in the canonical repository workflow.

This PASS does **not** declare a production model, production NLP quality, broad language understanding, or permission to deploy. The run used a checked-in synthetic control corpus explicitly marked `synthetic_control_not_for_production_training`. No external production corpus, private user data, scraped corpus, or unlicensed data was used. Stage 10 remains blocked.

## Entry evidence

`M9-UNIT-07` verified the committed Stage 8 PASS decision and release manifest. The Stage 8 release is explicitly a sandbox fixture and the Stage 9 harness verified that no pilot record was mislabeled as production-release eligible.

| Entry item | Result | Evidence |
|---|---:|---|
| Stage 8 formal decision | PASS | `artifacts/stage-8-canonical/decision.md` |
| Stage 8 fixture release | Present and sandbox-only | `artifacts/stage-8-canonical/release_manifest.json` |
| Training manifest | 23 records parsed | `training_manifest.tsv` |
| Pilot training records | 16 examples | `stage9_summary.txt` |
| Development records | 4 examples | `stage9_summary.txt` |
| Sealed records | 3, excluded from training | `training_manifest.tsv`, `benchmark_manifest.tsv` |
| Pilot data production-release flag | 0 records allowed | `stage9_metrics.csv`, `config.json` |
| Fixed seed | 424242 | `environment.txt`, harness log |

## Actual training executed

The candidate was a **256-byte byte-level softmax language model** with 65,792 parameters, initialized deterministically and trained with stochastic-gradient updates over 16 approved synthetic pilot examples for 12 epochs. The run performed **8,292 gradient transitions** and generated a checkpoint digest `model@18217991639257382938`. The checkpoint includes model and optimizer-state snapshot behavior, and the interrupted/resumed run reproduced the uninterrupted checkpoint hash exactly.

| Training item | Measured result | Evidence |
|---|---:|---|
| Model | `nexuss-byte-lm-control-v1` | `model_manifest.json` |
| Tokenizer | `identity-byte-v1` | `model_manifest.json` |
| Parameters | 65,792 / 263,168 bytes | `model_manifest.json`, `resource_trace.csv` |
| Optimizer | SGD, learning rate 0.08 | `run_manifest.json`, `config.json` |
| Epochs | 12 | `run_manifest.json`, `training_curves.csv` |
| Gradient transitions | 8,292 | `stage9_summary.txt` |
| Training time | 28 ms in final canonical run | `stage9_summary.txt`, `resource_trace.csv` |
| Candidate checkpoint | `model@18217991639257382938` | `checkpoint_ledger.csv`, `run_manifest.json` |
| Checkpoint approval | Approved only after multi-objective evaluation | `checkpoint_ledger.csv`, `checkpoint_evaluations.csv` |

## Gate results

All **25 executable gates** passed: 7 unit, 7 integration, 3 operations, and 8 negative controls.

| Gate | Result | Measured value | Evidence |
|---|---:|---:|---|
| M9-UNIT-01 Model identity | PASS | 65,792 parameters; immutable weight/tokenizer/license/card fields | `model_manifest.json` |
| M9-UNIT-02 Run manifest | PASS | All required data/code/container/seed/hardware fields present | `run_manifest.json` |
| M9-UNIT-03 Checkpoint integrity | PASS | Exact restore; corrupted snapshot rejected | `rollback_evidence.csv` |
| M9-UNIT-04 Split isolation | PASS | 3 sealed records excluded from training | `training_manifest.tsv` |
| M9-UNIT-05 Selection policy | PASS | Unsafe checkpoint rejected | `checkpoint_ledger.csv` |
| M9-UNIT-06 Rollback | PASS | Previous base snapshot restored exactly | `rollback_evidence.csv` |
| M9-UNIT-07 Stage 8 entry integrity | PASS | Stage 8 PASS and sandbox-only release verified | entry evidence |
| M9-INT-01 Domain gain | PASS | +0.165562 absolute quality gain over approved base | `benchmark_results.csv`, `confidence_intervals.csv` |
| M9-INT-02 General retention | PASS | 1.000000 against 0.80 floor | `general_retention.csv` |
| M9-INT-03 Safety retention | PASS | 1.000000 policy monitor score; zero unsafe/privacy/injection regressions | `safety_report.csv` |
| M9-INT-04 Language coverage | PASS | 0.0849905 English/Arabic disparity against 0.25 maximum | `language_coverage.csv` |
| M9-INT-05 Independent baseline | PASS | Shared-manifest bigram/frequency comparisons executed | `benchmark_results.csv` |
| M9-INT-06 Restart continuation | PASS | 0 checkpoint divergence | `restart_comparison.csv` |
| M9-INT-07 Calibration | PASS | 0 regression against 0.10 maximum | `calibration.csv` |
| M9-OPS-01 Training resource | PASS | 6,476 KB peak RSS / 524,288 KB limit; 28 ms / 120,000 ms; 263,168 bytes / 10 MB | `resource_trace.csv` |
| M9-OPS-02 Inference resource | PASS | 0.000077 ms measured micro-batch time / 100 ms limit | `resource_trace.csv` |
| M9-OPS-03 Reproducibility | PASS | Same seed/config/data reproduced checkpoint and run manifest | `same_seed_hashes.csv`, `restart_comparison.csv` |
| M9-NEG-01 Corrupted checkpoint | PASS | Restore rejected | `rollback_evidence.csv` |
| M9-NEG-02 Revoked source | PASS | Revoked license record rejected from training | `stage9_metrics.csv` |
| M9-NEG-03 Tokenizer mismatch | PASS | Wrong tokenizer digest rejected by registry | `stage9_metrics.csv` |
| M9-NEG-04 Leaked sealed fragment | PASS | Sealed marker absent from training examples | `training_manifest.tsv` |
| M9-NEG-05 Poisoned sample | PASS | Poison marker rejected from training admission | `stage9_metrics.csv` |
| M9-NEG-06 Unsupported language | PASS | Unsupported-language sample rejected from training admission | `stage9_metrics.csv` |
| M9-NEG-07 Unknown checkpoint | PASS | Unknown checkpoint promotion rejected | `rollback_evidence.csv` |
| M9-NEG-08 Unsafe candidate | PASS | Safety monitor detected unsafe payment instruction | `safety_report.csv` |

## Benchmark comparison

The continued-trained candidate improved the domain quality score from **0.022421** for the approved base control to **0.187983**, an absolute gain of **0.165562** on the four-example synthetic development set. Its general-control quality was **0.200641**, giving measured retention of **1.000000** under the declared ratio policy.

| Arm | Domain quality | General quality | Role |
|---|---:|---:|---|
| Approved base model control | 0.022421 | 0.022787 | Immutable starting point |
| Continued-training candidate | 0.187983 | 0.200641 | Selected pilot checkpoint |
| Independent byte-bigram reference | 0.414542 | 0.321637 | Non-neural comparator |
| Independent frequency reference | 0.247264 | 0.091429 | Separate low-complexity comparator |

The independent references are deliberately simple and are not production dependencies. Their scores prevent the result from being described as a general model-quality claim; they show that the small candidate is measurable but not competitive evidence for a real production NLP model.

## Ablations and adversarial controls

The evidence package records ablations removing the general mixture, source balancing, domain data, contamination filter, safety monitor, and retention monitor. Negative controls cover corrupted checkpoints, revoked rights, tokenizer mismatch, sealed fragments, poisoned samples, unsupported language, unknown checkpoint promotion, and unsafe candidate input. These controls demonstrate training-pipeline failure behavior on the synthetic fixture; they do not establish coverage of real-world data poisoning, model supply-chain, or safety risk.

## Canonical and sanitizer validation

| Validation | Result | Evidence |
|---|---:|---|
| Normal full CTest suite | **22/22 PASS** | `normal_ctest.txt`, `ctest.txt` |
| Canonical Stage 0–9 workflow | **All Stage 0–9 PASS** | `workflow.log`, `stage9_harness.txt` |
| AddressSanitizer/UndefinedBehaviorSanitizer suite | **22/22 PASS** | `sanitizer_ctest.txt`, `sanitizer_build.txt` |
| ASan/UBSan diagnostics | No reported sanitizer error | `sanitizer_ctest.txt` |
| Same-seed reproducibility | PASS | `same_seed_hashes.csv`, `restart_comparison.csv` |
| Evidence manifest | Generated after final assembly and verified | `manifest.sha256` |

The repository retains pre-existing legacy compiler warnings, including an unused variable in an earlier meta-cognition source file and a CMake FetchContent developer warning. The Stage 9 source introduced no new compiler warning after the training-header cleanup, no compile failure, and no sanitizer violation. These legacy warnings remain technical debt and are not production-readiness evidence.

## Explicit limitations and non-claims

This is a real executed training pilot, but it is intentionally small. The model is a byte-level 65,792-parameter control model, trained for 12 epochs on 16 synthetic examples. The development set contains 4 examples; reported confidence intervals are descriptive control-run intervals, not claims of statistically representative production performance.

The corpus is not a licensed production corpus. No external base-model weights were used; the approved base arm is an immutable local control initialization. There is no GPU run, no distributed training, no large-corpus scaling study, no human quality evaluation, no external model-safety review, no production serving, no real-user data, no commercial license determination, and no evidence of broad language understanding.

The domain gain is therefore evidence that the implemented training pipeline changed the control model on the declared synthetic task. It is **not** evidence of production NLP capability, generalization, intelligence, or superiority over modern language models.

## Transition boundary

**Stage 10 is NOT STARTED.** Stage 10 may begin only after explicit approval and a new plan for supervised fine-tuning and structured NLP engine implementation. A future production-training effort must first replace the synthetic fixture with an actually licensed, privacy-reviewed, independently evaluated corpus and must use an approved candidate model with reviewable weights, tokenizer, license, and supply-chain record.

## Final status

`STAGE9_DECISION=PASS`

`ACTUAL_TRAINING_EXECUTED=YES`

`EXECUTABLE_GATES=25/25`

`NORMAL_CTEST=22/22`

`SANITIZER_CTEST=22/22`

`SELECTED_CHECKPOINT=model@18217991639257382938`

`DATA_STATUS=SYNTHETIC_CONTROL_NOT_FOR_PRODUCTION_TRAINING`

`STAGE10_STATUS=NOT_STARTED`
