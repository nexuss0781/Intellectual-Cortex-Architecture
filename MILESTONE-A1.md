# Milestone A1 — Experience-to-Decision Loop

## One question

Can the existing Stage 1 learning controller improve a later decision from experience, while remaining reproducible and bounded?

## Environment

Use a deterministic two-context, two-action task. In context A, action 0 is rewarded and action 1 is not. In context B, action 1 is rewarded and action 0 is not. The context is encoded by two input neurons, and each action is represented by one output neuron. The experiment uses a small population and does not modify the 270K-neuron substrate.

## Required comparison

Run the same fixed-seed stream with learning disabled and learning enabled. The learned condition must use the existing `LearningController`; the control must use the same event stream and topology but no weight updates.

## Minimum success criteria

The learned condition must achieve at least 70% held-out decision accuracy, exceed the no-learning control by at least 15 percentage points, reproduce the same final hash on two same-seed runs, and remain within the configured weight bounds with zero bound violations.

## What this does not claim

This experiment does not claim general intelligence, consciousness, language understanding, wisdom in the human sense, or production readiness. It tests one narrow property: whether a bounded local learning mechanism creates a reusable context-to-action preference.

## Result format

The harness must emit a CSV containing seed, condition, train accuracy, held-out accuracy, improvement over control, final synapse weights, bound violations, and final state hash. It must also print a one-line PASS or FAIL decision.
