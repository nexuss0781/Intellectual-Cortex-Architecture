# Stage 9 — NLP Baseline Selection and Continued Pretraining

## Mission

Stage 9 establishes a reproducible, measured language-model baseline and tests whether licensed domain data improves target NLP performance without unacceptable regression, contamination, safety degradation, or unmeasured resource cost. It compares an approved base model, a small local baseline, and an independent reference under the same signed task and serving configuration.

The outcome is **not** a production model, a claim of broad understanding, or permission to launch. It is a selected, documented checkpoint and a continued-pretraining result with reproducible evidence.

## Entry conditions

Stage 8 must be `PASS`. The approved data release, data cards, source manifests, privacy report, contamination report, sealed evaluation custody, baseline protocol, and hardware budget must be available. The candidate base model must have a reviewable license, immutable identifier, tokenizer, documented intended use, known limitations, and security/supply-chain record.

## Model and run contract

```cpp
struct ModelIdentity {
    std::string model_id;
    std::string weight_digest;
    std::string tokenizer_digest;
    std::string license;
    std::string base_model_card_digest;
    uint64_t parameter_count;
};

struct TrainingRunManifest {
    std::string run_id;
    ModelIdentity base;
    std::string dataset_release_digest;
    std::string split_manifest_digest;
    std::string code_commit;
    std::string container_digest;
    std::string hardware_manifest;
    std::string optimizer_config_digest;
    std::string seed_policy;
    std::vector<std::string> checkpoint_digests;
};

struct CheckpointEvaluation {
    std::string checkpoint_digest;
    double domain_score;
    double general_retention;
    double safety_score;
    double citation_or_grounding_score;
    double p95_latency_ms;
    double peak_gpu_memory_mb;
    double energy_or_compute;
};

class TrainingRegistry {
public:
    bool start(const TrainingRunManifest&);
    bool record_checkpoint(const CheckpointEvaluation&);
    bool approve_checkpoint(const std::string& checkpoint_digest);
    bool reproduce(const std::string& run_id) const;
};
```

A production model card must state intended uses, prohibited uses, limitations, training/evaluation datasets, base-model relationship, parameters, hardware, safety findings, known biases, performance results, license, and release scope [1]. No checkpoint may be promoted without a complete run manifest and model card.

## Baseline design

The Stage 9 benchmark has three comparison arms:

| Arm | Purpose | Constraint |
|---|---|---|
| Approved base model | Measures adaptation value | Immutable public or licensed starting checkpoint |
| Small local model | Measures privacy/latency fallback value | Separate memory and latency budget |
| Independent reference | Guards against self-comparison | Same prompts, data boundary, and scoring protocol |

The program must report confidence intervals and absolute sample counts. It must distinguish model quality from retrieval quality, prompt quality, and tool policy. The independent reference is not automatically a production dependency; it is a comparator.

## Continued-pretraining protocol

The protocol begins with a small pilot. The pilot uses source-balanced domain data plus an approved general-data mixture to test domain gain, general retention, and safety. It uses fixed sequence length, token budget, optimizer, mixed-precision policy, checkpoint cadence, validation cadence, and stop criteria. Fine-tuning and continued pretraining must be configured with explicit train/validation splits and evaluation/checkpoint strategy; standard tooling treats fine-tuning as continuing from pretrained weights on a smaller task or domain dataset [2].

Training selection is multi-objective. The winning checkpoint must improve target-domain performance while preserving general capability, safety, language coverage, and resource envelope. Lowest loss alone cannot select a release candidate.

## Implementation work packages

| Work package | Deliverable |
|---|---|
| M9.1 | Base-model due-diligence record, license review, model card, and supply-chain digest |
| M9.2 | Reproducible distributed/single-node training environment with pinned container and hardware manifests |
| M9.3 | Training-run manifest, checkpoint ledger, deterministic seed policy, and restart support |
| M9.4 | Baseline benchmark harness with product, safety, multilingual, long-context, retrieval, and latency suites |
| M9.5 | Continued-pretraining pilot with source-balanced mixtures and fixed budgets |
| M9.6 | General-retention, safety-regression, calibration, and contamination monitors |
| M9.7 | Cost, energy, memory, throughput, and checkpoint-size accounting |
| M9.8 | Stage 9 ablation/restart/rollback harness and formal decision |

