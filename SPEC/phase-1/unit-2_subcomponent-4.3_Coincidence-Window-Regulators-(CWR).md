Kimi: **PHASE 1 | SUB-COMPONENT 4.3: Coincidence Window Regulators (CWR)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Generate a precisely delayed timing pulse that arrives at binding neurons between 1 and 4 milliseconds after each gamma cycle starts, thereby defining how long the binding window stays open.
- Make that delay programmable so the system can tune the window duration between 1 and 4 milliseconds without changing neuron hardware.
- Deliver this timing mark to every binding gate neuron via a dedicated inhibitory synapse so the mark is distinct from data-carrying excitatory inputs.
- Ensure the timing pulse repeats exactly once per gamma cycle and does not drift or jitter beyond half a millisecond over long runs.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| External pulse conductance | $\Delta g_{\text{pulse}}$ | $[4.5, 6.0]\ \text{nS}$ | Calibrated excitatory pulse delivered simultaneously to all CWR masters at $t \equiv 0 \pmod{25}$ |
| Programmable axonal delay | $\delta$ | $\{1, 2, 3, 4\}\ \text{ms}$ | Stored in synapse `delay` field; defines window close time |
| Initial phase | $\varphi_0$ | $[0, 2\pi)$ | Startup alignment, set to $0$ at initialization |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Window-close spike train | $S_{\text{CWR}}(t)$ | $\{0, 1\}$ | Binary indicator; $S_{\text{CWR}}(t)=1$ iff a CWR spike arrived at GBGN at tick $t$ |
| Effective binding window | $W(t)$ | $\{0, 1\}$ | $W(t) = \mathbb{I}\big[(t \bmod 25) \in [0, \delta]\big]$ |
| Close timestamp | $t_{\text{close}}$ | $\mathbb{Z}_{\geq 0}$ | $t_{\text{close}} = 25n + \delta$ for cycle $n$ |

#### 2.3 State Space Definition

Each CWR master neuron occupies a CI slot:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V$ | mV | $-70.0$ | Integrates pulse input |
| Excitatory conductance | $g_{\text{exc}}$ | nS | $0.0$ | Receives $\Delta g_{\text{pulse}}$ every 25 ticks |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Counts down refractory period |
| Oscillatory phase | $\varphi$ | rad | $0.0$ | Tracks gamma cycle via universal kernel |
| Type identifier | $\text{type\_id}$ | — | $0$ (CI) | Fixed neuron class |

Each CWR output synapse to GBGN carries:

| Field | Symbol | Unit | Value | Role |
|-------|--------|------|-------|------|
| Postsynaptic index | $\text{post\_id}$ | — | GBGN target | Fixed routing |
| Efficacy | $w_{\text{CWR}}$ | nS | $[0.1, 0.5]$ | Subthreshold marker weight |
| Axonal delay | $\delta$ | ms | $\{1,2,3,4\}$ | Programmable window duration |
| Tag byte 0 | $\text{tag}[0]$ | uint8 | `0b01100010` | Class=3 (LATERAL_INH), routing key=CWR |

#### 2.4 Governing Equations

**CWR Master Neuron (per tick, $dt = 1\ \text{ms}$):**

1. **Pulse injection (at $t \equiv 0 \pmod{25}$):**
   $$g_{\text{exc}}(t^+) = g_{\text{exc}}(t) + \Delta g_{\text{pulse}}$$

2. **Conductance decay (all ticks):**
   $$g_{\text{exc}}(t+1) = g_{\text{exc}}(t^+) \cdot \exp(-dt/\tau_{\text{exc}})$$
   with $\tau_{\text{exc}} = 5\ \text{ms}$.

3. **Synaptic current (standard biophysical form):**
   $$\tilde{I}_{\text{syn}} = g_{\text{exc}}(t^+) \cdot \bigl(E_{\text{exc}} - V(t)\bigr)$$
   where $E_{\text{exc}} = 0.0\ \text{mV}$.

4. **Membrane update (if $\text{spike\_timer} = 0$):**
   $$V(t+1) = V(t) + \frac{dt}{\tau_m}\Bigl[-\bigl(V(t) - V_{\text{rest}}\bigr) + R_m \cdot \tilde{I}_{\text{syn}}(t)\Bigr]$$
   with $V_{\text{rest}} = -70.0\ \text{mV}$, $\tau_m = 20\ \text{ms}$, $R_m = 1\ \text{M}\Omega$.

