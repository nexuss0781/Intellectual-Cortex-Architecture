Kimi: **PHASE 1 | SUB-COMPONENT 1.3: Semantic Pointer Projection Fibers (SPPF)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Copy each active neuron from the sparse encoding layer and forward its spike to downstream binding and memory circuits after a programmed delay between 1 and 5 milliseconds.
- Attach an 8-byte semantic label to every forwarded spike so that downstream coincidence detectors can match spikes carrying the same label.
- Guarantee that the sparse pattern leaving this layer has exactly the same number of active bits as the pattern entering it, with no added noise or dropped spikes.
- Ensure every forwarding neuron contacts no more than four downstream targets, regardless of how large the downstream pools grow.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| PSG spike indicator | $s_j(t)$ | $\{0,1\}$ | Binary firing flag from PSG neuron $j$ post-BSI |
| Active PSG index set | $\mathcal{A}(t)$ | $\mathcal{P}(\{1,\dots,D_{\text{sp}}\})$ | $\mathcal{A}(t) = \{j \mid s_j(t)=1\}$; $|\mathcal{A}(t)| \leq 32$ |
| Semantic category map | $\mathcal{L}$ | $\{0,1\}^{56}$ | Per-synapse 56-bit label in tag[1..7] |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Delayed spike arrival | $S_{ik}(t_{\text{arr}})$ | $\{0,1\}$ | Spike from SPPF relay $i$ arriving at target $k$ |
| Semantic tag payload | $\mathbf{L}_{ik}$ | $\{0,1\}^{56}$ | 7-byte label delivered with synaptic event |
| Effective arrival time | $t_{\text{arr},ik}$ | $\mathbb{Z}_{\geq 0}$ | $t_{\text{arr},ik} = t_{\text{fire},i} + \delta_{ik}$ |
| Forwarded sparsity | $\rho_{\text{fwd}}(t)$ | $[0, 0.025]$ | Density of active SPPF relays; equals PSG density |

#### 2.3 State Space Definition

Each SPPF relay neuron occupies a CI slot:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V$ | mV | $-70.0$ | LIF integration state |
| Excitatory conductance | $g_{\text{exc}}$ | nS | $0.0$ | Receives PSG input |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Standard countdown |
| Oscillatory phase | $\varphi$ | rad | $0.0$ | Gamma cycle tracking |
| Type identifier | $\text{type\_id}$ | — | $0$ (CI) | Fixed neuron class |
| Flags | $\text{flags}$ | uint16 | $\text{FLAG\_INTELLECTUAL\_POOL}$ | Pool membership |

Each SPPF output synapse to downstream target $k$ carries:

| Field | Symbol | Value | Role |
|-------|--------|-------|------|
| Postsynaptic index | $\text{post\_id}$ | Downstream neuron $k$ | Fixed routing |
| Efficacy | $w_{\text{out}}$ | $[0.3, 0.7]\ \text{nS}$ | Forwarding weight |
| Axonal delay | $\delta$ | $\{1,2,3,4,5\}\ \text{ms}$ | Programmable latency |
| Tag byte 0 | $\text{tag}[0]$ | $0\text{b}00000100$ | Class=0 (FEEDFORWARD); routing key=SPPF |
| Tag bytes 1–7 | $\text{tag}[1..7]$ | $\mathbf{L}_{ik} \in \{0,1\}^{56}$ | Semantic category label |
| Eligibility | $\text{eligibility}$ | $0.0$ | Not used in Phase 1 |

#### 2.4 Governing Equations

**SPPF Relay Neuron $i$ (per tick, $dt = 1\ \text{ms}$):**

1. **Input from PSG.** When PSG neuron $j$ connected to relay $i$ fires at tick $t$:
   $$g_{\text{exc},i}(t^+) = g_{\text{exc},i}(t) + w_{\text{in}}$$
   where $w_{\text{in}} = 5.5\ \text{nS}$ (suprathreshold drive).

2. **Conductance decay (all ticks):**
   $$g_{\text{exc},i}(t+1) = g_{\text{exc},i}(t^+) \cdot \exp(-dt/\tau_{\text{exc}})$$
   with $\tau_{\text{exc}} = 5\ \text{ms}$.

3. **Synaptic current (if $\text{spike\_timer} = 0$):**
   $$I_{\text{syn},i}(t) = g_{\text{exc},i}(t^+) \cdot \bigl(E_{\text{exc}} - V_i(t)\bigr)$$
   where $E_{\text{exc}} = 0.0\ \text{mV}$.

