# Stage 8 — Data Factory and Evaluation Integrity

## Mission

Stage 8 creates the governed data factory required for real NLP training and trustworthy evaluation. It turns approved sources into versioned, documented, privacy-aware, license-aware, de-duplicated, contamination-controlled training, validation, hidden-test, preference, safety, tool-use, grounding, and regression datasets.

The outcome is **not** a trained production model. It is an auditable data and evaluation system that can prove what was used, what was excluded, what can be deleted, and what is reserved for independent measurement.

## Entry conditions

Stage 7 must be `PASS`. The product charter, prohibited-use policy, data jurisdictions, risk register, benchmark charter, and named governance owners must be available. No source may enter the data factory without an approved acquisition record.

## Data contract

```cpp
struct SourceRecord {
    std::string source_id;
    std::string owner;
    std::string acquisition_method;
    std::string license_or_permission;
    std::string jurisdiction;
    std::string intended_use;
    std::string retention_policy;
    std::string content_hash;
    uint64_t collected_at;
    bool training_allowed;
    bool evaluation_allowed;
    bool removal_supported;
};

struct DataItem {
    std::string item_id;
    std::string source_id;
    std::string split;
    std::string language;
    std::string modality;
    std::string sensitivity;
    std::string content_hash;
    std::string provenance_hash;
    bool pii_detected;
    bool secret_detected;
    bool safety_reviewed;
};

struct DatasetRelease {
    std::string dataset_id;
    std::string version;
    std::string manifest_digest;
    std::string card_digest;
    std::string split_manifest_digest;
    std::string quality_report_digest;
    std::string approval_digest;
};

class DataFactory {
public:
    bool ingest(const SourceRecord&);
    std::vector<DataItem> process(const std::string& source_id);
    bool approve_release(const DatasetRelease&);
    bool delete_by_source(const std::string& source_id);
    bool allow_for_training(const std::string& item_id) const;
};
```

A dataset card must describe contents, intended use, licenses, languages, modalities, size, known biases, collection process, and limitations. A production card additionally records source manifest, PII treatment, rights review, retention, deletion path, quality statistics, split method, and contamination scan. These requirements extend standard dataset-card practice [1].

## Processing pipeline

| Step | Mandatory control | Failure behavior |
|---|---|---|
| Ingest | Source record, checksum, owner, permission/license, intended use | Quarantine source |
| Parse and triage | Format validation, malware scan, language/modality detection, size limits | Reject malformed item |
| Privacy | PII and secret detection, redaction/exclusion, sensitivity tag | Quarantine or redact; never silently pass |
| Rights | License compatibility, restricted-use policy, removal capability | Exclude unresolved source |
| Quality | Exact/near dedupe, boilerplate/spam filtering, quality score | Downweight or exclude |
| Safety | Harm taxonomy, toxic/abusive/illegal content controls, reviewer escalation | Isolate into safety set or exclude |
| Split | Source/time/entity-aware split, leakage checks, immutable manifests | Block release on overlap |
| Annotation | Schema validation, double annotation sample, adjudication | Flag low-agreement batch |
| Package | Tokenized shards, cards, manifests, signatures | No unsigned release |

## Dataset families

| Dataset family | Purpose | Required negative control |
|---|---|---|
| Continued-pretraining corpus | Domain language and terminology | Unsupported/unclear-license source rejected |
| SFT corpus | Desired response, citation, schema, clarification, refusal behavior | Invalid schema and fabricated citation examples rejected |
| Preference corpus | Chosen/rejected quality and safety comparisons | Rater identity/order bias test |
| Safety corpus | Refusal, safe transformation, escalation, privacy, injection cases | Benign request incorrectly refused and harmful request accepted |
| Tool-use corpus | Typed tool proposals and result handling | Out-of-schema or unauthorized call rejected |
| Grounding corpus | Form, retrieval, action, consequence links | Form-only association with no non-linguistic evidence not promoted |
| Hidden evaluation corpus | Final measurement only | Training/tuning access must fail |
| Regression corpus | Confirmed defects and incident cases | Removing fix must make test fail |

## Evaluation integrity protocol

The hidden test set has independent custody. It must be source-separated, time-separated where feasible, entity-separated, and near-duplicate scanned against all training and development data. Test prompts, labels, and scoring keys are access-controlled. No final-test result may be used to tune prompts, base-model choice, adapter, RAG parameters, safety rules, or thresholds. Any access is recorded and triggers a test-set version review.

The factory must maintain three evaluations: a **development set** for iteration, a **release set** for internal go/no-go decisions, and a **sealed set** for independent post-freeze measurement. Synthetic examples may augment development coverage but cannot be the only support for a production claim.

