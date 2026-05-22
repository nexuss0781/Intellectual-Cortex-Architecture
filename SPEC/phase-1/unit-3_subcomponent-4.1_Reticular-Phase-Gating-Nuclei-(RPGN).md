Kimi: **PHASE 1 | SUB-COMPONENT 4.1: Reticular Phase Gating Nuclei (RPGN)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Receive an external attentional demand signal between 0 and 1 and convert it into a smooth, sustained gain factor that controls how strongly binding and memory circuits operate.
- Broadcast this gain factor to all binding gate neurons and attractor memory neurons so their effective synaptic strengths scale proportionally.
- Ensure the gain factor cannot exceed 1 or drop below 0, preventing runaway excitation or complete silencing.
- Update the gain smoothly with a 100-millisecond time constant so brief noise in the demand signal does not destabilize the binding pathways.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| External precision demand | $\pi_{\text{input}}(t)$ | $[0, 1]$ | Scalar attentional gain command from stubbed Phase 3 or external bus |
| Phase reference | $\varphi_{\text{ref}}(t)$ | $[0, 2\pi)$ | Canonical gamma phase from 4.2 RSP |
| Binding window close | $t_{\text{close}}$ | $\mathbb{Z}_{\geq 0}$ | Timing mark from 4.3 CWR (used to gate broadcast) |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Precision state | $\pi_{\text{RPGN}}(t)$ | $[0, 1]$ | Low-pass filtered gain factor maintained by each PM neuron |
| Broadcast precision | $\pi_{\text{broadcast}}(t)$ | $[0, 1]$ | Value transmitted to targets via PRECISION_GATE synapses |
| Effective target gain | $\pi_j(t)$ | $[0, 1]$ | Precision field at target neuron $j$ after reception |

#### 2.3 State Space Definition

Each RPGN master neuron occupies a PM slot with the following active state:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V$ | mV | $-70.0$ | Standard LIF integration |
| Excitatory conductance | $g_{\text{exc}}$ | nS | $0.0$ | Receives external drive |
| Inhibitory conductance | $g_{\text{inh}}$ | nS | $0.0$ | Standard UIN field |
| Dynamic threshold | $\theta_{\text{dyn}}$ | mV | $-55.0$ | Adaptive firing bound |
| Slow gate | $s_{\text{slow}}$ | dimensionless | $0.0$ | Sustained activity trace |
| Oscillatory phase | $\varphi$ | rad | $0.0$ | Gamma cycle tracking |
| Precision | $\pi$ | dimensionless | $0.0$ | **PM-specific output gain factor** |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Standard countdown |
| STDP trace | $\text{trace}$ | dimensionless | $0.0$ | Eligibility trace |
| Type identifier | $\text{type\_id}$ | — | $2$ | PM (Precision Modulator) |
| Flags | $\text{flags}$ | uint16 | $\text{FLAG\_INTELLECTUAL\_POOL}$ | Pool membership |

Each PRECISION_GATE synapse to target $j$ carries:

| Field | Symbol | Unit / Value | Role |
|-------|--------|--------------|------|
| Postsynaptic index | $\text{post\_id}$ | — | Fixed routing to BG or AS neuron |
| Base efficacy | $w_{\text{PG}}$ | $0.5$ | Dimensionless scaling constant |
| Axonal delay | $\delta_{\text{PG}}$ | $[0, 1]\ \text{ms}$ | Near-instant broadcast |
| Precision payload | $\text{syn.precision}$ | $\pi_{\text{RPGN}}(t)$ | Current gain value at send time |
| Tag byte 0 | $\text{tag}[0]$ | $0\text{b}10000100$ | Class=4 (PRECISION_GATE); routing key=RPGN |

#### 2.4 Governing Equations

**RPGN PM Neuron (per tick, $dt = 1\ \text{ms}$):**

1. **Precision adaptation (PM-specific branch, universal kernel step 9):**
   $$\pi(t+1) = \text{clip}_{[0,1]}\Bigl(\pi(t) + \frac{dt}{\tau_\pi}\bigl(-\pi(t) + \pi_{\text{input}}(t)\bigr)\Bigr)$$
   with $\tau_\pi = 100\ \text{ms}$, so $dt/\tau_\pi = 0.01$.

