Kimi: **PHASE 1 | SUB-COMPONENT 2.1: Granule Binding Gate Neurons (GBGN)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Receive two distinct semantic pointers that arrive at the same binding neuron through different input pathways during the same gamma cycle.
- When the coincidence detection dendrites confirm both inputs carry matching timing signatures and semantic labels, produce a new bound pointer that structurally encodes the relationship between the two original pointers.
- Make the binding operation multiplicative rather than additive, so the output is not merely the sum of inputs but a distinct new pattern representing their conjunction.
- Ensure the bound pointer can later be unbound back into its constituents using phase-conjugate operations.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| Feedforward excitation | $g_{\text{exc}}(t)$ | $[0, \infty)\ \text{nS}$ | Standard excitatory input from SPPF |
| Inhibitory suppression | $g_{\text{inh}}(t)$ | $[0, \infty)\ \text{nS}$ | LATERAL_INH from CWR window close |
| Binding conductance | $g_{\text{bind}}(t)$ | $[0, \infty)\ \text{nS}$ | Multiplicative signal from CDD |
| Precision gain | $\pi(t)$ | $[0, 1]$ | From 4.1 RPGN; scales effective binding |
| Local phase | $\varphi(t)$ | $[0, 2\pi)$ | From GPLO entrainment |
| Phase reference | $\varphi_{\text{ref}}(t)$ | $[0, 2\pi)$ | Canonical gamma phase |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Bound pointer spike | $S_{\text{BG}}(t)$ | $\{0,1\}$ | Binary firing indicator |
| Bound semantic label | $\mathbf{L}_{\text{bound}}$ | $\{0,1\}^{56}$ | New composite tag for bound representation |
| Output conductance | $g_{\text{exc}}^{\text{out}}(t)$ | $[0, \infty)\ \text{nS}$ | Forwarded to downstream attractor/memory |
| Binding strength | $\beta(t)$ | $[0, 1]$ | Normalized binding activation |

#### 2.3 State Space Definition

Each GBGN neuron occupies a BG slot:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V$ | mV | $-70.0$ | LIF integration |
| Excitatory conductance | $g_{\text{exc}}$ | nS | $0.0$ | Standard feedforward input |
| Inhibitory conductance | $g_{\text{inh}}$ | nS | $0.0$ | Window-close suppression |
| Binding conductance | $g_{\text{bind}}$ | nS | $0.0$ | **Multiplicative binding signal (BG-specific)** |
| Dynamic threshold | $\theta_{\text{dyn}}$ | mV | $-55.0$ | Adaptive firing bound |
| Oscillatory phase | $\varphi$ | rad | $0.0$ | **Gamma phase for binding timing** |
| Precision | $\pi$ | dimensionless | $0.0$ | Attentional gain from RPGN |
| Slow gate | $s_{\text{slow}}$ | dimensionless | $0.0$ | Sustained activity trace |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Standard countdown |
| Type identifier | $\text{type\_id}$ | — | $4$ (BG) | Binding Gate class |
| Flags | $\text{flags}$ | uint16 | $\text{FLAG\_INTELLECTUAL\_POOL}$ | Pool membership |

Each GBGN output synapse carries:

| Field | Symbol | Value | Role |
|-------|--------|-------|------|
| Postsynaptic index | $\text{post\_id}$ | Downstream target | Fixed routing |
| Efficacy | $w_{\text{out}}$ | $[0.4, 0.6]\ \text{nS}$ | Forwarding weight |
| Axonal delay | $\delta$ | $[0, 2]\ \text{ms}$ | Output latency |
| Tag byte 0 | $\text{tag}[0]$ | $0\text{b}10100001$ | Class=5 (BINDING_PAIR); routing key=GBGN |
| Tag bytes 1–7 | $\text{tag}[1..7]$ | $\mathbf{L}_{\text{bound}}$ | Composite semantic label |

#### 2.4 Governing Equations

**GBGN Neuron (per tick, $dt = 1\ \text{ms}$):**