4. **Membrane update:**
   $$V_i(t+1) = V_i(t) + \frac{dt}{\tau_m}\Bigl[-\bigl(V_i(t) - V_{\text{rest}}\bigr) + R_m \cdot I_{\text{syn},i}(t)\Bigr]$$
   with $V_{\text{rest}} = -70.0\ \text{mV}$, $\tau_m = 20\ \text{ms}$, $R_m = 1\ \text{M}\Omega$.

5. **Instant relay firing condition:**
   Since $w_{\text{in}} = 5.5\ \text{nS} > \theta_g \approx 4.286\ \text{nS}$, the neuron fires on the first tick after input arrival:
   If $V_i(t+1) \geq \theta_{\text{base}} = -55.0\ \text{mV}$:
   - Emit spike: $s_i(t+1) = 1$
   - Reset: $V_i(t+1) \leftarrow V_{\text{reset}} = -75.0\ \text{mV}$
   - Refractory: $\text{spike\_timer}_i \leftarrow 5$

6. **Refractory countdown (if $\text{spike\_timer} > 0$):**
   $$\text{spike\_timer}_i \leftarrow \text{spike\_timer}_i - 1$$
   Skip steps 3–5.

7. **Phase rotation (universal kernel):**
   $$\varphi_i(t+1) = \bigl(\varphi_i(t) + \omega \cdot dt\bigr) \bmod 2\pi$$
   with $\omega = 2\pi \cdot 40\ \text{Hz}$.

**SPPF Synaptic Delivery to Downstream Target $k$:**

8. **Event generation.** If $s_i(t_{\text{fire}}) = 1$, for each outgoing synapse to target $k$:
   - Scheduled arrival: $t_{\text{arr}} = t_{\text{fire}} + \delta_{ik}$
   - Payload: $(w_{\text{out}}, \mathbf{L}_{ik})$ carried in synapse structure

9. **Target conductance increment upon arrival:**
   $$g_{\text{exc},k}(t_{\text{arr}}^+) = g_{\text{exc},k}(t_{\text{arr}}) + w_{\text{out}}$$

**Sparsity Preservation Invariant:**

10. **One-to-one relay mapping.** Each active PSG neuron $j \in \mathcal{A}(t)$ maps to exactly one SPPF relay $i = \sigma(j)$, where $\sigma$ is a fixed bijection. Therefore:
    $$|\{i \mid s_i(t_{\text{fire}}) = 1\}| = |\mathcal{A}(t)|$$
    and the forwarded density satisfies:
    $$\rho_{\text{fwd}} = \frac{|\mathcal{A}(t)|}{D_{\text{sp}}} = \rho_{\text{PSG}}$$

**Semantic Tag Invariant:**