2. **Standard LIF dynamics (if $\text{spike\_timer} = 0$):**
   Conductance decay and integration follow the universal kernel steps 2–4:
   $$g_{\text{exc}}(t+1) = g_{\text{exc}}(t) \cdot \exp(-dt/\tau_{\text{exc}})$$
   $$I_{\text{syn}} = g_{\text{exc}}(t) \cdot \bigl(E_{\text{exc}} - V(t)\bigr) + g_{\text{inh}}(t) \cdot \bigl(E_{\text{inh}} - V(t)\bigr)$$
   $$V(t+1) = V(t) + \frac{dt}{\tau_m}\Bigl[-\bigl(V(t) - V_{\text{rest}}\bigr) + R_m \cdot I_{\text{syn}}(t)\Bigr]$$

3. **Dynamic threshold (universal kernel step 6):**
   $$\theta_{\text{dyn}}(t+1) = \theta_{\text{dyn}}(t) + \frac{dt}{\tau_\theta}\Bigl[-\bigl(\theta_{\text{dyn}}(t) - \theta_{\text{base}}\bigr) + \beta \cdot S(t)\Bigr]$$
   where $S(t) \in \{0,1\}$ is the spike indicator, $\theta_{\text{base}} = -55.0\ \text{mV}$, $\tau_\theta = 100\ \text{ms}$, $\beta = 2.0\ \text{mV}$.

4. **Firing condition (universal kernel step 11):**
   If $V(t+1) \geq \theta_{\text{dyn}}(t+1)$:
   - Emit spike: $S(t+1) = 1$
   - Reset: $V(t+1) \leftarrow V_{\text{reset}} = -75.0\ \text{mV}$
   - Refractory: $\text{spike\_timer} \leftarrow 5$

5. **Refractory countdown (if $\text{spike\_timer} > 0$):**
   $$\text{spike\_timer} \leftarrow \text{spike\_timer} - 1$$
   Skip steps 2–4.

6. **Phase rotation (universal kernel step 7):**
   $$\varphi(t+1) = \bigl(\varphi(t) + \omega \cdot dt\bigr) \bmod 2\pi$$
   with $\omega = 2\pi \cdot 40\ \text{Hz}$.

7. **Slow gate (universal kernel step 5):**
   $$s_{\text{slow}}(t+1) = s_{\text{slow}}(t) + \frac{dt}{\tau_s}\bigl(-s_{\text{slow}}(t) + \alpha \cdot S(t)\bigr)$$
   with $\tau_s = 200\ \text{ms}$, $\alpha = 0.3$.

**PRECISION_GATE Broadcast to Target $j$:**

8. **Phase-locked broadcast trigger:**
   At $t = 25n + 1$ (one tick after RSP sync, within the open binding window):
   Each RPGN neuron emits a PRECISION_GATE event to all registered targets.

9. **Synaptic payload encoding:**
   The synapse carries the current precision value as its effective weight:
   $$w_{\text{eff}}(t) = w_{\text{PG}} \cdot \pi(t)$$

10. **Target precision update upon arrival:**
    Upon reception at target $j$ at tick $t_{\text{arr}}$:
    $$\pi_j(t_{\text{arr}}^+) = \pi_j(t_{\text{arr}}) + w_{\text{eff}}(t_{\text{send}})$$

11. **Target precision decay (universal kernel step 9, applied to all neurons):**
    $$\pi_j(t+1) = \text{clip}_{[0,1]}\Bigl(\pi_j(t) \cdot \exp(-dt/\tau_\pi) + \text{input}_{\text{PG}}(t)\Bigr)$$
    where $\text{input}_{\text{PG}}(t)$ is the sum of all PRECISION_GATE events received at tick $t$.

