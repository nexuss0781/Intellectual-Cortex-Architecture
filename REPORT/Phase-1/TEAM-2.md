# Phase 1 Mathematical Viability & Stability Proof — Technical Team 2

## Report Scope: Units 6–10 (Complete Phase 1 Assessment)

---

## Executive Summary

This report documents the complete mathematical proofing results for **Units 6–10** of Phase 1. All five sub-components have been exhaustively tested against their specifications using deterministic Python proof suites.

**Overall Verdict:** 4 of 5 units REJECTED due to fundamental mathematical formulation errors (L2 faults).

| Unit | Component | Verdict | L2 Faults | Criticality |
|------|-----------|---------|-----------|-------------|
| 6 | SPPF | REJECTED | 1 | HIGH — Decay ignored breaks timing calcs |
| 7 | GPLO | REJECTED | 3 | CRITICAL — Locking impossible |
| 8 | CDD | **APPROVED** | 0 | Ready for implementation |
| 9 | GBGN | REJECTED | 3 | CRITICAL — Polarity reversed |
| 10 | DPSG | REJECTED | 4 | HIGH — Zero output activity |

**Only Unit 8 (Coincidence Detection Dendrites) is mathematically sound and approved for implementation.**

A detailed issue specification document (`ISSUE.md`) has been prepared for the Mathematics Team to revise the rejected units.

---

## Unit 6: Semantic Pointer Projection Fibers (SPPF)

**Spec File:** `SPEC/phase-1/unit-6_subcomponent-1.3_Semantic-Pointer-Projection-Fibers-(SPPF).md`  
**Proof File:** `PROOF/phase-1/unit_6.py` (14 tests)

### Verdict: REJECTED (L2 Fault Detected)

### Summary

The SPPF specification is mathematically sound in structure but contains a critical error in Theorem 1's proof. The core relay dynamics are correctly specified, but the claimed threshold margin is incorrect due to ignored intra-tick decay.

### L2 Fault: Theorem 1 Proof Error — Intra-Tick Conductance Decay Ignored

**Location:** Spec Section 3.1, Theorem 1 (Guaranteed Relay Firing), Proof paragraph.

**Problem:** The spec's proof assumes conductance remains constant during the tick, but Eq. 2 specifies decay BEFORE computing synaptic current:

```
g[t+1] = g[t] × exp(-dt/τ) + Δg_spike
```

For w_in = 5.0 nS, τ_exc = 5 ms, dt = 1 ms:
- g_after_decay = 5.0 × e^(-0.2) = 4.094 nS
- I_syn = 4.094 × 70 = 286.6 pA
- dV = 0.05 × 286.6 = 14.33 mV
- V_new = -70 + 14.33 = -55.67 mV

Since -55.67 < -55.0 (threshold), the relay does **NOT** fire at w_in = 5.0 nS.

**Minimum Required Weight:** w_min = 15 / (0.05 × e^(-0.2) × 70) = **5.235 nS**

**Impact:** The spec claims w_in = 5.0 nS suffices. This is mathematically incorrect. The actual minimum is ~5.24 nS (4.7% higher). All downstream threshold calculations are affected.

**Recommended Revision:** Revise Theorem 1 to state the *effective* conductance after decay, not the peak injected conductance. Recalculate all thresholds using:
```
g_effective = g_peak × exp(-dt/τ)
```

---

## Unit 7: Gamma Phase Locking Oscillators (GPLO)

**Spec File:** `SPEC/phase-1/unit-7_subcomponent-2.2_Gamma-Phase-Locking-Oscillators-(GPLO).md`  
**Proof File:** `PROOF/phase-1/unit_7.py` (8 tests)

### Verdict: REJECTED (Multiple L2 Faults Detected)

### Summary

The GPLO specification contains fundamental mathematical errors in its phase-locking formulation. The Kuramoto-based locking mechanism, as discretized and pulsed, fails to achieve any of the claimed convergence bounds.

### L2 Fault #1: Pulsed Coupling Cannot Achieve Kuramoto Locking

**Location:** Spec Section 3, Theorem 2, Lines 78–85.

**Claimed Property:** "N oscillators with pulsed coupling K_pulse achieve phase lock within T_lock = π/(K_pulse × N)"

**Mathematical Contradiction:**

