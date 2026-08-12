# Milestone A3 — Restart and Transfer

## One question

After learning a long-term preference and restarting, does the preference remain useful when the environment changes slightly but preserves the same underlying relation?

## Training environment

The system trains on the A2 task: action 0 is tempting but has a negative delayed total outcome; action 1 has a positive delayed outcome.

## Transfer environment

After serialization and restart, the reward magnitudes and delay change. Action 0 remains ultimately worse, while action 1 remains ultimately better. The system receives no additional learning during transfer evaluation.

## Minimum success criteria

The restarted learned system must choose action 1 on at least 70% of transfer episodes, while a fresh no-learning control must remain below 70%. The serialized state hash must match after restoration.

## Interpretation

A pass demonstrates retention and narrow relational transfer in a controlled environment. It does not demonstrate broad generalization or human-like wisdom.