1. **Conductance decay (universal kernel step 2, BG-modified):**
   $$g_{\text{exc}}(t+1) = g_{\text{exc}}(t) \cdot \exp(-dt/\tau_{\text{exc}})$$
   $$g_{\text{inh}}(t+1) = g_{\text{inh}}(t) \cdot \exp(-dt/\tau_{\text{inh}})$$
   $$g_{\text{bind}}(t+1) = g_{\text{bind}}(t) \cdot \exp(-dt/\tau_{\text{bind}})$$
   with $\tau_{\text{exc}} = 5\ \text{ms}$, $\tau_{\text{inh}} = 10\ \text{ms}$, $\tau_{\text{bind}} = 20\ \text{ms}$.

2. **Precision-scaled synaptic current (BG-specific integration):**
   $$I_{\text{syn}}(t) = g_{\text{exc}}(V - E_{\text{exc}}) + g_{\text{inh}}(V - E_{\text{inh}}) + \pi(t) \cdot g_{\text{bind}}(V - E_{\text{bind}})$$
   where $E_{\text{exc}} = E_{\text{bind}} = 0.0\ \text{mV}$, $E_{\text{inh}} = -75.0\ \text{mV}$.

   The precision $\pi(t)$ multiplies only the binding term, making binding gain attention-modulated.

3. **Membrane update (universal kernel step 4):**
   $$V(t+1) = V(t) + \frac{dt}{\tau_m}\Bigl[-\bigl(V(t) - V_{\text{rest}}\bigr) + R_m \cdot I_{\text{syn}}(t)\Bigr]$$
   with $V_{\text{rest}} = -70.0\ \text{mV}$, $\tau_m = 20\ \text{ms}$, $R_m = 1\ \text{M}\Omega$.

4. **Dynamic threshold (universal kernel step 6):**
   $$\theta_{\text{dyn}}(t+1) = \theta_{\text{dyn}}(t) + \frac{dt}{\tau_\theta}\Bigl[-\bigl(\theta_{\text{dyn}}(t) - \theta_{\text{base}}\bigr) + \beta \cdot S(t)\Bigr]$$
   with $\theta_{\text{base}} = -55.0\ \text{mV}$, $\tau_\theta = 100\ \text{ms}$, $\beta = 2.0\ \text{mV}$.

5. **Phase rotation (universal kernel step 7):**
   $$\varphi(t+1) = \bigl(\varphi(t) + \omega \cdot dt\bigr) \bmod 2\pi$$
   with $\omega = 2\pi \cdot 40\ \text{Hz}$.

6. **Slow gate update (universal kernel step 5):**
   $$s_{\text{slow}}(t+1) = s_{\text{slow}}(t) + \frac{dt}{\tau_s}\bigl(-s_{\text{slow}}(t) + \alpha \cdot S(t)\bigr)$$
   with $\tau_s = 200\ \text{ms}$, $\alpha = 0.3$.

7. **Binding strength normalization:**
   $$\beta(t) = \sigma\bigl(\lambda_\beta \cdot (g_{\text{bind}}(t) - \theta_\beta)\bigr)$$
   where $\sigma(x) = 1/(1 + e^{-x})$ is the logistic sigmoid, $\lambda_\beta = 2.0\ \text{nS}^{-1}$, $\theta_\beta = 1.0\ \text{nS}$.
   This maps binding conductance to a bounded activation level $[0, 1]$.

8. **Firing condition (modified for binding gate):**
   The GBGN neuron fires when the binding-modulated potential exceeds threshold:
   $$V_{\text{eff}}(t) = V(t) + \gamma_{\text{bind}} \cdot \pi(t) \cdot g_{\text{bind}}(t) \cdot R_m$$
   
   If $V_{\text{eff}}(t+1) \geq \theta_{\text{dyn}}(t+1)$:
   - Emit spike: $S_{\text{BG}}(t+1) = 1$
   - Reset: $V(t+1) \leftarrow V_{\text{reset}} = -75.0\ \text{mV}$
   - Refractory: $\text{spike\_timer} \leftarrow 5$
   - Generate composite label: $\mathbf{L}_{\text{bound}} = \text{hash}(\mathbf{L}_1, \mathbf{L}_2, \varphi(t))$

   where $\gamma_{\text{bind}} = 5.0\ \text{mV/nS}$ is the binding gain conversion.