The spec adapts the continuous Kuramoto model but implements discrete pulsed coupling:
```
θ_i[t+1] = θ_i[t] + ω_i×dt + (K_pulse/N) × Σ_j sin(θ_j[t] - θ_i[t]) × δ(t mod T_pulse)
```

The Dirac delta means coupling occurs ONLY at pulse times (every 25 ms). Between pulses, oscillators drift freely.

**Effective coupling strength:**
```
K_effective = K_pulse × (dt / T_pulse) = K_pulse × 0.04  (96% reduction!)
```

**Counter-Example:**
```python
N = 8 oscillators, ω_i ~ Uniform(39, 41) Hz (Δω = 2 Hz)
K_pulse = 0.5 (as specified)
T_pulse = 25ms, dt = 1ms

Required: K_effective > Δω/2π = 0.32 Hz
Actual:   K_effective = 0.5 × 0.04 = 0.02 Hz

Result: Oscillators drift apart by 1.98 Hz per second
Time to decoherence: ~0.5 seconds
```

**Observed:** After 100+ ticks, max phase error = 3.07 rad (near π, complete loss of lock)

**Impact:** Gamma band synchrony cannot be maintained. The entire phase-coding mechanism fails.

**Recommended Revision:** Replace pulsed coupling with continuous weak coupling, OR increase K_pulse by 25× to compensate for duty cycle (but verify stability), OR revise Theorem 2 to include duty cycle factor: `T_lock = π × T_pulse / (K_pulse × dt × N)`

---

### L2 Fault #2: Phase Wrapping Boundary Condition Missing

**Location:** Spec Section 2.3, Equation 5, Lines 34–36.

**Issue:** No modulo-2π wrapping defined for phase updates.

**Contradiction:** Without wrapping, after 1000 ticks at ω=40Hz: θ = 40 radians. The sin(θ) function becomes numerically unstable for large arguments, and phase differences lose meaning.

**Recommended Revision:** Add explicit wrapping:
```
θ[t+1] = mod(θ[t] + ω×dt + coupling_term + π, 2π) - π
```

---

### L2 Fault #3: Order Parameter R Undefined for N < 3

**Location:** Spec Section 3, Definition 3, Lines 92–95.

**Claimed Property:** "R = |(1/N) × Σ_j exp(i×θ_j)| measures synchrony"

**Issue:** For N=1: R=1 always (trivial). For N=2: R=|cos((θ₁-θ₂)/2)| which has period π, not 2π.

**Impact:** Synchrony detection fails for small oscillator groups.

**Recommended Revision:** Add constraint N ≥ 3, or define alternative synchrony metric for small N.

---

## Unit 8: Coincidence Detection Dendrites (CDD)

**Spec File:** `SPEC/phase-1/unit-8_subcomponent-2.3_Coincidence-Detection-Dendrites-(CDD).md`  
**Proof File:** `PROOF/phase-1/unit_8.py` (22 tests)

### Verdict: APPROVED ✅

### Summary

The CDD specification is mathematically correct and stable. All 22 tests passed with no L2/L3 faults detected.

**Tested and Verified Invariants:**
1. Exact 56-bit semantic label matching (bitwise equality)
2. Temporal coincidence detection with Δt ≤ 2ms window (strict enforcement)
3. Proper buffer management (4 slots, FIFO eviction policy)
4. Multiplicative conductance generation: Δg = κ_bind × w₁×w₂ / w_scale (bounded output)
5. O(1) complexity per tick (≤20 operations confirmed)
6. State boundedness over extended simulation (1000+ ticks)
7. Dimensional consistency throughout all equations
8. Edge case handling (NaN, Inf, empty buffers, boundary values)

**Stability Summary:**
- No numerical instability under adversarial inputs
- No unbounded growth in any state variable
- Deterministic behavior across all test conditions

**Status:** Ready for implementation.

---

## Unit 9: Granule Binding Gate Neurons (GBGN)

**Spec File:** `SPEC/phase-1/unit-9_subcomponent-2.1_Granule-Binding-Gate-Neurons-(GBGN).md`  
**Proof File:** `PROOF/phase-1/unit_9.py` (18 tests)

### Verdict: REJECTED (Multiple L2 Faults Detected)

### Summary

The GBGN specification contains three critical dimensional and polarity errors that fundamentally break the binding mechanism.

### L2 Fault #1: Dimensional Inconsistency in Equation 8

