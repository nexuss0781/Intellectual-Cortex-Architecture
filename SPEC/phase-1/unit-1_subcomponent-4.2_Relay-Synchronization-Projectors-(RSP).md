Kimi: **PHASE 1 | SUB-COMPONENT 4.2: Relay Synchronization Projectors (RSP)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Provide a single, shared 40 Hz timing pulse to all intellectual neuron pools so that binding operations across distant populations agree on when a gamma cycle begins and ends.
- Guarantee that every intellectual pool receives this timing mark with nearly identical delay, ensuring no pool lags another by more than 1 millisecond.
- Maintain this timing signal indefinitely without cumulative clock drift, using a periodic external reference that resets the cycle boundary every 25 milliseconds.
- Keep the cost of receiving the timing signal constant for every neuron, regardless of how large the network grows.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| External pulse conductance | $\Delta g_{\text{pulse}}$ | $[4.5, 6.0]\ \text{nS}$ | Calibrated excitatory pulse delivered to every RSP master simultaneously at $t \equiv 0 \pmod{25}$ |
| Initial phase | $\varphi_0$ | $[0, 2\pi)$ | Startup alignment, set to $0$ at initialization |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Synchronization spike train | $S_i(t)$ | $\{0, 1\}$ | Binary spike indicator for master neuron $i$; $S_i(t)=1$ iff neuron $i$ fired at tick $t$ |
| Canonical phase reference | $\varphi_{\text{ref}}(t)$ | $[0, 2\pi)$ | $\varphi_{\text{ref}}(t) = \frac{2\pi \cdot t}{25} \bmod 2\pi$ |
| Broadcast delay | $\delta$ | $[0, 1]\ \text{ms}$ | Axonal delay from any RSP master to any target neuron |

#### 2.3 State Space Definition

Each RSP master neuron occupies a CI slot with the following active state:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V$ | mV | $-70.0$ | Integrates pulse input |
| Excitatory conductance | $g_{\text{exc}}$ | nS | $0.0$ | Receives $\Delta g_{\text{pulse}}$ every 25 ticks |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Counts down refractory period |
| Oscillatory phase | $\varphi$ | rad | $0.0$ | Tracks gamma cycle via universal kernel |
| Type identifier | $\text{type\_id}$ | — | $0$ (CI) | Fixed neuron class |

Target neurons in intellectual pools use their local $\varphi$ field (updated by the universal kernel) and receive RSP spikes via FEEDFORWARD synapses, incrementing their local $g_{\text{exc}}$ by $w_{\text{sync}}$ upon spike arrival.

#### 2.4 Governing Equations

**RSP Master Neuron (per tick, $dt = 1\ \text{ms}$):**

1. **Pulse injection (at $t \equiv 0 \pmod{25}$):**
   $$g_{\text{exc}}(t^+) = g_{\text{exc}}(t) + \Delta g_{\text{pulse}}$$

2. **Conductance decay (all ticks):**
   $$g_{\text{exc}}(t+1) = g_{\text{exc}}(t^+) \cdot \exp(-dt/\tau_{\text{exc}})$$
   with $\tau_{\text{exc}} = 5\ \text{ms}$.

3. **Synaptic current (standard biophysical convention):**
   $$I_{\text{syn}}(t) = g_{\text{exc}}(t^+) \cdot \bigl(E_{\text{exc}} - V(t)\bigr)$$
   where $E_{\text{exc}} = 0.0\ \text{mV}$.

4. **Membrane update (if $\text{spike\_timer} = 0$):**
   $$V(t+1) = V(t) + \frac{dt}{\tau_m}\Bigl[-\bigl(V(t) - V_{\text{rest}}\bigr) + R_m \cdot I_{\text{syn}}(t)\Bigr]$$
   with $V_{\text{rest}} = -70.0\ \text{mV}$, $\tau_m = 20\ \text{ms}$, $R_m = 1\ \text{M}\Omega$.

5. **Firing condition:**
   If $V(t+1) \geq \theta_{\text{base}} = -55.0\ \text{mV}$:
   - Emit spike: $S(t+1) = 1$
   - Reset: $V(t+1) \leftarrow V_{\text{reset}} = -75.0\ \text{mV}$
   - Refractory: $\text{spike\_timer} \leftarrow 5$

6. **Refractory countdown (if $\text{spike\_timer} > 0$):**
   $$\text{spike\_timer} \leftarrow \text{spike\_timer} - 1$$
   Skip steps 3–5.

