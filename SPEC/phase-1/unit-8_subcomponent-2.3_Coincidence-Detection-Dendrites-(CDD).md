Kimi: **PHASE 1 | SUB-COMPONENT 2.3: Coincidence Detection Dendrites (CDD)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Examine every pair of incoming synaptic events on a binding neuron and check whether they carry identical 8-byte semantic labels.
- Accept the pair only if both events arrive within a programmable time window of 1 to 4 milliseconds and the labels match bit-for-bit.
- When a valid pair is detected, trigger a multiplicative conductance increment that is larger than the sum of the individual inputs would produce, creating a nonlinear binding signal.
- Reject all single inputs or mismatched pairs, ensuring no binding occurs without genuine coincidence.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| Synaptic event stream | $\mathcal{E}(t)$ | Set of arriving events | Each event $e = (\text{post\_id}, w, \delta, \mathbf{L}, t_{\text{arr}})$ |
| Semantic label | $\mathbf{L}$ | $\{0,1\}^{56}$ | 7-byte tag from tag[1..7] |
| Binding window open | $t_{\text{open}}$ | $\mathbb{Z}_{\geq 0}$ | $t_{\text{open}} = 25n$ (RSP sync) |
| Binding window close | $t_{\text{close}}$ | $\mathbb{Z}_{\geq 0}$ | $t_{\text{close}} = 25n + \delta_{\text{CWR}}$ from 4.3 CWR |
| Window indicator | $W(t)$ | $\{0,1\}$ | $W(t) = 1 \iff t \in [t_{\text{open}}, t_{\text{close}}]$ |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Coincidence flag | $C(t)$ | $\{0,1\}$ | $1$ iff valid binding pair detected at $t$ |
| Binding conductance | $g_{\text{bind}}(t)$ | $[0, \infty)\ \text{nS}$ | Multiplicative accumulation on GBGN |
| Coincidence count | $N_{\text{coinc}}(t)$ | $\mathbb{Z}_{\geq 0}$ | Number of matched pairs at $t$ |

#### 2.3 State Space Definition

The CDD logic operates at the **dendritic computation level**, not as a separate neuron population. It is implemented as a computational stage within each GBGN neuron's input processing:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Event buffer (per GBGN) | $\mathcal{B}$ | Array[4] of events | Empty | Holds up to 4 recent unmatched events |
| Buffer timestamp | $t_{\text{buf},k}$ | ticks | $-1$ | Arrival tick of buffered event $k$ |
| Buffer label | $\mathbf{L}_{\text{buf},k}$ | $\{0,1\}^{56}$ | $\mathbf{0}$ | Label of buffered event $k$ |
| Buffer weight | $w_{\text{buf},k}$ | nS | $0.0$ | Weight of buffered event $k$ |
| Buffer valid flag | $v_k$ | $\{0,1\}$ | $0$ | Whether slot $k$ contains active event |

**GBGN target neuron state (receiving CDD output):**

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Binding conductance | $g_{\text{bind}}$ | nS | $0.0$ | Accumulates multiplicative binding signal |
| Phase | $\varphi$ | rad | $0.0$ | Gamma cycle tracking |

#### 2.4 Governing Equations

**Event Arrival and Buffering (per GBGN neuron, per tick):**

1. **Window check.** For each arriving event $e$ at tick $t$:
   $$W(t) = \mathbb{I}\big[(t \bmod 25) \leq \delta_{\text{CWR}}\big]$$
   If $W(t) = 0$, the event is discarded (binding window closed).

2. **Label extraction.** From synapse tag bytes:
   $$\mathbf{L} = (\text{tag}[1], \text{tag}[2], \dots, \text{tag}[7]) \in \{0,1\}^{56}$$

