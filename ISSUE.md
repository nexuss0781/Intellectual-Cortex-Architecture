# Phase 1 Mathematical Specification Issues — Technical Team 2

**Date:** 2024  
**Status:** CRITICAL — Specification Revision Required  
**Scope:** Units 6, 7, 9, 10 (4 of 5 components rejected)  
**Approved:** Unit 8 (CDD) only

---

## Executive Summary

Technical Team 2 has completed exhaustive mathematical proofing of Units 6–10. **Four of five specifications contain fundamental mathematical errors (L2 faults)** that prevent correct implementation. These are not transcription errors or edge-case oversights—they are contradictions, dimensional inconsistencies, and broken derivations in the source specifications themselves.

**Only Unit 8 (Coincidence Detection Dendrites) is mathematically sound and approved for implementation.**

This document details each L2 fault with:
1. The exact specification clause violated
2. The mathematical contradiction discovered
3. A counter-example demonstrating failure
4. Recommended revision direction for the Mathematics Team

---

## Unit 6: Spike-Phase Plasticity Filter (SPPF)

### Verdict: REJECTED

### L2 Fault #1: Theorem 1 Ignores Intra-Tick Conductance Decay

**Spec Location:** Section 3, Theorem 1, Lines 45–52  
**Claimed Property:** "A single coincident spike pair at t=0 produces Δg = κ_LTP × w_pre × w_post lasting until t=τ"

**Mathematical Contradiction:**

The spec defines conductance dynamics as:
```
g[t+1] = g[t] × (1 - dt/τ) + Δg_spike
```

However, Theorem 1's proof assumes:
```
g[t] = Δg_spike  for all t ∈ [0, τ]
```

This ignores the decay term `(1 - dt/τ)` applied every tick. For dt=1ms and τ=10ms:
- Tick 0: g[0] = Δg_spike
- Tick 1: g[1] = Δg_spike × 0.9  (10% loss)
- Tick 5: g[5] = Δg_spike × 0.59 (41% loss)
- Tick 10: g[10] = Δg_spike × 0.35 (65% loss)

**Counter-Example:**
```python
κ_LTP = 0.01, w_pre = 1.0, w_post = 1.0, τ = 10ms, dt = 1ms
Expected (per Theorem 1): g[t] = 0.01 for t ∈ [0, 10]
Actual (per dynamics):   g[5] = 0.0059, g[10] = 0.0035
Error: 65% deviation from claimed value
```

**Impact:** All downstream calculations relying on sustained conductance values (phase locking thresholds, plasticity integration windows) are invalid.

**Recommended Revision:**
1. Revise Theorem 1 to state the *integral* of conductance over time, not instantaneous value
2. Replace "lasting until t=τ" with "decaying exponentially with time constant τ"
3. Recalculate all threshold conditions using the decay-aware formula:
   ```
   g_effective = Δg_spike × Σ_{t=0}^{τ/dt} (1 - dt/τ)^t
               = Δg_spike × τ/dt × (1 - (1 - dt/τ)^{τ/dt})
   ```

---

## Unit 7: Gamma Phase Locking Oscillator (GPLO)

### Verdict: REJECTED

### L2 Fault #1: Pulsed Coupling Cannot Achieve Kuramoto Locking

**Spec Location:** Section 3, Theorem 2, Lines 78–85  
**Claimed Property:** "N oscillators with pulsed coupling K_pulse achieve phase lock within T_lock = π/(K_pulse × N)"

**Mathematical Contradiction:**

The spec adapts the continuous Kuramoto model:
```
dθ_i/dt = ω_i + (K/N) × Σ_j sin(θ_j - θ_i)  [Continuous]
```

But implements discrete pulsed coupling:
```
θ_i[t+1] = θ_i[t] + ω_i×dt + (K_pulse/N) × Σ_j sin(θ_j[t] - θ_i[t]) × δ(t mod T_pulse)
```

The Dirac delta `δ(t mod T_pulse)` means coupling occurs ONLY at pulse times. Between pulses, oscillators drift freely at their natural frequencies ω_i.

For locking to occur, the condition is:
```
|ω_i - ω_j| < K_effective
```

But with pulsed coupling:
```
K_effective = K_pulse × (dt / T_pulse)  [duty cycle reduction]
```

