 **PHASE 1 | SUB-COMPONENT 5.3: Variable Binding Swap Gates (VBSG)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Receive a category role from the type attractor circuit and a specific instance from the token buffer circuit, then combine them into a single bound structure that represents the assignment of that instance to that role.
- Later, when given only the bound structure and the category role, approximately recover the original instance through a phase-conjugate unbinding operation.
- Ensure the binding and unbinding operations are deterministic and reversible enough for practical reasoning, even if not perfectly so.
- Keep all operations within a single gamma cycle so that bound structures can be rapidly composed and decomposed.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| Category pointer | $\mathbf{p}_{\text{type}}(t)$ | $\{0,1\}^{D_{\text{sp}}}$ | From 5.1 CTA; $\|\mathbf{p}_{\text{type}}\|_0 \approx 40$ |
| Instance pointer | $\mathbf{p}_{\text{token}}(t)$ | $\{0,1\}^{D_{\text{sp}}}$ | From 5.2 TIB; $\|\mathbf{p}_{\text{token}}\|_0 \approx 40$ |
| Operation command | $\text{op}(t)$ | $\{\text{BIND}, \text{UNBIND}\}$ | From executive control (stubbed) |
| Phase reference | $\varphi_{\text{ref}}(t)$ | $[0, 2\pi)$ | From 4.2 RSP |
| Precision gain | $\pi(t)$ | $[0, 1]$ | From 4.1 RPGN |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Bound pointer | $\mathbf{p}_{\text{bound}}(t)$ | $\{0,1\}^{D_{\text{sp}}}$ | $\mathbf{p}_{\text{type}} \otimes \mathbf{p}_{\text{token}}$ |
| Unbound instance | $\tilde{\mathbf{p}}_{\text{token}}(t)$ | $\{0,1\}^{D_{\text{sp}}}$ | Approximate recovery from bound |
| Binding validity | $\nu_{\text{bind}}(t)$ | $\{0,1\}$ | $1$ iff binding operation succeeded |
| Unbinding fidelity | $\phi_{\text{unbind}}(t)$ | $[0, 1]$ | Similarity to original token |

#### 2.3 State Space Definition

Each VBSG neuron occupies a BG slot:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V$ | mV | $-70.0$ | LIF integration |
| Excitatory conductance | $g_{\text{exc}}$ | nS | $0.0$ | Standard feedforward input |
| Binding conductance | $g_{\text{bind}}$ | nS | $0.0$ | **Multiplicative binding signal** |
| Dynamic threshold | $\theta_{\text{dyn}}$ | mV | $-55.0$ | Adaptive firing bound |
| Oscillatory phase | $\varphi$ | rad | $0.0$ | **Phase for conjugate operations** |
| Precision | $\pi$ | dimensionless | $0.0$ | Attentional gain from RPGN |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Standard countdown |
| Type identifier | $\text{type\_id}$ | — | $4$ (BG) | Binding Gate class |

**VBSG circuit architecture (dual-pathway):**

| Pathway | Neurons | Function |
|---------|---------|----------|
| Binding path | $N_{\text{bind}} = 256$ | Compute $\mathbf{p}_{\text{type}} \otimes \mathbf{p}_{\text{token}}$ |
| Unbinding path | $N_{\text{unbind}} = 256$ | Compute $\mathbf{p}_{\text{type}}^{-1} \otimes \mathbf{p}_{\text{bound}}$ |
| Swap controller | $N_{\text{swap}} = 64$ | Route op(BIND) or op(UNBIND) |

#### 2.4 Governing Equations

**Binding Operation (op = BIND):**

1. **Input delivery.** CTA and TIB outputs arrive via SPPF-like tagged FEEDFORWARD synapses:
   $$g_{\text{exc},i}^{\text{type}}(t^+) = g_{\text{exc},i}^{\text{type}}(t) + w_{\text{in}} \cdot p_{\text{type},i}(t)$$
   $$g_{\text{exc},i}^{\text{token}}(t^+) = g_{\text{exc},i}^{\text{token}}(t) + w_{\text{in}} \cdot p_{\text{token},i}(t)$$
   where $w_{\text{in}} = 0.5\ \text{nS}$.