## Evaluation harness

| Test ID | Test | Pass condition |
|---|---|---|
| M9-UNIT-01 | Model identity | Weights, tokenizer, license, and base card digests are immutable and recorded |
| M9-UNIT-02 | Run manifest | A run cannot start with missing data, code, container, seed, or hardware fields |
| M9-UNIT-03 | Checkpoint integrity | Hash mismatch or missing optimizer state fails resume validation |
| M9-UNIT-04 | Split isolation | Training process cannot read sealed evaluation examples |
| M9-UNIT-05 | Selection policy | Lowest training loss cannot override safety/retention failure |
| M9-UNIT-06 | Rollback | Previous approved checkpoint and configuration resolve exactly |
| M9-UNIT-07 | Stage 8 entry integrity | Stage 8 PASS decision and release manifest are present; pilot data is not mislabeled production data |
| M9-INT-01 | Domain gain | Continued-pretrained checkpoint exceeds base-model target metric by approved margin |
| M9-INT-02 | General retention | General benchmark decline is within approved maximum |
| M9-INT-03 | Safety retention | Unsafe-compliance, privacy, and injection metrics do not regress beyond approved maximum |
| M9-INT-04 | Language coverage | Supported-language score remains within approved disparity bound |
| M9-INT-05 | Independent baseline | Report compares all arms under the same manifest and scoring |
| M9-INT-06 | Restart continuation | Interrupted training/resume differs from uninterrupted reference by no more than approved tolerance |
| M9-INT-07 | Calibration | Confidence quality/abstention does not degrade beyond approved tolerance |
| M9-OPS-01 | Training resource | Peak GPU/CPU memory, storage, time, and cost stay under signed budget |
| M9-OPS-02 | Inference resource | Candidate meets declared latency/throughput envelope on target hardware |
| M9-OPS-03 | Reproducibility | Same seed/config/data yields matching checkpoint/evaluation hashes within deterministic policy |

### Required ablations and adversarial tests

Compare the full mixture with no general-data mixture, no source balancing, no domain data, no contamination filter, no safety monitor, and no retention monitor. Include corrupted checkpoints, revoked source data, tokenizer mismatch, different hardware precision modes, deliberately leaked test fragments, poisoned samples, and unsupported-language prompts. The executable contract is **25 gates**: 7 unit, 7 integration, 3 operations, and 8 negative controls.

## Quantitative transition gates

| Gate | Required threshold |
|---|---:|
| Unit, integration, operations, and negative-control tests | 100% of 25 executable gates |
| Domain improvement over approved base | ≥ signed product margin with confidence interval |
| General benchmark retention | ≥ signed product floor |
| Safety/privacy/injection regression | ≤ signed maximum regression |
| Supported-language disparity | ≤ signed fairness/disparity bound |
| Sealed-evaluation contamination | 0 confirmed examples |
| Run/checkpoint/model-card completeness | 100% |
| Restart divergence | ≤ signed tolerance |
| Training and inference resource violations | 0 |
| Unapproved checkpoint promotion | 0 |
| Critical supply-chain/license findings | 0 unresolved |

## Evidence package

Store base-model due diligence, licenses, model cards, run manifests, tokenizer/config digests, training curves, checkpoint hashes, benchmark manifests/results/confidence intervals, general-retention and safety reports, calibration report, cost/energy/resource traces, restart comparisons, ablations, rollback evidence, normal/sanitizer logs where applicable, `stage9_metrics.csv`, `decision.md`, and `manifest.sha256`.

## Transition to Stage 10

Stage 10 may begin only after Stage 9 is `PASS`, the chosen checkpoint beats the approved baseline on the defined domain without violating retention/safety/resource gates, and the user explicitly approves supervised fine-tuning and structured NLP engine implementation.

## References

[1]: https://huggingface.co/docs/hub/en/model-cards "Hugging Face Model Cards"
[2]: https://huggingface.co/docs/transformers/main/en/training "Hugging Face Fine-tuning"
[3]: https://crfm.stanford.edu/helm/ "Stanford HELM"