5. **Firing condition:**
   If $V(t+1) \geq \theta_{\text{base}} = -55.0\ \text{mV}$:
   - Emit spike: $S_{\text{CWR}}(t+1) = 1$
   - Reset: $V(t+1) \leftarrow V_{\text{reset}} = -75.0\ \text{mV}$
   - Refractory: $\text{spike\_timer} \leftarrow 5$

6. **Refractory countdown (if $\text{spike\_timer} > 0$):**
   $$\text{spike\_timer} \leftarrow \text{spike\_timer} - 1$$
   Skip steps 3–5.

7. **Phase rotation (universal kernel):**
   $$\varphi(t+1) = \bigl(\varphi(t) + \omega \cdot dt\bigr) \bmod 2\pi$$
   with $\omega = 2\pi \cdot 40\ \text{Hz} = 80\pi\ \text{rad/s}$.

**CWR Synaptic Delivery to GBGN:**

8. **Spike propagation (event-driven):**
   When master fires at $t_{\text{fire}}$, synapse delivers event at:
   $$t_{\text{arrive}} = t_{\text{fire}} + \delta$$
   where $\delta \in \{1, 2, 3, 4\}$ ticks (ms) is read from the synapse `delay` field.

9. **Target conductance increment:**
   Upon arrival at GBGN target $j$:
   $$g_{\text{inh},j}(t_{\text{arrive}}^+) = g_{\text{inh},j}(t_{\text{arrive}}) + w_{\text{CWR}}$$

10. **Target conductance decay (GBGN universal kernel):**
    $$g_{\text{inh},j}(t+1) = g_{\text{inh},j}(t) \cdot \exp(-dt/\tau_{\text{inh}})$$
    with $\tau_{\text{inh}} = 10\ \text{ms}$.

**Binding Window Function (derived, not explicit state):**

11. **Boolean window indicator:**
    $$W(t) = \begin{cases} 1 & \text{if } (t \bmod 25) \leq \delta \\ 0 & \text{otherwise} \end{cases}$$

    This function is consumed by Sub-component 2.3 (CDD) to accept or reject coincidence events.

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Resting potential | $V_{\text{rest}}$ | $-70.0$ | mV | $-72.0$ | $-68.0$ | Baseline membrane state |
| Firing threshold | $\theta_{\text{base}}$ | $-55.0$ | mV | $-57.0$ | $-53.0$ | Spike emission boundary |
| Reset potential | $V_{\text{reset}}$ | $-75.0$ | mV | $-77.0$ | $-73.0$ | Post-spike membrane clamp |
| Membrane time constant | $\tau_m$ | $20.0$ | ms | $18.0$ | $22.0$ | Integration speed |
| Excitatory reversal | $E_{\text{exc}}$ | $0.0$ | mV | — | — | Excitatory current reversal |
| Membrane resistance | $R_m$ | $1.0$ | M$\Omega$ | $0.8$ | $1.2$ | Ohmic scaling |
| Excitatory synapse time constant | $\tau_{\text{exc}}$ | $5.0$ | ms | $4.5$ | $5.5$ | Conductance decay speed |
| Inhibitory synapse time constant | $\tau_{\text{inh}}$ | $10.0$ | ms | $9.0$ | $11.0$ | Inhibitory conductance decay |
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |
| Gamma frequency | $f_\gamma$ | $40.0$ | Hz | $38.0$ | $42.0$ | Oscillation frequency |
| Gamma period | $T_\gamma$ | $25.0$ | ticks | $24$ | $26$ | Cycle length in time steps |
| Pulse conductance | $\Delta g_{\text{pulse}}$ | $5.0$ | nS | $4.5$ | $6.0$ | External drive amplitude |
| CWR synaptic weight | $w_{\text{CWR}}$ | $0.2$ | nS | $0.1$ | $0.5$ | Subthreshold marker pulse |
| Programmable delay | $\delta$ | $4.0$ | ms | $1$ | $4$ | Window duration control |
| Master neuron count | $N_{\text{CWR}}$ | $64$ | — | $32$ | $128$ | Redundant window generators |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | CI ($\text{type\_id} = 0$) | Core Integrator for delay-line pacemaker |
| Output synapse type | LATERAL_INH (type 3) | Targets $g_{\text{inh}}$ on GBGN |
| Tag encoding | $\text{tag}[0] = 0\text{b}01100010$ | Bits [0:2]=3 (LATERAL_INH); bits [5:7]=010 (CWR routing key) |
| Source field | $g_{\text{exc}}$ (master) | Receives external pulse |
| Target field | $g_{\text{inh}}$ (GBGN) | Receives subthreshold marker |
| Delay field | `delay` (synapse) | Stores $\delta \in [1,4]$ ms |

