# Milestone A5 — Uncertainty and Abstention

## One question

Can the system distinguish a familiar situation, where it should act, from an unfamiliar situation, where it should abstain rather than make a confident random decision?

## Environment

The system trains on two known contexts with stable action outcomes. During evaluation it receives both known contexts and an unseen context whose action values are deliberately ambiguous. The decision rule must use the learned action margin as a simple confidence signal.

## Minimum success criteria

The learned system must achieve at least 80% accuracy on known contexts, abstain on at least 80% of unseen-context trials, and avoid more than 20% confident wrong actions on unseen trials. The saved state must reproduce the same decision behavior after restart, and the no-learning control must not pass the known-context criterion.

## Interpretation

A pass demonstrates a narrow uncertainty policy over familiar versus unfamiliar input. It does not establish calibrated confidence in the general machine-learning sense or human-like metacognition.