11. **Static tag integrity.** For every SPPF output synapse, the tag bytes are write-once at initialization and read-only during operation:
    $$\forall t \geq 0: \quad \text{syn.tag}[b](t) = \text{syn.tag}[b](0), \quad b \in \{0,\dots,7\}$$

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Relay count | $N_{\text{sppf}}$ | $2{,}048$ | — | $2{,}048$ | $2{,}048$ | 1:1 with PSG output dimension |
| Input drive weight | $w_{\text{in}}$ | $5.5$ | nS | $5.0$ | $6.0$ | Suprathreshold PSG→SPPF excitation |
| Output forward weight | $w_{\text{out}}$ | $0.5$ | nS | $0.3$ | $0.7$ | Subthreshold marker to downstream |
| Max output fan-out | $D_{\text{out}}$ | $4$ | synapses | $1$ | $4$ | Downstream targets per relay |
| Programmable delay | $\delta$ | $\{1,\dots,5\}$ | ms | $1$ | $5$ | Axonal latency range |
| Resting potential | $V_{\text{rest}}$ | $-70.0$ | mV | $-72.0$ | $-68.0$ | Baseline membrane state |
| Firing threshold | $\theta_{\text{base}}$ | $-55.0$ | mV | $-57.0$ | $-53.0$ | Spike emission boundary |
| Reset potential | $V_{\text{reset}}$ | $-75.0$ | mV | $-77.0$ | $-73.0$ | Post-spike membrane clamp |
| Membrane time constant | $\tau_m$ | $20.0$ | ms | $18.0$ | $22.0$ | Integration speed |
| Excitatory reversal | $E_{\text{exc}}$ | $0.0$ | mV | — | — | Excitatory current reversal |
| Membrane resistance | $R_m$ | $1.0$ | M$\Omega$ | $0.8$ | $1.2$ | Ohmic scaling |
| Excitatory synapse time constant | $\tau_{\text{exc}}$ | $5.0$ | ms | $4.5$ | $5.5$ | Conductance decay speed |
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |
| Gamma frequency | $f_\gamma$ | $40.0$ | Hz | $38.0$ | $42.0$ | Oscillation frequency |
| Gamma period | $T_\gamma$ | $25.0$ | ticks | $24$ | $26$ | Cycle length |
| Tag byte count | $B_{\text{tag}}$ | $8$ | bytes | $8$ | $8$ | Fixed semantic label size |
| Semantic label bits | $|\mathbf{L}|$ | $56$ | bits | $56$ | $56$ | Usable label in tag[1..7] |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | CI ($\text{type\_id} = 0$) | Core Integrator for relay function |
| Input synapse type | FEEDFORWARD (type 0) | From PSG to SPPF relay |
| Output synapse type | FEEDFORWARD (type 0) | From SPPF relay to downstream |
| Tag encoding | $\text{tag}[0] = 0\text{b}00000100$ | Class=0 (FEEDFORWARD); routing key=SPPF |
| Semantic label | $\text{tag}[1..7]$ | 56-bit category identifier, read-only after init |
| Source field | $g_{\text{exc}}$ (SPPF relay) | Receives PSG spikes |
| Target field (downstream) | $g_{\text{exc}}$ (GBGN/DPSG) | Standard excitatory delivery |

#### 2.7 Interface Contract

**Upstream providers:**
- **1.1 PSG / 1.2 BSI:** delivers the post-sparsification sparse pattern $\mathbf{s}(t) \in \{0,1\}^{D_{\text{sp}}}$ as spike events to SPPF relays.

**Downstream consumers:**
- **2.1 GBGN** (Granule Binding Gate Neurons): receives tagged FEEDFORWARD spikes for binding operations.
- **3.3 DPSG** (Dentate Pattern Separation Gating): receives tagged FEEDFORWARD spikes for hippocampal cleanup input.
- **Phase 2 Memory Architecture** (stubbed): receives clean semantic pointers via tagged output.

**Handshake format:**
- Input events carry $\text{post\_id}$ (SPPF relay index), $w_{\text{in}} = 5.5\ \text{nS}$, $\text{delay} = 0$, tag byte with SPPF-input routing key.
- Output events carry $\text{post\_id}$ (downstream target), $w_{\text{out}} \in [0.3, 0.7]\ \text{nS}$, $\text{delay} \in [1,5]\ \text{ms}$, and full 8-byte tag with semantic label.

---

### 3. Stability & Rigor Analysis

#### 3.1 Relay Fidelity and Timing

**Theorem 1 (Guaranteed Relay Firing).** An SPPF relay neuron at rest ($V = -70.0\ \text{mV}$) receiving a single PSG input with weight $w_{\text{in}} \geq 5.0\ \text{nS}$ fires exactly once on the tick immediately following input arrival.

**Proof.** From the discrete LIF update at rest with no inhibition:
$$V_{\text{new}} = -70 + 0.05 \cdot \bigl[0 + 1.0 \cdot w_{\text{in}} \cdot (0 - (-70))\bigr] = -70 + 3.5 \cdot w_{\text{in}}$$
For $w_{\text{in}} = 5.0\ \text{nS}$: $V_{\text{new}} = -70 + 17.5 = -52.5\ \text{mV} \geq -55.0\ \text{mV}$. The firing condition is satisfied. After firing, $V$ resets to $-75.0\ \text{mV}$ and $\text{spike\_timer} = 5$. Since PSG fires at most once per gamma cycle ($25\ \text{ms}$) and the refractory period is $5\ \text{ms}$, the relay is ready for the next cycle. The relay fires exactly once per PSG input event. ∎

**Corollary 1.1 (Zero Spike Loss).** Because $w_{\text{in}} > \theta_g \approx 4.286\ \text{nS}$ with margin $> 0.7\ \text{nS}$, and because the relay has no competing inhibition at this stage, every PSG spike produces exactly one SPPF relay spike. The mapping is bijective: $|\mathcal{A}_{\text{SPPF}}| = |\mathcal{A}_{\text{PSG}}|$.