9. **Refractory countdown (if $\text{spike\_timer} > 0$):**
   $$\text{spike\_timer} \leftarrow \text{spike\_timer} - 1$$
   Skip steps 2–6.

**Composite Label Generation:**

10. **Deterministic binding label.** When binding occurs, the output tag is computed as:
    $$\mathbf{L}_{\text{bound}} = \text{SHA3-224}(\mathbf{L}_1 \| \mathbf{L}_2 \| \lfloor \varphi \cdot 2^{16} / 2\pi \rfloor) \bmod 2^{56}$$
    where $\|$ denotes concatenation and the result is truncated to 56 bits. This ensures:
    - Determinism: same inputs → same label
    - Uniqueness: different inputs → different labels with high probability
    - Phase encoding: the gamma phase at binding is embedded in the label

**Unbinding Readiness:**

11. **Phase-conjugate unbinding.** The bound representation stores implicit phase information. For approximate unbinding of constituent $A$ from bound $A \circ B$:
    $$\mathbf{L}_A \approx \text{argmax}_{\mathbf{L} \in \text{lexicon}} \text{sim}\big(\mathbf{p}_{A \circ B} \otimes \mathbf{p}_B^{-1}, \mathbf{p}_A\big)$$
    where $\otimes$ is the binding operation and $^{-1}$ denotes approximate inverse. This is implemented in Phase 2 via attractor cleanup.

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Binding time constant | $\tau_{\text{bind}}$ | $20.0$ | ms | $18.0$ | $22.0$ | Binding conductance decay |
| Binding reversal | $E_{\text{bind}}$ | $0.0$ | mV | — | — | Same as excitatory |
| Precision gain range | $\pi$ | $[0, 1]$ | dimensionless | $0$ | $1$ | Attentional modulation |
| Binding gain conversion | $\gamma_{\text{bind}}$ | $5.0$ | mV/nS | $4.0$ | $6.0$ | Conductance-to-potential scaling |
| Sigmoid slope | $\lambda_\beta$ | $2.0$ | nS$^{-1}$ | $1.5$ | $2.5$ | Binding strength steepness |
| Sigmoid threshold | $\theta_\beta$ | $1.0$ | nS | $0.8$ | $1.2$ | Binding activation midpoint |
| Resting potential | $V_{\text{rest}}$ | $-70.0$ | mV | $-72.0$ | $-68.0$ | Baseline membrane state |
| Firing threshold base | $\theta_{\text{base}}$ | $-55.0$ | mV | $-57.0$ | $-53.0$ | Spike emission boundary |
| Reset potential | $V_{\text{reset}}$ | $-75.0$ | mV | $-77.0$ | $-73.0$ | Post-spike membrane clamp |
| Membrane time constant | $\tau_m$ | $20.0$ | ms | $18.0$ | $22.0$ | Integration speed |
| Membrane resistance | $R_m$ | $1.0$ | M$\Omega$ | $0.8$ | $1.2$ | Ohmic scaling |
| Dynamic threshold time constant | $\tau_\theta$ | $100.0$ | ms | $90.0$ | $110.0$ | Threshold adaptation speed |
| Dynamic threshold jump | $\beta$ | $2.0$ | mV | $1.5$ | $2.5$ | Post-spike threshold increase |
| Slow gate time constant | $\tau_s$ | $200.0$ | ms | $180.0$ | $220.0$ | Sustained activity decay |
| Slow gate increment | $\alpha$ | $0.3$ | — | $0.25$ | $0.35$ | Post-spike gate boost |
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |
| Gamma frequency | $f_\gamma$ | $40.0$ | Hz | $38.0$ | $42.0$ | Oscillation frequency |
| GBGN pool size | $N_{\text{GBGN}}$ | $256$ | — | $128$ | $512$ | Binding neurons per pool |
| Output fan-out | $D_{\text{out}}$ | $4$ | synapses | $2$ | $8$ | Downstream targets per GBGN |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | BG ($\text{type\_id} = 4$) | Binding Gate; key state is $g_{\text{bind}}$ |
| Input synapse type | FEEDFORWARD (type 0) | From SPPF for standard excitation |
| Input synapse type | LATERAL_INH (type 3) | From CWR for window close |
| Input synapse type | BINDING_PAIR (type 5) | Internal from CDD for $g_{\text{bind}}$ |
| Output synapse type | BINDING_PAIR (type 5) | To downstream with composite label |
| Tag encoding (input) | $\text{tag}[0] = 0\text{b}10100000$ | Class=5 (BINDING_PAIR); routing key=GBGN-input |
| Tag encoding (output) | $\text{tag}[0] = 0\text{b}10100001$ | Class=5 (BINDING_PAIR); routing key=GBGN-output |
| Source field | $g_{\text{bind}}$ | Multiplicative binding conductance |
| Target field (downstream) | $g_{\text{exc}}$ | Standard excitatory delivery |

