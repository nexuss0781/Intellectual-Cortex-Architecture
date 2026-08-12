# Milestone B1 — Integrated Experience-to-Decision Loop

## Objective

Connect the existing local learning controller and episodic memory into one bounded runtime loop:

`experience → action → delayed outcome → episodic storage → replay → local update → later decision`

## Task

The environment has two contexts and two actions. Context A rewards action 0; context B rewards action 1. Training is divided into two tasks. Task 1 is learned first, then Task 2 is learned. Replay may revisit stored Task 1 episodes during Task 2 learning.

## Conditions

The harness compares four conditions: no learning, learning without replay, learning with replay, and learning with replay followed by serialization and restart.

## Gates

The replay condition must reach at least 80% held-out accuracy on both tasks, retain at least 80% of Task 1 accuracy after Task 2, execute nonzero replay events, reproduce the same decision after restart, and report zero weight-bound violations. The first B1 run measures the integrated loop; a later capacity-limited variant will test whether replay improves retention over no replay.

## Boundary

A pass demonstrates one integrated learning-and-memory loop. It does not claim general intelligence, consciousness, or human wisdom.