12. **Effective target gain (consumed by downstream components):**
    $$g_{\text{eff},j}(t) = \pi_j(t) \cdot g_{\text{base},j}(t)$$
    This scaling is applied by the target neuron (GBGN or AS) during its own synaptic integration.

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Resting potential | $V_{\text{rest}}$ | $-70.0$ | mV | $-72.0$ | $-68.0$ | Baseline membrane state |
| Firing threshold base | $\theta_{\text{base}}$ | $-55.0$ | mV | $-57.0$ | $-53.0$ | Spike emission boundary |
| Reset potential | $V_{\text{reset}}$ | $-75.0$ | mV | $-77.0$ | $-73.0$ | Post-spike membrane clamp |
| Membrane time constant | $\tau_m$ | $20.0$ | ms | $18.0$ | $22.0$ | Integration speed |
| Excitatory reversal | $E_{\text{exc}}$ | $0.0$ | mV | — | — | Excitatory current reversal |
| Inhibitory reversal | $E_{\text{inh}}$ | $-75.0$ | mV | — | — | Inhibitory current reversal |
| Membrane resistance | $R_m$ | $1.0$ | M$\Omega$ | $0.8$ | $1.2$ | Ohmic scaling |
| Excitatory synapse time constant | $\tau_{\text{exc}}$ | $5.0$ | ms | $4.5$ | $5.5$ | Conductance decay speed |
| Inhibitory synapse time constant | $\tau_{\text{inh}}$ | $10.0$ | ms | $9.0$ | $11.0$ | Inhibitory conductance decay |
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |
| Precision adaptation time constant | $\tau_\pi$ | $100.0$ | ms | $90.0$ | $110.0$ | Smoothing of gain command |
| Dynamic threshold time constant | $\tau_\theta$ | $100.0$ | ms | $90.0$ | $110.0$ | Threshold adaptation speed |
| Dynamic threshold jump | $\beta$ | $2.0$ | mV | $1.5$ | $2.5$ | Post-spike threshold increase |
| Slow gate time constant | $\tau_s$ | $200.0$ | ms | $180.0$ | $220.0$ | Sustained activity decay |
| Slow gate increment | $\alpha$ | $0.3$ | — | $0.25$ | $0.35$ | Post-spike gate boost |
| Gamma frequency | $f_\gamma$ | $40.0$ | Hz | $38.0$ | $42.0$ | Oscillation frequency |
| Gamma period | $T_\gamma$ | $25.0$ | ticks | $24$ | $26$ | Cycle length in time steps |
| PRECISION_GATE base weight | $w_{\text{PG}}$ | $0.5$ | dimensionless | $0.4$ | $0.6$ | Broadcast scaling |
| Max broadcast delay | $\delta_{\text{PG}}$ | $1.0$ | ms | $0.0$ | $1.0$ | Axonal latency bound |
| RPGN master count | $N_{\text{RPGN}}$ | $256$ | — | $128$ | $512$ | Redundant gain controllers |
| Max targets per RPGN | $D_{\text{RPGN}}$ | $16$ | — | $8$ | $32$ | Fan-out to BG/AS pools |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | PM ($\text{type\_id} = 2$) | Precision Modulator; key state is `precision` |
| Output synapse type | PRECISION_GATE (type 4) | Targets the `precision` field of postsynaptic neurons |
| Tag encoding | $\text{tag}[0] = 0\text{b}10000100$ | Bits [0:2]=4 (PRECISION_GATE); bits [5:7]=100 (RPGN routing key) |
| Source field | `precision` (PM neuron) | Adapted toward $\pi_{\text{input}}$ every tick |
| Target field | `precision` (BG/AS neuron) | Consumed as multiplicative gain factor |
| Broadcast timing | Phase-locked to $t \equiv 1 \pmod{25}$ | One tick after RSP sync |

#### 2.7 Interface Contract

**Upstream providers:**
- **Input C bus:** delivers scalar $\pi_{\text{input}}(t) \in [0,1]$ each tick.
- **4.2 RSP:** delivers canonical phase $\varphi_{\text{ref}}(t)$; RPGN broadcasts at $\varphi \approx 0$ (cycle start).
- **4.3 CWR:** delivers window-close timing; RPGN broadcast occurs only while window is open ($t \bmod 25 \leq \delta_{\text{CWR}}$).

