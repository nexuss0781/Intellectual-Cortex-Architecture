# Stage 8 Transition Decision — Data Factory and Evaluation Integrity

## Decision

**PASS for Stage 8 only.** The governed data-factory and evaluation-integrity control plane passed its complete executable gate set from the final canonical workflow. This decision authorizes no model pretraining, fine-tuning, production data ingestion, user-facing deployment, sealed-set tuning, or Stage 9 implementation.

The PASS is limited to a **checked-in sandbox fixture** explicitly marked `sandbox_fixture_only_not_for_production_training`. No external production corpus, private user data, scraped corpus, or unlicensed data was ingested. The result proves that the repository can enforce the declared data rights, privacy, safety, split, contamination, sealed-custody, deletion, annotation, restart, and audit contracts when records are presented to it.

## Entry evidence

`D8-UNIT-08` verified the committed Stage 7 entry evidence in `artifacts/stage-7-canonical/decision.md` and `artifacts/stage-7-canonical/manifest.sha256`. The entry decision contains `STAGE7_DECISION=PASS`, and the entry manifest contains the expected Stage 7 metric reference. The Stage 8 source and record manifests were loaded from the checked-in configuration paths and processed by the executable pipeline.

| Entry item | Result | Evidence |
|---|---:|---|
| Stage 7 formal decision | PASS | `artifacts/stage-7-canonical/decision.md` |
| Stage 7 evidence manifest | Verified | `artifacts/stage-7-canonical/manifest.sha256` |
| Source manifest | 5 source records parsed | `source_manifest.tsv`, `source_registry.csv` |
| Record manifest | 18 records parsed | `record_manifest.tsv`, `raw_input_manifest.csv` |
| Fixture policy | Sandbox-only, not production training | `config.json`, `release_manifest.json` |
| Fixed seed | 424242 | `environment.txt`, harness log |

## Gate results

All **25 executable gates** passed: 8 unit, 6 integration, 3 operations, and 8 explicit negative-control gates.

| Gate | Result | Measured value | Evidence |
|---|---:|---:|---|
| D8-UNIT-01 Source completeness | PASS | 4 retained items with complete lineage | `processed_items.csv`, `stage8_metrics.csv` |
| D8-UNIT-02 Hash determinism | PASS | Identical release manifest hash on replay | `stage8_metrics.csv`, `restart_comparison.csv` |
| D8-UNIT-03 License and rights gate | PASS | 2 rights-blocked sources controlled | `license_decisions.csv` |
| D8-UNIT-04 Privacy gate | PASS | 1 PII and 1 secret canary controlled | `privacy_report.csv`, `secret_report.csv` |
| D8-UNIT-05 Deletion | PASS | Supported source deletion removed all references | `delete_report.csv` |
| D8-UNIT-06 Schema integrity | PASS | Invalid schema quarantined; valid schemas retained | `annotation_report.csv` |
| D8-UNIT-07 Dataset-card completeness | PASS | 14 required card sections present | `dataset_card.md` |
| D8-UNIT-08 Stage 7 entry integrity | PASS | Stage 7 PASS and manifest verified | entry evidence |
| D8-INT-01 Near-duplicate split isolation | PASS | 0 retained cross-split exact/near duplicates | `dedupe_report.csv`, `split_manifest.csv` |
| D8-INT-02 Hidden-set protection | PASS | Training access to sealed items blocked; evaluator access allowed | `sealed_access_audit.jsonl` |
| D8-INT-03 Contamination scan | PASS | Seeded benchmark fragment excluded from training | `contamination_report.csv` |
| D8-INT-04 Source/time split | PASS | No retained source spans multiple splits | `split_manifest.csv` |
| D8-INT-05 Annotation quality | PASS | 100% agreement among retained calibration examples | `annotation_report.csv` |
| D8-INT-06 Safety quarantine | PASS | Prompt injection and safety canaries routed to quarantine | `safety_quarantine.csv` |
| D8-OPS-01 Throughput | PASS | 1,800 records/second / 100 minimum | `resource_trace.csv` |
| D8-OPS-02 Restart integrity | PASS | 0 manifest/count difference after resume | `restart_comparison.csv` |
| D8-OPS-03 Auditability | PASS | 27 total source, transformation, access, and release audit events | `audit_trace.jsonl` |
| D8-NEG-01 Corrupted source row | PASS | Fail-closed parser rejection | `stage8_metrics.csv` |
| D8-NEG-02 Privacy/secret disabled-control | PASS | Sensitive canaries cannot enter training | `privacy_report.csv`, `secret_report.csv` |
| D8-NEG-03 Sealed benchmark read | PASS | Unauthorized training read blocked | `sealed_access_audit.jsonl` |
| D8-NEG-04 Known contamination | PASS | Benchmark fragment cannot enter training | `contamination_report.csv` |
| D8-NEG-05 Unsupported deletion | PASS | Source without removal support blocked | `delete_report.csv` |
| D8-NEG-06 Corrupted record row | PASS | Fail-closed parser rejection | `stage8_metrics.csv` |
| D8-NEG-07 Near-deduplication ablation | PASS | Removing control exposes seeded near duplicate | `ablations.csv` |
| D8-NEG-08 Independent canary detectors | PASS | PII, secret, and prompt-injection detectors active | `stage8_metrics.csv` |

