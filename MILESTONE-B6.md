# Milestone B6 — Large Neural Substrate Integration

## Objective

Run the validated contextual experience-memory-replay loop through the repository's actual large neural substrate rather than only through a tiny four-context fixture.

## Integration boundary

Use the existing neuron and synapse blocks, the substrate's spike/event path, the existing LearningController, and MemorySystem. Do not replace the substrate, add a new learning algorithm, or add a new cognitive stage.

## Gates

The harness must instantiate the documented large substrate configuration, execute at least 100,000 substrate events, preserve bounded weights, complete contextual held-out decisions at least 80% accurately, preserve the learned state after restart, and record peak resident memory and elapsed time. The small-loop B5 result remains the control; B6 is a scale integration test, not a claim that more neurons automatically produce more intelligence.

## Stop rule

After B6, stop implementation and perform the B7 final go/no-go review. No additional architecture expansion is allowed before that review.
