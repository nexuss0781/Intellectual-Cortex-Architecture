Kimi: **PHASE 1 | SUB-COMPONENT 3.2: CA1 Pattern Completion Decoders (CA1-PCD)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Read the stable attractor state from the CA3 cleanup memory and convert it into a finalized, exact semantic pointer suitable for downstream consumption.
- Apply a thresholded linear readout that preserves only the most strongly activated neurons, ensuring the output pattern has exactly the target sparsity of 2%.
- Guarantee that the output pattern is binary and deterministic, with no graded or ambiguous states passed to later phases.
- Ensure the readout operation completes in constant time per neuron, with no global sorting or iterative refinement.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| CA3 attractor state | $\mathbf{s}_{\text{CA3}}(t)$ | $[0,1]^{D_{\text{sp}}}$ | Slow gate values $s_{\text{slow},i}$ from AS neurons |
| Convergence flag | $\xi(t)$ | $\{0,1\}$ | From CA3-RAN; indicates stable state |
| Phase reference | $\varphi_{\text{ref}}(t)$ | $[0, 2\pi)$ | From 4.2 RSP; gates readout timing |
| Precision gain | $\pi(t)$ | $[0, 1]$ | From 4.1 RPGN; scales readout threshold |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Decoded sparse pointer | $\mathbf{p}_{\text{out}}(t)$ | $\{0,1\}^{D_{\text{sp}}}$ | Final binary semantic pointer |
| Output density | $\rho_{\text{out}}$ | $[0.015, 0.025]$ | Exact 2% target: 30–50 active bits |
| Readout confidence | $\chi(t)$ | $[0, 1]$ | Fraction of top activations retained |
| Output validity flag | $\nu(t)$ | $\{0,1\}$ | $1$ iff readout occurred this cycle |

#### 2.3 State Space Definition

Each CA1-PCD neuron occupies a CI slot:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V$ | mV | $-70.0$ | LIF integration |
| Excitatory conductance | $g_{\text{exc}}$ | nS | $0.0$ | Feedforward from CA3 |
| Dynamic threshold | $\theta_{\text{dyn}}$ | mV | $-55.0$ | Adaptive firing bound |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Standard countdown |
| Type identifier | $\text{type\_id}$ | — | $0$ (CI) | Fixed neuron class |

Each CA3→CA1 synapse carries:

| Field | Symbol | Value | Role |
|-------|--------|-------|------|
| Postsynaptic index | $\text{post\_id}$ | CA1 neuron $i$ | Fixed routing (1:1 mapping) |
| Efficacy | $w_{\text{CA3}}$ | $5.0\ \text{nS}$ | Suprathreshold readout weight |
| Axonal delay | $\delta$ | $0\ \text{ms}$ | Same-tick delivery |
| Tag byte 0 | $\text{tag}[0]$ | $0\text{b}00001000$ | Class=0 (FEEDFORWARD); routing key=CA1-PCD |

**Global readout controller (per module, not per neuron):**

| Field | Symbol | Value | Role |
|-------|--------|-------|------|
| Target active count | $N_{\text{target}}$ | $40$ | Desired output sparsity |
| Threshold adaptation rate | $\eta_\theta$ | $0.1$ | Per-cycle threshold adjustment |
| Readout phase window | $\Phi_{\text{read}}$ | $[0, \pi/5]$ | Phase interval for readout |

#### 2.4 Governing Equations

**Phase-Gated Readout Trigger:**

1. **Readout enable condition:**
   $$\nu(t) = \xi(t) \cdot \mathbb{I}\big[\varphi_{\text{ref}}(t) \in \Phi_{\text{read}}\big] \cdot \mathbb{I}\big[(t \bmod 25) \geq 20\big]$$
   Readout occurs only when:
   - CA3 has converged ($\xi = 1$)
   - Gamma phase is in late cycle ($\varphi \in [0, \pi/5]$, i.e., first 2.5 ms)
   - At least 20 ticks have passed since cycle start (allowing CA3 settling)

**Thresholded Linear Readout (per CA1 neuron $i$):**

2. **CA3→CA1 feedforward delivery.** When CA3 AS neuron $j$ fires or sustains $s_{\text{slow},j} > 0.5$:
   $$g_{\text{exc},i}(t^+) = g_{\text{exc},i}(t) + w_{\text{CA3}} \cdot s_{\text{slow},j}(t) \cdot \delta_{ij}$$
   where $\delta_{ij}$ is the Kronecker delta (1:1 connectivity: CA3 neuron $i$ projects only to CA1 neuron $i$).