**Location:** Spec Section 3, Equation 8, Lines 67–69.

**Formula:** `V_eff = V + γ_bind × π × g_bind × R_m`

**Dimensional Analysis:**
- V: [mV] ✓
- γ_bind: dimensionless ✓
- π: dimensionless ✓
- g_bind: [nS] = [10⁻⁹ S]
- R_m: [MΩ] = [10⁶ Ω]

Product: `nS × MΩ = 10⁻⁹ × 10⁶ = 10⁻³` (dimensionless ratio)

**Result:** `mV + (dimensionless) = INVALID` — cannot add voltage to dimensionless quantity.

**Missing Term:** The binding current should be `I_bind = g_bind × (V - E_bind)`, then `V_drop = I_bind × R_m`.

**Correct Form:**
```
V_eff = V + γ_bind × π × g_bind × (V - E_bind) × R_m
```

Units: `mV + (1) × (1) × (nS) × (mV) × (MΩ) = mV + mV` ✓

**Impact:** All voltage calculations are off by factor of `(V - E_bind) ≈ 70 mV`.

---

### L2 Fault #2: Binding Current Polarity Reversed

**Location:** Spec Section 2.2, Equation 2, Lines 28–30.

**Formula:** `I_total = ... + π × g_bind × (V - E_bind)`

**Given Parameters:**
- E_bind = 0 mV (reversal potential, excitatory)
- V_rest = -70 mV
- g_bind > 0 (conductance)

**Calculation:**
```
I_bind = π × g_bind × (-70 - 0) = -70 × π × g_bind  (NEGATIVE)
```

Negative current is **hyperpolarizing** (outward positive charge flow).

**Contradiction:** The spec states binding is "excitatory" and should depolarize the neuron toward threshold. But the equation produces hyperpolarization.

**Root Cause:** Sign error in Ohm's law. Correct form is:
```
I = g × (E_rev - V)  [NOT g × (V - E_rev)]
```

**Correct Form:**
```
I_bind = π × g_bind × (E_bind - V) = π × g_bind × (0 - (-70)) = +70 × π × g_bind
```

**Impact:** Binding **inhibits** instead of excites. Gateway neurons never fire from binding input.

---

### L2 Fault #3: Threshold Discrepancy in Theorem 1

**Location:** Spec Section 3, Theorem 1, Lines 78–82.

**Claimed Property:** "g_bind ≥ 3.0 nS guarantees spike within 1 tick"

**Analysis:** From corrected Equation 8:
```
ΔV = γ_bind × π × g_bind × (E_bind - V) × R_m
```

With parameters: γ_bind = 0.8, π = 1.0, g_bind = 3.0 nS, E_bind - V = 70 mV, R_m = 100 MΩ

```
ΔV = 0.8 × 1.0 × 3.0 × 70 × 100 = 16,800 mV = 16.8 V
```

This is **physically impossible** (biological neurons saturate at ~+50 mV).

**Actual Required g_bind:** To reach threshold from rest:
```
V_thresh - V_rest = -55 - (-70) = 15 mV
15 = 0.8 × 1.0 × g_bind × 70 × 100
g_bind = 15 / 5600 = 0.0027 nS
```

**Discrepancy:** Spec claims 3.0 nS needed; actual requirement is 0.0027 nS (**1000× smaller**).

**Impact:** Spec threshold is wildly conservative, causing missed binding opportunities.

**Recommended Revision:**
1. Fix Equation 2 sign: `I = g × (E_rev - V)`
2. Fix Equation 8 dimensions: include `(E_bind - V)` term
3. Recalculate Theorem 1 threshold using correct dynamics including membrane decay

---

## Unit 10: Dentate Pattern Separation Gating (DPSG)

**Spec File:** `SPEC/phase-1/unit-10_subcomponent-3.3_Dentate-Pattern-Separation-Gating-(DPSG).md`  
**Proof File:** `PROOF/phase-1/unit_10.py` (19 tests)

### Verdict: REJECTED (Multiple L2 Faults Detected)

### Summary

The DPSG specification contains four fundamental errors that completely break pattern separation functionality.

**Test Results:** 11 passed, 8 failed out of 19 tests

### L2 Fault #1: Zero Output Activity Due to Threshold Cascade

**Location:** Spec Section 2.4, Algorithm 1, Lines 52–68.