**Downstream consumers:**
- **2.1 GBGN** (Granule Binding Gate Neurons): receives $\pi_j(t)$ and scales binding conductance $g_{\text{bind}}$ by this factor.
- **3.1 CA3-RAN** (Hippocampal Cleanup): receives $\pi_j(t)$ and scales recurrent excitation strength by this factor.
- **5.3 VBSG** (Variable Binding Swap Gates): receives $\pi_j(t)$ to modulate binding/unbinding gain.

**Handshake format:**
- PRECISION_GATE events carry $\text{post\_id}$, $w_{\text{eff}} = w_{\text{PG}} \cdot \pi_{\text{RPGN}}$, $\text{delay} \in [0,1]\ \text{ms}$, and tag byte with RPGN routing key.
- The payload is multiplicative: target multiplies its base synaptic efficacy by the received precision sum.

---

### 3. Stability & Rigor Analysis

#### 3.1 Fixed-Point and Convergence

**Theorem 1 (Precision Adaptation Stability).** The precision update equation
$$\pi(t+1) = \pi(t) + \alpha\bigl(-\pi(t) + \pi_{\text{input}}(t)\bigr)$$
with $\alpha = dt/\tau_\pi = 0.01$ is a stable first-order linear system with pole at $z = 1 - \alpha = 0.99$.

**Proof.** For constant input $\pi_{\text{input}}(t) = \bar{\pi}$, the fixed point satisfies $\pi^* = \pi^* + \alpha(-\pi^* + \bar{\pi})$, yielding $\pi^* = \bar{\pi}$. The homogeneous solution is $\pi_h(t) = C \cdot (1-\alpha)^t$. Since $|1-\alpha| = 0.99 < 1$, the mode decays exponentially. The system is BIBO stable: bounded input $\pi_{\text{input}} \in [0,1]$ produces bounded output $\pi \in [0,1]$ (enforced by clipping). ∎

**Corollary 1.1 (Step Response).** For a step input from $0$ to $\bar{\pi}$ at $t = 0$:
$$\pi(t) = \bar{\pi} \cdot \bigl(1 - 0.99^t\bigr)$$
The response reaches $63\%$ of $\bar{\pi}$ at $t = \tau_\pi = 100$ ticks ($100\ \text{ms}$), and reaches $95\%$ at $t \approx 300$ ticks ($300\ \text{ms}$).

**Corollary 1.2 (Impulse Response Decay).** For a delta input of amplitude $A$:
$$\pi(t) = A \cdot \alpha \cdot 0.99^{t-1}, \quad t \geq 1$$
The impulse energy decays with time constant exactly $\tau_\pi = 100\ \text{ms}$.

#### 3.2 Convergence Bounds

**Theorem 2 (Tracking Error Bound).** For a ramp input $\pi_{\text{input}}(t) = r \cdot t$ with slope $r \leq 0.01\ \text{ms}^{-1}$ (i.e., full-scale traversal over $100\ \text{ms}$), the steady-state tracking lag is bounded by:
$$\limsup_{t \to \infty} \bigl|\pi(t) - \pi_{\text{input}}(t)\bigr| \leq r \cdot \tau_\pi = r \cdot 100\ \text{ms}.$$

**Proof.** In continuous time, the system is $\tau_\pi \dot{\pi} + \pi = \pi_{\text{input}}$. For ramp input $r \cdot t$, the particular solution is $\pi_p(t) = r(t - \tau_\pi) + r\tau_\pi = r \cdot t - r\tau_\pi + r\tau_\pi(1 - e^{-t/\tau_\pi})$. The steady-state lag is exactly $r \cdot \tau_\pi$. The discrete-time Euler approximation overestimates the time constant by $O(dt)$ but preserves the bound because $\alpha < 1$. ∎

**Theorem 3 (Settling Time After Step).** After a full-scale step (0 to 1 or 1 to 0), the precision output settles within $\varepsilon = 0.01$ (1%) of the final value in:
$$t_{\text{settle}} \leq \frac{\ln(1/\varepsilon)}{-\ln(1-\alpha)} \approx \frac{4.605}{0.01005} \approx 458\ \text{ticks} \approx 458\ \text{ms}.$$

