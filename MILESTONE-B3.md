# Milestone B3 — Contextual Task Identity

## Objective

Test whether the system can store two conflicting rules and retrieve the correct rule from task identity instead of overwriting one rule with another.

## Environment

There are two task contexts. In Task 1, Context A maps to action 0 and Context B maps to action 1. In Task 2, the same contexts use the reversed mapping. The task identity is presented as a separate context signal at decision time.

The system stores episodes for both task identities, replays them, and evaluates the correct rule for each task-context pair.

## Gates

The contextual learner must achieve at least 80% accuracy across all four task-context combinations, exceed a no-context control by at least 20 percentage points, execute replay events, and reproduce the same decisions after restart.

## Stop rule

Whether this passes or fails, do not add a new cognitive stage. Record the limitation and move to B4 only after review.

## Interpretation

A pass demonstrates context-indexed retrieval of conflicting rules in a controlled benchmark. It does not establish broad reasoning or general intelligence.