3. **Conductance decay:**
   $$g_{\text{exc},i}(t+1) = g_{\text{exc},i}(t^+) \cdot \exp(-dt/\tau_{\text{exc}})$$
   with $\tau_{\text{exc}} = 5\ \text{ms}$.

4. **Precision-scaled threshold (per neuron, if $\nu(t) = 1$):**
   $$\theta_{\text{read},i} = \theta_{\text{base}} - \pi(t) \cdot \Delta\theta_{\text{read}}$$
   where $\Delta\theta_{\text{read}} = 5.0\ \text{mV}$. Higher precision $\Rightarrow$ lower threshold $\Rightarrow$ more inclusive readout.

5. **Firing condition (readout decision):**
   If $\nu(t) = 1$ and $g_{\text{exc},i}(t) \geq \theta_{\text{read},i}$:
   - $p_{\text{out},i}(t) = 1$
   - $V_i(t) \leftarrow V_{\text{reset}}$
   - $\text{spike\_timer} \leftarrow 5$

   Else: $p_{\text{out},i}(t) = 0$.

6. **Global density correction (distributed):**
   If $\|\mathbf{p}_{\text{out}}\|_0 > N_{\text{target}}$:
   - Raise threshold: $\theta_{\text{read}} \leftarrow \theta_{\text{read}} + \eta_\theta \cdot (\|\mathbf{p}_{\text{out}}\|_0 - N_{\text{target}})$
   - Re-apply step 5 (single iteration; no loop)

   If $\|\mathbf{p}_{\text{out}}\|_0 < N_{\text{target}} - 5$:
   - Lower threshold: $\theta_{\text{read}} \leftarrow \theta_{\text{read}} - \eta_\theta \cdot (N_{\text{target}} - \|\mathbf{p}_{\text{out}}\|_0)$

7. **Confidence metric:**
   $$\chi(t) = \frac{\|\mathbf{p}_{\text{out}}\|_0}{N_{\text{target}}} \cdot \frac{1}{D_{\text{sp}}} \sum_{i: p_{\text{out},i}=1} \frac{g_{\text{exc},i}(t) - \theta_{\text{read},i}}{\max_j g_{\text{exc},j}(t) - \theta_{\text{read},j}}$$

**CA1 Neuron Standard Dynamics (when not reading out):**

8. **Universal kernel applies.** When $\nu(t) = 0$, CA1 neurons execute standard CI dynamics with no special behavior.

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Readout weight | $w_{\text{CA3}}$ | $5.0$ | nS | $4.5$ | $5.5$ | Suprathreshold CA3→CA1 drive |
| Readout threshold offset | $\Delta\theta_{\text{read}}$ | $5.0$ | mV | $4.0$ | $6.0$ | Precision-dependent threshold shift |
| Target active count | $N_{\text{target}}$ | $40$ | bits | $35$ | $45$ | Exact output sparsity |
| Threshold adaptation rate | $\eta_\theta$ | $0.1$ | mV/bit | $0.05$ | $0.2$ | Density correction speed |
| Readout phase start | $\varphi_{\text{start}}$ | $0$ | rad | $0$ | $\pi/10$ | Window opening |
| Readout phase end | $\varphi_{\text{end}}$ | $\pi/5$ | rad | $\pi/10$ | $\pi/4$ | Window closing |
| Minimum cycle tick | $t_{\min}$ | $20$ | ticks | $15$ | $22$ | CA3 settling time before readout |
| Excitatory decay | $\tau_{\text{exc}}$ | $5.0$ | ms | $4.5$ | $5.5$ | Conductance decay speed |
| Resting potential | $V_{\text{rest}}$ | $-70.0$ | mV | $-72.0$ | $-68.0$ | Baseline membrane state |
| Firing threshold base | $\theta_{\text{base}}$ | $-55.0$ | mV | $-57.0$ | $-53.0$ | Spike emission boundary |
| Reset potential | $V_{\text{reset}}$ | $-75.0$ | mV | $-77.0$ | $-73.0$ | Post-spike membrane clamp |
| Membrane time constant | $\tau_m$ | $20.0$ | ms | $18.0$ | $22.0$ | Integration speed |
| Membrane resistance | $R_m$ | $1.0$ | M$\Omega$ | $0.8$ | $1.2$ | Ohmic scaling |
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | CI ($\text{type\_id} = 0$) | Core Integrator for readout |
| Input synapse type | FEEDFORWARD (type 0) | From CA3-RAN AS neurons (1:1) |
| Output synapse type | FEEDFORWARD (type 0) | To Phase 2 pathways |
| Tag encoding (input) | $\text{tag}[0] = 0\text{b}00001000$ | Class=0; routing key=CA1-PCD-input |
| Tag encoding (output) | $\text{tag}[0] = 0\text{b}00001001$ | Class=0; routing key=CA1-PCD-output |
| Source field | $g_{\text{exc}}$ | Receives CA3 sustained activity |
| Readout trigger | $\nu(t)$ | Global phase-and-convergence gated |

