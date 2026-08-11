# Stage 0 — Substrate Stabilization

## Mission

Stage 0 converts the current Nexuss implementation from a promising prototype into a trustworthy experimental substrate. The output is not higher cognition. The output is a deterministic, physically coherent, testable kernel on which later learning claims can be believed.

The stage must correct known defects, eliminate silent failure, repair the end-to-end workflow, make state aliases explicit, and establish baseline measurements for speed and resident memory. No Stage 1 learning experiment may be used as evidence until this stage passes.

## Entry conditions

The repository must build from a clean checkout with C++17 and CMake. The existing low-level test suites may pass, but their results are treated only as a starting baseline. The implementation currently contains an excitatory-current sign defect in the UIN kernel and a stale build-script reference to `genesis_sim`; both are mandatory Stage 0 work.

## Non-goals

Stage 0 does not add language learning, semantic memory, planning, self-modeling, or new neuron classes. It does not optimize the system by changing the declared mathematical model. It does not waive a failing correctness test because an existing test suite passes.

## Target repository changes

| Component | Required change |
|---|---|
| `src/intellectual/uin_engine.h` | Use the declared reversal-potential convention consistently. Excitatory input must depolarize from rest; inhibitory input must hyperpolarize. |
| `tests/uin_tests.cpp` | Replace no-op membrane assertions with direction, magnitude, threshold, and boundary tests. |
| `build_and_run.sh` | Invoke the executable actually defined by CMake, run all test targets, and fail if any target is missing. |
| `src/types.h` | Document or isolate every alias between legacy substrate fields and UIN semantic fields. |
| `src/meta_cognition.cpp` | Make bounds, allocation failure, and reconfiguration failure observable rather than silently returning. |
| `tests/stage0_*` | Add deterministic, property, regression, serialization-preparation, and resource tests. |
| `artifacts/stage-0/` | Store configuration, logs, benchmark results, hashes, and the transition decision. |

## Physical contracts

The membrane update must use one sign convention. With excitatory reversal `E_exc = 0 mV`, inhibitory reversal `E_inh = -75 mV`, and resting potential `V_rest = -70 mV`, the conductance current must be:

```text
I_syn = g_exc × (E_exc − V) + g_inh × (E_inh − V) + g_bind × (E_bind − V)
```

For positive excitatory conductance at rest, `ΔV > 0`. For positive inhibitory conductance at rest, `ΔV < 0`. Conductances must decay monotonically when no event arrives. Potentials, thresholds, refractory timers, traces, and queues must remain bounded under the declared stress configuration.

Every public method that accepts an index, count, ID, or memory budget must validate it. Out-of-range input must produce a typed error or a recorded rejected operation. A silent `return` is not an acceptable error path for a research kernel.

## Determinism contract

Every experiment accepts a 64-bit seed. The seed must control all stochastic connectivity, input generation, replay noise, and allocation decisions. A deterministic run must produce identical event traces, weight arrays, and summary metrics when executed twice with the same compiler, configuration, seed, and input manifest. The harness must record a SHA-256 hash of the input manifest and final serialized state.

Floating-point tolerances must be explicit. Bitwise identity is required for deterministic mode on the same build; cross-platform comparisons use declared absolute and relative tolerances. Any use of uninitialized memory, unordered iteration over nondeterministic containers, or time-based random seeding is a Stage 0 failure.

## State alias contract

The current design reuses arrays such as `atp_level`, `avg_firing_rate`, `recovery_variable`, and `layer_id` for UIN meanings. The implementation may keep this memory optimization, but it must expose named accessors and a versioned `StateSchema` describing each alias. A later module must not directly infer that `atp_level` is energy when the neuron is in an intellectual pool and threshold when it is in a UIN pool.

```cpp
struct StateSchema {
    uint32_t version;
    bool uses_uin_overlay;
    enum class FieldMeaning { ENERGY, THETA_DYN, RECOVERY, PHASE, RATE, S_SLOW };
    FieldMeaning atp_meaning;
    FieldMeaning recovery_meaning;
    FieldMeaning rate_meaning;
};
```

A schema mismatch during load or module attachment must fail loudly with the expected and actual versions.

## Required implementation sequence

First, repair the current sign convention and add direct voltage-direction tests. Second, repair the build workflow and run every target produced by CMake. Third, add checked arithmetic for resource calculations and typed error reporting. Fourth, formalize state aliases and add schema validation. Fifth, add deterministic trace capture and a minimal state snapshot API. Sixth, add sanitizers and stress tests. Optimization is allowed only after the correctness suite is green.

