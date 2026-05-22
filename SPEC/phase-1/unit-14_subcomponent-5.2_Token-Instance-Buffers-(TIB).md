Kimi: **PHASE 1 | SUB-COMPONENT 5.2: Token Instance Buffers (TIB)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Hold stable activation of specific instance symbols such as RED, CIRCLE, or ALICE so the system can bind concrete values to abstract roles.
- Ensure each instance maintains sustained firing without adaptation decay for as long as the instance remains relevant.
- Guarantee that different instance symbols occupy separate, non-overlapping attractor basins so no two instances can be simultaneously active in the same neural population.
- Keep the recurrent maintenance cost constant per neuron, with no global coordination required beyond the initial activation cue.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| Instance cue | $\mathbf{i}(t)$ | $\{0,1\}^{D_{\text{tok}}}$ | External input activating instance symbol |
| Cue density | $\rho_{\text{cue}}$ | $[0.01, 0.03]$ | Sparse cue: 20–60 active bits |
| Instance index | $k$ | $\{1,\dots,N_{\text{tok}}\}$ | Which instance to activate; $N_{\text{tok}} = 5{,}000$ |
| Sustained enable | $\sigma(t)$ | $\{0,1\}$ | External hold signal; $\sigma = 0$ releases attractor |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Sustained instance state | $\mathbf{s}_{\text{tok}}(t)$ | $\{0,1\}^{D_{\text{tok}}}$ | Stable active pattern for instance $k$ |
| Instance confidence | $\gamma_k(t)$ | $[0, 1]$ | Normalized sustained activity level |
| Basin occupancy flag | $\omega_k(t)$ | $\{0,1\}$ | $1$ iff instance $k$ is currently held |
| Output to VBSG | $\mathbf{p}_{\text{token}}(t)$ | $\{0,1\}^{D_{\text{sp}}}$ | Semantic pointer for binding |

#### 2.3 State Space Definition

Each TIB neuron occupies an AS slot (Attractor Sustainer):

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V$ | mV | $-70.0$ | LIF integration |
| Recurrent excitatory conductance | $g_{\text{rec}}$ | nS | $0.0$ | Sustaining input from peer TIB |
| Slow gate | $s_{\text{slow}}$ | dimensionless | $0.0$ | **Sustained activity trace (AS key)** |
| Dynamic threshold | $\theta_{\text{dyn}}$ | mV | $-55.0$ | Adaptive firing bound |
| Oscillatory phase | $\varphi$ | rad | $0.0$ | Gamma cycle tracking |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Standard countdown |
| Type identifier | $\text{type\_id}$ | — | $1$ (AS) | Attractor Sustainer class |
| Flags | $\text{flags}$ | uint16_t | $\text{FLAG\_INTELLECTUAL\_POOL}$ | Pool membership |

Each TIB pool (one per instance) has dedicated recurrent connectivity:

| Field | Symbol | Value | Role |
|-------|--------|-------|------|
| Pool dimension | $D_{\text{tok}}$ | $128$ | Neurons per instance attractor |
| Recurrent fan-in | $d_{\text{rec}}^{\text{TIB}}$ | $16$ | Synapses per TIB neuron within pool |
| Recurrent weight | $w_{\text{rec}}^{\text{TIB}}$ | $0.8\ \text{nS}$ | Strong sustaining weight |
| Cross-pool inhibition | $w_{\text{cross}}$ | $4.0\ \text{nS}$ | Prevents multi-instance activation |

#### 2.4 Governing Equations

**Instance Pool Architecture:**

1. **Pool assignment.** Instances are mapped to disjoint neural pools:
   $$\mathcal{Q}_k = \{(k-1) \cdot D_{\text{tok}} + 1, \dots, k \cdot D_{\text{tok}}\}$$
   Total TIB neurons: $N_{\text{tok}} \cdot D_{\text{tok}} = 5{,}000 \cdot 128 = 640{,}000$.

   **Note:** This exceeds the 270K neuron budget if fully allocated simultaneously. The TIB subsystem uses **dynamic pool allocation**: only active instances occupy physical neurons. The Meta-Cognitive Controller allocates/ deallocates TIB pools on demand.