**Proof.** The envelope of the transient is $(1-\alpha)^t$. Solving $(0.99)^t \leq 0.01$ yields $t \geq \ln(0.01)/\ln(0.99) \approx 458$. ∎

#### 3.3 Numerical Stability

**Theorem 4 (No Overflow or Divergence).** All state variables $(V, g_{\text{exc}}, g_{\text{inh}}, \theta_{\text{dyn}}, s_{\text{slow}}, \varphi, \pi, \text{spike\_timer})$ remain bounded for all $t \geq 0$.

**Proof.**
- $V \in [-75, -52.5]\ \text{mV}$ by reset and threshold; leak pulls toward $-70\ \text{mV}$.
- $g_{\text{exc}} \in [0, \Delta g_{\text{max}}]$ by bounded input and exponential decay.
- $g_{\text{inh}} \in [0, G_{\text{inh,max}}]$ by bounded input and decay.
- $\theta_{\text{dyn}} \in [\theta_{\text{base}}, \theta_{\text{base}} + \beta] = [-55, -53]\ \text{mV}$ approximately (actually can grow with repeated spikes but bounded by adaptation dynamics; with $\beta = 2$ and decay, it stays within $[-55, -50]$ practically).
- $s_{\text{slow}} \in [0, \alpha] = [0, 0.3]$ by construction.
- $\varphi \in [0, 2\pi)$.
- $\pi \in [0, 1]$ by explicit clipping.
- $\text{spike\_timer} \in \{0,\dots,5\}$.
All variables are bounded. ∎

**Theorem 5 (Float32 Precision Error Bound).** The precision adaptation accumulates rounding error bounded by:
$$\varepsilon_{\text{round}}(t) \leq \varepsilon_{\text{machine}} \cdot \frac{1 - (1-\alpha)^t}{\alpha} < \varepsilon_{\text{machine}} / \alpha \approx 1.19 \times 10^{-5}$$
for IEEE 754 float32 ($\varepsilon_{\text{machine}} \approx 1.19 \times 10^{-7}$).

**Proof.** Each update involves one multiplication by $(1-\alpha) = 0.99$ and one addition of $\alpha \cdot \pi_{\text{input}}$. The recurrence for rounding error $e(t)$ satisfies $e(t+1) \leq (1-\alpha) \cdot e(t) + \varepsilon_{\text{machine}}$. The steady-state bound is $\varepsilon_{\text{machine}} / \alpha$. Since $\alpha = 0.01$, the bound is $\sim 10^{-5}$, negligible relative to the $[0,1]$ dynamic range. ∎

#### 3.4 Complexity Proof

**Theorem 6 (O(1) Per-Neuron Cost).** The RPGN PM neuron update consumes exactly 30 scalar operations per tick: 25 for the universal kernel plus 5 for precision adaptation and clipping.

**Proof.** Universal kernel: 25 FLOPs (proven in UIN report). PM-specific additions:
- Compute error: $-\pi(t) + \pi_{\text{input}}(t)$ (1 subtraction)
- Scale: $\alpha \cdot \text{error}$ (1 multiplication)
- Update: $\pi(t) + \text{scaled\_error}$ (1 addition)
- Clip to $[0,1]$: 2 comparisons + conditional assignments (effectively 2 FLOPs).
Total: $25 + 5 = 30$ scalar operations. No loops, no recursion, no dynamic allocation. ∎

**Theorem 7 (O(1) Broadcast Cost).** Delivery of PRECISION_GATE events from one RPGN neuron to its targets costs $O(D_{\text{RPGN}})$ where $D_{\text{RPGN}} \leq 16$ is a fixed constant. Per target, reception is $O(k)$ where $k \leq 2$ is the number of RPGN inputs.

**Proof.** Each RPGN neuron iterates over its outgoing synapse list of fixed length $\leq 16$. For each synapse, it performs one multiplication ($w_{\text{PG}} \cdot \pi$) and one delivery call. This is $O(1)$ because 16 is a compile-time bound. Each target receives from at most 2 RPGN neurons (redundancy), so per-target reception is $O(1)$. No global aggregation or all-to-all communication occurs. ∎