#### 2.7 Interface Contract

**Upstream providers:**
- **1.3 SPPF** (Semantic Pointer Projection Fibers): delivers feedforward excitation with semantic labels.
- **2.3 CDD** (Coincidence Detection Dendrites): delivers $g_{\text{bind}}$ increment upon validated coincidence.
- **4.1 RPGN** (Reticular Phase Gating Nuclei): delivers precision gain $\pi(t)$ via PRECISION_GATE.
- **4.3 CWR** (Coincidence Window Regulators): delivers LATERAL_INH at window close.
- **2.2 GPLO** (Gamma Phase Locking Oscillators): entrains local phase $\varphi(t)$.

**Downstream consumers:**
- **3.3 DPSG** (Dentate Pattern Separation Gating): receives bound pointers for hippocampal cleanup.
- **Phase 2 Memory Architecture** (stubbed): receives bound semantic pointers for storage and retrieval.
- **5.3 VBSG** (Variable Binding Swap Gates): receives bound type-token pairs.

**Handshake format:**
- Input events carry standard FEEDFORWARD, LATERAL_INH, or BINDING_PAIR format with semantic labels.
- Output events carry BINDING_PAIR format with composite label $\mathbf{L}_{\text{bound}}$ computed via hash.

---

### 3. Stability & Rigor Analysis

#### 3.1 Binding Activation and Firing

**Theorem 1 (Binding-Dependent Firing).** A GBGN neuron at rest with zero standard excitation ($g_{\text{exc}} = 0$) and full precision ($\pi = 1$) fires if and only if:
$$g_{\text{bind}} \geq \frac{\theta_{\text{base}} - V_{\text{rest}}}{\gamma_{\text{bind}}} = \frac{15}{5.0} = 3.0\ \text{nS}$$

**Proof.** With $g_{\text{exc}} = g_{\text{inh}} = 0$, the effective potential is:
$$V_{\text{eff}} = V_{\text{rest}} + \gamma_{\text{bind}} \cdot \pi \cdot g_{\text{bind}} = -70 + 5.0 \cdot g_{\text{bind}}$$
Setting $V_{\text{eff}} = \theta_{\text{base}} = -55$:
$$-55 = -70 + 5.0 \cdot g_{\text{bind}} \implies g_{\text{bind}} = 3.0\ \text{nS}$$
For $g_{\text{bind}} < 3.0$: $V_{\text{eff}} < -55$, no firing. For $g_{\text{bind}} \geq 3.0$: firing occurs. ∎

**Corollary 1.1 (Precision Gating).** With partial precision $\pi < 1$, the required binding conductance increases:
$$g_{\text{bind,min}}(\pi) = \frac{3.0}{\pi}\ \text{nS}$$
At $\pi = 0.5$: $g_{\text{bind,min}} = 6.0\ \text{nS}$. At $\pi = 0$: binding is disabled regardless of $g_{\text{bind}}$.

**Theorem 2 (No Binding Without Coincidence).** If CDD detects no coincidence ($g_{\text{bind}} = 0$), the GBGN neuron behaves as a standard CI neuron with threshold $\theta_{\text{base}}$. It fires only from $g_{\text{exc}}$ exceeding the standard LIF threshold $\theta_g \approx 4.286\ \text{nS}$.

**Proof.** With $g_{\text{bind}} = 0$, equation 2 reduces to standard synaptic current. Equation 8 reduces to $V_{\text{eff}} = V$. The firing condition becomes $V \geq \theta_{\text{dyn}}$, identical to CI dynamics. ∎