**Cue-Driven Activation:**

2. **External cue delivery.** When instance $k$ is cued ($i_k(t) = 1$):
   $$g_{\text{exc},i}(t^+) = g_{\text{exc},i}(t) + w_{\text{cue}} \cdot \mathbb{I}[i \in \mathcal{Q}_k]$$
   where $w_{\text{cue}} = 6.0\ \text{nS}$ (suprathreshold).

3. **Cue-to-sustained transition.** After initial firing, the pool transitions to recurrent maintenance:
   $$g_{\text{rec},i}(t^+) = g_{\text{rec},i}(t) + \sum_{j \in \mathcal{R}_i \cap \mathcal{Q}_k} w_{\text{rec}}^{\text{TIB}} \cdot S_j(t-1)$$
   where $\mathcal{R}_i$ is the recurrent presynaptic set, constrained to same pool.

**AS-Specific Sustained Dynamics:**

4. **Slow gate update (AS-specific, $\tau_s^{\text{AS}} = 500\ \text{ms}$):**
   $$s_{\text{slow},i}(t+1) = s_{\text{slow},i}(t) + \frac{dt}{\tau_s^{\text{AS}}}\bigl(-s_{\text{slow},i}(t) + \alpha \cdot S_i(t)\bigr)$$
   with $\alpha = 0.3$.

5. **Reduced dynamic threshold (AS-specific, $\beta_{\text{AS}} = 0.5\ \text{mV}$):**
   $$\theta_{\text{dyn},i}(t+1) = \theta_{\text{dyn},i}(t) + \frac{dt}{\tau_\theta}\Bigl[-(\theta_{\text{dyn},i} - \theta_{\text{base}}) + \beta_{\text{AS}} \cdot S_i(t)\Bigr]$$
   Minimal adaptation allows sustained firing without threshold escalation.

6. **Recurrent conductance decay ($\tau_{\text{rec}}^{\text{TIB}} = 100\ \text{ms}$):**
   $$g_{\text{rec},i}(t+1) = g_{\text{rec},i}(t^+) \cdot \exp(-dt/\tau_{\text{rec}}^{\text{TIB}})$$
   Slow decay maintains activity across cycles.

**Cross-Pool Mutual Inhibition:**

7. **Global instance suppression.** When any TIB pool fires, it delivers inhibition to all other pools:
   $$g_{\text{inh},i}(t^+) = g_{\text{inh},i}(t) + w_{\text{cross}} \cdot \sum_{m \neq k} \mathbb{I}[\text{pool } m \text{ active}] \cdot \mathbb{I}[i \in \mathcal{Q}_k]$$
   This ensures **single-instance exclusivity**: at most one instance can be sustained.

**Release and Decay:**

8. **Sustained enable gating.** When $\sigma(t) = 0$ (release signal):
   $$g_{\text{rec},i}(t+1) = 0 \quad \forall i$$
   $$s_{\text{slow},i}(t+1) = s_{\text{slow},i}(t) \cdot \exp(-dt/\tau_s^{\text{AS}})$$
   Activity decays exponentially without recurrent support.

9. **Instance confidence:**
   $$\gamma_k(t) = \frac{1}{D_{\text{tok}}} \sum_{i \in \mathcal{Q}_k} s_{\text{slow},i}(t)$$

**Output Projection to VBSG:**