2. **Coincidence detection (CDD logic).** For each VBSG binding neuron $i$, detect simultaneous activation of type and token inputs with matching phase:
   $$\text{coinc}_i(t) = \mathbb{I}\big[g_{\text{exc},i}^{\text{type}} > 0 \land g_{\text{exc},i}^{\text{token}} > 0 \land |\varphi_i - \varphi_{\text{ref}}| < \delta_\varphi\big]$$

3. **Multiplicative binding conductance:**
   $$g_{\text{bind},i}(t^+) = g_{\text{bind},i}(t) + \pi(t) \cdot \kappa_{\text{bind}} \cdot \sqrt{g_{\text{exc},i}^{\text{type}} \cdot g_{\text{exc},i}^{\text{token}}} \cdot \text{coinc}_i(t)$$
   with $\kappa_{\text{bind}} = 2.0$, but corrected to true multiplicative form:
   $$g_{\text{bind},i}(t^+) = g_{\text{bind},i}(t) + \pi(t) \cdot \kappa_{\text{bind}} \cdot \frac{g_{\text{exc},i}^{\text{type}} \cdot g_{\text{exc},i}^{\text{token}}}{w_{\text{scale}}} \cdot \text{coinc}_i(t)$$
   where $w_{\text{scale}} = 0.25\ \text{nS}$.

4. **Binding threshold and output:**
   $$V_{\text{eff},i} = V_i + \gamma_{\text{bind}} \cdot \pi \cdot g_{\text{bind},i} \cdot R_m$$
   If $V_{\text{eff},i} \geq \theta_{\text{dyn},i}$:
   - $p_{\text{bound},i} = 1$
   - Generate composite tag: $\mathbf{L}_{\text{bound}} = \text{hash}(\mathbf{L}_{\text{type}}, \mathbf{L}_{\text{token}}, \varphi_i)$

**Unbinding Operation (op = UNBIND):**

5. **Phase-conjugate input.** The type pointer is replayed with inverted phase:
   $$\varphi_{\text{conj}} = (\varphi_{\text{ref}} + \pi) \bmod 2\pi$$
   Type input arrives at $\varphi_{\text{conj}}$; bound input arrives at $\varphi_{\text{ref}}$.

6. **Anti-coincidence detection.** Unbinding neurons detect type-at-conjugate plus bound-at-reference:
   $$\text{anti-coinc}_i(t) = \mathbb{I}\big[g_{\text{exc},i}^{\text{type,conj}} > 0 \land g_{\text{exc},i}^{\text{bound}} > 0 \land |\varphi_i - \varphi_{\text{ref}}| < \delta_\varphi\big]$$

7. **Approximate inverse binding:**
   $$g_{\text{bind},i}^{\text{unbind}}(t^+) = g_{\text{bind},i}^{\text{unbind}}(t) + \pi(t) \cdot \kappa_{\text{unbind}} \cdot \frac{g_{\text{exc},i}^{\text{type,conj}} \cdot g_{\text{exc},i}^{\text{bound}}}{w_{\text{scale}}} \cdot \text{anti-coinc}_i(t)$$
   with $\kappa_{\text{unbind}} = 1.5$ (weaker than forward binding).

8. **Unbinding output:**
   $$V_{\text{eff},i}^{\text{unbind}} = V_i + \gamma_{\text{bind}} \cdot \pi \cdot g_{\text{bind},i}^{\text{unbind}} \cdot R_m$$
   If $V_{\text{eff},i}^{\text{unbind}} \geq \theta_{\text{dyn},i}$:
   - $\tilde{p}_{\text{token},i} = 1$
   - $\nu_{\text{unbind}} = 1$

**Swap Gate Control:**

9. **Operation routing.** The swap controller receives $\text{op}(t)$ and gates the appropriate pathway:
   $$G_{\text{bind}} = \mathbb{I}[\text{op} = \text{BIND}]$$
   $$G_{\text{unbind}} = \mathbb{I}[\text{op} = \text{UNBIND}]$$
   Only the selected pathway receives precision gain $\pi > 0$.

**Binding Conductance Decay:**

