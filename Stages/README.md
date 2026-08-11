# Nexuss Brain Engine Stage Specifications

This directory is the executable research and engineering contract for turning Nexuss Neural Cognition and the Intellectual Cortex Architecture into a persistent, continually learnable brain engine. Each stage is an independent Markdown specification. A stage may be implemented only after its entry conditions are met, and it may be declared complete only when its evaluation harness produces the required artifacts and every transition gate passes.

The stage order is deliberately dependency-driven rather than feature-driven. A system must first be physically correct and reproducible, then learn locally, then preserve state and consolidate memory, then coordinate attention and prediction, then acquire language, then reason and monitor itself, and finally learn through grounding and developmental transfer.

| Stage | File | Primary outcome | Depends on |
|---|---|---|---|
| 0 | [`STAGE-00-SUBSTRATE-STABILIZATION.md`](STAGE-00-SUBSTRATE-STABILIZATION.md) | Correct, deterministic, reproducible substrate | Existing repository |
| 1 | [`STAGE-01-LOCAL-LEARNING-KERNEL.md`](STAGE-01-LOCAL-LEARNING-KERNEL.md) | Measurable online synaptic learning | Stage 0 |
| 2 | [`STAGE-02-PERSISTENT-MEMORY-REPLAY.md`](STAGE-02-PERSISTENT-MEMORY-REPLAY.md) | State-preserving episodic memory and selective consolidation | Stage 1 |
| 3 | [`STAGE-03-PREDICTIVE-WORKSPACE.md`](STAGE-03-PREDICTIVE-WORKSPACE.md) | Prediction, attention, precision, and global broadcast | Stage 2 |
| 4 | [`STAGE-04-LANGUAGE-ACQUISITION.md`](STAGE-04-LANGUAGE-ACQUISITION.md) | Grounded compositional language learning | Stage 3 |
| 5 | [`STAGE-05-REASONING-EXECUTIVE-METACOGNITION.md`](STAGE-05-REASONING-EXECUTIVE-METACOGNITION.md) | Causal reasoning, planning, confidence, and strategy control | Stage 4 |
| 6 | [`STAGE-06-GROUNDING-DEVELOPMENTAL-TRANSFER.md`](STAGE-06-GROUNDING-DEVELOPMENTAL-TRANSFER.md) | Multimodal developmental learning and transfer | Stage 5 |

## Shared definition of done

A stage is not complete because code compiles, fields exist, or a demo produces plausible output. Completion requires a deterministic harness, quantitative metrics, negative controls, ablations, resource measurements, serialized evidence, and a documented pass/fail decision. Every stage must record compiler version, commit SHA, operating-system information, random seed, input manifest hash, configuration, peak resident memory, throughput, and complete test output.

Each implementation must expose stable interfaces rather than allowing later modules to access internal arrays opportunistically. Events must carry a timestamp, source, destination, event type, payload identifier, and provenance identifier. Persistent objects must have stable IDs. All stochastic components must accept an explicit seed and support deterministic replay.

## Global test conventions

Tests use GoogleTest for unit and invariant tests and standalone C++ or Python harnesses for protocol, learning, and benchmark tests. A passing test must fail when the target behavior is removed. Tests that only assert “the function ran” are invalid. Every behavioral test must include at least one positive case, one negative case, and one boundary case.

A transition gate is conjunctive: **all** required tests must pass, no severity-1 correctness defect may remain open, and no performance or memory limit may be exceeded. If a stage fails, the implementation remains at that stage; later-stage work may proceed only in isolated experimental branches and cannot be called integrated progress.

## Resource baseline

The current repository publishes a 500 MB formula based on 88 bytes per neuron and 32 bytes per synapse with a 1.15 overhead factor. The full nominal configuration is approximately 500 MB before cognitive indexes, queues, logs, allocators, and serialization buffers. The brain-engine stages therefore use a recommended 350 MB substrate envelope and reserve approximately 150 MB for memory, workspace, indexes, instrumentation, and safety headroom. Every stage must measure actual resident memory; formula-only accounting is insufficient.

## Required evidence layout

Each implementation should produce the following repository artifacts:

```text
artifacts/stage-N/
├── config.json
├── manifest.sha256
├── build.txt
├── unit-tests.txt
├── integration-tests.txt
├── benchmark.csv
├── ablation.csv
├── memory.csv
├── traces.jsonl
├── decision.md
└── plots/
```

The `decision.md` file must state `PASS` or `FAIL`, list every gate, link to the relevant artifact, and identify the next stage or the blocking defect. A stage transition is invalid without this decision file.

## Cross-stage invariants

The following invariants must remain true throughout the program. Neuron and synapse IDs must remain valid after allocation, serialization, and compaction. A saved and restored system must produce the same deterministic continuation for the same input stream. Learning must be bounded by explicit weight, trace, firing-rate, and event-queue limits. No module may silently discard an out-of-bounds event or failed migration; failures must be observable and testable. Any claim of memory preservation must compare task performance before and after later learning, not merely compare allocated bytes.

## Recommended implementation order within each stage

Implement data structures and interfaces first. Implement deterministic kernels second. Add behavioral tests before optimization. Implement the benchmark and ablation harness before tuning thresholds. Run the complete stage harness, inspect artifacts, fix failures, and only then optimize. The final transition test must run from a clean build and a fixed seed.