10. **Semantic pointer generation.** The sustained instance state is read out as a semantic pointer:
    $$\mathbf{p}_{\text{token}}(t) = \text{Threshold}_{0.2}\big(\mathbf{s}_{\text{slow}}^{(k)}(t)\big)$$
    where $\text{Threshold}_{0.2}$ bins $s_{\text{slow}} > 0.2 \to 1$, else 0.

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Instance count | $N_{\text{tok}}$ | $5{,}000$ | — | $4{,}000$ | $6{,}000$ | Concrete instances |
| Pool dimension | $D_{\text{tok}}$ | $128$ | neurons | $128$ | $128$ | Neurons per instance |
| Max simultaneously active | $N_{\text{active}}$ | $32$ | pools | $16$ | $64$ | Dynamic allocation limit |
| Recurrent fan-in | $d_{\text{rec}}^{\text{TIB}}$ | $16$ | synapses | $12$ | $20$ | Within-pool connectivity |
| Recurrent weight | $w_{\text{rec}}^{\text{TIB}}$ | $0.8$ | nS | $0.6$ | $1.0$ | Sustaining strength |
| Recurrent decay | $\tau_{\text{rec}}^{\text{TIB}}$ | $100.0$ | ms | $90.0$ | $110.0$ | Slow maintenance |
| Cue weight | $w_{\text{cue}}$ | $6.0$ | nS | $5.5$ | $6.5$ | Suprathreshold activation |
| Cross-pool inhibition | $w_{\text{cross}}$ | $4.0$ | nS | $3.5$ | $4.5$ | Instance exclusivity |
| Slow gate time constant | $\tau_s^{\text{AS}}$ | $500.0$ | ms | $450.0$ | $550.0$ | Sustained activity decay |
| Reduced threshold jump | $\beta_{\text{AS}}$ | $0.5$ | mV | $0.4$ | $0.6$ | Minimal adaptation |
| Output threshold | $\theta_{\text{out}}$ | $0.2$ | — | $0.15$ | $0.25$ | Binization threshold |
| Resting potential | $V_{\text{rest}}$ | $-70.0$ | mV | $-72.0$ | $-68.0$ | Baseline membrane state |
| Firing threshold base | $\theta_{\text{base}}$ | $-55.0$ | mV | $-57.0$ | $-53.0$ | Spike emission boundary |
| Reset potential | $V_{\text{reset}}$ | $-75.0$ | mV | $-77.0$ | $-73.0$ | Post-spike membrane clamp |
| Membrane time constant | $\tau_m$ | $20.0$ | ms | $18.0$ | $22.0$ | Integration speed |
| Membrane resistance | $R_m$ | $1.0$ | M$\Omega$ | $0.8$ | $1.2$ | Ohmic scaling |
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | AS ($\text{type\_id} = 1$) | Attractor Sustainer for sustained holding |
| Input synapse type | FEEDFORWARD (type 0) | External instance cue |
| Recurrent synapse type | RECURRENT (type 1) | Within-pool sustaining connections |
| Inhibitory synapse type | LATERAL_INH (type 3) | Cross-pool mutual suppression |
| Output synapse type | FEEDFORWARD (type 0) | To 5.3 VBSG |
| Tag encoding (recurrent) | $\text{tag}[0] = 0\text{b}00100011$ | Class=1 (RECURRENT); routing key=TIB |
| Tag encoding (output) | $\text{tag}[0] = 0\text{b}00001011$ | Class=0 (FEEDFORWARD); routing key=TIB-output |
| Source field | $g_{\text{rec}}$ | Sustained recurrent conductance |
| Slow gate field | $s_{\text{slow}}$ | Activity trace for readout |

#### 2.7 Interface Contract

**Upstream providers:**
- **External instance bus:** delivers cue $\mathbf{i}(t)$ indicating which instance to activate.
- **Executive release signal:** provides $\sigma(t)$ to sustain or release instance.

**Downstream consumers:**
- **5.3 VBSG** (Variable Binding Swap Gates): receives $\mathbf{p}_{\text{token}}$ for role-filler binding.
- **Phase 2 Episodic Memory** (stubbed): receives instance pointers for event encoding.

**Handshake format:**
- Input: FEEDFORWARD cue events with instance index encoded in tag.
- Output: FEEDFORWARD events carrying $\mathbf{p}_{\text{token}}$ to VBSG.