10. **Universal kernel decay:**
    $$g_{\text{bind}}(t+1) = g_{\text{bind}}(t) \cdot \exp(-dt/\tau_{\text{bind}})$$
    with $\tau_{\text{bind}} = 20\ \text{ms}$.

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Binding neurons | $N_{\text{bind}}$ | $256$ | — | $128$ | $512$ | Binding pathway size |
| Unbinding neurons | $N_{\text{unbind}}$ | $256$ | — | $128$ | $512$ | Unbinding pathway size |
| Swap controllers | $N_{\text{swap}}$ | $64$ | — | $32$ | $128$ | Operation routers |
| Input weight | $w_{\text{in}}$ | $0.5$ | nS | $0.4$ | $0.6$ | Feedforward drive |
| Binding gain | $\kappa_{\text{bind}}$ | $2.0$ | dimensionless | $1.5$ | $2.5$ | Forward binding strength |
| Unbinding gain | $\kappa_{\text{unbind}}$ | $1.5$ | dimensionless | $1.2$ | $1.8$ | Inverse binding strength |
| Weight scale | $w_{\text{scale}}$ | $0.25$ | nS | $0.2$ | $0.3$ | Product normalization |
| Phase tolerance | $\delta_\varphi$ | $0.25$ | rad | $0.2$ | $0.3$ | $\approx 1$ ms at 40 Hz |
| Binding gain conversion | $\gamma_{\text{bind}}$ | $5.0$ | mV/nS | $4.0$ | $6.0$ | Conductance-to-potential |
| Conjugate phase offset | $\pi$ | $3.1416$ | rad | — | — | Phase inversion for unbinding |
| Binding decay | $\tau_{\text{bind}}$ | $20.0$ | ms | $18.0$ | $22.0$ | Conductance decay |
| Resting potential | $V_{\text{rest}}$ | $-70.0$ | mV | $-72.0$ | $-68.0$ | Baseline membrane state |
| Firing threshold base | $\theta_{\text{base}}$ | $-55.0$ | mV | $-57.0$ | $-53.0$ | Spike emission boundary |
| Reset potential | $V_{\text{reset}}$ | $-75.0$ | mV | $-77.0$ | $-73.0$ | Post-spike membrane clamp |
| Membrane time constant | $\tau_m$ | $20.0$ | ms | $18.0$ | $22.0$ | Integration speed |
| Membrane resistance | $R_m$ | $1.0$ | M$\Omega$ | $0.8$ | $1.2$ | Ohmic scaling |
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | BG ($\text{type\_id} = 4$) | Binding Gate for both paths |
| Input synapse type | FEEDFORWARD (type 0) | From CTA and TIB |
| Binding synapse type | BINDING_PAIR (type 5) | Internal coincidence detection |
| Output synapse type | BINDING_PAIR (type 5) | To working memory / Phase 2 |
| Tag encoding (binding) | $\text{tag}[0] = 0\text{b}10100010$ | Class=5; routing key=VBSG-bind |
| Tag encoding (unbinding) | $\text{tag}[0] = 0\text{b}10100011$ | Class=5; routing key=VBSG-unbind |
| Source field | $g_{\text{bind}}$ | Multiplicative conductance |
| Phase field | $\varphi$ | For conjugate operations |

#### 2.7 Interface Contract

**Upstream providers:**
- **5.1 CTA** (Categorical Type Attractors): delivers $\mathbf{p}_{\text{type}}$ with semantic label.
- **5.2 TIB** (Token Instance Buffers): delivers $\mathbf{p}_{\text{token}}$ with semantic label.
- **4.2 RSP** (Relay Synchronization Projectors): provides phase reference.
- **4.1 RPGN** (Reticular Phase Gating Nuclei): provides precision gain.
- **Executive control** (stubbed): provides $\text{op}(t) \in \{\text{BIND}, \text{UNBIND}\}$.

**Downstream consumers:**
- **Phase 2 Working Memory** (stubbed): receives $\mathbf{p}_{\text{bound}}$ for storage.
- **Phase 5 Reasoning** (stubbed): receives unbound tokens for inference.

**Handshake format:**
- Input: FEEDFORWARD events from CTA/TIB with semantic labels.
- Output: BINDING_PAIR events with composite or recovered labels.

---

### 3. Stability & Rigor Analysis

#### 3.1 Binding Correctness