3. **Buffer insertion.** Find first empty slot $k$ where $v_k = 0$:
   - $t_{\text{buf},k} \leftarrow t$
   - $\mathbf{L}_{\text{buf},k} \leftarrow \mathbf{L}$
   - $w_{\text{buf},k} \leftarrow w_{\text{out}}$ (from SPPF)
   - $v_k \leftarrow 1$

   If no empty slot exists, overwrite oldest event:
   $$k_{\text{oldest}} = \arg\min_{k: v_k=1} t_{\text{buf},k}$$
   (Deterministic tie-breaking by lowest index.)

**Coincidence Detection (per GBGN neuron, per tick, after all arrivals):**

4. **Pairwise label matching.** For each pair $(k_1, k_2)$ with $k_1 < k_2$, $v_{k_1} = v_{k_2} = 1$:
   $$\text{match}(k_1, k_2) = \mathbb{I}\big[\mathbf{L}_{\text{buf},k_1} = \mathbf{L}_{\text{buf},k_2}\big] \cdot \mathbb{I}\big[|t_{\text{buf},k_1} - t_{\text{buf},k_2}| \leq \Delta t_{\max}\big]$$
   where $\Delta t_{\max} = 2$ ticks ($2\ \text{ms}$) is the maximum temporal separation for coincidence.

5. **Multiplicative conductance generation.** For each matched pair:
   $$g_{\text{bind}}(t^+) = g_{\text{bind}}(t) + \kappa_{\text{bind}} \cdot \sqrt{w_{\text{buf},k_1} \cdot w_{\text{buf},k_2}}$$
   where $\kappa_{\text{bind}} = 2.0$ is the binding gain factor. The geometric mean produces superlinear summation:
   $$\sqrt{w_1 \cdot w_2} > \frac{w_1 + w_2}{2} \iff (w_1 - w_2)^2 < 0$$
   Wait—this is incorrect. The geometric mean is actually $\leq$ the arithmetic mean. We need true multiplicative enhancement. Corrected:
   $$g_{\text{bind}}(t^+) = g_{\text{bind}}(t) + \kappa_{\text{bind}} \cdot w_{\text{buf},k_1} \cdot w_{\text{buf},k_2} \cdot \frac{1}{w_{\text{scale}}}$$
   where $w_{\text{scale}} = 0.25\ \text{nS}$ is a normalization constant ensuring the product has the right dimension and magnitude. With $w_1 = w_2 = 0.5\ \text{nS}$:
   $$\Delta g_{\text{bind}} = 2.0 \cdot \frac{0.5 \cdot 0.5}{0.25} = 2.0 \cdot 1.0 = 2.0\ \text{nS}$$
   This exceeds the linear sum $w_1 + w_2 = 1.0\ \text{nS}$ by factor 2, achieving true multiplicative enhancement.

6. **Buffer cleanup.** After processing all pairs at tick $t$, invalidate all events with age $> \Delta t_{\max}$:
   $$v_k \leftarrow 0 \quad \text{if} \quad t - t_{\text{buf},k} > \Delta t_{\max}$$

**GBGN Universal Kernel Integration:**

7. **Binding conductance decay (GBGN step 2):**
   $$g_{\text{bind}}(t+1) = g_{\text{bind}}(t^+) \cdot \exp(-dt/\tau_{\text{bind}})$$
   with $\tau_{\text{bind}} = 20\ \text{ms}$.

