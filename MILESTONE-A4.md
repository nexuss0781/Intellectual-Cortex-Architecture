# Milestone A4 — Two-Step Decision Chain

## One question

Can the learner assign a final outcome across two sequential decisions instead of learning only one isolated action preference?

## Environment

Each episode has two steps. At step one, the system chooses route A or route B. At step two, it chooses action 0 or action 1. Only one complete path is good: route B followed by action 1 receives `+1.0`; every other path receives `-0.5` after the second decision. The final outcome is delayed until the end of the episode.

The learner receives eligibility traces for both selected decisions and one final reward. The no-learning control uses the same episode stream but keeps all weights fixed.

## Minimum success criteria

The learned system must select the complete good path on at least 70% of held-out episodes, exceed the no-learning control by at least 20 percentage points, preserve both learned preferences after serialization and restart, and report zero weight-bound violations.

## Interpretation

A pass demonstrates short-horizon credit assignment over a two-step chain in a controlled environment. It does not establish general planning, consciousness, or human-like wisdom.