**Corollary 7.1 (Network-Wide Cost).** For $N_{\text{RPGN}} = 256$ masters and $N_{\text{targets}} \leq 40{,}000$ (BG + AS pools), the total RPGN-related work per tick is $O(N_{\text{RPGN}} + N_{\text{targets}}) = O(N)$ with small constants.

#### 3.5 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Precision increment | $\alpha \cdot (-\pi + \pi_{\text{input}})$ | $(\text{ms}/\text{ms}) \cdot \text{dimensionless} = \text{dimensionless}$ | ✓ |
| Membrane leak | $-(V - V_{\text{rest}})$ | mV | ✓ |
| Ohmic term | $R_m \cdot I_{\text{syn}}$ | $\text{M}\Omega \cdot \text{nS} \cdot \text{mV} = \text{mV}$ | ✓ |
| Membrane increment | $(dt/\tau_m) \cdot [\dots]$ | $(\text{ms}/\text{ms}) \cdot \text{mV} = \text{mV}$ | ✓ |
| Threshold adaptation | $(dt/\tau_\theta) \cdot [-\Delta\theta + \beta \cdot S]$ | $(\text{ms}/\text{ms}) \cdot \text{mV} = \text{mV}$ | ✓ |
| Phase increment | $\omega \cdot dt$ | $(\text{rad}/\text{s}) \cdot \text{s} = \text{rad}$ | ✓ |
| Effective gain | $w_{\text{PG}} \cdot \pi$ | $\text{dimensionless} \cdot \text{dimensionless} = \text{dimensionless}$ | ✓ |
| Target precision decay | $\exp(-dt/\tau_\pi)$ | dimensionless | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test RPGN-MC-01: Precision Step Response**
- **Procedure:** Initialize $\pi = 0$. Apply $\pi_{\text{input}} = 1.0$ at $t = 0$. Record $\pi(t)$ for $t = 0$ to $500$.
- **Pass criterion:** $\pi(100)$ must be within $[0.62, 0.65]$ (theoretical $1 - 0.99^{100} \approx 0.634$). $\pi(300)$ must be $\geq 0.95$. Final value must be exactly $1.0$ (within float32 precision).
- **Measurement:** $\pi(t)$ trajectory vs. theoretical $1 - 0.99^t$.

**Test RPGN-MC-02: Precision Decay Response**
- **Procedure:** Initialize $\pi = 1.0$. Apply $\pi_{\text{input}} = 0.0$ at $t = 0$. Record $\pi(t)$.
- **Pass criterion:** $\pi(100)$ must be within $[0.35, 0.38]$ (theoretical $0.99^{100} \approx 0.366$). $\pi(500)$ must be $\leq 0.01$.
- **Measurement:** Exponential decay envelope.

**Test RPGN-MC-03: Clipping Bounds**
- **Procedure:** Apply $\pi_{\text{input}} = 2.0$ (out of bounds) for 100 ticks, then $\pi_{\text{input}} = -1.0$ for 100 ticks.
- **Pass criterion:** $\pi(t)$ must never exceed $1.0$ or drop below $0.0$ at any tick.
- **Measurement:** $\max \pi(t)$ and $\min \pi(t)$ over the run.

**Test RPGN-MC-04: Sinusoidal Tracking Lag**
- **Procedure:** Apply $\pi_{\text{input}}(t) = 0.5 + 0.5 \cdot \sin(2\pi f t)$ with $f = 1\ \text{Hz}$ (period $1000\ \text{ms}$). Record $\pi(t)$.
- **Pass criterion:** Output amplitude must be $\geq 0.45$ (attenuation $< 10\%$ at $f \ll 1/\tau_\pi$). Phase lag must be $\leq 10$ ticks ($\leq 10\ \text{ms}$) for this low frequency.
- **Measurement:** Cross-correlation peak and amplitude ratio.

**Test RPGN-MC-05: LIF Firing Integrity**
- **Procedure:** Inject sufficient $g_{\text{exc}}$ to drive RPGN PM neuron above threshold. Verify spike emission and reset.
- **Pass criterion:** Must fire when $V \geq \theta_{\text{dyn}}$; must reset to $-75\ \text{mV}$; must enter 5-tick refractory.
- **Measurement:** Spike flag, $V_{\text{post}}$, $\text{spike\_timer}$.