7. **Phase rotation (universal kernel):**
   $$\varphi(t+1) = \bigl(\varphi(t) + \omega \cdot dt\bigr) \bmod 2\pi$$
   with $\omega = 2\pi \cdot 40\ \text{Hz} = 80\pi\ \text{rad/s}$, so $\omega \cdot dt = 2\pi/25\ \text{rad}$.

**Target Neuron (receiving RSP input):**

8. **Spike delivery (upon receiving RSP spike at delay $\delta$):**
   $$g_{\text{exc}}^{\text{target}}(t_{\text{arrive}}^+) = g_{\text{exc}}^{\text{target}}(t_{\text{arrive}}) + w_{\text{sync}}$$
   where $w_{\text{sync}} \in [0.1, 1.0]\ \text{nS}$ (subthreshold, non-firing).

9. **Local phase (autonomous, universal kernel):**
   $$\varphi_{\text{local}}(t+1) = \bigl(\varphi_{\text{local}}(t) + 2\pi/25\bigr) \bmod 2\pi$$
   initialized to $\varphi_{\text{local}}(0) = 2\pi \cdot \delta / 25$ to account for axonal delay.

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
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |
| Gamma frequency | $f_\gamma$ | $40.0$ | Hz | $38.0$ | $42.0$ | Oscillation frequency |
| Gamma period | $T_\gamma$ | $25.0$ | ticks | $24$ | $26$ | Cycle length in time steps |
| Phase increment | $\Delta\varphi$ | $2\pi/25$ | rad/tick | $0.24$ | $0.27$ | Per-tick phase advance |
| Pulse conductance | $\Delta g_{\text{pulse}}$ | $5.0$ | nS | $4.5$ | $6.0$ | External drive amplitude |
| Sync synaptic weight | $w_{\text{sync}}$ | $0.5$ | nS | $0.1$ | $1.0$ | Subthreshold broadcast signal |
| Max broadcast delay | $\delta_{\max}$ | $1.0$ | ms | $0.0$ | $1.0$ | Axonal latency bound |
| Master neuron count | $N_{\text{master}}$ | $64$ | — | $32$ | $128$ | Redundant pacemakers |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | CI ($\text{type\_id} = 0$) | Core Integrator for pacemaker function |
| Synapse type (master→target) | FEEDFORWARD (type 0) | Standard excitatory delivery |
| Tag encoding | $\text{tag}[0]$ bits $[0:2] = 0$ (FEEDFORWARD); bits $[5:7] = 001$ (RSP routing key) | Distinguishes sync spikes from data spikes |
| Source field | $g_{\text{exc}}$ (master) | Receives external pulse |
| Target field | $g_{\text{exc}}$ (target) | Receives sync weight increment |
| Phase field | $\varphi$ | Universal kernel step 7 |

#### 2.7 Interface Contract

**Upstream providers:**
- System clock: delivers $t$ and the external pulse trigger every 25 ticks.
- Meta-Cognitive Controller: allocates $N_{\text{master}}$ CI slots from the intellectual pool.

**Downstream consumers:**
- **2.2 GPLO** (Gamma Phase Locking Oscillators): receives sync spikes to entrain local BG populations.
- **2.3 CDD** (Coincidence Detection Dendrites): uses sync spike arrival as the $t=0$ boundary for $\pm 2\ \text{ms}$ coincidence windows.
- **3.1 CA3-RAN** (Hippocampal Cleanup): receives sync spikes to align attractor update timing with gamma cycles.
- All other intellectual pools: receive sync spikes for cycle alignment.

**Handshake format:**
- Spike packets carry $\text{post\_id}$ (target index), $w = w_{\text{sync}}$, $\text{delay} = \delta \in [0,1]\ \text{ms}$ (0–1 ticks), and tag byte with RSP routing key.
- No continuous variables are transmitted; synchronization is purely event-driven.

---

### 3. Stability & Rigor Analysis

#### 3.1 Fixed-Point and Periodicity

**Theorem 1 (Exact 40 Hz Periodicity).** Under the pulse protocol $\Delta g_{\text{pulse}} \geq 4.286\ \text{nS}$, each RSP master neuron fires exactly once every 25 ticks, with inter-spike interval $T_{\text{ISI}} = 25\ \text{ms} \pm 0\ \text{ms}$.