## Implementation work packages

| Work package | Deliverable |
|---|---|
| D8.1 | Source registry, license/permission schema, and content-hash ledger |
| D8.2 | Deterministic parsers and native text ingestion for CSV, JSONL, Markdown, and plain text; isolated library adapters for PDF, image, audio, and video |
| D8.3 | Privacy/secret scanning, redaction, sensitivity classification, and deletion workflow |
| D8.4 | Deduplication, quality scoring, spam/boilerplate filters, and source balancing |
| D8.5 | Safety labeling and quarantine workflow with reviewer escalation |
| D8.6 | Split manager, contamination scanner, hidden-test custody, and access audit |
| D8.7 | Annotation platform schema, rater calibration, inter-rater agreement, and adjudication |
| D8.8 | Dataset cards, release manifest, signed data package, and reproducibility harness |

## Evaluation harness

| Test ID | Test | Pass condition |
|---|---|---|
| D8-UNIT-01 | Source completeness | 100% of retained items resolve to approved source records |
| D8-UNIT-02 | Hash determinism | Reprocessing unchanged input yields identical item and release manifests |
| D8-UNIT-03 | License gate | Missing or incompatible license/permission blocks training release |
| D8-UNIT-04 | Privacy gate | Seeded PII and secrets are redacted/quarantined and audited |
| D8-UNIT-05 | Deletion | Source removal identifies and removes every linked item/release reference |
| D8-UNIT-06 | Schema integrity | Invalid annotation or tool schema fails validation |
| D8-UNIT-07 | Dataset-card completeness | Required card fields are present for every release |
| D8-INT-01 | Near-duplicate split isolation | Cross-split duplicate/near-duplicate rate is below declared maximum |
| D8-INT-02 | Hidden-set protection | Training or tuning process cannot read sealed examples |
| D8-INT-03 | Contamination scan | Known seeded benchmark examples are detected and excluded from train |
| D8-INT-04 | Source/time split | Source and time leakage checks pass for release and sealed sets |
| D8-INT-05 | Annotation quality | Agreement and adjudication gates pass on calibration sample |
| D8-INT-06 | Safety quarantine | High-risk content is routed per declared policy with no silent drop |
| D8-OPS-01 | Throughput | Pipeline meets declared documents/items per hour under resource budget |
| D8-OPS-02 | Restart integrity | Interrupted job resumes without duplicate or missing manifest entries |
| D8-OPS-03 | Auditability | 100% of source, transformation, reviewer, and release actions are traceable |

### Required ablations and adversarial tests

Run the same harness without near-deduplication, without source-level splitting, without privacy filtering, without secret scanning, without sealed-set access control, and without annotation adjudication. Each removal must degrade the corresponding integrity metric. Add adversarial malformed files, poisoned duplicates, hidden benchmark fragments, license-conflict records, deletion requests, PII canaries, prompt-injection text in documents, and corrupted manifests.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit/integration/operations tests | 100% |
| Retained items with approved source lineage | 100% |
| Missing/invalid rights records in training release | 0 |
| Source deletion completeness | 100% |
| Seeded PII/secret handling | 100% detected and controlled |
| Sealed-set unauthorized reads | 0 |
| Known contamination examples retained in train | 0 |
| Cross-split exact duplicate rate | 0 |
| Cross-split near-duplicate rate | ≤ declared product threshold |
| Dataset-card coverage | 100% |
| Annotation agreement | ≥ pre-approved task-specific threshold |
| Untraceable transformations/releases | 0 |
| Pipeline restart data loss/duplication | 0 |

## Evidence package

Store source registry exports, license decisions, data cards, source and release manifests, ingestion logs, privacy/secret reports, deletion reports, dedupe/contamination reports, split manifests, annotation agreement/adjudication reports, safety quarantine ledger, pipeline throughput traces, audit logs, `stage8_metrics.csv`, `ablations.csv`, `resource_trace.csv`, normal/sanitizer logs, `decision.md`, and `manifest.sha256`.

## Transition to Stage 9

Stage 9 may begin only after Stage 8 is `PASS`, a signed release-quality data package exists, the sealed evaluation set has independent custody, all critical rights/privacy/contamination findings are resolved, and the user explicitly approves base-model and continued-pretraining implementation.

## References

[1]: https://huggingface.co/docs/hub/en/datasets-cards "Hugging Face Dataset Cards"
[2]: https://doi.org/10.6028/NIST.AI.600-1 "NIST Generative AI Profile"
[3]: https://csrc.nist.gov/pubs/sp/800/218/a/ipd "NIST AI Secure Software Development Profile"