**Theorem 2 (Delay Accuracy).** The total latency from PSG spike emission to downstream target conductance increment is:
$$T_{\text{latency}} = 1 + \delta_{ik}\ \text{ticks}$$
where $1$ tick is the SPPF relay integration time and $\delta_{ik} \in \{1,2,3,4,5\}$ is the programmed axonal delay. The variance of this latency is zero (deterministic).

**Proof.** PSG fires at tick $t$. SPPF relay receives the event (with delay 0) and integrates during tick $t$. By Theorem 1, it fires at tick $t+1$. The output synapse schedules arrival at $t_{\text{arr}} = (t+1) + \delta_{ik}$. The event system delivers exactly at this tick. No jitter is introduced because delays are stored as integer ticks in float32 with exact representation for $\{1,2,3,4,5\}$. ∎

**Corollary 2.1 (Delay Bound).** Since $\delta_{ik} \leq 5\ \text{ms}$ and relay integration is $1\ \text{ms}$, total latency $\leq 6\ \text{ms}$. This is well within the gamma cycle period of $25\ \text{ms}$, ensuring downstream arrival within the same cycle.

#### 3.2 Convergence Bounds

**Theorem 3 (Post-Fire Recovery).** After an SPPF relay fires at $t = 0$, its membrane potential recovers as:
$$\bigl|V(t) - V_{\text{rest}}\bigr| \leq 5 \cdot \exp(-t/\tau_m)\ \text{mV}$$
Full recovery ($|V - V_{\text{rest}}| < 0.25\ \text{mV}$) is achieved by $t = 20\ \text{ms}$.

**Proof.** Identical to prior sub-component recovery proofs. Leak dynamics with $\tau_m = 20\ \text{ms}$. ∎

**Theorem 4 (Conductance Decay Between Cycles).** If a PSG spike arrives at $t = 0$ and the next arrives at $t = 25$ (next gamma cycle), residual conductance from the first event is:
$$g_{\text{exc}}(25) = w_{\text{in}} \cdot \exp(-25/5) = w_{\text{in}} \cdot e^{-5} \approx 0.0067 \cdot w_{\text{in}} < 0.04\ \text{nS}$$
This is negligible and does not affect threshold crossing on the next cycle.

**Proof.** Exponential decay with $\tau_{\text{exc}} = 5\ \text{ms}$. At $t = 25\ \text{ms}$: $25/5 = 5$ time constants. $e^{-5} \approx 0.0067$. ∎

#### 3.3 Numerical Stability

**Theorem 5 (No State Divergence).** All SPPF relay state variables $(V_i, g_{\text{exc},i}, \text{spike\_timer}_i, \varphi_i)$ remain bounded for all $t \geq 0$.

**Proof.**
- $V_i \in [-75, -52.5]\ \text{mV}$ by reset and threshold.
- $g_{\text{exc},i} \in [0, w_{\text{in}}]$ by single-input design (resets between cycles).
- $\text{spike\_timer}_i \in \{0,\dots,5\}$.
- $\varphi_i \in [0, 2\pi)$.
All bounded. ∎

**Theorem 6 (Tag Immutability).** The semantic tag bytes $\text{tag}[0..7]$ are stored in the SynapseBlock as uint8 constants. They are never modified by the universal kernel or any Phase 1 update rule. Therefore:
$$\Pr\bigl[\text{tag}[b](t) \neq \text{tag}[b](0)\bigr] = 0 \quad \forall t > 0, \forall b \in \{0,\dots,7\}$$

**Proof.** The UIN universal kernel (Section 12.1) updates only neuron state fields $(V, g_{\text{exc}}, g_{\text{inh}}, \dots)$. Synapse fields $(w, \text{delay}, \text{eligibility}, \text{pred\_coeff}, \text{precision}, \text{tag})$ are not written by the per-tick kernel. The `deliver_spike` function reads the tag but does not modify it. Therefore tag bytes are constant for all $t$. ∎

**Theorem 7 (Float32 Delay Exactness).** The programmed delays $\delta_{ik} \in \{1,2,3,4,5\}$ are represented exactly in IEEE 754 float32 because all are small integers. The conversion to integer ticks $\delta_{\text{ticks}} = \text{round}(\delta_{ik} / dt)$ with $dt = 1\ \text{ms}$ is exact.