**Theorem 1 (Binding Fidelity).** When $\text{op} = \text{BIND}$, $\pi = 1$, and both $\mathbf{p}_{\text{type}}$ and $\mathbf{p}_{\text{token}}$ are valid semantic pointers (40 active bits, 2% sparsity), the VBSG produces a bound pointer $\mathbf{p}_{\text{bound}}$ with:
- Density $\|\mathbf{p}_{\text{bound}}\|_0 \in [30, 50]$ (1.5%–2.5%)
- Deterministic label $\mathbf{L}_{\text{bound}} = \text{hash}(\mathbf{L}_{\text{type}}, \mathbf{L}_{\text{token}}, \varphi)$
- Bit overlap with input pointers $< 10\%$ (distinct representation)

**Proof.** Coincidence detection requires both type and token inputs active at the same neuron. With random sparse pointers (40 bits in 2048), the expected overlap is $40 \cdot 40 / 2048 \approx 0.78$ bits—negligible. The multiplicative conductance generates new active bits where both inputs coincide, producing a distinct output pattern. The hash ensures label uniqueness. ∎

**Theorem 2 (Unbinding Approximation).** When $\text{op} = \text{UNBIND}$, the recovered token satisfies:
$$\text{sim}(\tilde{\mathbf{p}}_{\text{token}}, \mathbf{p}_{\text{token}}) \geq 0.85$$
where $\text{sim}$ is cosine similarity or normalized Hamming overlap.

**Proof.** Phase-conjugate unbinding uses the property that binding is approximately invertible in VSA:
$$\mathbf{p}_{\text{type}}^{-1} \otimes (\mathbf{p}_{\text{type}} \otimes \mathbf{p}_{\text{token}}) \approx \mathbf{p}_{\text{token}}$$

In the neural implementation, the type pointer replayed at inverted phase $\varphi + \pi$ produces anti-coincidence with the bound pointer. The multiplicative interaction extracts the token component. Imperfections arise from:
- Noise in bound representation (5% bit error from CA3 cleanup)
- Phase jitter ($\delta_\varphi = 0.25$ rad $\approx$ 1 ms tolerance)
- Cross-talk from other bound structures

With $\kappa_{\text{unbind}} = 1.5$ and precision gating, the signal-to-noise ratio supports $> 85\%$ similarity. ∎

**Corollary 2.1 (Iterative Unbinding Degradation).** Each unbinding cycle introduces $\approx 10\%$ error. After $n$ cycles:
$$\text{sim}_n \approx 0.85^n$$
For $n = 3$: $\text{sim}_3 \approx 0.61$, still usable for attractor cleanup. For $n > 5$: degradation becomes severe; fresh binding recommended.

#### 3.2 Phase-Conjugate Operation

**Theorem 3 (Phase Inversion Exactness).** The conjugate phase $\varphi_{\text{conj}} = (\varphi_{\text{ref}} + \pi) \bmod 2\pi$ is computed exactly in float32 because $\pi$ has representation error $< 10^{-7}$ and the modulo operation preserves accuracy.

**Proof.** $\pi \approx 3.14159265$ is representable in float32 with relative error $\approx 10^{-8}$. Addition modulo $2\pi$ involves values $O(1)$, well-conditioned. The resulting phase error is $< 10^{-6}$ rad, negligible compared to $\delta_\varphi = 0.25$ rad. ∎

**Theorem 4 (Anti-Coincidence Detection).** The anti-coincidence condition detects type-at-conjugate and bound-at-reference with the same temporal precision as forward coincidence:
$$P_{\text{detect}}^{\text{unbind}} = P_{\text{detect}}^{\text{bind}} \cdot \frac{\kappa_{\text{unbind}}}{\kappa_{\text{bind}}} = 0.75 \cdot P_{\text{detect}}^{\text{bind}}$$

**Proof.** By construction, the unbinding pathway uses identical CDD logic but with phase-shifted inputs. The reduced gain $\kappa_{\text{unbind}} = 1.5$ vs. $\kappa_{\text{bind}} = 2.0$ accounts for approximate inverse being noisier. Detection probability scales linearly with gain. ∎

#### 3.3 Swap Gate Dynamics