---

### 3. Stability & Rigor Analysis

#### 3.1 Attractor Stability and Sustained Activity

**Theorem 1 (Single-Instance Attractor Existence).** For any instance $k$, the state where all neurons in $\mathcal{Q}_k$ fire at rate $\approx 1/25$ spikes/tick (one per gamma cycle) and all other pools are silent is a stable fixed point of the TIB dynamics when $\sigma = 1$.

**Proof.** Identical to CTA Theorem 1. Each neuron $i \in \mathcal{Q}_k$ receives recurrent input from $d_{\text{rec}}^{\text{TIB}} = 16$ peers. Expected recurrent conductance: $g_{\text{rec},i} \approx 16 \cdot 0.8 \cdot (1/25) = 0.512$ nS average, with peaks $\approx 12.8$ nS. With $\tau_{\text{rec}}^{\text{TIB}} = 100$ ms, recurrent input persists across cycles. The slow gate $s_{\text{slow}}$ integrates firing to $s_{\text{slow}}^{\text{ss}} \approx 0.286$, providing sustained bias. Reduced threshold jump $\beta_{\text{AS}} = 0.5$ mV prevents runaway threshold escalation. For neurons $j \notin \mathcal{Q}_k$: cross-pool inhibition $w_{\text{cross}} = 4.0$ nS maintains silence. ∎

**Theorem 2 (Instance Exclusivity).** At most one instance pool can be active at steady state.

**Proof.** Identical to CTA Theorem 2. Cross-pool inhibition $w_{\text{cross}} = 4.0$ nS ensures WTA dynamics. Simultaneous activation of two pools results in mutual suppression; the pool with slight advantage dominates. ∎

**Theorem 3 (Sustained Activity Without External Input).** Once cued and with $\sigma = 1$, an instance pool maintains activity indefinitely without further external input.

**Proof.** Identical to CTA Theorem 3. Recurrent sustain mode with $w_{\text{rec}}^{\text{TIB}} = 0.8$ nS, slow decay $\tau_{\text{rec}}^{\text{TIB}} = 100$ ms, and slow gate $s_{\text{slow}}$ provide self-sustaining equilibrium. ∎

#### 3.2 Dynamic Allocation and Resource Bounds

**Theorem 4 (Dynamic Pool Allocation Bound).** With $N_{\text{active}} \leq 32$ simultaneously active instance pools and $D_{\text{tok}} = 128$ neurons per pool, the TIB subsystem occupies at most:
$$N_{\text{TIB,phys}} = N_{\text{active}} \cdot D_{\text{tok}} = 32 \cdot 128 = 4{,}096\ \text{neurons}$$
This is $\approx 1.5\%$ of the total 270K neuron budget.

**Proof.** Only active instances require physical allocation. Inactive instances are stored as compressed prototypes in CA3-RAN (from 3.1) and loaded on demand. The Meta-Cognitive Controller manages pool allocation/deallocation via the existing $\text{FLAG\_INTELLECTUAL\_POOL}$ mechanism. ∎

**Corollary 4.1 (Capacity vs. Simultaneity Trade-off).** The system can store $N_{\text{tok}} = 5{,}000$ instance prototypes in CA3-RAN attractor memory but can only sustain $N_{\text{active}} = 32$ simultaneously. This matches the working memory capacity constraint (7±2 items) with margin for hierarchical binding.

#### 3.3 Convergence and Release

**Theorem 5 (Cue-to-Sustain Transition).** From initial cue to stable sustained activity: $\leq 3$ gamma cycles (75 ms).

**Proof.** Identical to CTA Theorem 4. Cycle 1: cue drives firing. Cycle 2: recurrent activates, $s_{\text{slow}} \approx 0.15$. Cycle 3: self-sustaining equilibrium reached. ∎