#### 3.2 Multiplicative Nonlinearity

**Theorem 3 (Superlinear Binding Response).** For two inputs with weights $w_1, w_2$ that generate binding conductance $g_{\text{bind}} = \kappa_{\text{bind}} \cdot w_1 w_2 / w_{\text{scale}}$ (from CDD), the GBGN firing probability is superlinear in the input product when $g_{\text{bind}}$ is near threshold.

**Proof.** Near threshold, the firing probability is approximately:
$$P_{\text{fire}} \approx \sigma\bigl(\lambda_V \cdot (V_{\text{eff}} - \theta_{\text{dyn}})\bigr)$$
where $\sigma$ is steep. Since $V_{\text{eff}}$ depends on $g_{\text{bind}} \propto w_1 w_2$, and $g_{\text{exc}}$ depends on $w_1 + w_2$ (linear), the binding term dominates for coincident inputs:
$$\frac{\partial^2 P_{\text{fire}}}{\partial w_1 \partial w_2}\bigg|_{g_{\text{bind}} \approx \theta} > 0$$
This positive cross-partial derivative confirms superlinear interaction. ∎

**Corollary 3.1 (AND-Gate Characteristic).** The GBGN neuron implements a soft AND gate: it fires strongly when both inputs are present (coincident, high $g_{\text{bind}}$) but weakly or not at all when only one input is present (no coincidence, $g_{\text{bind}} = 0$).

#### 3.3 Convergence Bounds

**Theorem 4 (Binding Conductance Decay).** After a coincidence event produces $g_{\text{bind}}(0) = g_0$, the residual at time $t$ is:
$$g_{\text{bind}}(t) = g_0 \cdot \exp(-t/\tau_{\text{bind}})$$
At $t = 20\ \text{ms}$: $g_{\text{bind}} = g_0 \cdot e^{-1} \approx 0.368 g_0$.
At $t = 25\ \text{ms}$ (cycle end): $g_{\text{bind}} = g_0 \cdot e^{-1.25} \approx 0.287 g_0$.

**Proof.** Exponential decay with $\tau_{\text{bind}} = 20\ \text{ms}$. ∎

**Theorem 5 (Post-Fire Recovery).** After firing at $t = 0$, membrane potential recovers as:
$$\bigl|V(t) - V_{\text{rest}}\bigr| \leq 5 \cdot \exp(-t/\tau_m)\ \text{mV}$$
Full recovery by $t = 20\ \text{ms}$.

**Proof.** Identical to prior sub-component recovery proofs. ∎

#### 3.4 Numerical Stability

**Theorem 6 (No State Divergence).** All GBGN state variables $(V, g_{\text{exc}}, g_{\text{inh}}, g_{\text{bind}}, \theta_{\text{dyn}}, \varphi, s_{\text{slow}}, \pi, \text{spike\_timer})$ remain bounded for all $t \geq 0$.

**Proof.**
- $V \in [-75, -52.5]\ \text{mV}$ by reset and threshold.
- $g_{\text{exc}} \in [0, g_{\text{max}}^{\text{exc}}]$ by bounded input.
- $g_{\text{inh}} \in [0, g_{\text{max}}^{\text{inh}}]$ by bounded inhibition.
- $g_{\text{bind}} \in [0, g_{\text{max}}^{\text{bind}}]$ by CDD output bound.
- $\theta_{\text{dyn}} \in [-55, -50]\ \text{mV}$ practically.
- $\varphi \in [0, 2\pi)$.
- $s_{\text{slow}} \in [0, 0.3]$.
- $\pi \in [0, 1]$ by clipping.
- $\text{spike\_timer} \in \{0,\dots,5\}$.
All bounded. ∎

**Theorem 7 (Sigmoid Numerical Stability).** The binding strength $\beta = \sigma(\lambda_\beta(g_{\text{bind}} - \theta_\beta))$ is computed in float32. For $g_{\text{bind}} \in [0, 10]\ \text{nS}$:
- Argument range: $\lambda_\beta \cdot (g_{\text{bind}} - \theta_\beta) \in [-2, 18]$
- $\sigma(-2) \approx 0.12$, $\sigma(18) \approx 1.0$ (saturated)
- No overflow: $e^{18} \approx 6.6 \times 10^7$, well within float32 range ($\sim 10^{38}$)
- No underflow: $e^{-18} \approx 1.5 \times 10^{-8}$, above float32 minimum normal ($\sim 10^{-38}$)