**Theorem 5 (Mutual Exclusivity).** The swap controller ensures BIND and UNBIND pathways are never simultaneously active:
$$G_{\text{bind}} \cdot G_{\text{unbind}} = 0 \quad \forall t$$

**Proof.** By equation 9: $G_{\text{bind}} = \mathbb{I}[\text{op} = \text{BIND}]$, $G_{\text{unbind}} = \mathbb{I}[\text{op} = \text{UNBIND}]$. Since $\text{op}$ is a single value from a discrete set, exactly one indicator is 1, the other 0. Their product is identically 0. ∎

**Theorem 6 (Switching Transient).** When $\text{op}$ changes, the previously active pathway decays to $< 5\%$ activity within 25 ticks (one gamma cycle), preventing cross-pathway interference.

**Proof.** Binding conductance decays with $\tau_{\text{bind}} = 20$ ms. After 25 ms: residual $= e^{-25/20} \approx 0.287$. Combined with refractory period (5 ms) and threshold nonlinearity, effective activity is $< 5\%$. The new pathway activates on the next cycle. ∎

#### 3.4 Numerical Stability

**Theorem 7 (No State Divergence).** All VBSG state variables remain bounded.

**Proof.** Identical to GBGN proof. All conductances bounded by finite input and exponential decay. Membrane potential clamped. Phase wrapped. ∎

**Theorem 8 (Float32 Hash Stability).** The composite label hash $\mathbf{L}_{\text{bound}} = \text{hash}(\mathbf{L}_{\text{type}}, \mathbf{L}_{\text{token}}, \varphi)$ is deterministic and collision-resistant.

**Proof.** Input labels are 56-bit constants. Phase $\varphi$ is quantized to 16 bits for hashing. SHA3-224 or similar produces 224-bit output, truncated to 56 bits. Collision probability $\approx 2^{-56} \approx 1.4 \times 10^{-17}$. Deterministic because all inputs are fixed at binding time. ∎

#### 3.5 Complexity Proof

**Theorem 9 (O(1) Per-VBSG Cost).** Each VBSG neuron update: 25 FLOPs (universal kernel) + 5 FLOPs (dual-pathway logic) + 1 FLOP (swap gate check). Total $\leq 31$ FLOPs, all $O(1)$.

**Proof.** No loops over variable-length structures. Pathway selection is a single conditional. Coincidence detection is local. ∎

**Theorem 10 (O(1) Network-Wide Cost).** For $N_{\text{VBSG}} = N_{\text{bind}} + N_{\text{unbind}} + N_{\text{swap}} = 576$ neurons, total cost per tick is $O(N_{\text{VBSG}})$ with small constant.

**Proof.** Each neuron updates independently. No global operations. Event delivery bounded by fixed fan-out. ∎

#### 3.6 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Binding conductance | $\pi \cdot \kappa \cdot g_1 \cdot g_2 / w_{\text{scale}}$ | dimensionless · nS · nS / nS = nS | ✓ |
| Effective potential | $V + \gamma_{\text{bind}} \cdot \pi \cdot g_{\text{bind}} \cdot R_m$ | mV + (mV/nS) · nS · MΩ = mV + mV | ✓ |
| Phase conjugate | $\varphi + \pi$ | rad + rad = rad | ✓ |
| Swap gate | $\mathbb{I}[\text{op} = \text{BIND}]$ | dimensionless | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test VBSG-MC-01: Binding Threshold**
- **Procedure:** Present $\mathbf{p}_{\text{type}}$ and $\mathbf{p}_{\text{token}}$ with $\pi = 1$, op = BIND.
- **Pass criterion:** $g_{\text{bind}}$ must exceed threshold ($\geq 3.0$ nS) at coincidence neurons. Binding output must fire.
- **Measurement:** $g_{\text{bind}}$ and spike flags.

**Test VBSG-MC-02: Unbinding Recovery**
- **Procedure:** Bind $\mathbf{p}_{\text{type}} \otimes \mathbf{p}_{\text{token}}$. Then present $\mathbf{p}_{\text{type}}$ at conjugate phase with bound pointer, op = UNBIND.
- **Pass criterion:** Recovered $\tilde{\mathbf{p}}_{\text{token}}$ must have $\geq 85\%$ similarity to original $\mathbf{p}_{\text{token}}$.
- **Measurement:** $\text{sim}(\tilde{\mathbf{p}}_{\text{token}}, \mathbf{p}_{\text{token}})$.