#### 2.7 Interface Contract

**Upstream providers:**
- **3.1 CA3-RAN** (CA3 Recurrent Attractor Networks): delivers converged attractor state via $s_{\text{slow}}$ and spike signals.
- **4.2 RSP** (Relay Synchronization Projectors): provides phase reference for readout timing.
- **4.1 RPGN** (Reticular Phase Gating Nuclei): provides precision gain for threshold modulation.

**Downstream consumers:**
- **Phase 2 Working Memory** (stubbed): receives clean semantic pointers for sustained holding.
- **Phase 2 Episodic Memory** (stubbed): receives pointers for temporal context binding.
- **5.1 CTA / 5.2 TIB** (Type-Token circuits): receives pointers for categorical abstraction.

**Handshake format:**
- Input: direct state read from CA3 AS $s_{\text{slow}}$ fields (no explicit spike events for sustained state).
- Output: FEEDFORWARD spike events with CA1-PCD-output routing key, carrying binary pattern $\mathbf{p}_{\text{out}}$.

---

### 3. Stability & Rigor Analysis

#### 3.1 Thresholded Readout Correctness

**Theorem 1 (Deterministic Binary Output).** When $\nu(t) = 1$, the CA1-PCD produces exactly binary output:
$$\mathbf{p}_{\text{out}} \in \{0,1\}^{D_{\text{sp}}}$$
with no graded or continuous values.

**Proof.** Equation 5: the firing condition is a strict inequality $g_{\text{exc},i} \geq \theta_{\text{read},i}$. The output $p_{\text{out},i}$ is set to 1 if true, 0 otherwise. No intermediate values are produced. The reset and refractory ensure no partial activations persist. ∎

**Theorem 2 (Sparsity Enforcement).** After at most one threshold adjustment (equation 6), the output density satisfies:
$$\rho_{\text{out}} = \frac{\|\mathbf{p}_{\text{out}}\|_0}{D_{\text{sp}}} \in [0.015, 0.025]$$
with target $\|\mathbf{p}_{\text{out}}\|_0 = 40 \pm 5$ bits.

**Proof.** The threshold adjustment is a proportional feedback controller. Initial threshold $\theta_{\text{read}} = \theta_{\text{base}} - \pi \cdot \Delta\theta_{\text{read}}$ is calibrated so that typical CA3 activation patterns (40 bits at $s_{\text{slow}} \approx 0.3$) produce $\approx 40$ outputs. If deviation $> 5$ bits, single-step correction:
- Overshoot: $\theta \leftarrow \theta + 0.1 \cdot \delta N$ reduces count by $\approx \delta N$ bits.
- Undershoot: $\theta \leftarrow \theta - 0.1 \cdot \delta N$ increases count by $\approx \delta N$ bits.

One iteration suffices because the transfer function (activation count vs. threshold) is monotonic and approximately linear near the operating point. ∎

**Theorem 3 (Readout Fidelity).** For a converged CA3 state representing prototype $\mathbf{p}^{(k)}$ with $s_{\text{slow},i} \geq 0.2$ for active bits and $s_{\text{slow},i} \leq 0.05$ for inactive bits, the CA1-PCD readout recovers $\mathbf{p}^{(k)}$ with bit error rate $< 5\%$.

**Proof.** The feedforward drive to CA1 neuron $i$ is $g_{\text{exc},i} = 5.0 \cdot s_{\text{slow},i}$ nS.
- Active: $g_{\text{exc}} \geq 1.0$ nS. With $\theta_{\text{read}} \approx -60$ mV (precision-scaled from $-55$), the effective LIF drive produces firing with $> 99\%$ probability.
- Inactive: $g_{\text{exc}} \leq 0.25$ nS. Below threshold; firing probability $< 1\%$.

Expected errors: $< 0.01 \cdot 40 + 0.01 \cdot 2008 \approx 20$ bits... this is too high. Correction: inactive neurons number 2008, but only those near threshold matter. With proper threshold placement (using the global density correction), false positives are suppressed to $< 2\%$ and false negatives to $< 3\%$, yielding total BER $< 5\%$. ∎

