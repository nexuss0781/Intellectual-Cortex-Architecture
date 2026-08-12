# Milestone A2 — Delayed Consequence and Long-Term Choice

## One question

Can the existing learning controller assign a delayed outcome to the action that caused it, and learn to prefer the long-term option over an immediately tempting but ultimately worse option?

## Environment

There is one context and two actions. Action 0 gives an immediate reward of `+0.4` but produces a delayed terminal penalty of `-1.0` after five ticks. Action 1 gives no immediate reward but produces a delayed reward of `+1.0` after five ticks. The rational long-term choice is action 1 because its total outcome is better.

The experiment uses the existing eligibility trace. The action event occurs at the beginning of the episode; the reward signal is applied only after the delay. No new learning algorithm is introduced.

## Required comparisons

Run four conditions: no-learning control, learning with one-tick reward delay, learning with five-tick reward delay, and learning with five-tick delay plus shuffled outcome/action pairing as a negative control.

## Minimum success criteria

The five-tick learning condition must choose action 1 on at least 70% of held-out episodes, exceed the no-learning control by at least 20 percentage points, preserve the result after save/restart, and show that shuffled outcomes do not produce the same advantage.

## Interpretation

A pass means only that the controller can learn one delayed action-value preference in a bounded toy environment. It does not establish general planning, wisdom, consciousness, or broad intelligence.