**Parameters:**
- θ_active = 0.3 (activation threshold)
- θ_sep = 0.7 (separation threshold)
- w_max = 1.0 (max weight)
- sparsity_target = 0.1

**Algorithm Flow:**
1. Compute activity: `a_i = Σ_j w_ij × x_j`
2. Apply activation: `active[i] = (a_i > θ_active)`
3. Apply separation: suppress lower-weight neuron if similarity > θ_sep

**Cascade Failure:**
- With sparse inputs (sparsity_target = 0.1), average input has 10% active bits
- Expected activity: `a_i ≈ 0.1 × w_avg ≈ 0.05` (assuming w_avg ≈ 0.5)
- Activation threshold: `θ_active = 0.3`
- Result: `a_i < θ_active` for nearly all neurons → **zero activations**

**Counter-Example:**
```python
Input: 10-bit vector with 1 active bit (10% sparse)
Weights: uniform w_ij = 0.5
Connectivity: each neuron connects to 20% of inputs

Expected a_i = 0.2 × 0.5 × 1 = 0.1
Activation: 0.1 < 0.3 ✗ (none activate)
```

**Observed output density:** 0% across all input densities (tested 0.01 to 0.5)

**Impact:** Gate produces zero output. Pattern separation never occurs.

**Recommended Revision:** Make θ_active adaptive: `θ_active = k × mean(a_i)` where k ≈ 0.5, OR reduce θ_active to 0.05–0.1 range, OR redefine activity normalization.

---

### L2 Fault #2: Similarity Metric Undefined for Empty Sets

**Location:** Spec Section 2.3, Equation 6, Lines 45–47.

**Equation:** `similarity(i,j) = |active_i ∩ active_j| / |active_i ∪ active_j|` (Jaccard index)

**Issue:** When both neurons have zero activity:
```
active_i = ∅, active_j = ∅
similarity = |∅| / |∅| = 0/0 = UNDEFINED
```

**Impact:** Division-by-zero error. Suppression logic crashes.

**Recommended Revision:** Define `similarity(∅, ∅) = 0`, or add guard: `if |active_i ∪ active_j| == 0: return 0`

---

### L2 Fault #3: Weight Update Rule Violates Boundedness

**Location:** Spec Section 2.5, Equation 9, Lines 78–81.

**Equation:** `w_ij[t+1] = w_ij[t] + η × (pre_i × post_j - w_ij[t])`

**Analysis:** The spec constrains `w_ij ∈ [0, w_max]`, but the update can produce negative weights:

```
If pre_i × post_j = 0 and w_ij > 0:
Δw = -η × w_ij < 0
w_new = w_ij × (1 - η)
```

For η > 1: `w_new < 0` (violates bound)

**Counter-Example:**
```python
w_ij = 0.5, η = 1.5, pre_i × post_j = 0
w_new = 0.5 × (1 - 1.5) = -0.25  ✗
```

**Impact:** Weights become negative, breaking non-negativity constraint required for interpretation as "connection strength."

**Recommended Revision:** Constrain η ≤ 1.0, OR add explicit clipping: `w_ij = clamp(w_new, 0, w_max)`

---

### L2 Fault #4: Separation Guarantee Requires Unstated Assumptions

**Location:** Spec Section 3, Theorem 2, Lines 95–102.

**Claimed Property:** "Output patterns have pairwise similarity ≤ θ_sep"

**Hidden Assumption:** Theorem assumes sufficient iterations of the suppression loop. But Algorithm 1 runs suppression for exactly 1 pass.

**Counter-Example:**
```python
Three neurons A, B, C all mutually similar (> θ_sep)
Weights: w_A > w_B > w_C

Pass 1:
- Compare(A,B): suppress B
- Compare(A,C): suppress C
Result: Only A active (singleton set)
```

In all cases, we get singleton output. This trivially satisfies similarity ≤ θ_sep (no pairs exist), but the spec claims "rich, separated representations," implying multiple active neurons.

**Impact:** The separation guarantee is mathematically correct but **vacuously true**—it produces minimal (often singleton) outputs, not rich representations.

**Recommended Revision:** Add constraint: "suppression only if alternative exists with similarity < θ_sep", OR limit suppression to top-K most-similar pairs, OR revise theorem to state: "Output has minimum cardinality K while maintaining pairwise similarity ≤ θ_sep"

---

## Cross-Unit Analysis

