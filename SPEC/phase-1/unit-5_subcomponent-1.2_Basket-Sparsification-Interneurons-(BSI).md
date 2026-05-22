Kimi: **PHASE 1 | SUB-COMPONENT 1.2: Basket Sparsification Interneurons (BSI)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Receive spike notifications from a fixed local group of 64 excitatory encoding neurons and compute how many of them fired.
- Fire an inhibitory pulse back to that same group whenever two or more members fired, so that only the strongest one or two members remain active in subsequent ticks.
- Ensure every inhibitory neuron only needs to monitor its own group of 64, never the entire population.
- Keep the inhibitory signal bounded so it suppresses weak responses without permanently silencing the entire group.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| PSG spike vector (pool $m$) | $\mathbf{s}_m(t)$ | $\{0,1\}^K$ | Binary indicators for $K$ PSG neurons in local pool $m$ |
| Pool index | $m$ | $\{1, \dots, M\}$ | $M = 32$ disjoint pools covering all PSG neurons |
| Pool size | $K$ | $64$ | Constant neurons per pool |
| PSG-to-BSI excitatory weight | $w_{\text{bsi,in}}$ | $[2.2, 2.8]\ \text{nS}$ | Feedforward excitation from PSG to BSI |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| BSI spike indicator (pool $m$) | $S_{\text{bsi},m}(t)$ | $\{0,1\}$ | $1$ iff BSI neuron $m$ fired at tick $t$ |
| Inhibitory conductance (PSG $i$ in pool $m$) | $g_{\text{inh},i}(t)$ | $[0, \infty)\ \text{nS}$ | Accumulated LATERAL_INH from BSI $m$ |
| Effective inhibition state | $G_{\text{inh},m}(t)$ | $[0, M_{\text{inh}}]\ \text{nS}$ | Total inhibitory drive delivered to pool $m$ |

#### 2.3 State Space Definition

Each BSI neuron occupies a CI slot:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V_{\text{bsi}}$ | mV | $-70.0$ | LIF integration of pooled PSG input |
| Excitatory conductance | $g_{\text{exc}}$ | nS | $0.0$ | Receives PSG spikes from pool members |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Standard countdown |
| Oscillatory phase | $\varphi$ | rad | $0.0$ | Gamma cycle tracking |
| Type identifier | $\text{type\_id}$ | — | $0$ (CI) | Fixed neuron class |
| Flags | $\text{flags}$ | uint16 | $\text{FLAG\_INTELLECTUAL\_POOL}$ | Pool membership |

Each BSI→PSG synapse carries:

| Field | Symbol | Value | Role |
|-------|--------|-------|------|
| Postsynaptic index | $\text{post\_id}$ | PSG neuron $i \in \mathcal{P}_m$ | Fixed routing within pool |
| Efficacy | $w_{\text{inh}}$ | $[1.5, 2.5]\ \text{nS}$ | LATERAL_INH suppression weight |
| Axonal delay | $\delta_{\text{inh}}$ | $0\ \text{ms}$ | Same-tick delivery (0 ticks) |
| Tag byte 0 | $\text{tag}[0]$ | $0\text{b}01100001$ | Class=3 (LATERAL_INH); routing key=BSI |

#### 2.4 Governing Equations

**BSI Neuron $m$ (per tick, $dt = 1\ \text{ms}$):**

1. **Spike arrival from PSG pool.** At the start of tick $t$, for each PSG neuron $j \in \mathcal{P}_m$ that fired at $t-1$:
   $$g_{\text{exc}}(t^+) = g_{\text{exc}}(t) + w_{\text{bsi,in}} \cdot s_j(t-1)$$
   Equivalently, if $N_m(t-1) = \sum_{j \in \mathcal{P}_m} s_j(t-1)$ is the pool activity count:
   $$g_{\text{exc}}(t^+) = g_{\text{exc}}(t) + w_{\text{bsi,in}} \cdot N_m(t-1)$$

2. **Conductance decay (all ticks):**
   $$g_{\text{exc}}(t+1) = g_{\text{exc}}(t^+) \cdot \exp(-dt/\tau_{\text{exc}})$$
   with $\tau_{\text{exc}} = 5\ \text{ms}$.

3. **Synaptic current (if $\text{spike\_timer} = 0$):**
   $$I_{\text{syn}}(t) = g_{\text{exc}}(t^+) \cdot \bigl(E_{\text{exc}} - V(t)\bigr)$$
   where $E_{\text{exc}} = 0.0\ \text{mV}$.