#### 2.7 Interface Contract

**Upstream providers:**
- Global system clock: delivers $t$ and the external pulse trigger every 25 ticks.
- Meta-Cognitive Controller: allocates $N_{\text{CWR}}$ CI slots from the intellectual pool.

**Downstream consumers:**
- **2.1 GBGN** (Granule Binding Gate Neurons): receives delayed LATERAL_INH marker; uses arrival time as $t_{\text{close}}$.
- **2.3 CDD** (Coincidence Detection Dendrites): consumes $W(t)$ and $t_{\text{close}}$ to bracket valid coincidence intervals.

**Handshake format:**
- Spike packets carry $\text{post\_id}$ (GBGN target), $w = w_{\text{CWR}}$, $\text{delay} = \delta$ (1–4 ticks), and tag byte with CWR routing key.
- The binding window duration is configured at initialization by writing $\delta$ into the synapse delay fields; no runtime parameter stream is required.

---

### 3. Stability & Rigor Analysis

#### 3.1 Fixed-Point and Periodicity

**Theorem 1 (Exact 40 Hz Periodicity).** Under the pulse protocol $\Delta g_{\text{pulse}} \geq 4.286\ \text{nS}$, each CWR master neuron fires exactly once every 25 ticks, with inter-spike interval $T_{\text{ISI}} = 25\ \text{ms}$.

**Proof.** Identical to Sub-component 4.2 RSP Theorem 1. At $t = n \cdot 25$, just before pulse, $V = V_{\text{rest}} = -70\ \text{mV}$ (full recovery guaranteed by $25\ \text{ms} > 5\ \text{ms}$ refractory plus $3\tau_m$ leak recovery). The pulse injects $\Delta g_{\text{pulse}}$. Using the corrected biophysical form:
$$V_{\text{new}} = -70 + 0.05 \cdot \bigl[0 + 1.0 \cdot \Delta g_{\text{pulse}} \cdot (0 - (-70))\bigr] = -70 + 3.5 \cdot \Delta g_{\text{pulse}}.$$
For $\Delta g_{\text{pulse}} = 5.0\ \text{nS}$: $V_{\text{new}} = -52.5\ \text{mV} \geq -55.0\ \text{mV}$. Firing occurs. Post-fire reset to $-75\ \text{mV}$ and 5-tick refractory. By $t = (n+1)\cdot 25$, leak dynamics restore $V$ to $-70\ \text{mV}$. State repeats exactly, establishing a periodic orbit with $T = 25$ ticks. ∎

**Corollary 1.1 (Delay Precision).** Because $t_{\text{fire}} = 25n$ exactly, the arrival time $t_{\text{arrive}} = 25n + \delta$ is deterministic to the tick resolution. The window duration is exactly $\delta$ ms with zero cycle-to-cycle variance in the firing time.

#### 3.2 Convergence Bounds

**Theorem 2 (Post-Reset Recovery).** After a spike at $t = 0$, the membrane potential converges to $V_{\text{rest}}$ exponentially:
$$\bigl|V(t) - V_{\text{rest}}\bigr| \leq \bigl|V_{\text{reset}} - V_{\text{rest}}\bigr| \cdot \exp(-t/\tau_m) = 5 \cdot \exp(-t/20)\ \text{mV}.$$

**Proof.** During refractory and recovery, no synaptic input is present. The update is $V(t+1) = V(t) + 0.05 \cdot (V_{\text{rest}} - V(t))$, the Euler discretization of $\tau_m\, dV/dt = -(V - V_{\text{rest}})$. The exact solution is exponential decay; the discrete scheme is stable because $dt/\tau_m = 0.05 < 1.0$. ∎

**Practical bound.** At $t = 20\ \text{ms}$, $|V - V_{\text{rest}}| < 0.25\ \text{mV}$. Full recovery is achieved well before the next pulse at $t = 25\ \text{ms}$.

#### 3.3 Numerical Stability

**Theorem 3 (Delay Quantization Error).** The programmed delay $\delta \in \{1,2,3,4\}$ ms is stored as a float32 in the synapse `delay` field. The quantization error is bounded by:
$$\varepsilon_\delta \leq \varepsilon_{\text{machine}} \cdot \delta_{\max} \approx 1.19 \times 10^{-7} \cdot 4 = 4.76 \times 10^{-7}\ \text{ms}.$$
This is negligible relative to the $1\ \text{ms}$ tick resolution; the effective delay is exact in integer ticks.