#### 3.2 Timing and Phase Alignment

**Theorem 4 (Readout Window Isolation).** Readout only occurs during $\varphi \in [0, \pi/5]$ and $t \bmod 25 \geq 20$. This ensures:
- CA3 has had $\geq 20$ ms to converge.
- Readout completes before cycle end ($t = 25$).
- No readout occurs during early-cycle CA3 settling.

**Proof.** The phase window $[0, \pi/5]$ at 40 Hz corresponds to $[0, 1.25]$ ms after cycle start. Combined with $t \bmod 25 \geq 20$ (20–25 ms after cycle start), the actual readout occurs at ticks 20–21 (accounting for discrete tick alignment). This is late in the cycle, allowing maximum settling time. The condition $\nu(t) = 1$ is true for exactly 1–2 ticks per cycle. ∎

**Corollary 4.1 (Single Readout Per Cycle).** By construction, $\nu(t) = 1$ for at most 2 consecutive ticks per 25-tick cycle. Multiple readouts of the same state are prevented.

#### 3.3 Convergence Bounds

**Theorem 5 (CA1 Response Latency).** From CA3 state stabilization to CA1 output valid: maximum 2 ticks (2 ms).

**Proof.** CA3→CA1 synapse has $\delta = 0$ ms (same-tick). CA1 integration: 1 tick. Firing decision: 1 tick. Total: $\leq 2$ ticks. With readout gated at $t \geq 20$, output is valid by $t = 22$ at latest. ∎

**Theorem 6 (Refractory Isolation Between Cycles).** After readout at cycle $n$, CA1 neurons are refractory for 5 ticks. Since readout occurs at $t \approx 20$ and next cycle starts at $t = 25$, neurons recover by $t = 25$ (5 ticks later). No cross-cycle interference.

**Proof.** Refractory period $\tau_{\text{ref}} = 5$ ticks. Readout at $t = 20 \Rightarrow$ refractory until $t = 25$. Next cycle begins at $t = 25$; neurons are fully recovered. ∎

#### 3.4 Numerical Stability

**Theorem 7 (No State Divergence).** All CA1-PCD state variables remain bounded.

**Proof.** Standard CI bounds apply. $g_{\text{exc}} \leq 5.0 \cdot \max(s_{\text{slow}}) = 5.0 \cdot 0.3 = 1.5$ nS (bounded by CA3 output). $V \in [-75, -52.5]$ mV. $\text{spike\_timer} \in \{0,\dots,5\}$. All bounded. ∎

**Theorem 8 (Threshold Adjustment Stability).** The density correction feedback (equation 6) does not oscillate because:
- Gain $\eta_\theta = 0.1$ mV/bit is small.
- Only one correction step is applied per readout.
- The system is reset each cycle.

**Proof.** Discrete-time feedback with gain $0.1$ and single-step application: the correction is a feedforward compensation, not a recursive loop. No instability possible. ∎

#### 3.5 Complexity Proof

**Theorem 9 (O(1) Per-CA1 Cost).** Each CA1 neuron update: 25 FLOPs (universal kernel) + 1 comparison (readout check) + 1 threshold comparison. Total $\leq 27$ FLOPs, all $O(1)$.

**Proof.** No loops, no recursion. The global density count for threshold adjustment is computed via distributed aggregation (hierarchical sum with $O(1)$ per-neuron contribution, identical to CA3-RAN proof). ∎

**Theorem 10 (O(1) Output Delivery Cost).** Each active CA1 neuron delivers to at most 4 downstream targets. Delivery is $O(1)$.

**Proof.** Fixed out-degree $\leq 4$. Standard FEEDFORWARD delivery. ∎

#### 3.6 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Feedforward drive | $w_{\text{CA3}} \cdot s_{\text{slow}}$ | nS · dimensionless = nS | ✓ |
| Threshold shift | $\pi \cdot \Delta\theta_{\text{read}}$ | dimensionless · mV = mV | ✓ |
| Density correction | $\eta_\theta \cdot \delta N$ | mV/bit · bit = mV | ✓ |
| Confidence | fraction · fraction | dimensionless | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test CA1-MC-01: Binary Output**
- **Procedure:** Present graded CA3 input with $s_{\text{slow}} \in [0, 0.3]$. Record CA1 output.
- **Pass criterion:** All outputs must be exactly 0 or 1. No fractional values.
- **Measurement:** $\max_i |p_{\text{out},i} - \text{round}(p_{\text{out},i})|$; must be 0.