## Data-factory result

The executable processed **18 records from 5 source records**. It retained **4 records** and quarantined or excluded **14 records**. The retained records have approved source lineage, valid annotation schemas, no PII, no secrets, no benchmark markers, no hidden-set flag, and no unresolved rights issue.

| Control outcome | Count | Decision |
|---|---:|---|
| Retained training/development records | 4 | Eligible for this sandbox fixture release only |
| Quarantined/excluded records | 14 | Not eligible for training release |
| PII canaries | 1 | Redacted/quarantined |
| Secret canaries | 1 | Redacted/quarantined |
| Exact duplicates | 1 | Quarantined |
| Near duplicates | 2 | Quarantined, including cross-split adversarial case |
| Known benchmark fragments | 2 | Training excluded; sealed example retained only in sealed custody |
| Rights/deletion failures | 2 | Quarantined |
| Safety/prompt-injection cases | 2 | Safety-reviewed quarantine |
| Sealed evaluation records | 2 | Unreadable by training; independently readable only by evaluator role |

A signed fixture release manifest was generated with dataset ID `stage8-fixture`, version `1.0.0`, four retained items, and two sealed items. It is not a production dataset release and must not be used as evidence of production training-data adequacy.

## Ablation and adversarial result

All declared controls demonstrated a measurable protective effect. Removing near-deduplication exposed a seeded near duplicate; removing source split management created a source conflict; removing privacy filtering retained two sensitive canaries; removing secret scanning retained the secret canary; removing sealed custody exposed a hidden item; and removing annotation validation retained an invalid schema. These are control-plane ablations over the sandbox fixture, not estimates of risk in a future real corpus.

## Canonical and sanitizer validation

| Validation | Result | Evidence |
|---|---:|---|
| Normal full CTest suite | **21/21 PASS** | `ctest.txt` |
| Canonical Stage 0–8 harness sequence | **All Stage 0–8 PASS** | `workflow.log`, `stage8_harness.txt` |
| AddressSanitizer/UndefinedBehaviorSanitizer CTest suite | **21/21 PASS** | `sanitizer_ctest.txt`, `sanitizer_build.txt` |
| ASan/UBSan diagnostics | No reported sanitizer error | `sanitizer_ctest.txt` |
| Deterministic seed | 424242 | `environment.txt` |
| Stage 8 management-plane RSS | 4,136 KB measured / 65,536 KB limit | `resource_trace.csv` |
| Fixture throughput | 1,800 records/second / 100 minimum | `resource_trace.csv` |
| Evidence manifest | Generated after final assembly and verified | `manifest.sha256` |

The repository still emits pre-existing compiler warnings in legacy files. Stage 8 introduced no compile error or sanitizer violation. These warnings remain technical debt and are not interpreted as production-readiness evidence.

## Evidence inventory

The canonical directory contains source and record manifests; source registry, license, raw input, processed item, quarantine, privacy, secret, deduplication, quality, contamination, split, annotation, safety, deletion, sealed-access, audit, dataset-card, release-manifest, metric, ablation, resource, restart, configuration, normal build/CTest, sanitizer build/CTest, environment, workflow, and formal decision files, plus `manifest.sha256`.

## Explicit limitations and non-claims

This PASS does not establish that a production corpus has been acquired, that any dataset license is suitable for commercial deployment, that privacy risk is solved for arbitrary real data, that the fixture’s detectors cover all PII or secrets, that the pipeline is secure against all poisoning or malware, that annotation quality generalizes beyond the fixture, that the throughput scales to large corpora, or that any model has been trained or fine-tuned.

This PASS does not authorize downloading, scraping, or ingesting external data. It does not authorize using personal conversations. It does not establish production legal compliance, security certification, human evaluation, model capability, or general intelligence. The fixture is deliberately small and synthetic/control-oriented.

## Transition boundary

**Stage 9 is NOT STARTED.** Stage 9 may begin only after explicit user approval and a separately approved plan for an actual licensed corpus, model selection, compute budget, data-use rights, and independent evaluation. No base-model selection, continued pretraining, fine-tuning, or user-facing deployment has begun under this decision.

## Final status

`STAGE8_DECISION=PASS`

`EXECUTABLE_GATES=25/25`

`NORMAL_CTEST=21/21`

`SANITIZER_CTEST=21/21`

`RETAINED_FIXTURE_ITEMS=4`

`STAGE9_STATUS=NOT_STARTED`