**Proof.** IEEE 754 float32 has 24 bits of mantissa precision. The value 4.0 is represented exactly ($2^2$). Values 1.0, 2.0, 3.0 are also exact in binary floating point. Therefore all valid $\delta$ values are stored without representation error. The integer tick conversion $\delta_{\text{ticks}} = \text{round}(\delta / dt)$ with $dt = 1\ \text{ms}$ is exact. ∎

**Theorem 4 (No State Divergence).** All state variables $(V, g_{\text{exc}}, g_{\text{inh}}, \text{spike\_timer}, \varphi)$ remain bounded for all $t \geq 0$.

**Proof.**
- $V \in [-75, -52.5]\ \text{mV}$ by reset and threshold; leak pulls toward $-70\ \text{mV}$.
- $g_{\text{exc}} \in [0, \Delta g_{\text{pulse}}]$ by pulsed injection and exponential decay.
- $g_{\text{inh}} \in [0, N_{\text{CWR}} \cdot w_{\text{CWR}}]$ by bounded convergent input and decay; with $N_{\text{CWR}} = 64$ and $w_{\text{CWR}} = 0.2\ \text{nS}$, $\sup g_{\text{inh}} \leq 12.8\ \text{nS}$ (though typical active count is $\leq 4$ simultaneous arrivals).
- $\text{spike\_timer} \in \{0,\dots,5\}$.
- $\varphi \in [0, 2\pi)$.
All variables are bounded. ∎

#### 3.4 Complexity Proof

**Theorem 5 (O(1) Per-Neuron Cost).** The CWR master update consumes exactly 8 arithmetic operations and 2 conditional branches per tick. The CWR synaptic delivery to a GBGN target consumes exactly 1 synaptic weight application per target per spike event.

**Proof (Master).** Identical to RSP Theorem 5: $\leq 11$ scalar operations per tick, no loops, no recursion, no dynamic allocation.

**Proof (Delivery).** Each CWR master has constant out-degree $D_{\text{CWR}} \leq 64$ to GBGN targets (independent of total network size). Per spike, the system iterates over the outgoing synapse list of the firing master only. Since $D_{\text{CWR}}$ is bounded by a constant, delivery is $O(1)$ per spike. Per target per tick, reception is $O(k)$ where $k$ is the number of CWR inputs to that target; $k \leq 4$ by design (redundant masters). Thus $O(1)$. ∎

**Corollary 5.1 (Network-Wide Step Cost).** For $N$ GBGN targets receiving CWR input, the total CWR-related cost per global tick is $O(N)$, with a small constant factor determined by bounded fan-in.

#### 3.5 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Leak term | $-(V - V_{\text{rest}})$ | mV | ✓ |
| Ohmic term | $R_m \cdot \tilde{I}_{\text{syn}}$ | $\text{M}\Omega \cdot \text{nS} \cdot \text{mV} = \text{mV}$ | ✓ |
| Membrane increment | $(dt/\tau_m) \cdot [\dots]$ | $(\text{ms}/\text{ms}) \cdot \text{mV} = \text{mV}$ | ✓ |
| Conductance decay | $\exp(-dt/\tau_{\text{exc}})$ | dimensionless | ✓ |
| Delay | $\delta$ | ms | ✓ |
| Window function | $\mathbb{I}[(t \bmod 25) \leq \delta]$ | dimensionless | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test CWR-MC-01: Pulse Threshold Crossing**
- **Procedure:** Initialize CWR master at $V = -70\ \text{mV}$, $g_{\text{exc}} = 0$. Inject $\Delta g_{\text{pulse}} = 4.5, 5.0, 6.0\ \text{nS}$ at $t = 0$.
- **Pass criterion:** $V$ after update $\geq -55.0\ \text{mV}$ for all values in range. Spike must emit within 1 tick.
- **Measurement:** $V_{\text{pre}}$, $V_{\text{post}}$, spike flag.

**Test CWR-MC-02: Refractory Enforcement**
- **Procedure:** Deliver two consecutive external pulses at $t = 0$ and $t = 1$.
- **Pass criterion:** No second spike at $t = 1$; $\text{spike\_timer}$ must read $4$ after decrement.
- **Measurement:** Spike flags at $t = 0$ and $t = 1$.

**Test CWR-MC-03: Exact Periodicity**
- **Procedure:** Run CWR master for 1,000 ticks. Record all spike emission ticks.
- **Pass criterion:** All inter-spike intervals must equal exactly 25 ticks. Zero exceptions.
- **Measurement:** $\max | \text{ISI}_i - 25 | = 0$.