4. **Membrane update:**
   $$V(t+1) = V(t) + \frac{dt}{\tau_m}\Bigl[-\bigl(V(t) - V_{\text{rest}}\bigr) + R_m \cdot I_{\text{syn}}(t)\Bigr]$$
   with $V_{\text{rest}} = -70.0\ \text{mV}$, $\tau_m = 20\ \text{ms}$, $R_m = 1\ \text{M}\Omega$.

5. **Firing condition:**
   If $V(t+1) \geq \theta_{\text{base}} = -55.0\ \text{mV}$:
   - Emit spike: $S_{\text{bsi},m}(t+1) = 1$
   - Reset: $V(t+1) \leftarrow V_{\text{reset}} = -75.0\ \text{mV}$
   - Refractory: $\text{spike\_timer} \leftarrow 5$

6. **Refractory countdown (if $\text{spike\_timer} > 0$):**
   $$\text{spike\_timer} \leftarrow \text{spike\_timer} - 1$$
   Skip steps 3–5.

7. **Phase rotation (universal kernel):**
   $$\varphi(t+1) = \bigl(\varphi(t) + \omega \cdot dt\bigr) \bmod 2\pi$$
   with $\omega = 2\pi \cdot 40\ \text{Hz}$.

**BSI→PSG Inhibitory Delivery:**

8. **LATERAL_INH spike delivery.** If $S_{\text{bsi},m}(t) = 1$, for each target PSG neuron $i \in \mathcal{P}_m$:
   $$g_{\text{inh},i}(t^+) = g_{\text{inh},i}(t) + w_{\text{inh}}$$

9. **Target inhibitory decay (PSG universal kernel):**
   $$g_{\text{inh},i}(t+1) = g_{\text{inh},i}(t^+) \cdot \exp(-dt/\tau_{\text{inh}})$$
   with $\tau_{\text{inh}} = 10\ \text{ms}$.

**Pool Activity Aggregation (structural invariant):**

10. **Pool firing count.** The number of PSG neurons that fired in pool $m$ at tick $t$:
    $$N_m(t) = \sum_{j \in \mathcal{P}_m} s_j(t)$$
    This sum is over a constant-size set $|\mathcal{P}_m| = K = 64$, so it is $O(1)$.

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Pool size | $K$ | $64$ | neurons | $64$ | $64$ | Local neighborhood cardinality |
| Number of pools | $M$ | $32$ | — | $32$ | $32$ | Total BSI neurons |
| PSG-to-BSI weight | $w_{\text{bsi,in}}$ | $2.5$ | nS | $2.2$ | $2.8$ | Excitation per PSG spike |
| BSI-to-PSG inhibition | $w_{\text{inh}}$ | $2.0$ | nS | $1.5$ | $2.5$ | LATERAL_INH suppression |
| Resting potential | $V_{\text{rest}}$ | $-70.0$ | mV | $-72.0$ | $-68.0$ | Baseline membrane state |
| Firing threshold | $\theta_{\text{base}}$ | $-55.0$ | mV | $-57.0$ | $-53.0$ | Spike emission boundary |
| Reset potential | $V_{\text{reset}}$ | $-75.0$ | mV | $-77.0$ | $-73.0$ | Post-spike membrane clamp |
| Membrane time constant | $\tau_m$ | $20.0$ | ms | $18.0$ | $22.0$ | Integration speed |
| Excitatory reversal | $E_{\text{exc}}$ | $0.0$ | mV | — | — | Excitatory current reversal |
| Membrane resistance | $R_m$ | $1.0$ | M$\Omega$ | $0.8$ | $1.2$ | Ohmic scaling |
| Excitatory synapse time constant | $\tau_{\text{exc}}$ | $5.0$ | ms | $4.5$ | $5.5$ | BSI input conductance decay |
| Inhibitory synapse time constant | $\tau_{\text{inh}}$ | $10.0$ | ms | $9.0$ | $11.0$ | PSG inhibition decay |
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |
| Gamma frequency | $f_\gamma$ | $40.0$ | Hz | $38.0$ | $42.0$ | Oscillation frequency |
| Gamma period | $T_\gamma$ | $25.0$ | ticks | $24$ | $26$ | Cycle length |
| BSI firing threshold conductance | $\theta_{g,\text{bsi}}$ | $30/7$ | nS | $4.0$ | $4.6$ | Derived from LIF (same as PSG) |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | CI ($\text{type\_id} = 0$) | Core Integrator for pool integration |
| Input synapse type | FEEDFORWARD (type 0) | From PSG neurons in same pool |
| Output synapse type | LATERAL_INH (type 3) | To PSG neurons in same pool |
| Tag encoding (input) | $\text{tag}[0] = 0\text{b}00000011$ | Class=0 (FEEDFORWARD); routing key=BSI-input |
| Tag encoding (output) | $\text{tag}[0] = 0\text{b}01100001$ | Class=3 (LATERAL_INH); routing key=BSI |
| Source field | $g_{\text{exc}}$ (BSI) | Receives pooled PSG spikes |
| Target field | $g_{\text{inh}}$ (PSG) | Receives suppression conductance |