For T_pulse = 25ms (gamma period) and dt = 1ms:
```
K_effective = K_pulse × 0.04  (96% reduction!)
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

**Impact:** Gamma band synchrony cannot be maintained. The entire phase-coding mechanism fails.

**Recommended Revision:**
1. Replace pulsed coupling with continuous weak coupling
2. OR increase K_pulse by factor of 25× to compensate for duty cycle (but verify stability)
3. OR reduce T_pulse to match dt (continuous coupling), but this breaks gamma rhythm generation
4. Revise Theorem 2 to include the duty cycle factor: `T_lock = π × T_pulse / (K_pulse × dt × N)`

---

### L2 Fault #2: Phase Wrapping Boundary Condition Missing

**Spec Location:** Section 2.3, Equation 5, Lines 34–36  
**Issue:** No modulo-2π wrapping defined for phase updates

**Contradiction:**
```
θ[t+1] = θ[t] + ω×dt + coupling_term
```

Without wrapping:
- After 1000 ticks at ω=40Hz: θ = 40×1000×0.001 = 40 radians
- sin(θ) becomes numerically unstable for large θ
- Phase differences lose meaning

**Recommended Revision:**
Add explicit wrapping:
```
θ[t+1] = mod(θ[t] + ω×dt + coupling_term + π, 2π) - π
```

---

### L2 Fault #3: Order Parameter R Undefined for N < 3

**Spec Location:** Section 3, Definition 3, Lines 92–95  
**Claimed Property:** "R = |(1/N) × Σ_j exp(i×θ_j)| measures synchrony"

**Issue:** For N=1: R=1 always (trivial). For N=2: R=|cos((θ₁-θ₂)/2)| which has period π, not 2π.

**Impact:** Synchrony detection fails for small oscillator groups.

**Recommended Revision:**
Add constraint N ≥ 3, or define alternative synchrony metric for small N.

---

## Unit 9: Granule Binding Gate Neurons (GBGN)

### Verdict: REJECTED

### L2 Fault #1: Dimensional Inconsistency in Effective Voltage Equation

**Spec Location:** Section 3, Equation 8, Lines 67–69  
**Equation:** `V_eff = V + γ_bind × π × g_bind × R_m`

**Dimensional Analysis:**
- V: mV ✓
- γ_bind: dimensionless ✓
- π: dimensionless ✓
- g_bind: nS (nanosiemens)
- R_m: MΩ (megaohms)

Product: `nS × MΩ = 10⁻⁹ S × 10⁶ Ω = 10⁻³` (dimensionless factor)

But voltage requires: `Current × Resistance = (Conductance × Voltage) × Resistance`

The equation computes: `mV + (dimensionless) × (dimensionless) = mV` ✗

**Missing Term:** The binding current should be `I_bind = g_bind × (V - E_bind)`, then `V_drop = I_bind × R_m`.

**Correct Form:**
```
V_eff = V + γ_bind × π × g_bind × (V - E_bind) × R_m
```

Units: `mV + (1) × (1) × (nS) × (mV) × (MΩ) = mV + mV` ✓

**Impact:** All voltage calculations are off by factor of `(V - E_bind) ≈ 70mV`.

---

### L2 Fault #2: Binding Current Polarity Reversed

**Spec Location:** Section 2.2, Equation 2, Lines 28–30  
**Equation:** `I_total = ... + π × g_bind × (V - E_bind)`

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

**Spec Location:** Section 3, Theorem 1, Lines 78–82  
**Claimed Property:** "g_bind ≥ 3.0 nS guarantees spike within 1 tick"

**Analysis:**
From corrected Equation 8:
```
ΔV = γ_bind × π × g_bind × (E_bind - V) × R_m
```

With parameters:
- γ_bind = 0.8, π = 1.0, g_bind = 3.0 nS
- E_bind - V = 70 mV, R_m = 100 MΩ

```
ΔV = 0.8 × 1.0 × 3.0 × 70 × 100 = 16,800 mV = 16.8 V
```

This is **physically impossible** (biological neurons saturate at ~+50 mV).

**Actual Required g_bind:**
To reach threshold from rest:
```
V_thresh - V_rest = -55 - (-70) = 15 mV
15 = 0.8 × 1.0 × g_bind × 70 × 100
g_bind = 15 / 5600 = 0.0027 nS
```

**Discrepancy:** Spec claims 3.0 nS needed; actual requirement is 0.0027 nS (1000× smaller).

**Impact:** Spec threshold is wildly conservative, causing missed binding opportunities.

**Recommended Revision:**
1. Fix Equation 2 sign: `I = g × (E_rev - V)`
2. Fix Equation 8 dimensions: include `(E_bind - V)` term
3. Recalculate Theorem 1 threshold using correct dynamics including membrane decay

---

## Unit 10: Dynamic Pattern Separation Gate (DPSG)

### Verdict: REJECTED

### L2 Fault #1: Zero Output Activity Due to Threshold Cascade

**Spec Location:** Section 2.4, Algorithm 1, Lines 52–68  
**Parameters:**
- θ_active = 0.3 (activation threshold)
- θ_sep = 0.7 (separation threshold)
- w_max = 1.0 (max weight)
- sparsity_target = 0.1

**Algorithm Flow:**
1. Compute activity: `a_i = Σ_j w_ij × x_j`
2. Apply activation: `active[i] = (a_i > θ_active)`
3. Apply separation: if `active[i] && active[j] && similarity(i,j) > θ_sep`: suppress lower-weight neuron

**Cascade Failure:**
- With sparse inputs (sparsity_target = 0.1), average input has 10% active bits
- Expected activity: `a_i ≈ 0.1 × w_avg ≈ 0.05` (assuming w_avg ≈ 0.5)
- Activation threshold: `θ_active = 0.3`
- Result: `a_i < θ_active` for nearly all neurons → **zero activations**

**Counter-Example:**
```python
Input: 10-bit vector with 1 active bit (10% sparse)
Weights: uniform w_ij = 0.5
Activity: a_i = 0.5 × 1 = 0.5 for connected neurons, 0 otherwise
Activation: 0.5 > 0.3 ✓ (some activate)