**Proof.** Direct evaluation of float32 range limits. The sigmoid is numerically safe across the entire operating domain. ∎

#### 3.5 Complexity Proof

**Theorem 8 (O(1) Per-GBGN Cost).** The GBGN neuron update consumes 25 FLOPs for the universal kernel plus 5 FLOPs for BG-specific operations (binding term in current, effective potential, sigmoid, label hash).

**Proof.** Universal kernel: 25 FLOPs (proven in UIN report). BG additions:
- Binding current term: 1 multiplication ($\pi \cdot g_{\text{bind}}$)
- Effective potential: 1 multiplication and 1 addition
- Sigmoid evaluation: 1 exponential or LUT lookup + 1 division
- Label hash: computed only on firing (amortized)
Total: $\leq 30$ FLOPs, all $O(1)$. ∎

**Theorem 9 (O(1) Output Delivery Cost).** Each GBGN neuron delivers to at most $D_{\text{out}} = 4$ downstream targets. Delivery is $O(1)$.

**Proof.** Fixed out-degree $\leq 4$. Iteration over constant-size list. ∎

#### 3.6 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Binding current | $\pi \cdot g_{\text{bind}} \cdot (V - E_{\text{bind}})$ | $\text{dimensionless} \cdot \text{nS} \cdot \text{mV} = \text{pA}$ | ✓ |
| Effective potential | $V + \gamma_{\text{bind}} \cdot \pi \cdot g_{\text{bind}} \cdot R_m$ | $\text{mV} + (\text{mV/nS}) \cdot \text{nS} \cdot \text{M}\Omega = \text{mV} + \text{mV}$ | ✓ |
| Sigmoid argument | $\lambda_\beta \cdot (g_{\text{bind}} - \theta_\beta)$ | $\text{nS}^{-1} \cdot \text{nS} = \text{dimensionless}$ | ✓ |
| Conductance decay | $\exp(-dt/\tau_{\text{bind}})$ | dimensionless | ✓ |
| Phase increment | $\omega \cdot dt$ | $(\text{rad}/\text{s}) \cdot \text{s} = \text{rad}$ | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test GBGN-MC-01: Binding Threshold**
- **Procedure:** Initialize GBGN at rest with $\pi = 1$, $g_{\text{exc}} = g_{\text{inh}} = 0$. Inject $g_{\text{bind}} = 2.5, 3.0, 3.5\ \text{nS}$.
- **Pass criterion:** $g_{\text{bind}} = 2.5$ must NOT fire. $g_{\text{bind}} = 3.0$ and $3.5$ must fire.
- **Measurement:** Spike flag for each conductance level.

**Test GBGN-MC-02: Precision Gating**
- **Procedure:** Fix $g_{\text{bind}} = 4.0\ \text{nS}$. Vary $\pi = 0.0, 0.5, 1.0$.
- **Pass criterion:** $\pi = 0.0$: no firing. $\pi = 0.5$: effective $g_{\text{bind}} = 2.0 < 3.0$, no firing. $\pi = 1.0$: effective $g_{\text{bind}} = 4.0 > 3.0$, fires.
- **Measurement:** Spike flags and effective potential.

**Test GBGN-MC-03: Standard Excitation Without Binding**
- **Procedure:** Set $g_{\text{bind}} = 0$, $\pi = 1$. Inject $g_{\text{exc}} = 4.0, 4.286, 5.0\ \text{nS}$.
- **Pass criterion:** Behavior must match standard CI neuron: $4.0$ no fire, $4.286$ fire at boundary, $5.0$ fire.
- **Measurement:** Spike flags; compare to PSG threshold test.

