# Milestone B4 — Larger Integrated Workload

## Objective

Run the validated contextual experience-memory-replay loop at a materially larger workload without changing the architecture.

## Workload

Process 100,000 recorded episodes across four task-context combinations. Each episode includes context, action, delayed outcome, episodic capture, and periodic replay. The learner must continue to use bounded synaptic weights.

## Gates

The harness must process at least 100,000 episodes, retain at least 80% held-out accuracy across all contextual combinations, execute replay, keep memory state internally consistent, preserve the learned state after restart, and report zero bound violations. Peak resident memory is recorded rather than assigned an arbitrary pass threshold; the result is used to choose the next scale test.

## Stop rule

This is a workload and stability test only. Do not add new cognitive stages, new learning algorithms, or new claims based on the result.