**Proof.** At $t = n \cdot 25$, just before pulse: $V = V_{\text{rest}} = -70\ \text{mV}$ (the neuron has been idle since the previous reset at $t = (n-1)\cdot 25$, and $25\ \text{ms} > 5\ \text{ms}$ refractory plus full recovery time). The pulse injects $g_{\text{exc}} = \Delta g_{\text{pulse}}$. By Equation 4:
$$V_{\text{new}} = -70 + 0.05 \cdot \bigl[0 + 1.0 \cdot \Delta g_{\text{pulse}} \cdot (0 - (-70))\bigr] = -70 + 3.5 \cdot \Delta g_{\text{pulse}}.$$
For $\Delta g_{\text{pulse}} = 5.0\ \text{nS}$: $V_{\text{new}} = -70 + 17.5 = -52.5\ \text{mV}$. Since $-52.5\ \text{mV} \geq -55.0\ \text{mV}$, the firing condition is satisfied. After firing, $V$ is clamped to $-75\ \text{mV}$ and $\text{spike\_timer} = 5$. For ticks $t = n\cdot 25 + 1$ through $(n+1)\cdot 25 - 1$, no further pulses arrive; the neuron integrates only leak, converging exponentially toward $V_{\text{rest}}$. By $t = (n+1)\cdot 25$, $V$ has returned to $-70\ \text{mV}$. Therefore the state at $t = (n+1)\cdot 25$ is identical to the state at $t = n\cdot 25$, establishing a periodic orbit with period $T = 25$ ticks. ∎

**Corollary 1.1 (Phase Alignment).** The spike emission occurs at the same global phase $\varphi_{\text{ref}} = 0 \pmod{2\pi}$ every cycle, because the pulse is delivered at $t \equiv 0 \pmod{25}$ and the latency from pulse to spike is $< 1$ tick ($<< 1\ \text{ms}$), negligible relative to $T_\gamma = 25\ \text{ms}$.

#### 3.2 Convergence Bounds

**Theorem 2 (Post-Reset Recovery).** After a spike at $t = 0$, the membrane potential converges to $V_{\text{rest}}$ exponentially:
$$\bigl|V(t) - V_{\text{rest}}\bigr| \leq \bigl|V_{\text{reset}} - V_{\text{rest}}\bigr| \cdot \exp(-t/\tau_m) = 5 \cdot \exp(-t/20)\ \text{mV}.$$

**Proof.** During refractory and recovery, $I_{\text{syn}} = 0$. The update reduces to $V(t+1) = V(t) + (dt/\tau_m)\cdot\bigl(V_{\text{rest}} - V(t)\bigr)$, the Euler discretization of $\tau_m\, dV/dt = -(V - V_{\text{rest}})$. The exact solution is exponential decay; the discrete update overestimates the time constant by $O(dt^2)$ but remains stable because $dt/\tau_m = 0.05 < 1.0$. ∎

**Practical bound.** At $t = 20\ \text{ms}$ (20 ticks), $|V - V_{\text{rest}}| < 0.05 \cdot 5 = 0.25\ \text{mV}$. The neuron is fully recovered well before the next pulse.

#### 3.3 Numerical Stability

**Theorem 3 (Phase Drift Bound).** The autonomous phase update $\varphi(t+1) = \bigl(\varphi(t) + 2\pi/25\bigr) \bmod 2\pi$, computed in float32, accumulates absolute error bounded by $\varepsilon_{\text{machine}} \cdot t$, where $\varepsilon_{\text{machine}} \approx 1.19 \times 10^{-7}$ for IEEE 754 single precision.

**Proof.** Each addition introduces relative rounding error $\leq \varepsilon_{\text{machine}}$. The increment $2\pi/25 \approx 0.2513$ has float32 representation error $\delta_0 \approx 10^{-8}$. After $T$ additions, total phase error $\leq T \cdot (\varepsilon_{\text{machine}} \cdot 2\pi/25 + \delta_0) \approx T \cdot 3 \times 10^{-8}\ \text{rad/tick}$. Over one hour ($T = 3.6 \times 10^6$ ticks), error $< 0.11\ \text{rad} \approx 6.3^\circ$. This is well within the gamma-cycle tolerance ($\pm 2\ \text{ms} = \pm 0.5\ \text{rad}$ at 40 Hz). Periodic spike-based alignment every 25 ticks resets any accumulated drift in target interpretation. ∎

**Theorem 4 (No State Divergence).** All state variables $(V, g_{\text{exc}}, \text{spike\_timer}, \varphi)$ remain bounded for all $t \geq 0$.

**Proof.**
- $V$ is clamped to $[-75, -52.5]\ \text{mV}$ by reset and threshold; leak pulls toward $-70\ \text{mV}$.
- $g_{\text{exc}}$ decays exponentially and receives bounded pulses; $\sup \leq \Delta g_{\text{pulse}} = 6.0\ \text{nS}$.
- $\text{spike\_timer} \in \{0,1,2,3,4,5\}$, bounded by construction.
- $\varphi \in [0, 2\pi)$, bounded by modulo operation.
Therefore no variable diverges. ∎