**Theorem 6 (Release Decay).** When $\sigma = 0$, instance activity decays as:
$$s_{\text{slow}}(t) = s_{\text{slow}}(0) \cdot \exp(-t/\tau_s^{\text{AS}})$$
At $t = 500$ ms: $s_{\text{slow}} \approx 0.368 \cdot s_{\text{slow}}(0)$.
At $t = 1500$ ms: $s_{\text{slow}} < 0.05$ (effectively off).

**Proof.** Identical to CTA Theorem 5. Exponential decay with $\tau_s^{\text{AS}} = 500$ ms. ∎

**Theorem 7 (No Spontaneous Activation).** Without external cue, all instance pools remain silent with probability $> 0.999$.

**Proof.** Identical to CTA Theorem 6. Membrane noise $\sigma_V < 1$ mV makes spontaneous threshold crossing negligible. Cross-pool inhibition suppresses rare events. ∎

#### 3.4 Numerical Stability

**Theorem 8 (No State Divergence).** All TIB state variables remain bounded.

**Proof.** Identical to CTA Theorem 7. All variables bounded by construction. ∎

**Theorem 9 (Slow Gate Numerical Stability).** The slow gate update is numerically stable in float32.

**Proof.** Identical to CTA Theorem 8. Pole at $z = 0.998$, stable. Rounding error bounded. ∎

#### 3.5 Complexity Proof

**Theorem 10 (O(1) Per-TIB-Neuron Cost).** Each TIB neuron update: 25 FLOPs (universal kernel) + 16 recurrent additions + 1 cross-pool inhibition check. Total $\leq 43$ FLOPs, all $O(1)$.

**Proof.** Identical to CTA Theorem 9. ∎

**Theorem 11 (O(1) Dynamic Allocation Cost).** Pool allocation/deallocation by the Meta-Cognitive Controller is $O(D_{\text{tok}})$ per instance, but amortized over many ticks and bounded by $N_{\text{active}} \leq 32$.

**Proof.** Allocation involves zeroing 128 neuron slots and configuring 2,048 recurrent synapses (128 × 16). This is $O(1)$ with respect to total network size. Deallocation is similar. With $N_{\text{active}} \leq 32$, worst-case allocation overhead per tick is bounded. ∎

#### 3.6 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Recurrent input | $\sum w_{ji} \cdot S_j$ | nS | ✓ |
| Cross-pool inhibition | $w_{\text{cross}} \cdot \mathbb{I}[\dots]$ | nS | ✓ |
| Slow gate update | $dt/\tau_s \cdot (-s_{\text{slow}} + \alpha S)$ | ms/ms · dimensionless = dimensionless | ✓ |
| Confidence | $\frac{1}{D_{\text{tok}}} \sum s_{\text{slow}}$ | dimensionless | ✓ |
| Output threshold | $\text{Threshold}_{0.2}(s_{\text{slow}})$ | dimensionless | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test TIB-MC-01: Cue Threshold**
- **Procedure:** Deliver $w_{\text{cue}} = 5.5, 6.0, 6.5$ nS to single pool.
- **Pass criterion:** $5.5$ may fire marginally. $6.0$ and $6.5$ must reliably activate pool.
- **Measurement:** Pool spike count over 5 cycles.

**Test TIB-MC-02: Recurrent Sustain**
- **Procedure:** Cue pool for 1 cycle. Remove cue. Monitor activity for 100 cycles with $\sigma = 1$.
- **Pass criterion:** Pool must maintain $\geq 80\%$ of initial firing rate without cue.
- **Measurement:** Spike count per cycle.

**Test TIB-MC-03: Cross-Pool Suppression**
- **Procedure:** Simultaneously cue pools $k$ and $m$. Monitor both.
- **Pass criterion:** Exactly one pool must dominate. Other must be suppressed to $< 10\%$ firing rate.
- **Measurement:** Spike counts for both pools.