#### 2.7 Interface Contract

**Upstream providers:**
- **1.1 PSG** (Pyramidal SDR Generators): delivers spike events from neurons in pool $\mathcal{P}_m$ to BSI $m$.

**Downstream consumers:**
- **1.1 PSG** (same pool): receives LATERAL_INH suppression from BSI $m$.
- **1.3 SPPF** (Semantic Pointer Projection Fibers): receives the post-BSI sparse PSG pattern for forwarding.

**Handshake format:**
- BSI input spikes carry $\text{post\_id}$ (BSI target pool), $w_{\text{bsi,in}}$, $\text{delay} = 0$, and tag byte with BSI-input routing key.
- BSI output spikes carry $\text{post\_id}$ (PSG target within pool), $w_{\text{inh}}$, $\text{delay} = 0$, and tag byte with BSI routing key.

---

### 3. Stability & Rigor Analysis

#### 3.1 BSI Threshold and Pool Activation

**Theorem 1 (BSI Firing Threshold).** For BSI neuron $m$ at rest ($V = V_{\text{rest}}$) with no prior conductance, the minimum number of simultaneous PSG inputs required to trigger firing is exactly:
$$N_{\text{thr}} = \left\lceil \frac{\theta_g}{w_{\text{bsi,in}}} \right\rceil = \left\lceil \frac{30/7}{2.5} \right\rceil = \left\lceil 1.714 \right\rceil = 2$$

**Proof.** From the discrete LIF update at rest:
$$V_{\text{new}} = -70 + 3.5 \cdot g_{\text{exc}}$$
For firing: $V_{\text{new}} \geq -55 \implies g_{\text{exc}} \geq 30/7 \approx 4.286\ \text{nS}$.
With $N$ PSG inputs each of weight $w_{\text{bsi,in}} = 2.5\ \text{nS}$: $g_{\text{exc}} = 2.5N$.
For $N = 1$: $g_{\text{exc}} = 2.5 < 4.286$, BSI does not fire.
For $N = 2$: $g_{\text{exc}} = 5.0 \geq 4.286$, BSI fires.
Therefore $N_{\text{thr}} = 2$. ∎

**Corollary 1.1 (Single-Winner Pass-Through).** If exactly 1 PSG neuron in pool $m$ fires, BSI $m$ does not fire. The winner receives no inhibition from BSI and remains free to continue firing if its feedforward drive persists.

**Corollary 1.2 (Multi-Winner Suppression).** If $\geq 2$ PSG neurons in pool $m$ fire simultaneously, BSI $m$ fires and delivers $w_{\text{inh}} = 2.0\ \text{nS}$ of inhibition to all $K$ PSG neurons in the pool.

#### 3.2 Winner-Take-All Convergence

**Theorem 2 (WTA Convergence in Bounded Time).** Consider a pool $m$ where PSG neurons receive sustained feedforward excitation. Let the PSG neuron with strongest drive have effective conductance $g_{\text{exc}}^{(1)}$ and the second-strongest have $g_{\text{exc}}^{(2)}$. If:
$$g_{\text{exc}}^{(1)} - g_{\text{exc}}^{(2)} > w_{\text{inh}} \cdot \frac{\tau_{\text{exc}}}{\tau_{\text{exc}} + \tau_{\text{inh}}}$$
then the pool converges to exactly 1 active PSG neuron within at most 3 ticks.