8. **Synaptic current with binding term (GBGN step 3):**
   $$I_{\text{syn}} = g_{\text{exc}}(V - E_{\text{exc}}) + g_{\text{inh}}(V - E_{\text{inh}}) + g_{\text{bind}}(V - E_{\text{bind}})$$
   where $E_{\text{bind}} = 0.0\ \text{mV}$.

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Buffer size | $B$ | $4$ | slots | $4$ | $4$ | Maximum simultaneous events per GBGN |
| Max temporal separation | $\Delta t_{\max}$ | $2$ | ticks | $1$ | $3$ | Coincidence time tolerance |
| Binding gain | $\kappa_{\text{bind}}$ | $2.0$ | dimensionless | $1.5$ | $2.5$ | Multiplicative enhancement factor |
| Weight scale | $w_{\text{scale}}$ | $0.25$ | nS | $0.2$ | $0.3$ | Product normalization |
| Window duration | $\delta_{\text{CWR}}$ | $4$ | ms | $1$ | $4$ | From CWR programming |
| Binding decay time constant | $\tau_{\text{bind}}$ | $20.0$ | ms | $18.0$ | $22.0$ | Conductance decay speed |
| Binding reversal | $E_{\text{bind}}$ | $0.0$ | mV | — | — | Same as excitatory |
| Tag match width | $|\mathbf{L}|$ | $56$ | bits | $56$ | $56$ | Exact match required |
| GBGN buffer count | $N_{\text{buf}}$ | $1$ | per GBGN | $1$ | $1$ | Single buffer per neuron |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | BG ($\text{type\_id} = 4$) | Binding Gate receives CDD output |
| Synapse type (input) | FEEDFORWARD (type 0) | From SPPF with semantic labels |
| Synapse type (output) | BINDING_PAIR (type 5) | GBGN→GBGN for binding propagation |
| Tag encoding | $\text{tag}[0]$ bits [0:2] = 5 (BINDING_PAIR) | Distinguishes binding synapses |
| Source field | Event buffer $\mathcal{B}$ | Dendritic computation state |
| Target field | $g_{\text{bind}}$ (BG neuron) | Multiplicative conductance |
| Label field | $\text{tag}[1..7]$ | 56-bit semantic identifier |

#### 2.7 Interface Contract

**Upstream providers:**
- **1.3 SPPF** (Semantic Pointer Projection Fibers): delivers tagged FEEDFORWARD events with semantic labels $\mathbf{L}$ and weights $w_{\text{out}}$.
- **4.3 CWR** (Coincidence Window Regulators): provides window indicator $W(t)$ via LATERAL_INH timing marks.
- **4.2 RSP** (Relay Synchronization Projectors): provides cycle boundary $t_{\text{open}} = 25n$.

**Downstream consumers:**
- **2.1 GBGN** (Granule Binding Gate Neurons): consumes $g_{\text{bind}}$ increment as part of its own membrane dynamics.

**Handshake format:**
- Input events carry $\text{post\_id}$ (GBGN target), $w_{\text{out}}$, $\text{delay}$, and full 8-byte tag including $\mathbf{L}$.
- CDD output is internal to GBGN dendritic computation; no external spike event is generated for $g_{\text{bind}}$ increment.

---

### 3. Stability & Rigor Analysis

#### 3.1 Coincidence Detection Correctness

**Theorem 1 (Exact Label Matching).** Two events $e_1, e_2$ are declared coincident if and only if their 56-bit semantic labels are identical and their arrival times differ by at most $\Delta t_{\max} = 2$ ticks:
$$C(e_1, e_2) = 1 \iff \mathbf{L}_1 = \mathbf{L}_2 \land |t_1 - t_2| \leq 2$$

**Proof.** By construction in equations 4–5: the match function requires bitwise equality of all 56 label bits (implemented as 7-byte comparison) and temporal proximity within the buffer window. No fuzzy matching or Hamming distance tolerance is applied. ∎

**Theorem 2 (False Positive Bound).** The probability of a false positive coincidence (two unrelated events with random labels matching) is:
$$P_{\text{fp}} = 2^{-56} \approx 1.39 \times 10^{-17}$$

**Proof.** With uniformly random 56-bit labels, the probability that two independent labels collide is exactly $2^{-56}$. This is negligible; for practical event rates ($< 10^6$ events/sec), expected false positives $< 10^{-11}$ per second. ∎

**Theorem 3 (Multiplicative Enhancement).** For two matched events with weights $w_1, w_2 \in [0.3, 0.7]\ \text{nS}$, the binding conductance increment satisfies:
$$\Delta g_{\text{bind}} = \kappa_{\text{bind}} \cdot \frac{w_1 \cdot w_2}{w_{\text{scale}}} \geq \kappa_{\text{bind}} \cdot \frac{w_{\min}^2}{w_{\text{scale}}} = 2.0 \cdot \frac{0.09}{0.25} = 0.72\ \text{nS}$$
and
$$\Delta g_{\text{bind}} \leq 2.0 \cdot \frac{0.49}{0.25} = 3.92\ \text{nS}$$