## Evaluation harness

The Stage 0 harness must be runnable from a clean build with one command, for example:

```bash
cmake -S Nexuss-Neural-Cognition -B build-stage0 -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-stage0 --parallel
ctest --test-dir build-stage0 --output-on-failure
./build-stage0/stage0_harness \
  --seed 424242 \
  --config configs/stage0.json \
  --artifact-dir artifacts/stage-0
```

The harness must emit machine-readable CSV or JSON metrics and a human-readable `decision.md`. It must run the following test groups.

| Test ID | Test | Pass condition | Failure meaning |
|---|---|---|---|
| S0-PHY-01 | Excitatory direction | From `V=-70`, `g_exc=1`, `g_inh=0`, voltage increases after one tick | Sign or update equation is wrong |
| S0-PHY-02 | Inhibitory direction | From `V=-70`, `g_exc=0`, `g_inh=1`, voltage decreases after one tick | Inhibitory convention is wrong |
| S0-PHY-03 | Magnitude equation | Numerical voltage agrees with the declared discrete equation within `1e-5 mV` for 100 random bounded states | Implementation does not match specification |
| S0-PHY-04 | Threshold boundary | Exactly-boundary input follows the documented `>=` or `>` policy and is tested explicitly | Ambiguous spike semantics |
| S0-PHY-05 | Refractory | No spike can occur while timer is nonzero; timer reaches zero exactly as specified | Illegal repeated firing |
| S0-PHY-06 | Conductance decay | Each conductance follows `exp(-dt/tau)` within `1e-5` relative error over 1,000 ticks | Numerical instability or wrong constant |
| S0-PHY-07 | Boundedness | No NaN, infinity, or out-of-range state in 1,000,000 ticks of bounded random stimulation | Kernel can diverge |
| S0-DET-01 | Repeatability | Two same-seed runs have identical trace and final-state hashes | Hidden nondeterminism |
| S0-DET-02 | Seed separation | At least 95% of distinct seeds produce non-identical stochastic traces | Seed is ignored or ineffective |
| S0-ERR-01 | Bounds | Invalid indices and counts produce typed errors and no memory corruption | Silent unsafe failure |
| S0-OPS-01 | Full workflow | Clean build, all registered tests, demos, and harness complete with exit code 0 | Operational drift remains |
| S0-MEM-01 | Memory baseline | Resident memory is recorded for 1K, 10K, 100K, and target-scale configurations | No trustworthy capacity baseline |
| S0-SAN-01 | Sanitizer run | AddressSanitizer and UndefinedBehaviorSanitizer complete without errors | Memory or undefined behavior defect |

The harness must include negative controls. It must deliberately run the old sign equation in a test-only model and demonstrate that S0-PHY-01 fails. It must deliberately alter the random seed and demonstrate that S0-DET-02 detects a changed trace. A test that cannot detect its target defect is not a valid gate.

## Quantitative transition gates

Stage 0 passes only when all of the following conditions hold:

| Gate | Required threshold |
|---|---:|
| Core and UIN test pass rate | 100% |
| Stage 0 test pass rate | 100% |
| Sanitizer errors | 0 |
| NaN or infinity events | 0 |
| Same-seed trace mismatches | 0 of 3 repeated runs |
| Excitatory direction violations | 0 of 10,000 cases |
| Inhibitory direction violations | 0 of 10,000 cases |
| Invalid-input silent failures | 0 |
| Full workflow exit code | 0 |
| Resident-memory measurement coverage | 4 required scales present |

A single severity-1 correctness defect, silent memory corruption, missing artifact, or unverified executable blocks transition regardless of other scores.

## Evidence package

The stage decision must link to compiler output, `ctest` output, sanitizer output, `benchmark.csv`, voltage-direction data, state hashes, and the exact configuration. The decision must include the repaired issue list and a statement that later learning experiments are now allowed to use this substrate.

## Transition to Stage 1

Stage 1 may begin only when Stage 0 is marked `PASS`. Stage 1 must consume the corrected UIN kernel through public interfaces and may not fork a second untested membrane implementation.

## References

[1]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/Nexuss-Neural-Cognition/src/intellectual/uin_engine.h "Nexuss UIN kernel contract"

[2]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/Nexuss-Neural-Cognition/tests/uin_tests.cpp "Nexuss UIN tests"

[3]: https://github.com/nexuss0781/Intellectual-Cortex-Architecture/blob/main/Nexuss-Neural-Cognition/build_and_run.sh "Nexuss build workflow"