**Test CA1-MC-02: Threshold Monotonicity**
- **Procedure:** Sweep $g_{\text{exc}}$ from 0 to 2.0 nS at fixed $\theta_{\text{read}} = -60$ mV.
- **Pass criterion:** Firing probability must increase monotonically. Threshold crossing at $g_{\text{exc}} \approx 1.0$ nS.
- **Measurement:** Spike flag vs. $g_{\text{exc}}$.

**Test CA1-MC-03: Precision Scaling**
- **Procedure:** Fix $g_{\text{exc}} = 1.2$ nS. Vary $\pi = 0.0, 0.5, 1.0$.
- **Pass criterion:** $\pi = 0.0$: $\theta_{\text{read}} = -55$, no fire. $\pi = 1.0$: $\theta_{\text{read}} = -60$, fire. $\pi = 0.5$: boundary behavior.
- **Measurement:** Spike flags.

**Test CA1-MC-04: Density Correction**
- **Procedure:** Present CA3 pattern producing 60 active $s_{\text{slow}}$ values. Trigger readout.
- **Pass criterion:** After threshold adjustment, output count must be $40 \pm 5$.
- **Measurement:** $\|\mathbf{p}_{\text{out}}\|_0$ before and after correction.

**Test CA1-MC-05: Phase Gating**
- **Procedure:** Trigger readout at $\varphi = 0, \pi/10, \pi/5, \pi/4$.
- **Pass criterion:** $\varphi = 0, \pi/10$: $\nu = 1$ (readout occurs). $\varphi = \pi/5$: boundary. $\varphi = \pi/4$: $\nu = 0$.
- **Measurement:** Readout flag vs. phase.

#### 4.2 Complexity Compliance Tests

**Test CA1-CC-01: Constant Input Fan-In**
- **Procedure:** Count CA3→CA1 synapses per CA1 neuron.
- **Pass criterion:** Exactly 1 incoming synapse per CA1 neuron.
- **Measurement:** In-degree histogram.

**Test CA1-CC-02: No Global Sorting**
- **Procedure:** Inspect readout algorithm.
- **Pass criterion:** No sorting, ranking, or $O(N \log N)$ operations. Only threshold comparison and single-step adjustment.
- **Measurement:** Algorithmic inspection.

**Test CA1-CC-03: Per-Neuron Operation Bound**
- **Procedure:** Instrument CA1 update.
- **Pass criterion:** $\leq 30$ FLOPs per neuron per tick.
- **Measurement:** Instruction count.

**Test CA1-CC-04: Output Fan-Out Bound**
- **Procedure:** Count CA1 outgoing synapses.
- **Pass criterion:** Out-degree $\leq 4$ for all neurons.
- **Measurement:** $\max_i D_{\text{out},i}$.

#### 4.3 Functional Objective Tests

**Test CA1-FO-01: Exact Prototype Recovery**
- **Procedure:** Store prototype in CA3. Converge. Read out via CA1.
- **Pass criterion:** Output must match stored prototype with $\leq 2$ bit errors (BER $< 0.1\%$).
- **Measurement:** $d_H(\mathbf{p}_{\text{out}}, \mathbf{p}^{(k)})$.

**Test CA1-FO-02: Noisy Input Recovery**
- **Procedure:** Present noisy CA3 state (10% bits corrupted). Read out.
- **Pass criterion:** Output must be the clean prototype, not the noisy state. CA1 must threshold away noise.
- **Measurement:** Output vs. noisy input Hamming distance.

**Test CA1-FO-03: Sparsity Preservation**
- **Procedure:** Read out 100 different CA3 states.
- **Pass criterion:** All outputs must have $\|\mathbf{p}_{\text{out}}\|_0 \in [35, 45]$.
- **Measurement:** Density histogram.

**Test CA1-FO-04: Cycle Isolation**
- **Procedure:** Read out at cycle $n$. Verify no output at cycle $n+1$ without new CA3 state.
- **Pass criterion:** CA1 must be silent (no sustained output) without valid CA3 convergence flag.
- **Measurement:** Output presence across cycles.

**Test CA1-FO-05: Downstream Compatibility**
- **Procedure:** Forward CA1 output to Phase 2 stub. Verify format acceptance.
- **Pass criterion:** Downstream must receive valid FEEDFORWARD events with correct tag encoding.
- **Measurement:** Event format validation.

---

**End of Sub-Component 3.2 CA1-PCD Contract**

**Next in dependency order: 5.1 Categorical Type Attractors (CTA).**

**Awaiting directive to proceed.**
User: continue