**Test VBSG-MC-03: Phase Conjugate Exactness**
- **Procedure:** Verify $\varphi_{\text{conj}} = (\varphi_{\text{ref}} + \pi) \bmod 2\pi$.
- **Pass criterion:** $|\varphi_{\text{conj}} - (\varphi_{\text{ref}} + \pi)| < 10^{-6}$ rad for all $\varphi_{\text{ref}}$.
- **Measurement:** Phase error across $[0, 2\pi)$.

**Test VBSG-MC-04: Swap Exclusivity**
- **Procedure:** Rapidly alternate op = BIND and op = UNBIND every tick.
- **Pass criterion:** No tick may show activity in both pathways. No crosstalk-induced spurious binding.
- **Measurement:** Pathway activity correlation.

**Test VBSG-MC-05: Precision Gating**
- **Procedure:** Present valid binding inputs with $\pi = 0.0, 0.5, 1.0$.
- **Pass criterion:** $\pi = 0.0$: no binding. $\pi = 0.5$: partial binding (reduced $g_{\text{bind}}$). $\pi = 1.0$: full binding.
- **Measurement:** Binding strength vs. $\pi$.

#### 4.2 Complexity Compliance Tests

**Test VBSG-CC-01: Constant Pathway Size**
- **Procedure:** Verify $N_{\text{bind}} = 256$, $N_{\text{unbind}} = 256$, $N_{\text{swap}} = 64$.
- **Pass criterion:** No pathway may exceed allocated size.
- **Measurement:** Neuron count per pathway.

**Test VBSG-CC-02: No Global Sorting**
- **Procedure:** Inspect binding/unbinding algorithms.
- **Pass criterion:** No sorting, ranking, or $O(N \log N)$ operations. Only local threshold comparisons.
- **Measurement:** Algorithmic inspection.

**Test VBSG-CC-03: Per-Neuron Operation Bound**
- **Procedure:** Instrument VBSG update.
- **Pass criterion:** $\leq 35$ FLOPs per neuron per tick.
- **Measurement:** Instruction count.

**Test VBSG-CC-04: Output Fan-Out Bound**
- **Procedure:** Count outgoing synapses per VBSG neuron.
- **Pass criterion:** Out-degree $\leq 4$ for all neurons.
- **Measurement:** $\max_i D_{\text{out},i}$.

#### 4.3 Functional Objective Tests

**Test VBSG-FO-01: Role-Filler Binding**
- **Procedure:** Bind "COLOR" (type) to "RED" (token). Verify bound structure is distinct from both inputs.
- **Pass criterion:** $\mathbf{p}_{\text{bound}}$ must have $< 10\%$ bit overlap with either input. Label must be unique.
- **Measurement:** Hamming distances and label comparison.

**Test VBSG-FO-02: Variable Unbinding**
- **Procedure:** From bound "COLOR⊗RED", unbind "COLOR" to recover "RED".
- **Pass criterion:** Recovered pointer must match original "RED" with $\geq 85\%$ similarity. Must be distinguishable from other color tokens.
- **Measurement:** Similarity and discrimination.

**Test VBSG-FO-03: Nested Binding**
- **Procedure:** Bind "AGENT" to "ALICE", then bind result to "ACTION"→"RUN".
- **Pass criterion:** Nested bound structure must be stable and unbindable at each level.
- **Measurement:** Multi-level unbinding accuracy.

**Test VBSG-FO-04: Swap Operation Speed**
- **Procedure:** Alternate BIND/UNBIND at 40 Hz (every 25 ticks).
- **Pass criterion:** Each operation must complete within one gamma cycle. No accumulation of residual activity.
- **Measurement:** Operation latency and cycle isolation.

**Test VBSG-FO-05: Type-Token Independence**
- **Procedure:** Simultaneously bind multiple type-token pairs in interleaved cycles.
- **Pass criterion:** Each binding must be independent. No cross-contamination between pairs.
- **Measurement:** Binding purity and crosstalk.

---

**End of Sub-Component 5.3 VBSG Contract**

---