**Proof.** Direct substitution of bounds. The product form ensures $\Delta g_{\text{bind}}$ is superlinear in individual weights when both are present, but sublinear when only one arrives (no coincidence = no binding). ∎

**Corollary 3.1 (Subthreshold Isolation).** A single unmatched input event produces $\Delta g_{\text{bind}} = 0$. The GBGN neuron integrates only standard $g_{\text{exc}}$ and $g_{\text{inh}}$ from non-coincident inputs.

#### 3.2 Temporal Window Constraints

**Theorem 4 (Window Compliance).** Events arriving outside $[t_{\text{open}}, t_{\text{close}}]$ are unconditionally rejected:
$$W(t) = 0 \implies \forall e \text{ arriving at } t: e \text{ is discarded}$$

**Proof.** Equation 1: the window indicator is checked before buffer insertion. If $W(t) = 0$, the event bypasses the buffer entirely. ∎

**Theorem 5 (Buffer Overflow Handling).** With buffer size $B = 4$ and maximum event rate $\lambda_{\max} = 4$ events/tick per GBGN (bounded by SPPF fan-in), the buffer never overflows during normal operation. In pathological cases, oldest-event overwrite preserves the most recent candidates.

**Proof.** Each GBGN receives from at most 4 SPPF relays (by SPPF out-degree bound). Events decay from buffer after $\Delta t_{\max} = 2$ ticks. With $\lambda \leq 4$ and drain rate $\geq 2$ events/tick (age-based eviction), steady-state occupancy $\leq 4 = B$. ∎

#### 3.3 Convergence Bounds

**Theorem 6 (Binding Conductance Decay).** After a coincidence event at $t = 0$, $g_{\text{bind}}$ decays as:
$$g_{\text{bind}}(t) = g_{\text{bind}}(0) \cdot \exp(-t/\tau_{\text{bind}})$$
At $t = 20\ \text{ms}$: $g_{\text{bind}} = g_{\text{bind}}(0) \cdot e^{-1} \approx 0.368 \cdot g_{\text{bind}}(0)$.
At $t = 25\ \text{ms}$ (cycle end): $g_{\text{bind}} = g_{\text{bind}}(0) \cdot e^{-1.25} \approx 0.287 \cdot g_{\text{bind}}(0)$.

**Proof.** Exponential decay with $\tau_{\text{bind}} = 20\ \text{ms}$. ∎

**Theorem 7 (Cycle Isolation).** Residual $g_{\text{bind}}$ from cycle $n$ at the start of cycle $n+1$ ($t = 25\ \text{ms}$ later) is bounded by:
$$g_{\text{bind}}(25) \leq 3.92 \cdot e^{-1.25} \approx 1.12\ \text{nS}$$
This is below the typical GBGN firing threshold contribution from binding alone, preventing cross-cycle contamination.

**Proof.** Maximum $\Delta g_{\text{bind}} = 3.92\ \text{nS}$ from Theorem 3. Multiply by $e^{-25/20} = e^{-1.25} \approx 0.287$. ∎

#### 3.4 Numerical Stability

**Theorem 8 (No State Divergence).** The CDD buffer state $(t_{\text{buf},k}, \mathbf{L}_{\text{buf},k}, w_{\text{buf},k}, v_k)$ for all $k \in \{1,\dots,4\}$ remains bounded for all $t \geq 0$.

**Proof.**
- $t_{\text{buf},k} \in [t - \Delta t_{\max}, t]$ (bounded by eviction).
- $\mathbf{L}_{\text{buf},k} \in \{0,1\}^{56}$ (finite discrete set).
- $w_{\text{buf},k} \in [0.3, 0.7]$ (bounded by SPPF output weights).
- $v_k \in \{0,1\}$.
All bounded. ∎