#### 3.4 Complexity Proof

**Theorem 5 (O(1) Per-Neuron Cost).** The RSP master update consumes exactly 8 arithmetic operations and 2 conditional branches per tick. The RSP broadcast delivery to any target neuron consumes exactly 1 synaptic weight application per target per spike event.

**Proof (Master).**
1. Check $t \bmod 25 = 0$ (1 integer modulo or bit-test).
2. Add $\Delta g_{\text{pulse}}$ to $g_{\text{exc}}$ (1 FLOP, conditional).
3. Multiply $g_{\text{exc}}$ by $\exp(-0.2)$ (1 FLOP).
4. Compute $I_{\text{syn}} = g_{\text{exc}} \cdot (E_{\text{exc}} - V)$ (2 FLOPs: subtraction, multiplication).
5. Compute $V$ update: 1 subtraction, 1 multiplication, 1 addition (3 FLOPs).
6. Compare $V \geq \theta_{\text{base}}$ (1 comparison).
7. If true: assignment $V \leftarrow V_{\text{reset}}$, $\text{spike\_timer} \leftarrow 5$ (2 assignments).
8. Decrement $\text{spike\_timer}$ if $> 0$ (1 integer op, conditional).
Total: $\leq 11$ scalar operations, all $O(1)$. No loops, no recursion, no dynamic allocation.

**Proof (Target).**
- Each target receives from $k \in [2, 4]$ RSP masters (constant, independent of network size).
- Per arriving spike: $g_{\text{exc}} \leftarrow g_{\text{exc}} + w_{\text{sync}}$ (1 FLOP).
- Per tick: autonomous $\varphi$ update (1 addition, 1 modulo).
- Total per target per tick: $\leq 5$ operations $= O(1)$. ∎

**Corollary 5.1 (Network-Wide Step Cost).** For $N$ intellectual neurons receiving RSP input, the total RSP-related cost per global tick is $O(N)$, dominated by the $O(N)$ autonomous $\varphi$ updates already required by the universal kernel. The broadcast adds at most $k \cdot N = O(N)$ synaptic events with $k$ constant.

#### 3.5 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Leak term | $-(V - V_{\text{rest}})$ | mV | ✓ |
| Ohmic term | $R_m \cdot I_{\text{syn}}$ | $\text{M}\Omega \cdot \text{nS} \cdot \text{mV} = \text{mV}$ | ✓ (since $\text{nS} \cdot \text{mV} = \text{pA}$; $\text{M}\Omega \cdot \text{pA} = \text{mV}$) |
| Membrane increment | $(dt/\tau_m) \cdot [\text{leak} + \text{Ohmic}]$ | $(\text{ms}/\text{ms}) \cdot \text{mV} = \text{mV}$ | ✓ |
| Phase increment | $\omega \cdot dt$ | $(\text{rad}/\text{s}) \cdot \text{s} = \text{rad}$ | ✓ |
| Conductance decay | $\exp(-dt/\tau_{\text{exc}})$ | dimensionless | ✓ |
| Delay bound | $\delta_{\max}$ | ms | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test RSP-MC-01: Pulse Threshold Crossing**
- **Procedure:** Initialize RSP master at $V = -70\ \text{mV}$, $g_{\text{exc}} = 0$. Inject $\Delta g_{\text{pulse}} = 4.5, 5.0, 6.0\ \text{nS}$ at $t = 0$.
- **Pass criterion:** $V$ after update $\geq -55.0\ \text{mV}$ for all $\Delta g_{\text{pulse}} \in [4.5, 6.0]\ \text{nS}$. Firing must occur within 1 tick of injection.
- **Measurement:** Record $V_{\text{pre}}$, $V_{\text{post}}$, spike flag.

**Test RSP-MC-02: Refractory Enforcement**
- **Procedure:** Force two consecutive pulses at $t = 0$ and $t = 1$ (1 ms apart).
- **Pass criterion:** Second pulse must not produce a spike; $\text{spike\_timer}$ must read $4$ at $t = 1$ (after decrement from 5).
- **Measurement:** Spike flag at $t = 0$ and $t = 1$.

**Test RSP-MC-03: Periodicity**
- **Procedure:** Run RSP master for 1,000 ticks (1 second). Record all spike times.
- **Pass criterion:** Inter-spike intervals must all equal exactly 25 ticks. Zero exceptions.
- **Measurement:** Histogram of ISI values; max deviation $= 0$ ticks.