**Proof.** Integers $1, 2, 3, 4, 5$ are exactly representable in float32 (mantissa has 24 bits, sufficient for all integers $< 2^{24}$). Division by $1.0$ is exact. Rounding of exact integers yields the same integers. ∎

#### 3.4 Complexity Proof

**Theorem 8 (O(1) Per-Relay Cost).** Updating an SPPF relay neuron consumes exactly 25 FLOPs for the universal kernel. The relay fires at most once per 25 ticks, so the average cost is $25/25 = 1$ effective FLOP per tick amortized, bounded by 25 FLOPs worst-case.

**Proof.** Universal kernel: 25 scalar operations (proven in UIN report). No additional PM-specific or AS-specific branches are executed (CI type). The neuron receives from exactly 1 PSG neuron, so input event processing is 1 addition. Total $\leq 26$ operations, all $O(1)$. ∎

**Theorem 9 (O(1) Per-Event Delivery Cost).** When SPPF relay $i$ fires, it delivers exactly $D_{\text{out},i} \leq 4$ spike events to downstream targets. Each delivery requires reading the synapse structure (constant-time memory access) and enqueueing the event. This is $O(1)$ because $D_{\text{out},i}$ is bounded by a compile-time constant.

**Proof.** By construction, each SPPF relay has at most 4 outgoing synapses. The event delivery iterates over this fixed-size list. No dynamic allocation or variable-length iteration occurs. ∎

**Corollary 9.1 (Network-Wide SPPF Cost).** For $N_{\text{sppf}} = 2{,}048$ relays and active set size $|\mathcal{A}(t)| \leq 32$, the total SPPF-related work per tick is:
$$C_{\text{SPPF}} = \underbrace{25 \cdot N_{\text{sppf}}}_{\text{kernel updates}} + \underbrace{D_{\text{out}} \cdot |\mathcal{A}(t)|}_{\text{event deliveries}} \leq 51{,}200 + 128 = O(N)$$
with extremely small constant factors relative to the 270K neuron budget.

#### 3.5 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Input weight | $w_{\text{in}}$ | nS | ✓ |
| Relay membrane leak | $-(V - V_{\text{rest}})$ | mV | ✓ |
| Relay Ohmic term | $R_m \cdot I_{\text{syn}}$ | $\text{M}\Omega \cdot \text{nS} \cdot \text{mV} = \text{mV}$ | ✓ |
| Relay membrane increment | $(dt/\tau_m) \cdot [\dots]$ | $(\text{ms}/\text{ms}) \cdot \text{mV} = \text{mV}$ | ✓ |
| Output weight | $w_{\text{out}}$ | nS | ✓ |
| Delay | $\delta$ | ms | ✓ |
| Tag field | $\text{tag}[b]$ | uint8 (dimensionless) | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test SPPF-MC-01: Relay Threshold**
- **Procedure:** Initialize SPPF relay at rest. Inject single PSG input with $w_{\text{in}} = 5.0, 5.5, 6.0\ \text{nS}$.
- **Pass criterion:** Relay must fire on the next tick for all weights in range. $V_{\text{new}}$ must be $\geq -55.0\ \text{mV}$.
- **Measurement:** Spike flag and $V_{\text{new}}$.

**Test SPPF-MC-02: One-to-One Mapping**
- **Procedure:** Activate $k = 1, 5, 10, 20, 32$ distinct PSG neurons. Record SPPF relay firings.
- **Pass criterion:** Exactly $k$ SPPF relays must fire. No more, no less. Bijection must hold.
- **Measurement:** SPPF spike count vs. PSG spike count.

**Test SPPF-MC-03: Delay Accuracy**
- **Procedure:** For each programmed delay $\delta \in \{1,2,3,4,5\}\ \text{ms}$, trigger a PSG spike and measure arrival time at downstream target.
- **Pass criterion:** Arrival tick must equal exactly $t_{\text{PSG}} + 1 + \delta$. Zero variance across 100 trials.
- **Measurement:** $t_{\text{arrive}} - t_{\text{PSG}}$.

**Test SPPF-MC-04: Conductance Decay Between Cycles**
- **Procedure:** Inject $w_{\text{in}} = 5.5\ \text{nS}$ at $t = 0$. Record $g_{\text{exc}}$ at $t = 25$ with no further input.
- **Pass criterion:** $g_{\text{exc}}(25) \leq 0.04\ \text{nS}$ (residual $< 1\%$ of peak).
- **Measurement:** $g_{\text{exc}}(25)$.