**Test CWR-MC-04: Delay Field Accuracy**
- **Procedure:** Configure synapse with $\delta = 1.0, 2.0, 3.0, 4.0\ \text{ms}$. Fire CWR master at $t = 0$. Record arrival tick at target.
- **Pass criterion:** Arrival tick must equal exactly $\delta$ for each setting. Float32 representation must be exact (no rounding error).
- **Measurement:** $t_{\text{arrive}} - t_{\text{fire}}$.

**Test CWR-MC-05: Target Conductance Increment**
- **Procedure:** Set GBGN target $g_{\text{inh}} = 0$. Deliver CWR spike with $w_{\text{CWR}} = 0.2\ \text{nS}$ and $\delta = 1\ \text{ms}$.
- **Pass criterion:** At $t = 1$, $g_{\text{inh}}$ must read exactly $0.2\ \text{nS}$ before decay.
- **Measurement:** $g_{\text{inh}}$ at $t = 1^-$ and $t = 1^+$.

#### 4.2 Complexity Compliance Tests

**Test CWR-CC-01: Constant Fan-Out**
- **Procedure:** Count outgoing LATERAL_INH synapses per CWR master for networks at scales 1K, 10K, 100K, 270K neurons.
- **Pass criterion:** Out-degree per CWR master must be $\leq 64$ and independent of total network size.
- **Measurement:** Synapse count per master / total GBGN targets.

**Test CWR-CC-02: Target Reception Bound**
- **Procedure:** For random GBGN targets, count incoming CWR synapses.
- **Pass criterion:** In-degree from CWR must be $\leq 4$ for all targets (redundant masters).
- **Measurement:** Max and mean CWR in-degree across 1,000 random targets.

**Test CWR-CC-03: No Global Operations**
- **Procedure:** Inspect CWR update and delivery algorithms.
- **Pass criterion:** No iteration over $N_{\text{total}}$ or $S_{\text{total}}$. Only per-neuron or per-synapse constant-time operations.
- **Measurement:** Static algorithmic inspection.

#### 4.3 Functional Objective Tests

**Test CWR-FO-01: Window Duration Precision**
- **Procedure:** Configure $\delta = 1, 2, 3, 4\ \text{ms}$. For each setting, run 100 cycles. Measure time from RSP sync ($t = 25n$) to CWR arrival at GBGN.
- **Pass criterion:** Mean arrival offset must equal $\delta$ with standard deviation $< 0.5\ \text{ms}$. All arrivals must occur within $\pm 0.5\ \text{ms}$ of programmed $\delta$.
- **Measurement:** $\text{offset}_i = t_{\text{arrive},i} - 25n$; compute mean and std.

**Test CWR-FO-02: Cycle-to-Cycle Consistency**
- **Procedure:** Run CWR→GBGN delivery for 1,000 cycles at $\delta = 4\ \text{ms}$. Record arrival jitter.
- **Pass criterion:** Jitter (max deviation from mean) must never exceed $1.0\ \text{ms}$. Standard deviation across all arrivals $< 0.5\ \text{ms}$.
- **Measurement:** $\sigma(t_{\text{arrive}})$ and $\max |t_{\text{arrive}} - (25n + 4)|$.

**Test CWR-FO-03: Subthreshold Marker Integrity**
- **Procedure:** Isolate a GBGN target. Deliver only CWR input (no other synaptic activity). Record peak $V$ deviation.
- **Pass criterion:** Peak depolarization or hyperpolarization from CWR marker must be $< 2.0\ \text{mV}$. The marker must not cause GBGN to fire.
- **Measurement:** $\max |V(t) - V_{\text{rest}}|$ over 10 ms after CWR arrival.

**Test CWR-FO-04: Window Edge Definition**
- **Procedure:** Using a test harness that simulates CDD logic, verify that coincidence events arriving at $t = 0, 1, 2, 3, 4\ \text{ms}$ after sync are accepted when $\delta = 4$, and events at $t = 5\ \text{ms}$ are rejected.
- **Pass criterion:** Acceptance rate for $t \leq \delta$ must be $100\%$; rejection rate for $t > \delta$ must be $100\%$.
- **Measurement:** Boolean acceptance vector across arrival times.

**Test CWR-FO-05: Master Redundancy**
- **Procedure:** Disable 50% of CWR masters (32 of 64). Verify window timing at GBGN targets.
- **Pass criterion:** FO-01 and FO-02 must still pass. Remaining masters maintain timing precision.
- **Measurement:** Same as FO-01 and FO-02 under degraded master count.

---

**End of Sub-Component 4.3 CWR Contract**

**Next in dependency order: 4.1 Reticular Phase Gating Nuclei (RPGN).**

**Awaiting directive to proceed.**
User: continue
