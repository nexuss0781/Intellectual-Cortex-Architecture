# Stage 8 Design — Governed Data Factory and Evaluation Integrity

## Decision boundary

Stage 8 will implement an auditable data-factory engine and evaluation-integrity control plane. Because no licensed production corpus or user-authorized private data has been supplied, the implementation will use a small checked-in `sandbox_fixture` source explicitly marked `internal-test-permission` and `not_for_production_training`. This proves the factory behavior without pretending that a real production corpus has been acquired.

No source can enter the retained training or evaluation release unless its acquisition record has an owner, permission/license, jurisdiction, intended use, retention policy, content hash, timestamp, and removal support. Unclear rights, missing removal capability, PII, secrets, malformed records, contamination markers, and sealed-set access attempts fail closed.

## Real executable application path

The harness loads `configs/stage8_records.tsv` and processes it through the same pipeline used by the product contract:

```text
source registry
  → checksum and rights admission
  → record parser and format validation
  → language/modality classification
  → PII and secret scanner
  → redaction/quarantine decision
  → exact and near-duplicate detection
  → quality and safety triage
  → source/time/entity-aware split
  → contamination scan
  → annotation schema and agreement checks
  → dataset-card and release-manifest generation
  → sealed-set custody and audit log
  → deletion-by-source verification
```

The checked-in records include ordinary allowed fixtures, a PII canary, a secret canary, an unclear-license source, an exact duplicate, a near duplicate, a seeded benchmark fragment, a prompt-injection document, a safety-quarantine item, and hidden evaluation examples. Only the valid non-sensitive records may enter the training release. The remaining records must be represented in quarantine or exclusion reports; they may not silently disappear.

## Stable interfaces

The implementation adds `src/production/stage8_data_factory.h` with:

- `SourceRecord`, `DataItem`, `DatasetRelease`, `AnnotationRecord`, `SplitManifest`, `AccessEvent`, and `DataFactoryReport`.
- `DataFactory::ingest`, `process`, `approve_release`, `delete_by_source`, and `allow_for_training`.
- deterministic canonical serialization, content/provenance hashes, privacy/secret scanning, exact/near dedupe, contamination scanning, source/time split assignment, schema validation, safety quarantine, sealed-set custody, and append-only audit events.
- `Stage8Pipeline` for loading the checked-in source manifest and records, producing training/development/release/sealed splits, dataset cards, and machine-readable reports.

## Data and evaluation contract

The source fixture is not a production corpus. Its purpose is to prove that the factory can distinguish retained, redacted, quarantined, excluded, and sealed records. A future production source must replace it with a licensed record and a signed acquisition decision; the harness must fail if the source is presented without the required rights fields.

The hidden evaluation set is independently represented in the manifest and cannot be read through the training/tuning API. An attempted sealed read is recorded as a security event and returns no payload. A release is blocked if any known benchmark fragment is retained in training, if an exact duplicate crosses splits, if a near duplicate exceeds the declared threshold, or if any retained item lacks source lineage.

## Failure-injection plan

The harness verifies the immutable Stage 7 PASS decision and manifest, then actively tests missing permission, incompatible license, PII, secret, malformed row, exact duplicate, near duplicate, seeded contamination, prompt injection, safety escalation, invalid annotation schema, hidden-set unauthorized read, deletion request, corrupted manifest, interrupted restart, and disabled-control ablations. Each control must fail closed and emit evidence. The executable contract is 25 gates: 8 unit, 6 integration, 3 operations, and 8 negative controls.

## Quantitative implementation budget

The management-plane pipeline must remain below a 64 MB RSS budget and process at least 100 records per second on the checked-in fixture. This is a control-plane budget, not a claim about future large-corpus throughput or model training economics. Restart tests must produce byte-identical release manifests with no duplicated or missing item IDs.

## Evidence contract

The harness writes:

```text
source_registry.csv
license_decisions.csv
raw_input_manifest.csv
processed_items.csv
quarantine_ledger.csv
privacy_report.csv
secret_report.csv
dedupe_report.csv
quality_report.csv
contamination_report.csv
split_manifest.csv
annotation_report.csv
safety_quarantine.csv
delete_report.csv
sealed_access_audit.jsonl
audit_trace.jsonl
dataset_card.md
release_manifest.json
stage8_metrics.csv
ablations.csv
resource_trace.csv
restart_comparison.csv
```

The canonical workflow additionally writes configuration, input records, normal/sanitizer build and test logs, environment metadata, formal decision, and a SHA-256 manifest. The final decision must state the fixture limitation and `Stage 9 NOT STARTED`.

## Transition boundary

Stage 8 PASS authorizes only a governed data-factory capability. It does not authorize using scraped or personal data, training a model, fine-tuning a model, opening sealed evaluation custody, or deploying a model. Stage 9 may begin only after explicit user approval and a separately approved licensed corpus plan.