### Common Patterns of Failure

**Pattern 1: Discrete-Time Effects Ignored**
- Unit 6: Intra-tick conductance decay ignored in theorem proof
- Unit 7: Continuous Kuramoto theory misapplied to pulsed discrete system
- Unit 10: Parameter interactions in discrete time not analyzed (threshold cascade)

**Pattern 2: Dimensional Inconsistencies**
- Unit 9: Voltage equation mixes units incorrectly (mV + dimensionless)
- Unit 9: Current polarity sign error (hyperpolarizing instead of excitatory)

**Pattern 3: Parameter Cascades**
- Unit 7: Effective coupling strength reduced 96% by duty cycle
- Unit 9: Threshold off by 1000× due to missing voltage term
- Unit 10: Activation threshold too high for sparse inputs

**Pattern 4: Overclaimed Theorems**
- All rejected units have theorems with proofs that do not match governing equations
- Vacuous truths presented as meaningful guarantees (Unit 10)

**Pattern 5: Boundary Conditions Missing**
- Unit 7: No phase wrapping (unbounded growth)
- Unit 7: Order parameter undefined for N < 3
- Unit 10: Jaccard index undefined for empty sets

### Complexity Compliance

All five units correctly specify O(1) per-neuron complexity. **No L3 faults detected.**

---

## Summary of Findings (All Units 6–10)

| Unit | Component | Verdict | Fault Level | Description |
|------|-----------|---------|-------------|-------------|
| 6 | SPPF | REJECTED | L2 | Theorem 1 proof ignores intra-tick decay |
| 7 | GPLO | REJECTED | L2 (×3) | Phase locking broken; convergence claims unsound; missing boundaries |
| 8 | CDD | **APPROVED** | — | Mathematically correct and stable |
| 9 | GBGN | REJECTED | L2 (×3) | Dimensional errors; polarity reversed; threshold discrepancy |
| 10 | DPSG | REJECTED | L2 (×4) | Pattern separation broken; zero output; undefined ops; unbounded weights; vacuous theorem |

---

## Recommendations

### Priority 1: Critical Mathematical Errors (Blockers)

1. **Unit 9 (GBGN):** Fix dimensional analysis in Eq. 8 and current polarity in Eq. 2 immediately. These are sign and unit errors that completely invert intended behavior.

2. **Unit 7 (GPLO):** Complete reformulation of phase-locking mechanism required. Pulsed Kuramoto cannot achieve gamma-band synchrony. Consider:
   - Continuous weak coupling
   - Alternative oscillator models (Winfree, Stuart-Landau)
   - Explicit phase-resetting curves (PRCs)

3. **Unit 10 (DPSG):** Full parameter revision needed. Current configuration produces zero output. Redesign activation threshold mechanism and add bounds checking to weight updates.

### Priority 2: Proof Corrections (High)

4. **Unit 6 (SPPF):** Update Theorem 1 proof to include decay term. Recalculate all derived thresholds using `g_effective = g_peak × exp(-dt/τ)`.

5. **Unit 7 (GPLO):** Add phase wrapping boundary condition and small-N handling for order parameter.

6. **Unit 10 (DPSG):** Define behavior for empty sets in similarity metric and add non-trivial separation guarantees (minimum cardinality constraints).

### Process Recommendation

**All L2 faults must be addressed by the Mathematics Team before implementation proceeds.** Technical Team 2 will re-run the complete proof suite on revised specifications (v2) before any code development begins.

---

## Conclusion

Of the five sub-components assessed (Units 6–10), only **Unit 8 (CDD)** passes mathematical validation. The remaining four units contain fundamental specification errors that would cause catastrophic failure if implemented as written.

**Critical insight:** These are **mathematical formulation errors** in the specifications themselves (L2 faults), not implementation bugs or transcription errors. The specs must be corrected before implementation can proceed.

**Recommendation:** Proceed to Phase 2 implementation **ONLY for Unit 8 (CDD)**. Halt all other development until L2 faults are addressed through specification revision.

A comprehensive issue document (`ISSUE.md`) has been prepared with detailed counter-examples and recommended revisions for submission to the Mathematics Team.

---

**Report Date:** 2024  
**Author:** Technical Team 2 — Elite Mathematical Specialists  
**Classification:** Internal Engineering Document  
**Review Status:** Pending Mathematics Team Revision (v2 specs required)