**Test RSP-MC-04: Phase Rotation Accuracy**
- **Procedure:** Initialize $\varphi = 0$. Run 25 ticks. Record $\varphi(t)$ at each tick.
- **Pass criterion:** $\varphi(25)$ must equal $0.0 \pmod{2\pi}$ within float32 precision ($\sim 10^{-6}\ \text{rad}$).
- **Measurement:** $|\varphi(25)|$ absolute error.

**Test RSP-MC-05: Recovery Trajectory**
- **Procedure:** Fire master at $t = 0$. Record $V(t)$ for $t = 1$ to $25$ without further pulses.
- **Pass criterion:** $V(25)$ must be within $0.5\ \text{mV}$ of $V_{\text{rest}} = -70.0\ \text{mV}$.
- **Measurement:** $|V(25) - (-70.0)|$.

#### 4.2 Complexity Compliance Tests

**Test RSP-CC-01: Constant Fan-Out**
- **Procedure:** Count outgoing FEEDFORWARD synapses per RSP master for networks at scales 1K, 10K, 100K, 270K neurons.
- **Pass criterion:** Out-degree per master must be $\leq 4,096$ and independent of total network size (constant or slowly growing due to pool allocation, but never $O(N)$).
- **Measurement:** Synapse count per master / total targets.

**Test RSP-CC-02: Target Reception Bound**
- **Procedure:** For any target neuron in any pool, count incoming RSP synapses.
- **Pass criterion:** In-degree from RSP must be $\leq 4$ (redundant masters) for all targets.
- **Measurement:** Max and mean RSP in-degree across 1,000 random targets.

**Test RSP-CC-03: No Global Summation**
- **Procedure:** Inspect the RSP update and delivery algorithms for loops over all neurons or synapses.
- **Pass criterion:** No instruction may iterate over $N_{\text{total}}$ or $S_{\text{total}}$. Only per-neuron or per-synapse constant-time operations permitted.
- **Measurement:** Static code analysis or algorithmic inspection.

#### 4.3 Functional Objective Tests

**Test RSP-FO-01: Zero-Lag Coherence**
- **Procedure:** Deploy 64 RSP masters. Measure spike arrival times at 100 randomly selected targets across NSEL, EBG, and HCM pools.
- **Pass criterion:** All arrival times must agree within $\pm 1\ \text{ms}$ ($\pm 1$ tick) of the global reference $t \equiv 0 \pmod{25}$. Standard deviation of arrival jitter $< 0.5\ \text{ms}$.
- **Measurement:** Time difference between earliest and latest spike arrival in each cycle.

**Test RSP-FO-02: Cycle Boundary Alignment**
- **Procedure:** Run system for 100 cycles. At each cycle start, query $\varphi_{\text{local}}$ in 50 targets from different pools.
- **Pass criterion:** All $\varphi_{\text{local}}$ values must be within $\pm 0.25\ \text{rad}$ ($\pm 1\ \text{ms}$) of the canonical $\varphi_{\text{ref}} = 0$ at cycle boundary, accounting for axonal delay initialization.
- **Measurement:** $\varphi_{\text{local}} - \varphi_{\text{ref}}$ at spike arrival tick.

**Test RSP-FO-03: Subthreshold Broadcast**
- **Procedure:** Deliver RSP spikes to a target CI neuron at $V_{\text{rest}}$ with no other inputs. Record $V$ deviation.
- **Pass criterion:** Peak $V$ deviation must be $< 2.0\ \text{mV}$ (well below the $15\ \text{mV}$ needed to reach threshold from rest). No target may fire solely from RSP input.
- **Measurement:** $\max(V) - V_{\text{rest}}$ after sync spike arrival.

**Test RSP-FO-04: Long-Term Drift Resistance**
- **Procedure:** Run RSP synchronization for 1,000,000 ticks ($\approx 16.7$ minutes). Measure phase offset at target neurons every 1,000 ticks.
- **Pass criterion:** Phase offset must never exceed $\pm 0.5\ \text{rad}$ ($\pm 2\ \text{ms}$). Any float32 drift must be corrected by the periodic spike alignment.
- **Measurement:** Maximum recorded $|\varphi_{\text{local}} - \varphi_{\text{ref}}|$ over full run.

**Test RSP-FO-05: Master Redundancy**
- **Procedure:** Disable 50% of RSP masters (32 of 64). Verify synchronization quality at targets.
- **Pass criterion:** Zero-lag coherence and cycle boundary alignment tests must still pass. System tolerates master failure without timing degradation.
- **Measurement:** Same as FO-01 and FO-02 under degraded master count.

---

**End of Sub-Component 4.2 RSP Contract**

**Awaiting approval to proceed to the next sub-component in dependency order: 4.3 Coincidence Window Regulators (CWR).**
User: continue