**Test GBGN-MC-04: Inhibitory Suppression**
- **Procedure:** Set $g_{\text{exc}} = 5.0$, $g_{\text{bind}} = 5.0$, $\pi = 1$. Add $g_{\text{inh}} = 2.0, 4.0, 6.0\ \text{nS}$.
- **Pass criterion:** Sufficient inhibition must prevent firing. Compute exact threshold from equation 2.
- **Measurement:** Spike flags vs. $g_{\text{inh}}$.

**Test GBGN-MC-05: Sigmoid Binding Strength**
- **Procedure:** Sweep $g_{\text{bind}}$ from $0$ to $5\ \text{nS}$. Record $\beta(g_{\text{bind}})$.
- **Pass criterion:** $\beta(0) \approx 0.12$. $\beta(1.0) = 0.5$ (midpoint). $\beta(5.0) \approx 1.0$.
- **Measurement:** $\beta$ trajectory vs. theoretical $\sigma(2(g - 1))$.

#### 4.2 Complexity Compliance Tests

**Test GBGN-CC-01: Constant Output Fan-Out**
- **Procedure:** Count outgoing BINDING_PAIR synapses per GBGN.
- **Pass criterion:** Out-degree $\leq 4$ for all neurons.
- **Measurement:** $\max_i D_{\text{out},i}$.

**Test GBGN-CC-02: Input Type Diversity**
- **Procedure:** Verify GBGN receives FEEDFORWARD, LATERAL_INH, PRECISION_GATE, and BINDING_PAIR inputs.
- **Pass criterion:** All four synapse types must be present. No other types.
- **Measurement:** Synapse type histogram per GBGN.

**Test GBGN-CC-03: No Global Binding State**
- **Procedure:** Inspect GBGN update algorithm.
- **Pass criterion:** No global variable shared across GBGN neurons. Each operates on local state only.
- **Measurement:** Static inspection for cross-neuron references.

**Test GBGN-CC-04: Label Generation Cost**
- **Procedure:** Measure hash computation cost when GBGN fires.
- **Pass criterion:** Hash must be $O(1)$ with $\leq 50$ FLOPs (e.g., truncated SHA3 or lightweight hash).
- **Measurement:** Instruction count for label generation.

#### 4.3 Functional Objective Tests

**Test GBGN-FO-01: Binding Operation Fidelity**
- **Procedure:** Present two semantic pointers $\mathbf{p}_A, \mathbf{p}_B$ through SPPF with matching labels. Verify GBGN produces bound output.
- **Pass criterion:** GBGN must fire when CDD detects coincidence. Output label must be deterministic function of input labels and phase.
- **Measurement:** Spike count, output label, input label correlation.

**Test GBGN-FO-02: No-Binding Isolation**
- **Procedure:** Present only $\mathbf{p}_A$ (no second pointer). Verify no binding output.
- **Pass criterion:** GBGN must not fire from binding pathway. Standard excitation may fire if suprathreshold, but no composite label generated.
- **Measurement:** Spike flags and output label presence.

**Test GBGN-FO-03: Precision Modulation of Binding**
- **Procedure:** Present matched pair with $\pi = 0.2, 0.5, 0.8, 1.0$.
- **Pass criterion:** Binding response (spike probability or latency) must increase monotonically with $\pi$. At $\pi = 0$, binding disabled.
- **Measurement:** Spike latency and rate vs. $\pi$.

**Test GBGN-FO-04: Phase-Encoded Label Uniqueness**
- **Procedure:** Bind same pair $\mathbf{p}_A, \mathbf{p}_B$ at 8 different gamma phases.
- **Pass criterion:** Output labels must differ across phases with $> 95\%$ probability. Same phase must produce identical label.
- **Measurement:** Label collision statistics.

**Test GBGN-FO-05: Temporal Isolation Between Cycles**
- **Procedure:** Trigger binding at $t = 20$. Verify no residual binding affects next cycle at $t = 25$.
- **Pass criterion:** $g_{\text{bind}}(25) < 20\%$ of peak. New inputs at $t = 25$ start fresh.
- **Measurement:** $g_{\text{bind}}$ trajectory across cycle boundary.

---

**End of Sub-Component 2.1 GBGN Contract**

**Next in dependency order: 3.3 Dentate Pattern Separation Gating (DPSG).**

**Awaiting directive to proceed.**
User: continue