#### 4.2 Complexity Compliance Tests

**Test RPGN-CC-01: Constant Fan-Out**
- **Procedure:** Count outgoing PRECISION_GATE synapses per RPGN master across all network scales.
- **Pass criterion:** Out-degree must be $\leq 16$ for every master, independent of total network size.
- **Measurement:** $\max_{i} D_{\text{out},i}$.

**Test RPGN-CC-02: Target Reception Bound**
- **Procedure:** For random BG and AS targets, count incoming RPGN synapses.
- **Pass criterion:** In-degree from RPGN must be $\leq 2$ for all targets.
- **Measurement:** Max and mean RPGN in-degree across 1,000 random targets.

**Test RPGN-CC-03: No Global Aggregation**
- **Procedure:** Inspect precision broadcast algorithm.
- **Pass criterion:** No instruction may sum over all neurons or all synapses. Only per-neuron constant-time operations.
- **Measurement:** Static code / algorithmic inspection.

**Test RPGN-CC-04: PM Branch Isolation**
- **Procedure:** Verify that non-PM neurons do not execute precision adaptation step 9.
- **Pass criterion:** Type-specific branch on $\text{type\_id} = 2$ must guard the precision adaptation; other types skip it.
- **Measurement:** Instruction path analysis.

#### 4.3 Functional Objective Tests

**Test RPGN-FO-01: Gain Modulation of Binding**
- **Procedure:** Configure a GBGN target with base binding conductance $g_{\text{bind}} = 1.0\ \text{nS}$. Set RPGN $\pi = 0.0$ and verify $g_{\text{eff}} = 0$. Ramp RPGN $\pi$ to $1.0$ over 100 ticks.
- **Pass criterion:** GBGN effective conductance must track $g_{\text{eff}} = \pi \cdot g_{\text{base}}$ with correlation $R^2 > 0.99$.
- **Measurement:** Time series of $\pi(t)$ and $g_{\text{eff}}(t)$.

**Test RPGN-FO-02: Gain Modulation of Attractor Memory**
- **Procedure:** Configure a CA3-RAN AS target with base recurrent weight $w_{\text{rec}} = 0.5\ \text{nS}$. Sweep RPGN $\pi$ across $[0, 1]$.
- **Pass criterion:** Effective recurrent strength must equal $\pi \cdot w_{\text{rec}}$ at steady state. At $\pi = 0$, recurrent strength must be $0$ (attractor disabled). At $\pi = 1$, full strength restored.
- **Measurement:** Steady-state effective weight vs. $\pi$.

**Test RPGN-FO-03: Phase-Locked Broadcast Timing**
- **Procedure:** Run RSP and RPGN together for 100 cycles. Record PRECISION_GATE arrival times at 50 targets.
- **Pass criterion:** All arrivals must occur within $t \in [25n, 25n+2]$ (i.e., within the first 2 ms of each gamma cycle). No broadcasts outside this window.
- **Measurement:** Arrival tick modulo 25.

**Test RPGN-FO-04: Redundancy Without Conflict**
- **Procedure:** Connect 2 RPGN masters to the same target with different precision values ($\pi_1 = 0.3$, $\pi_2 = 0.7$).
- **Pass criterion:** Target precision must equal the sum (or average, depending on implementation) without overflow beyond $[0,1]$. If summed, clip at $1.0$.
- **Measurement:** Target precision field after both inputs delivered.

**Test RPGN-FO-05: Noise Rejection**
- **Procedure:** Apply $\pi_{\text{input}}$ consisting of a constant $0.5$ plus high-frequency noise (amplitude $\pm 0.2$, period $5\ \text{ms}$).
- **Pass criterion:** Output $\pi(t)$ must remain within $[0.45, 0.55]$; noise attenuation $\geq 80\%$ (output ripple $\leq 0.04$).
- **Measurement:** Peak-to-peak ripple amplitude at output.

---

**End of Sub-Component 4.1 RPGN Contract**

**Next in dependency order: 1.1 Pyramidal SDR Generators (PSG).**

**Awaiting directive to proceed.**
User: continue