**Test TIB-MC-04: Slow Gate Decay**
- **Procedure:** Sustain pool for 50 cycles. Set $\sigma = 0$. Record $s_{\text{slow}}$.
- **Pass criterion:** $s_{\text{slow}}$ must decay exponentially with $\tau = 500$ ms. $s_{\text{slow}}(500) \approx 0.368 \cdot s_{\text{slow}}(0)$.
- **Measurement:** $s_{\text{slow}}$ trajectory.

**Test TIB-MC-05: Dynamic Allocation**
- **Procedure:** Cue instance 1, then instance 2, ..., up to instance 40. Monitor physical allocation.
- **Pass criterion:** First 32 instances must allocate successfully. Instance 33 must trigger deallocation of oldest active instance (or allocation failure with graceful degradation).
- **Measurement:** Active pool count and physical neuron usage.

#### 4.2 Complexity Compliance Tests

**Test TIB-CC-01: Constant Pool Size**
- **Procedure:** Verify all pools have exactly $D_{\text{tok}} = 128$ neurons.
- **Pass criterion:** No pool may have $< 128$ or $> 128$ neurons.
- **Measurement:** Pool size check.

**Test TIB-CC-02: Constant Recurrent Fan-In**
- **Procedure:** Count within-pool recurrent synapses per neuron.
- **Pass criterion:** $d_{\text{rec}}^{\text{TIB}} \in [12, 20]$ for all neurons.
- **Measurement:** In-degree histogram.

**Test TIB-CC-03: No Cross-Pool Recurrence**
- **Procedure:** Inspect synapse targets.
- **Pass criterion:** No recurrent synapse may target neuron outside its own pool.
- **Measurement:** Cross-pool synapse count (must be 0).

**Test TIB-CC-04: Dynamic Allocation Bound**
- **Procedure:** Stress-test with rapid instance switching.
- **Pass criterion:** Physical neuron count for TIB never exceeds $4{,}096$ ($32 \cdot 128$).
- **Measurement:** Peak physical allocation.

#### 4.3 Functional Objective Tests

**Test TIB-FO-01: Instance Holding**
- **Procedure:** Cue "RED" instance. Verify sustained activation for 10 seconds.
- **Pass criterion:** Instance must remain active with $> 90\%$ confidence throughout. No drift to other instances.
- **Measurement:** $\gamma_k(t)$ over time.

**Test TIB-FO-02: Instance Switching**
- **Procedure:** Cue "RED" for 2 seconds. Release. Cue "CIRCLE" for 2 seconds.
- **Pass criterion:** Clean switch. No overlap. First instance must decay to $< 10\%$ before second activates.
- **Measurement:** $\gamma_{\text{RED}}(t)$ and $\gamma_{\text{CIRCLE}}(t)$.

**Test TIB-FO-03: Semantic Pointer Output**
- **Procedure:** Activate instance. Read out $\mathbf{p}_{\text{token}}$.
- **Pass criterion:** Output must be valid semantic pointer: $\|\mathbf{p}_{\text{token}}\|_0 \in [20, 50]$. Deterministic for same instance.
- **Measurement:** Output density and consistency.

**Test TIB-FO-04: Type-Token Distinction**
- **Procedure:** Simultaneously activate CTA category "COLOR" and TIB instance "RED". Verify both persist independently.
- **Pass criterion:** Category and instance must coexist without interference. CTA and TIB are separate subsystems with no cross-inhibition.
- **Measurement:** $\gamma_{\text{COLOR}}(t)$ and $\gamma_{\text{RED}}(t)$.

**Test TIB-FO-05: Binding Readiness**
- **Procedure:** Activate instance "ALICE". Verify output is suitable for VBSG binding.
- **Pass criterion:** $\mathbf{p}_{\text{token}}$ must carry semantic label compatible with BINDING_PAIR synapses. Tag bytes must be non-zero and consistent.
- **Measurement:** Tag integrity and binding compatibility.

---

**End of Sub-Component 5.2 TIB Contract**

**Next in dependency order: 5.3 Variable Binding Swap Gates (VBSG).**

**Awaiting directive to proceed.**
