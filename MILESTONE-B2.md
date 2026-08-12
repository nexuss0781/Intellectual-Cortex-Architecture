# Milestone B2 — Continual Learning Under Interference

## Objective

Test whether episodic replay protects an earlier learned task after the system learns a conflicting second task.

## Environment

Task 1 maps context A to action 0 and context B to action 1. Task 2 reverses both mappings. After Task 2, the system is evaluated on both task identities. The task identity is included in the episode context, so the experiment measures memory retention and contextual retrieval rather than pretending that contradictory rules are globally compatible.

## Conditions

Compare learning without replay, learning with replay of Task 1 episodes during Task 2, and a no-learning control. The replay condition must serialize and reload its memory before final evaluation.

## Minimum success criteria

The replay condition must achieve at least 80% on Task 1 before interference, achieve at least 80% on the current Task 2 before replay restoration, restore Task 1 to at least 80% after replay, execute nonzero replay events, and reproduce the final memory state after restart. The no-replay condition is expected to show measurable Task 1 degradation under the deliberately conflicting update. Because the two tasks intentionally use conflicting shared weights, the benchmark does not require both contradictory policies to be active simultaneously; it measures replay-based restoration of the earlier task.

## Interpretation

A pass demonstrates contextual continual learning with replay in a controlled benchmark. It does not establish lifelong learning across arbitrary domains.
