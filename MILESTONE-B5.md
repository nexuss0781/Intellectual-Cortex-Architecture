# Milestone B5 — Reproducibility and Retention Across Seeds

## Objective

Confirm that the validated contextual experience-memory-replay loop is not dependent on one favorable random seed.

## Evaluation

Run the B3 contextual learner with five fixed seeds: `11`, `22`, `33`, `44`, and `55`. For every seed, measure held-out accuracy across all four task-context combinations, replay count, restart accuracy, state-hash equality after restart, and bound violations.

Run each seed twice. The same seed must produce the same final state hash and the same accuracy, while different seeds may produce different hashes.

## Gates

Every seed must achieve at least 80% accuracy and at least 80% restart accuracy. Every seed must execute replay, report zero bound violations, and reproduce its own state hash on the second run. The aggregate mean accuracy must be at least 90%.

## Stop rule

This is the final reliability check before B6 substrate integration. Do not change the learning algorithm or add architecture during B5.