But with realistic connectivity (each neuron connects to 20% of inputs):
P(connected) = 0.2
Expected a_i = 0.2 × 0.5 × 1 = 0.1
Activation: 0.1 < 0.3 ✗ (none activate)
```

**Impact:** Gate produces zero output. Pattern separation never occurs.

**Recommended Revision:**
1. Make θ_active adaptive: `θ_active = k × mean(a_i)` where k ≈ 0.5
2. OR reduce θ_active to 0.05–0.1 range
3. OR redefine activity normalization (currently undefined whether raw sum or normalized)

---

### L2 Fault #2: Similarity Metric Undefined for Empty Sets

**Spec Location:** Section 2.3, Equation 6, Lines 45–47  
**Equation:** `similarity(i,j) = |active_i ∩ active_j| / |active_i ∪ active_j|` (Jaccard index)

**Issue:** When both neurons have zero activity:
```
active_i = ∅, active_j = ∅
similarity = |∅| / |∅| = 0/0 = UNDEFINED
```

**Impact:** Division-by-zero error. Suppression logic crashes.

**Recommended Revision:**
Define `similarity(∅, ∅) = 0` (no overlap implies no similarity), or add guard:
```
if |active_i ∪ active_j| == 0: return 0
```

---

### L2 Fault #3: Weight Update Rule Violates Boundedness

**Spec Location:** Section 2.5, Equation 9, Lines 78–81  
**Equation:** `w_ij[t+1] = w_ij[t] + η × (pre_i × post_j - w_ij[t])`

**Analysis:**
This is a standard Oja-like rule, BUT the spec constrains `w_ij ∈ [0, w_max]`.

The update can produce negative weights:
```
If pre_i × post_j = 0 and w_ij > 0:
Δw = -η × w_ij < 0
w_new = w_ij - η × w_ij = w_ij × (1 - η)
```

For η > 1: `w_new < 0` (violates bound)

**Counter-Example:**
```python
w_ij = 0.5, η = 1.5, pre_i × post_j = 0
w_new = 0.5 × (1 - 1.5) = -0.25  ✗
```

**Impact:** Weights become negative, breaking non-negativity constraint required for interpretation as "connection strength."

**Recommended Revision:**
1. Constrain η ≤ 1.0
2. OR add explicit clipping: `w_ij = clamp(w_new, 0, w_max)`
3. OR use multiplicative update: `w_new = w_ij × (1 + η × (pre×post/w_ij - 1))`

---

### L2 Fault #4: Separation Guarantee Requires Unstated Assumptions

**Spec Location:** Section 3, Theorem 2, Lines 95–102  
**Claimed Property:** "Output patterns have pairwise similarity ≤ θ_sep"

**Hidden Assumption:** Theorem assumes sufficient iterations of the suppression loop. But Algorithm 1 runs suppression for exactly 1 pass.

**Counter-Example:**
```python
Three neurons A, B, C all mutually similar (> θ_sep)
Weights: w_A > w_B > w_C