**Theorem 9 (Integer Exactness of Label Comparison).** The 56-bit label comparison is implemented as 7 sequential byte comparisons or a single 64-bit integer comparison (with 8-bit padding). Both are exact; no floating-point error occurs.

**Proof.** uint8 equality is bitwise exact. Seven AND-OR reductions produce exact match result. If using uint64_t: cast is exact, comparison is exact. ∎

#### 3.5 Complexity Proof

**Theorem 10 (O(1) Per-GBGN Cost).** The CDD coincidence detection on a single GBGN neuron consumes exactly:
- Buffer insertion: $\leq 4$ slot checks (constant)
- Pairwise matching: $\binom{4}{2} = 6$ comparisons (constant)
- Conductance update: $\leq 6$ increments (constant)
- Cleanup: $\leq 4$ age checks (constant)

Total: $\leq 20$ operations, all $O(1)$.

**Proof.** Buffer size $B = 4$ is compile-time constant. Pairwise comparisons over fixed set: $\binom{B}{2} = 6$. No loops over variable-length structures. No recursion. ∎

**Corollary 10.1 (Network-Wide CDD Cost).** For $N_{\text{GBGN}}$ binding gate neurons, total CDD cost per tick is $O(N_{\text{GBGN}})$ with small constant factor.

#### 3.6 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Window indicator | $\mathbb{I}[\dots]$ | dimensionless | ✓ |
| Label match | $\mathbb{I}[\mathbf{L}_1 = \mathbf{L}_2]$ | dimensionless | ✓ |
| Temporal check | $\mathbb{I}[|t_1 - t_2| \leq 2]$ | dimensionless | ✓ |
| Product normalization | $w_1 \cdot w_2 / w_{\text{scale}}$ | $\text{nS} \cdot \text{nS} / \text{nS} = \text{nS}$ | ✓ |
| Binding increment | $\kappa_{\text{bind}} \cdot (\dots)$ | $\text{dimensionless} \cdot \text{nS} = \text{nS}$ | ✓ |
| Conductance decay | $\exp(-dt/\tau_{\text{bind}})$ | dimensionless | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test CDD-MC-01: Exact Label Match**
- **Procedure:** Inject two events with identical labels $\mathbf{L} = (0x01, 0x02, \dots, 0x07)$ at $t = 0$ and $t = 1$.
- **Pass criterion:** $C(t) = 1$ at $t = 1$. $g_{\text{bind}}$ must increase by $\Delta g_{\text{bind}} = 2.0 \cdot w_1 \cdot w_2 / 0.25$.
- **Measurement:** Match flag and $g_{\text{bind}}$ before/after.

**Test CDD-MC-02: Label Mismatch**
- **Procedure:** Inject two events with labels differing in exactly 1 bit (e.g., $\mathbf{L}_1$ vs. $\mathbf{L}_1 \oplus (1 \ll 20)$).
- **Pass criterion:** $C(t) = 0$. $g_{\text{bind}}$ must not increase.
- **Measurement:** Match flag and $g_{\text{bind}}$.

**Test CDD-MC-03: Temporal Separation Boundary**
- **Procedure:** Inject two matching events at $t = 0$ and $t = 2$ (max separation), then at $t = 0$ and $t = 3$ (exceeds bound).
- **Pass criterion:** Match for $|t_1 - t_2| = 2$. No match for $|t_1 - t_2| = 3$.
- **Measurement:** Match flags for both cases.

**Test CDD-MC-04: Single Event Rejection**
- **Procedure:** Inject exactly one event with any label.
- **Pass criterion:** No $g_{\text{bind}}$ increment. Buffer holds 1 event; no pairwise match possible.
- **Measurement:** $g_{\text{bind}}$ and buffer occupancy.