**Proof sketch.** 
- **Tick 1:** Feedforward input arrives. Strongest PSG neurons fire. If $\geq 2$ fire, BSI detects this (via $N_m \geq 2$) and fires in the same tick (or next tick, depending on queue order; we account for worst-case 1-tick delay).
- **Tick 2:** BSI inhibition $w_{\text{inh}}$ arrives at all PSG neurons in the pool. The second-strongest PSG neuron, whose margin over threshold was less than $w_{\text{inh}}$, is pushed below firing threshold. The strongest PSG neuron, with sufficient margin, remains above threshold.
- **Tick 3:** Only the strongest PSG neuron fires. BSI detects $N_m = 1$ and does not fire. No inhibition is delivered. The system reaches steady state with exactly 1 winner.

The condition ensures the conductance gap exceeds the effective inhibition after accounting for decay dynamics. With $w_{\text{inh}} = 2.0\ \text{nS}$ and typical PSG drive $g_{\text{exc}} \in [4.5, 10]\ \text{nS}$, the margin condition is satisfied for well-separated inputs. ∎

**Theorem 3 (Global Sparsity Guarantee).** With $M = 32$ pools and WTA convergence to $\leq 1$ winner per pool, the global output density is bounded by:
$$\rho_{\text{out}} = \frac{\sum_{m=1}^M N_m(\infty)}{D_{\text{sp}}} \leq \frac{M}{D_{\text{sp}}} = \frac{32}{2048} = 0.015625 = 1.5625\%$$

**Proof.** By Theorem 2, each pool converges to at most 1 active neuron. With $M = 32$ pools and $D_{\text{sp}} = 2048$ total PSG neurons:
$$\rho_{\text{out}} \leq \frac{32}{2048} = \frac{1}{64} = 0.015625$$
This satisfies the Phase 1 sparsity requirement $\rho_{\text{out}} \in [0.015, 0.025]$. ∎

**Corollary 3.1 (Lower Sparsity Bound).** If some pools receive no feedforward drive above PSG threshold, they produce 0 winners. Therefore the actual density satisfies:
$$0 \leq \rho_{\text{out}} \leq 0.015625$$
The system naturally produces sparser output when input is weak.

#### 3.3 Convergence Bounds

**Theorem 4 (Inhibitory Decay).** After BSI delivers inhibition $w_{\text{inh}}$ at $t = 0$, the inhibitory conductance at a PSG neuron decays as:
$$g_{\text{inh}}(t) = w_{\text{inh}} \cdot \exp(-t/\tau_{\text{inh}})$$
At $t = 10\ \text{ms}$: $g_{\text{inh}} = w_{\text{inh}} \cdot e^{-1} \approx 0.368 \cdot w_{\text{inh}}$.
At $t = 25\ \text{ms}$ (one gamma cycle): $g_{\text{inh}} = w_{\text{inh}} \cdot e^{-2.5} \approx 0.082 \cdot w_{\text{inh}}$.

**Proof.** Direct solution of the discrete exponential decay with $\tau_{\text{inh}} = 10\ \text{ms}$. ∎

**Theorem 5 (BSI Recovery).** After BSI fires at $t = 0$, its membrane potential recovers toward $V_{\text{rest}}$ as:
$$\bigl|V(t) - V_{\text{rest}}\bigr| \leq 5 \cdot \exp(-t/\tau_m)\ \text{mV}$$
Full recovery ($|V - V_{\text{rest}}| < 0.25\ \text{mV}$) is achieved by $t = 20\ \text{ms}$.

**Proof.** Identical to prior sub-component recovery proofs. ∎

#### 3.4 Numerical Stability

**Theorem 6 (No State Divergence).** All BSI state variables $(V, g_{\text{exc}}, \text{spike\_timer}, \varphi)$ and all PSG inhibitory conductances $g_{\text{inh},i}$ remain bounded for all $t \geq 0$.

**Proof.**
- $V \in [-75, -52.5]\ \text{mV}$ by reset and threshold.
- $g_{\text{exc}} \in [0, K \cdot w_{\text{bsi,in}}] = [0, 160]\ \text{nS}$ by bounded pool size.
- $g_{\text{inh},i} \in [0, w_{\text{inh}}]$ per delivery event, decaying exponentially; with repeated BSI firing at most once per 25 ticks, $\sup g_{\text{inh},i} \leq w_{\text{inh}} / (1 - e^{-25/10}) \approx 2.3\ \text{nS}$.
- $\text{spike\_timer} \in \{0,\dots,5\}$.
- $\varphi \in [0, 2\pi)$.
All bounded. ∎