Pass 1:
- Compare(A,B): suppress B (B inactive)
- Compare(A,C): suppress C (C inactive)
- Compare(B,C): skipped (B already inactive)

Result: Only A active. Similarity undefined (singleton set).

But what if suppression order differs?
- Compare(B,C) first: suppress C
- Compare(A,B) second: suppress B
Result: Only A active. Same outcome.

Now try different weights: w_B > w_A > w_C
- Compare(A,B): suppress A
- Compare(B,C): suppress C
Result: Only B active.

In all cases, we get singleton output. This trivially satisfies similarity ≤ θ_sep (no pairs exist).

But the spec claims "rich, separated representations," implying multiple active neurons.
The theorem is vacuously true but practically useless.
```

**Impact:** The separation guarantee is mathematically correct but trivial—it produces minimal (often singleton) outputs.

**Recommended Revision:**
1. Add constraint: "suppression only if alternative exists with similarity < θ_sep"
2. OR limit suppression to top-K most-similar pairs
3. OR revise theorem to state: "Output has minimum cardinality K while maintaining pairwise similarity ≤ θ_sep"

---

## Unit 8: Coincidence Detection Dendrites (CDD)

### Verdict: APPROVED ✅

**No L2/L3 faults found.** All mathematical formulations are:
- Dimensionally consistent
- Stable under adversarial inputs
- Bounded in state space
- O(1) complexity per tick
- Correctly handle edge cases (NaN, Inf, empty buffers)

**Tested Invariants:**
1. Label matching is exact (56-bit equality)
2. Temporal window is strictly enforced (Δt ≤ 2ms)
3. Buffer eviction is FIFO-correct
4. Conductance formula produces bounded output
5. No complexity violations (≤20 operations/tick confirmed)

**Ready for implementation.**

---

## Summary Table

| Unit | Component | Verdict | L2 Faults | Criticality |
|------|-----------|---------|-----------|-------------|
| 6 | SPPF | REJECTED | 1 | HIGH — Decay ignored breaks all timing calcs |
| 7 | GPLO | REJECTED | 3 | CRITICAL — Locking impossible, phase unwrapped |
| 8 | CDD | **APPROVED** | 0 | Ready for implementation |
| 9 | GBGN | REJECTED | 3 | CRITICAL — Polarity reversed, dimensions wrong |
| 10 | DPSG | REJECTED | 4 | HIGH — Zero output, undefined ops, unbounded weights |

---

## Recommended Actions for Mathematics Team

### Immediate (Blocker for Implementation):

1. **Unit 7 (GPLO):** Redesign coupling mechanism. Pulsed Kuramoto is fundamentally broken for gamma frequencies. Consider:
   - Continuous weak coupling
   - Alternative oscillator models (Winfree, Stuart-Landau)
   - Explicit phase-resetting curves (PRCs)

2. **Unit 9 (GBGN):** Fix sign error in Equation 2 and dimensional error in Equation 8. Recalibrate all thresholds.

3. **Unit 10 (DPSG):** Redesign activation threshold mechanism. Current cascade produces zero output. Add bounds checking to weight updates.

### High Priority:

4. **Unit 6 (SPPF):** Revise Theorem 1 to account for exponential decay. Recalculate all derived thresholds.

5. **Unit 7 (GPLO):** Add phase wrapping and small-N handling.

6. **Unit 10 (DPSG):** Define behavior for empty sets and add non-trivial separation guarantees.

---

## Process Notes

- All proofs implemented in Python with deterministic computation graphs
- Fault attribution followed strict L0→L3 hierarchy
- No speculative fixes applied to specifications
- Counter-examples are reproducible via proof suites in `/workspace/PROOF/phase-1/`

**Next Steps:** Mathematics Team to issue revised specifications (v2) addressing all L2 faults. Technical Team 2 will re-run proof suite on v2 specs before implementation begins.

---

**Prepared by:** Technical Team 2 — Elite Mathematical Specialists  
**Review Status:** Pending Mathematics Team Revision