**Test CDD-MC-05: Window Rejection**
- **Procedure:** Configure $\delta_{\text{CWR}} = 4$. Inject events at $t = 5$ (outside window).
- **Pass criterion:** Events at $t = 5$ discarded. No buffer insertion. No binding.
- **Measurement:** Buffer state and $g_{\text{bind}}$.

#### 4.2 Complexity Compliance Tests

**Test CDD-CC-01: Constant Buffer Size**
- **Procedure:** Verify buffer allocation per GBGN neuron.
- **Pass criterion:** Exactly 4 slots per GBGN. No dynamic allocation.
- **Measurement:** Memory audit of GBGN dendritic structures.

**Test CDD-CC-02: Fixed Pairwise Count**
- **Procedure:** Count comparison operations in coincidence detection.
- **Pass criterion:** Exactly $\binom{4}{2} = 6$ label comparisons per GBGN per tick. No variable-length iteration.
- **Measurement:** Static code inspection or instruction counting.

**Test CDD-CC-03: No Global Aggregation**
- **Procedure:** Inspect CDD algorithm across all GBGN neurons.
- **Pass criterion:** Each GBGN operates independently. No cross-neuron state sharing. No global sum or broadcast.
- **Measurement:** Algorithmic inspection.

**Test CDD-CC-04: Event Rate Bound**
- **Procedure:** Stress-test with maximum SPPF fan-in (4 events/tick) to single GBGN.
- **Pass criterion:** Buffer handles overflow gracefully (oldest overwrite). No crash or unbounded growth.
- **Measurement:** Buffer occupancy over 100 ticks.

#### 4.3 Functional Objective Tests

**Test CDD-FO-01: Binding Fidelity**
- **Procedure:** Present two semantic pointers $\mathbf{p}_A, \mathbf{p}_B$ through SPPF with shared label $\mathbf{L}_{\text{bind}}$. Measure GBGN $g_{\text{bind}}$ response.
- **Pass criterion:** $g_{\text{bind}}$ must exceed $g_{\text{exc}}$ from either input alone by factor $\geq 1.5$. Multiplicative enhancement must be demonstrable.
- **Measurement:** $g_{\text{bind}}$ vs. $g_{\text{exc}}$ for single and paired inputs.

**Test CDD-FO-02: Cross-Label Isolation**
- **Procedure:** Present $\mathbf{p}_A$ with label $\mathbf{L}_1$ and $\mathbf{p}_B$ with label $\mathbf{L}_2 \neq \mathbf{L}_1$ simultaneously.
- **Pass criterion:** No binding conductance generated. GBGN must treat inputs as independent (standard FEEDFORWARD integration only).
- **Measurement:** $g_{\text{bind}}$ and $g_{\text{exc}}$ under mixed labels.

**Test CDD-FO-03: Temporal Precision**
- **Procedure:** Deliver matching events at various separations: 0 ms, 1 ms, 2 ms, 3 ms.
- **Pass criterion:** Binding occurs for 0, 1, 2 ms. No binding for 3 ms.
- **Measurement:** Binary binding outcome vs. temporal separation.

**Test CDD-FO-04: Buffer Eviction**
- **Procedure:** Deliver 5 events with identical labels at 1-tick intervals (overwhelming buffer).
- **Pass criterion:** Only the 4 most recent events retained. Oldest event evicted. Binding still occurs among retained events.
- **Measurement:** Buffer contents and binding outcomes after each arrival.

**Test CDD-FO-05: Cycle Boundary Isolation**
- **Procedure:** Trigger binding at end of cycle $n$ ($t = 24$). Verify no residual binding affects cycle $n+1$ ($t = 25$).
- **Pass criterion:** $g_{\text{bind}}(25)$ must be $< 20\%$ of peak. New cycle starts with clean binding state.
- **Measurement:** $g_{\text{bind}}(24)$ vs. $g_{\text{bind}}(25)$.

---

**End of Sub-Component 2.3 CDD Contract**

**Next in dependency order: 2.1 Granule Binding Gate Neurons (GBGN).**

**Awaiting directive to proceed.**
User: continue
