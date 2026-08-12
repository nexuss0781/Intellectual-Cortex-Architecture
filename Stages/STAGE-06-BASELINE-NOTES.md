# Stage 6 Baseline Notes

## Entry status

Stage 5 is formally PASS at commit `13268c1`. Stage 6 is authorized by the user's explicit approval, but no later stage is authorized.

## Required Stage 6 capability

Stage 6 must demonstrate reusable structure learned from deterministic multimodal experience, not isolated task memorization. It must connect modality adapters, a shared typed event/pointer space, perceptual/language/action-consequence memory, predictive world modeling, planning, confidence, safety, and environment feedback.

## Required environment contract

The environment must implement deterministic `reset(seed)`, `observe()`, bounded `act(command)`, `feedback()`, versioned `describe()`, and `snapshot()`/restore. The action space must include safe no-op, clarification, observation, and abstention.

## Required curriculum

D1 stable form-to-percept association; D2 form-to-action and consequence prediction; D3 roles, temporal order, and simple goals; D4 negation, uncertainty, and exceptions; D5 new entities/appearance transfer; D6 relational transfer to a new domain or language. Interleaved review, distribution shifts, and long-horizon retention are mandatory.

## Required controls and claims

The harness must compare the full engine with scratch, form-only, perception-only, task-specific memorizer, no-replay, and no-semantic-transfer controls. Transfer must be measured by sample efficiency or equal-exposure score advantage. Claims must be linked to stored manifests, traces, checkpoints, hashes, safety logs, and calibration/resource artifacts.

## Quantitative gates

The implementation must meet all unit/integration/operations thresholds in the Stage 6 specification: 100% deterministic and snapshot unit gates; D1–D4 task thresholds; visual/sensory variation >=75%; cross-domain transfer >=30% fewer examples or >=15 percentage-point equal-exposure advantage; cross-language structural transfer >=60% and >=15 points above surface matching; long-horizon retention >=80%; OOD uncertainty/abstention >=90%; unsafe-action blocking >=99%; save/load divergence <=5 points; and zero memory-budget violations.

## Compatibility constraints

Reuse the existing `genesis::` namespace, C++17, CMake/CTest, stable memory/event conventions, language semantic pointers, persistent memory/replay, predictive workspace, reasoning/provenance, executive control, confidence, and abstention. Do not hide an external end-to-end model behind the grounding label. Keep resource usage bounded under the 500 MB substrate constraint.