**Theorem 7 (Pool Sum Exactness).** The pool activity sum $N_m(t) = \sum_{j \in \mathcal{P}_m} s_j(t)$ is computed over a set of exactly $K = 64$ elements. The integer sum is exact in 32-bit arithmetic since $K < 2^{31}$.

**Proof.** $N_m(t) \in \{0, 1, \dots, 64\}$. This fits in a single CPU register. No overflow possible. ∎

#### 3.5 Complexity Proof

**Theorem 8 (O(1) Per-BSI Cost).** Updating BSI neuron $m$ consumes exactly 11 scalar operations for the LIF kernel plus $K = 64$ additions for spike arrival summation. Since $K$ is a compile-time constant, the cost is $O(1)$.

**Proof.** 
- Spike arrival: summing $N_m(t-1)$ requires iterating over the 64 PSG neurons in pool $m$. This is 64 additions, but 64 is a fixed constant.
- LIF kernel: 11 operations (same as RSP/CWR masters).
- Total: $75$ operations worst-case, all $O(1)$. No loops over $M$ or $D_{\text{sp}}$. ∎

**Theorem 9 (O(1) Per-PSG Cost).** Each PSG neuron receives from exactly 1 BSI neuron (its pool's BSI). Reception is 1 conductance increment per BSI spike. Inhibition decay is 1 multiplication. Total $O(1)$.

**Proof.** By construction, each PSG neuron belongs to exactly 1 pool and receives LATERAL_INH from exactly 1 BSI neuron. Fan-in from BSI is 1. Decay is 1 FLOP. ∎

**Corollary 9.1 (Network-Wide BSI Cost).** For $M = 32$ BSI neurons and $D_{\text{sp}} = 2048$ PSG targets, total BSI-related work per tick is $O(M + D_{\text{sp}}) = O(N)$ with small constants.

#### 3.6 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Pool activity sum | $N_m \cdot w_{\text{bsi,in}}$ | $\text{dimensionless} \cdot \text{nS} = \text{nS}$ | ✓ |
| BSI membrane leak | $-(V - V_{\text{rest}})$ | mV | ✓ |
| BSI Ohmic term | $R_m \cdot I_{\text{syn}}$ | $\text{M}\Omega \cdot \text{nS} \cdot \text{mV} = \text{mV}$ | ✓ |
| BSI membrane increment | $(dt/\tau_m) \cdot [\dots]$ | $(\text{ms}/\text{ms}) \cdot \text{mV} = \text{mV}$ | ✓ |
| Inhibitory delivery | $w_{\text{inh}}$ | nS | ✓ |
| Inhibitory decay | $\exp(-dt/\tau_{\text{inh}})$ | dimensionless | ✓ |
| Threshold count | $\lceil \theta_g / w_{\text{bsi,in}} \rceil$ | $\text{nS} / \text{nS} = \text{dimensionless}$ | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test BSI-MC-01: Threshold Count**
- **Procedure:** Initialize BSI at rest. Deliver $N = 1, 2, 3$ simultaneous PSG input spikes with $w_{\text{bsi,in}} = 2.5\ \text{nS}$ each.
- **Pass criterion:** $N = 1$ must NOT fire BSI. $N = 2$ and $N = 3$ must fire BSI.
- **Measurement:** Spike flag for each $N$.

**Test BSI-MC-02: Subthreshold Integration**
- **Procedure:** Deliver 1 PSG spike ($2.5\ \text{nS}$), wait 1 tick, deliver another 1 PSG spike ($2.5\ \text{nS}$) while first has decayed.
- **Pass criterion:** With $\tau_{\text{exc}} = 5\ \text{ms}$, after 1 tick residual is $2.5 \cdot e^{-0.2} \approx 2.05\ \text{nS}$. Total $\approx 4.55\ \text{nS} > 4.286$, so BSI should fire on the second tick.
- **Measurement:** $g_{\text{exc}}$ before and after second spike; spike flag.

**Test BSI-MC-03: Conductance Decay**
- **Procedure:** Inject $g_{\text{exc}} = 10.0\ \text{nS}$ into BSI. Record decay for 20 ticks.
- **Pass criterion:** $g_{\text{exc}}(5) \approx 3.679\ \text{nS}$; $g_{\text{exc}}(10) \approx 1.353\ \text{nS}$; $g_{\text{exc}}(20) \approx 0.183\ \text{nS}$.
- **Measurement:** Trajectory vs. theoretical $\exp(-t/5)$.

**Test BSI-MC-04: Refractory Period**
- **Procedure:** Force BSI firing. Within 5 ticks, attempt to trigger again with 3 PSG spikes.
- **Pass criterion:** No second spike during $\text{spike\_timer} > 0$.
- **Measurement:** Spike flags at $t_{\text{fire}}$ and $t_{\text{fire}} + 2$.

**Test BSI-MC-05: Inhibitory Delivery Magnitude**
- **Procedure:** Fire BSI. Measure $g_{\text{inh}}$ increment at a target PSG neuron.
- **Pass criterion:** $g_{\text{inh}}$ must increase by exactly $w_{\text{inh}} = 2.0\ \text{nS}$ upon spike arrival, before decay.
- **Measurement:** $g_{\text{inh}}$ at target before and after delivery.

#### 4.2 Complexity Compliance Tests

**Test BSI-CC-01: Constant Pool Size**
- **Procedure:** For every BSI neuron, count PSG neurons in its pool.
- **Pass criterion:** All pools must have exactly $K = 64$ members. No pool may have $< 64$ or $> 64$.
- **Measurement:** $\min$ and $\max$ pool size.

**Test BSI-CC-02: BSI Fan-In Bound**
- **Procedure:** For random BSI neurons, count incoming synapses from PSG.
- **Pass criterion:** In-degree must equal exactly $K = 64$ for all BSI neurons.
- **Measurement:** In-degree histogram.

**Test BSI-CC-03: PSG BSI Fan-In Bound**
- **Procedure:** For random PSG neurons, count incoming LATERAL_INH synapses from BSI.
- **Pass criterion:** In-degree from BSI must be exactly 1 for all PSG neurons.
- **Measurement:** BSI in-degree at 1,000 random PSG neurons.

**Test BSI-CC-04: No Global Summation**
- **Procedure:** Inspect BSI update algorithm.
- **Pass criterion:** No instruction may sum over all $D_{\text{sp}}$ PSG neurons or all $M$ BSI pools. Only per-pool constant-size operations.
- **Measurement:** Static algorithmic inspection.

#### 4.3 Functional Objective Tests

**Test BSI-FO-01: Sparsity Enforcement**
- **Procedure:** Present strong feedforward input to all 2,048 PSG neurons simultaneously (worst-case all-fire scenario). Run for 10 ticks.
- **Pass criterion:** By tick 5, the number of active PSG neurons must be $\leq 40$ (density $\leq 0.0195$). By tick 10, density must be $\leq 0.0156$ (32 winners).
- **Measurement:** Active PSG count per tick.

**Test BSI-FO-02: Winner-Take-All Isolation**
- **Procedure:** Present input to a single pool $m$ that drives exactly 5 PSG neurons above threshold. Record which PSG neurons remain active after 5 ticks.
- **Pass criterion:** Exactly 1 PSG neuron in pool $m$ remains active. The other 4 must be suppressed by BSI inhibition.
- **Measurement:** Active PSG count in pool $m$ at $t = 0, 1, 2, 3, 4, 5$.

**Test BSI-FO-03: Cross-Pool Independence**
- **Procedure:** Activate 3 different pools with different input strengths. Record activity in each.
- **Pass criterion:** Each pool must converge independently to 1 winner. Activity in one pool must not affect sparsity in another pool.
- **Measurement:** Winner count per pool over time.

**Test BSI-FO-04: Inhibitory Decay and Recovery**
- **Procedure:** Trigger BSI firing in a pool. Measure PSG neuron inhibition over 50 ticks with no further input.
- **Pass criterion:** $g_{\text{inh}}$ must decay to $< 0.1\ \text{nS}$ within 25 ticks. PSG neurons must be able to fire again once inhibition decays.
- **Measurement:** $g_{\text{inh}}(t)$ trajectory.

**Test BSI-FO-05: Sparse Input Pass-Through**
- **Procedure:** Present input that naturally drives only 20 PSG neurons across the entire layer (1% density).
- **Pass criterion:** BSI must not fire in pools where only 1 PSG neuron is active. All 20 winners must remain active. No additional suppression must occur.
- **Measurement:** Active PSG count before and after BSI engagement.

---

**End of Sub-Component 1.2 BSI Contract**

**Next in dependency order: 1.3 Semantic Pointer Projection Fibers (SPPF).**

**Awaiting directive to proceed.**
User: continue