**Test SPPF-MC-05: Refractory Isolation**
- **Procedure:** Attempt to drive SPPF relay twice within 5 ticks with two separate PSG spikes.
- **Pass criterion:** Second PSG spike must not produce a second SPPF spike during refractory. The relay must ignore the second input.
- **Measurement:** Spike flags at $t_1$ and $t_2$.

#### 4.2 Complexity Compliance Tests

**Test SPPF-CC-01: Constant Output Fan-Out**
- **Procedure:** For every SPPF relay, count outgoing FEEDFORWARD synapses.
- **Pass criterion:** Out-degree must be $\leq 4$ for all relays. No relay may project to $> 4$ targets.
- **Measurement:** $\max_{i} D_{\text{out},i}$.

**Test SPPF-CC-02: Constant Input Fan-In**
- **Procedure:** For every SPPF relay, count incoming FEEDFORWARD synapses from PSG.
- **Pass criterion:** In-degree must be exactly $1$ for all relays.
- **Measurement:** PSG in-degree histogram.

**Test SPPF-CC-03: No Global Iteration**
- **Procedure:** Inspect SPPF update and event delivery algorithms.
- **Pass criterion:** No instruction may iterate over all $N_{\text{sppf}}$ relays or all downstream targets globally. Only per-relay constant-size operations.
- **Measurement:** Static algorithmic inspection.

**Test SPPF-CC-04: Tag Memory Footprint**
- **Procedure:** Measure memory consumed by SPPF synapse tags.
- **Pass criterion:** Tag storage must be $8$ bytes per synapse, no dynamic allocation. Total SPPF synapse memory $\leq N_{\text{sppf}} \cdot D_{\text{out}} \cdot 32\ \text{bytes} \leq 2{,}048 \cdot 4 \cdot 32 = 262{,}144\ \text{bytes} = 256\ \text{KB}$.
- **Measurement:** Memory audit.

#### 4.3 Functional Objective Tests

**Test SPPF-FO-01: Sparsity Preservation**
- **Procedure:** Present 50 different sparse PSG patterns with densities $0.005, 0.01, 0.015$. Record downstream arrival density.
- **Pass criterion:** Downstream density must equal input density exactly (within count statistics). No amplification or attenuation.
- **Measurement:** $\rho_{\text{in}}$ vs. $\rho_{\text{out}}$ correlation; $R^2 > 0.999$.

**Test SPPF-FO-02: Semantic Tag Delivery**
- **Procedure:** Initialize 10 SPPF relays with distinct 56-bit semantic labels. Trigger each relay and inspect the tag bytes received at downstream targets.
- **Pass criterion:** All 8 tag bytes must match initialization values exactly. No bit flips, truncation, or reordering.
- **Measurement:** Byte-by-byte comparison.

**Test SPPF-FO-03: Multi-Target Broadcast**
- **Procedure:** Configure SPPF relay $i$ with $D_{\text{out}} = 4$ targets. Trigger relay and verify all 4 targets receive the event.
- **Pass criterion:** All 4 targets must show conductance increment at the correct delayed tick. No target may miss the event.
- **Measurement:** Target $g_{\text{exc}}$ at scheduled arrival times.

**Test SPPF-FO-04: Latency Bound Under Load**
- **Procedure:** Trigger all 32 possible PSG winners simultaneously. Measure maximum arrival delay at downstream.
- **Pass criterion:** Latest arrival must be $\leq 6\ \text{ms}$ ($1\ \text{ms}$ relay + $5\ \text{ms}$ max delay). All arrivals must occur within the same gamma cycle.
- **Measurement:** $\max_{i,k} (t_{\text{arrive},ik} - t_{\text{trigger}})$.

**Test SPPF-FO-05: Pattern Identity Preservation**
- **Procedure:** Present a specific 32-bit sparse pattern through PSG→SPPF→downstream. Record the pattern at each stage.
- **Pass criterion:** The set of active neuron indices must be identical at PSG output, SPPF output, and downstream input. The pattern must not be scrambled, duplicated, or dropped.
- **Measurement:** Set equality of active indices across stages.

---

**End of Sub-Component 1.3 SPPF Contract**

**Next in dependency order: 2.2 Gamma Phase Locking Oscillators (GPLO).**

**Awaiting directive to proceed.**
User: continue
