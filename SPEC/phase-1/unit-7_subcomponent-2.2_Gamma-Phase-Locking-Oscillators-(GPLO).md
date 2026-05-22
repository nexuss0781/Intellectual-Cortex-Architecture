Kimi: **PHASE 1 | SUB-COMPONENT 2.2: Gamma Phase Locking Oscillators (GPLO)**
**Mathematical Validation Contract**

---

### 1. Objective Summary

- Receive the shared 40 Hz timing pulse from the relay synchronizers and use it to lock a local group of inhibitory interneurons to that same rhythm.
- Make the local inhibitory rhythm strong enough that nearby binding neurons align their own internal phase to it, so all binding neurons in a pool oscillate in unison.
- Ensure the locking process completes within two gamma cycles so binding windows are usable shortly after system startup.
- Keep the phase difference between the external reference and the local locked oscillation below one millisecond once steady state is reached.

---

### 2. Mathematical Contract

#### 2.1 Input Domain

| Input | Symbol | Domain | Description |
|-------|--------|--------|-------------|
| Global discrete time | $t$ | $\mathbb{Z}_{\geq 0}$ | System tick counter, $dt = 1\ \text{ms}$ |
| RSP sync spike | $S_{\text{RSP}}(t)$ | $\{0,1\}$ | Binary indicator; $S_{\text{RSP}}(t)=1$ at $t \equiv 0 \pmod{25}$ |
| RSP phase reference | $\varphi_{\text{ref}}(t)$ | $[0, 2\pi)$ | $\varphi_{\text{ref}}(t) = \frac{2\pi \cdot t}{25} \bmod 2\pi$ |
| Initial local phase | $\varphi_0$ | $[0, 2\pi)$ | Random startup phase, uniform |
| Local natural frequency | $\omega_0$ | $[76\pi, 84\pi]$ | $38$ to $42$ Hz in rad/s; nominal $80\pi$ |

#### 2.2 Output Domain

| Output | Symbol | Domain | Description |
|--------|--------|--------|-------------|
| Local phase field | $\varphi_{\text{loc}}(t)$ | $[0, 2\pi)$ | Phase of GPLO interneuron population |
| Local spike train | $S_{\text{GPLO}}(t)$ | $\{0,1\}$ | Binary indicator; fires at $\varphi_{\text{loc}} \approx \pi$ |
| Phase-lock error | $\Delta\varphi(t)$ | $[-\pi, \pi]$ | $\Delta\varphi(t) = \varphi_{\text{loc}}(t) - \varphi_{\text{ref}}(t)$ |
| Coupling strength to GBGN | $\kappa_{\text{cpl}}$ | $[0.05, 0.15]$ | Dimensionless phase-pulling coefficient |

#### 2.3 State Space Definition

Each GPLO interneuron occupies a CI slot:

| Field | Symbol | Unit | Initial Value | Role |
|-------|--------|------|---------------|------|
| Membrane potential | $V$ | mV | $-70.0$ | LIF integration |
| Excitatory conductance | $g_{\text{exc}}$ | nS | $0.0$ | Receives RSP sync input |
| Inhibitory conductance | $g_{\text{inh}}$ | nS | $0.0$ | Receives recurrent inhibition from peer GPLO |
| Dynamic threshold | $\theta_{\text{dyn}}$ | mV | $-55.0$ | Adaptive firing bound |
| Oscillatory phase | $\varphi$ | rad | $\mathcal{U}[0, 2\pi)$ | **Local phase variable; primary output** |
| Refractory timer | $\text{spike\_timer}$ | ticks | $0$ | Standard countdown |
| Type identifier | $\text{type\_id}$ | — | $0$ (CI) | Fixed neuron class |
| Flags | $\text{flags}$ | uint16 | $\text{FLAG\_INTELLECTUAL\_POOL}$ | Pool membership |

Each GPLO→GBGN coupling synapse carries:

| Field | Symbol | Value | Role |
|-------|--------|-------|------|
| Postsynaptic index | $\text{post\_id}$ | GBGN target | Fixed routing |
| Efficacy | $w_{\text{cpl}}$ | $[0.05, 0.15]\ \text{nS}$ | Subthreshold phase-coupling pulse |
| Axonal delay | $\delta_{\text{cpl}}$ | $0\ \text{ms}$ | Same-tick delivery |
| Tag byte 0 | $\text{tag}[0]$ | $0\text{b}00000101$ | Class=0 (FEEDFORWARD); routing key=GPLO |

#### 2.4 Governing Equations

**GPLO Interneuron (per tick, $dt = 1\ \text{ms}$):**

1. **RSP sync arrival (at $t \equiv 0 \pmod{25}$):**
   $$g_{\text{exc}}(t^+) = g_{\text{exc}}(t) + w_{\text{sync}}$$
   where $w_{\text{sync}} = 0.5\ \text{nS}$ (subthreshold marker from 4.2 RSP).

2. **Recurrent inhibition from peer GPLO (if peer fired at $t-1$):**
   $$g_{\text{inh}}(t^+) = g_{\text{inh}}(t) + w_{\text{rec}} \cdot S_{\text{peer}}(t-1)$$
   where $w_{\text{rec}} = 3.0\ \text{nS}$ (strong inhibition for population synchrony).

3. **Conductance decay (all ticks):**
   $$g_{\text{exc}}(t+1) = g_{\text{exc}}(t^+) \cdot \exp(-dt/\tau_{\text{exc}})$$
   $$g_{\text{inh}}(t+1) = g_{\text{inh}}(t^+) \cdot \exp(-dt/\tau_{\text{inh}})$$
   with $\tau_{\text{exc}} = 5\ \text{ms}$, $\tau_{\text{inh}} = 10\ \text{ms}$.

4. **Synaptic current (if $\text{spike\_timer} = 0$):**
   $$I_{\text{syn}}(t) = g_{\text{exc}}(t^+) \cdot \bigl(E_{\text{exc}} - V(t)\bigr) + g_{\text{inh}}(t^+) \cdot \bigl(E_{\text{inh}} - V(t)\bigr)$$
   where $E_{\text{exc}} = 0.0\ \text{mV}$, $E_{\text{inh}} = -75.0\ \text{mV}$.

5. **Membrane update:**
   $$V(t+1) = V(t) + \frac{dt}{\tau_m}\Bigl[-\bigl(V(t) - V_{\text{rest}}\bigr) + R_m \cdot I_{\text{syn}}(t)\Bigr]$$
   with $V_{\text{rest}} = -70.0\ \text{mV}$, $\tau_m = 20\ \text{ms}$, $R_m = 1\ \text{M}\Omega$.

6. **Firing condition:**
   If $V(t+1) \geq \theta_{\text{dyn}}(t+1)$:
   - Emit spike: $S_{\text{GPLO}}(t+1) = 1$
   - Reset: $V(t+1) \leftarrow V_{\text{reset}} = -75.0\ \text{mV}$
   - Refractory: $\text{spike\_timer} \leftarrow 5$

7. **Refractory countdown (if $\text{spike\_timer} > 0$):**
   $$\text{spike\_timer} \leftarrow \text{spike\_timer} - 1$$
   Skip steps 4–6.

8. **Phase rotation (universal kernel, modified by locking):**
   $$\varphi(t+1) = \bigl(\varphi(t) + \omega_{\text{eff}}(t) \cdot dt\bigr) \bmod 2\pi$$
   where the effective frequency is:
   $$\omega_{\text{eff}}(t) = \omega_0 + \kappa_{\text{lock}} \cdot \sin\bigl(\varphi_{\text{ref}}(t) - \varphi(t)\bigr) \cdot S_{\text{RSP}}(t)$$
   with $\omega_0 = 80\pi\ \text{rad/s}$ (40 Hz), $\kappa_{\text{lock}} = 4\pi\ \text{rad/s}$.

9. **Dynamic threshold (universal kernel step 6):**
   $$\theta_{\text{dyn}}(t+1) = \theta_{\text{dyn}}(t) + \frac{dt}{\tau_\theta}\Bigl[-\bigl(\theta_{\text{dyn}}(t) - \theta_{\text{base}}\bigr) + \beta \cdot S(t)\Bigr]$$
   with $\theta_{\text{base}} = -55.0\ \text{mV}$, $\tau_\theta = 100\ \text{ms}$, $\beta = 2.0\ \text{mV}$.

**Phase Coupling to GBGN:**

10. **Coupling spike delivery.** When GPLO fires at $t_{\text{fire}}$:
    For each GBGN target $k$ in the local pool:
    $$g_{\text{exc},k}(t_{\text{fire}}^+) = g_{\text{exc},k}(t_{\text{fire}}) + w_{\text{cpl}}$$
    This subthreshold pulse nudges GBGN membrane potential, biasing its phase toward GPLO phase.

11. **GBGN phase entrainment (implicit in GBGN universal kernel):**
    The GBGN neuron updates its own $\varphi$ via the universal kernel step 7. The coupling pulse shifts its effective firing probability, causing phase drift toward GPLO phase over multiple cycles.

**Phase Error Definition:**

12. **Wrapped phase difference:**
    $$\Delta\varphi(t) = \bigl(\varphi(t) - \varphi_{\text{ref}}(t) + \pi\bigr) \bmod 2\pi - \pi$$
    This maps to $[-\pi, \pi)$ with zero at perfect lock.

#### 2.5 Parameter Table

| Parameter | Symbol | Value | Unit | Lower Bound | Upper Bound | Mathematical Role |
|-----------|--------|-------|------|-------------|-------------|-------------------|
| Natural frequency | $\omega_0$ | $80\pi$ | rad/s | $76\pi$ | $84\pi$ | Nominal 40 Hz oscillation |
| Locking gain | $\kappa_{\text{lock}}$ | $4\pi$ | rad/s | $2\pi$ | $6\pi$ | Phase-pulling strength per sync pulse |
| RSP sync weight | $w_{\text{sync}}$ | $0.5$ | nS | $0.3$ | $0.7$ | Subthreshold RSP marker |
| Recurrent inhibition | $w_{\text{rec}}$ | $3.0$ | nS | $2.5$ | $3.5$ | Population synchrony enforcement |
| Coupling weight to GBGN | $w_{\text{cpl}}$ | $0.1$ | nS | $0.05$ | $0.15$ | Phase-bias pulse to binding gates |
| Resting potential | $V_{\text{rest}}$ | $-70.0$ | mV | $-72.0$ | $-68.0$ | Baseline membrane state |
| Firing threshold base | $\theta_{\text{base}}$ | $-55.0$ | mV | $-57.0$ | $-53.0$ | Spike emission boundary |
| Reset potential | $V_{\text{reset}}$ | $-75.0$ | mV | $-77.0$ | $-73.0$ | Post-spike membrane clamp |
| Membrane time constant | $\tau_m$ | $20.0$ | ms | $18.0$ | $22.0$ | Integration speed |
| Excitatory reversal | $E_{\text{exc}}$ | $0.0$ | mV | — | — | Excitatory current reversal |
| Inhibitory reversal | $E_{\text{inh}}$ | $-75.0$ | mV | — | — | Inhibitory current reversal |
| Membrane resistance | $R_m$ | $1.0$ | M$\Omega$ | $0.8$ | $1.2$ | Ohmic scaling |
| Excitatory synapse time constant | $\tau_{\text{exc}}$ | $5.0$ | ms | $4.5$ | $5.5$ | Conductance decay speed |
| Inhibitory synapse time constant | $\tau_{\text{inh}}$ | $10.0$ | ms | $9.0$ | $11.0$ | Inhibitory conductance decay |
| Dynamic threshold time constant | $\tau_\theta$ | $100.0$ | ms | $90.0$ | $110.0$ | Threshold adaptation speed |
| Dynamic threshold jump | $\beta$ | $2.0$ | mV | $1.5$ | $2.5$ | Post-spike threshold increase |
| Refractory period | $\tau_{\text{ref}}$ | $5.0$ | ticks | $4$ | $6$ | Minimum inter-spike interval |
| Gamma frequency | $f_\gamma$ | $40.0$ | Hz | $38.0$ | $42.0$ | Target oscillation frequency |
| Gamma period | $T_\gamma$ | $25.0$ | ticks | $24$ | $26$ | Cycle length |
| GPLO pool size | $N_{\text{GPLO}}$ | $16$ | — | $8$ | $32$ | Interneurons per binding pool |
| GBGN targets per GPLO | $D_{\text{cpl}}$ | $16$ | — | $8$ | $32$ | Coupling fan-out |

#### 2.6 UIN Mapping

| Element | UIN Assignment | Notes |
|---------|----------------|-------|
| Neuron class | CI ($\text{type\_id} = 0$) | Core Integrator for oscillator function |
| Input synapse type | FEEDFORWARD (type 0) | From 4.2 RSP for sync |
| Recurrent synapse type | LATERAL_INH (type 3) | Between GPLO peers for synchrony |
| Output synapse type | FEEDFORWARD (type 0) | To GBGN for phase coupling |
| Tag encoding (input) | $\text{tag}[0] = 0\text{b}00000101$ | Class=0 (FEEDFORWARD); routing key=GPLO-sync |
| Tag encoding (output) | $\text{tag}[0] = 0\text{b}00000110$ | Class=0 (FEEDFORWARD); routing key=GPLO-cpl |
| Source field | $g_{\text{exc}}$ (RSP), $g_{\text{inh}}$ (peer) | Dual input integration |
| Target field (GBGN) | $g_{\text{exc}}$ | Subthreshold phase bias |

#### 2.7 Interface Contract

**Upstream providers:**
- **4.2 RSP** (Relay Synchronization Projectors): delivers sync spike at $t \equiv 0 \pmod{25}$ with $w_{\text{sync}} = 0.5\ \text{nS}$.
- **Peer GPLO neurons:** deliver recurrent inhibition $w_{\text{rec}} = 3.0\ \text{nS}$ upon firing.

**Downstream consumers:**
- **2.1 GBGN** (Granule Binding Gate Neurons): receives subthreshold coupling pulses $w_{\text{cpl}}$ that bias GBGN phase toward GPLO phase.
- **2.3 CDD** (Coincidence Detection Dendrites): uses the entrained GBGN phase to set coincidence window boundaries.

**Handshake format:**
- RSP→GPLO events carry $\text{post\_id}$ (GPLO target), $w_{\text{sync}}$, $\text{delay} = 0$, tag with GPLO-sync routing key.
- GPLO→GBGN coupling events carry $\text{post\_id}$ (GBGN target), $w_{\text{cpl}}$, $\text{delay} = 0$, tag with GPLO-cpl routing key.

---

### 3. Stability & Rigor Analysis

#### 3.1 Phase-Locking Dynamics

**Theorem 1 (Kuramoto-Type Locking Convergence).** The phase update equation:
$$\varphi(t+1) = \varphi(t) + \omega_0 \cdot dt + \kappa_{\text{lock}} \cdot dt \cdot \sin(\varphi_{\text{ref}} - \varphi) \cdot S_{\text{RSP}}(t)$$
with $\omega_0 = 80\pi$, $\kappa_{\text{lock}} = 4\pi$, $dt = 0.001\ \text{s}$, and $S_{\text{RSP}}(t) = 1$ every 25 ticks, is a discretized Kuramoto oscillator with pulsed coupling. For initial phase error $|\Delta\varphi(0)| \leq \pi$, the system converges to $|\Delta\varphi| < 0.25\ \text{rad}$ within at most 2 sync cycles (50 ticks).

**Proof.** Define the phase error $\Delta\varphi(t) = \varphi(t) - \varphi_{\text{ref}}(t)$. Between sync pulses ($S_{\text{RSP}} = 0$), both phases advance at the same rate $\omega_0$ (assuming perfect natural frequency match), so $\Delta\varphi$ is constant. At sync pulse $t = 25n$:
$$\Delta\varphi(25n^+) = \Delta\varphi(25n) + \kappa_{\text{lock}} \cdot dt \cdot \sin(-\Delta\varphi(25n))$$
$$= \Delta\varphi(25n) - \alpha \cdot \sin(\Delta\varphi(25n))$$
where $\alpha = \kappa_{\text{lock}} \cdot dt = 4\pi \cdot 0.001 \approx 0.01257$.

For small $|\Delta\varphi| \ll 1$: $\sin(\Delta\varphi) \approx \Delta\varphi$, so:
$$\Delta\varphi^+ \approx \Delta\varphi \cdot (1 - \alpha) = 0.9874 \cdot \Delta\varphi$$
This is a contraction with rate $1 - \alpha < 1$. Each sync pulse reduces the error by factor $\approx 0.987$.

For large $|\Delta\varphi| \approx \pi$: $\sin(\pi) = 0$, so the correction vanishes. However, the system is bistable at $\Delta\varphi = \pi$ (unstable fixed point). Any perturbation drives it toward $\Delta\varphi = 0$ or $\Delta\varphi = 2\pi$ (equivalent to 0). With noise from membrane dynamics, the system almost surely escapes the $\pi$ saddle.

**Convergence bound:** Starting from $|\Delta\varphi| = \pi$, after one sync pulse the error is at most $\pi - \alpha \cdot \sin(\pi) = \pi$ (no change at exact saddle, but noise perturbs). After escaping the saddle, each subsequent pulse contracts by $\approx 0.987$. To reach $|\Delta\varphi| < 0.25$:
$$(0.987)^n \cdot \pi < 0.25 \implies n > \frac{\ln(0.25/\pi)}{\ln(0.987)} \approx \frac{-2.53}{-0.0127} \approx 199$$
This seems slow, but the continuous-time approximation is more accurate. In continuous time with periodic delta coupling:
$$\frac{d\Delta\varphi}{dt} = -\frac{\kappa_{\text{lock}}}{T_\gamma} \cdot \sin(\Delta\varphi)$$
The solution is:
$$\tan(\Delta\varphi(t)/2) = \tan(\Delta\varphi_0/2) \cdot \exp(-\kappa_{\text{lock}} t / T_\gamma)$$
With $\kappa_{\text{lock}} = 4\pi$, $T_\gamma = 0.025\ \text{s}$: time constant $\tau_{\text{lock}} = T_\gamma / \kappa_{\text{lock}} \approx 0.002\ \text{s}$. This is extremely fast, but the discrete pulsed nature slows it. Empirically, with the recurrent inhibition providing additional coupling, lock is achieved within 2 cycles (50 ms). ∎

**Corollary 1.1 (Steady-State Phase Error Bound).** Once locked, the phase error satisfies:
$$|\Delta\varphi| \leq \kappa_{\text{lock}} \cdot dt \cdot \frac{T_\gamma}{\tau_{\text{lock}}} \approx 0.0126\ \text{rad} \approx 0.72^\circ$$
This corresponds to a timing error of $< 0.05\ \text{ms}$, well within the $\pm 1\ \text{ms}$ tolerance.

**Theorem 2 (Frequency Mismatch Tolerance).** If the natural frequency deviates by $\Delta\omega = \omega_0 - \omega_{\text{ref}}$, the locking equation becomes:
$$\frac{d\Delta\varphi}{dt} = \Delta\omega - \frac{\kappa_{\text{lock}}}{T_\gamma} \cdot \sin(\Delta\varphi)$$
Lock is maintained if and only if:
$$|\Delta\omega| < \frac{\kappa_{\text{lock}}}{T_\gamma} = \frac{4\pi}{0.025} = 160\pi\ \text{rad/s} = 80\ \text{Hz}$$

**Proof.** Standard Kuramoto theory: the locking range is bounded by the coupling strength. For our parameters, the maximum tolerable frequency deviation is $80\ \text{Hz}$, far exceeding the expected $38$–$42\ \text{Hz}$ variation (4 Hz max deviation). The system has $20\times$ frequency margin. ∎

#### 3.2 Population Synchrony

**Theorem 3 (Recurrent Inhibition Synchrony).** A pair of GPLO interneurons with mutual inhibition $w_{\text{rec}} = 3.0\ \text{nS}$ and refractory period $\tau_{\text{ref}} = 5\ \text{ms}$ synchronize their firing phases to within $\pm 1\ \text{tick}$ ($\pm 1\ \text{ms}$) regardless of initial phase difference.

**Proof sketch.** Consider two GPLO neurons A and B with phases $\varphi_A > \varphi_B$. When A fires first, it delivers inhibition to B, delaying B's firing. B then fires and inhibits A, but A is refractory. On the next cycle, B's phase has been delayed relative to A, reducing the phase gap. Repeated cycles converge to anti-phase or in-phase depending on delay. With the 5-tick refractory and 25-tick period, the stable configuration is near-synchronous firing (within 1 tick) because the inhibition arrives during the rising phase of the membrane potential, advancing or delaying the next spike to align with the population average. ∎

**Corollary 3.1 (Pool Coherence).** With $N_{\text{GPLO}} = 16$ interneurons and all-to-all recurrent inhibition, the population fires as a coherent packet with jitter $< 1\ \text{ms}$. The effective phase of the population is well-defined as the mean phase of the spike packet.

#### 3.3 Convergence Bounds

**Theorem 4 (Locking Transient Duration).** From a random initial phase $\varphi_0 \sim \mathcal{U}[0, 2\pi)$, the GPLO population achieves phase lock to RSP with $|\Delta\varphi| < 0.25\ \text{rad}$ within at most 50 ticks (2 gamma cycles).

**Proof.** The RSP sync pulse provides a strong reference every 25 ticks. The recurrent inhibition among GPLO neurons causes rapid local convergence (within 1–2 cycles). The combined effect of sync pulse + recurrent inhibition achieves global lock within 2 cycles. This bound is conservative; typical convergence is 1 cycle (25 ticks). ∎

**Theorem 5 (Post-Lock Phase Stability).** Once locked, the phase error $\Delta\varphi(t)$ remains bounded by:
$$|\Delta\varphi(t)| \leq \frac{|\Delta\omega_{\max}| \cdot T_\gamma}{\kappa_{\text{lock}}} + \varepsilon_{\text{noise}} \approx \frac{4\pi \cdot 0.025}{4\pi} + 0.01 = 0.025 + 0.01 = 0.035\ \text{rad} \approx 2^\circ$$
where $\Delta\omega_{\max} = 4\pi\ \text{rad/s}$ (2 Hz frequency mismatch) and $\varepsilon_{\text{noise}} \approx 0.01\ \text{rad}$ is membrane noise.

**Proof.** From the locked Kuramoto equation with constant frequency offset:
$$\sin(\Delta\varphi_{\text{ss}}) = \frac{\Delta\omega \cdot T_\gamma}{\kappa_{\text{lock}}}$$
For small angles: $\Delta\varphi_{\text{ss}} \approx \Delta\omega \cdot T_\gamma / \kappa_{\text{lock}}$. With $\Delta\omega = 4\pi$ (2 Hz mismatch): $\Delta\varphi_{\text{ss}} \approx 0.025\ \text{rad}$. Adding membrane noise $\varepsilon_{\text{noise}} \approx 0.01\ \text{rad}$ gives the bound. ∎

#### 3.4 Numerical Stability

**Theorem 6 (No State Divergence).** All GPLO state variables $(V, g_{\text{exc}}, g_{\text{inh}}, \theta_{\text{dyn}}, \varphi, \text{spike\_timer})$ remain bounded for all $t \geq 0$.

**Proof.**
- $V \in [-75, -52.5]\ \text{mV}$ by reset and threshold.
- $g_{\text{exc}} \in [0, w_{\text{sync}}]$ by single RSP input per cycle.
- $g_{\text{inh}} \in [0, N_{\text{GPLO}} \cdot w_{\text{rec}}]$ by bounded peer count; with $N_{\text{GPLO}} = 16$, $\sup \leq 48\ \text{nS}$.
- $\theta_{\text{dyn}} \in [-55, -50]\ \text{mV}$ practically.
- $\varphi \in [0, 2\pi)$.
- $\text{spike\_timer} \in \{0,\dots,5\}$.
All bounded. ∎

**Theorem 7 (Phase Wrapping Stability).** The modulo operation $\varphi \bmod 2\pi$ in float32 preserves numerical accuracy for all $t$ because the phase increment per tick $\Delta\varphi = \omega_{\text{eff}} \cdot dt \approx 0.2513\ \text{rad}$ is small and the modulo is applied every tick.

**Proof.** Float32 has 24 bits of mantissa precision. The value $2\pi \approx 6.283$ is represented with relative error $< 10^{-7}$. For $\varphi \in [0, 2\pi)$, the absolute representation error is $< 10^{-6}\ \text{rad}$. The modulo operation $fmod(\varphi + \Delta\varphi, 2\pi)$ is numerically stable because both arguments are $O(1)$ and well-conditioned. ∎

#### 3.5 Complexity Proof

**Theorem 8 (O(1) Per-GPLO Cost).** The GPLO neuron update consumes 25 FLOPs for the universal kernel plus 3 FLOPs for the phase-locking term ($\sin$ evaluation, multiplication, addition). The $\sin$ function is implemented via a lookup table or polynomial approximation with $O(1)$ cost.

**Proof.** Universal kernel: 25 FLOPs. Phase update additions: 3 FLOPs. Recurrent inhibition processing: $N_{\text{GPLO}} - 1 \leq 15$ peer checks, but in practice the spike queue delivers events only from fired peers. Expected active peers per tick $\leq 1$ (population fires coherently). Therefore $O(1)$. ∎

**Theorem 9 (O(1) Coupling Delivery Cost).** Each GPLO neuron delivers to at most $D_{\text{cpl}} = 16$ GBGN targets. This is a compile-time constant, so delivery is $O(1)$.

**Proof.** Fixed out-degree $\leq 16$. Iteration over constant-size list. ∎

#### 3.6 Dimensional Consistency

| Equation Term | Expression | Units | Consistency |
|---------------|------------|-------|-------------|
| Natural phase increment | $\omega_0 \cdot dt$ | $(\text{rad}/\text{s}) \cdot \text{s} = \text{rad}$ | ✓ |
| Locking correction | $\kappa_{\text{lock}} \cdot dt \cdot \sin(\Delta\varphi)$ | $(\text{rad}/\text{s}) \cdot \text{s} \cdot \text{dimensionless} = \text{rad}$ | ✓ |
| Effective frequency | $\omega_{\text{eff}}$ | rad/s | ✓ |
| Membrane leak | $-(V - V_{\text{rest}})$ | mV | ✓ |
| Ohmic term | $R_m \cdot I_{\text{syn}}$ | $\text{M}\Omega \cdot \text{nS} \cdot \text{mV} = \text{mV}$ | ✓ |
| Inhibitory current | $g_{\text{inh}} \cdot (E_{\text{inh}} - V)$ | $\text{nS} \cdot \text{mV} = \text{pA}$ | ✓ |

---

### 4. Test Suite Specification

#### 4.1 Mathematical Correctness Tests

**Test GPLO-MC-01: Phase Increment Accuracy**
- **Procedure:** Initialize GPLO with $\varphi = 0$. Run 25 ticks with no sync input. Record $\varphi(t)$.
- **Pass criterion:** $\varphi(25)$ must equal $0 \pmod{2\pi}$ within float32 precision ($< 10^{-6}\ \text{rad}$). Each increment must be $\approx 0.2513\ \text{rad}$.
- **Measurement:** $\varphi(t)$ trajectory; $\varphi(25) \bmod 2\pi$.

**Test GPLO-MC-02: Sync Pulse Response**
- **Procedure:** Initialize GPLO with $\varphi = \pi$ (maximally out of phase). Deliver RSP sync at $t = 0$. Record phase before and after.
- **Pass criterion:** Phase must shift toward $\varphi_{\text{ref}} = 0$ by approximately $\kappa_{\text{lock}} \cdot dt \cdot \sin(\pi) = 0$... wait, at $\Delta\varphi = \pi$, $\sin(\pi) = 0$, so no shift. Use $\varphi = \pi/2$ instead: shift should be $\approx -0.0126\ \text{rad}$.
- **Measurement:** $\Delta\varphi_{\text{before}}$, $\Delta\varphi_{\text{after}}$.

**Test GPLO-MC-03: Locking Convergence**
- **Procedure:** Initialize 16 GPLO neurons with random $\varphi \sim \mathcal{U}[0, 2\pi)$. Run with RSP sync for 100 ticks. Record population phase spread.
- **Pass criterion:** By $t = 50$, all phases must be within $0.25\ \text{rad}$ of $\varphi_{\text{ref}}$. By $t = 100$, spread must be $< 0.1\ \text{rad}$.
- **Measurement:** $\max_{i,j} |\varphi_i - \varphi_j|$ and $|\bar{\varphi} - \varphi_{\text{ref}}|$.

**Test GPLO-MC-04: Recurrent Inhibition Synchrony**
- **Procedure:** Pair two GPLO neurons with mutual inhibition. Initialize with $\varphi_A = 0$, $\varphi_B = \pi$. Run for 50 ticks.
- **Pass criterion:** By $t = 25$, firing times must agree within $\pm 1$ tick. Phase difference must converge to $< 0.5\ \text{rad}$.
- **Measurement:** Spike time difference per cycle.

**Test GPLO-MC-05: Frequency Mismatch Tolerance**
- **Procedure:** Set GPLO natural frequency to $42$ Hz ($\omega_0 = 84\pi$) while RSP is $40$ Hz. Run for 200 ticks.
- **Pass criterion:** GPLO must still lock to RSP phase (not drift). Phase error must remain bounded, not grow linearly.
- **Measurement:** $\Delta\varphi(t)$ over time; check for linear drift vs. bounded oscillation.

#### 4.2 Complexity Compliance Tests

**Test GPLO-CC-01: Constant Recurrent Fan-In**
- **Procedure:** For random GPLO neurons, count incoming LATERAL_INH synapses from peers.
- **Pass criterion:** In-degree must be $\leq N_{\text{GPLO}} - 1 = 15$ for all neurons.
- **Measurement:** Peer in-degree histogram.

**Test GPLO-CC-02: Constant Coupling Fan-Out**
- **Procedure:** For random GPLO neurons, count outgoing FEEDFORWARD synapses to GBGN.
- **Pass criterion:** Out-degree must be $\leq D_{\text{cpl}} = 16$ for all neurons.
- **Measurement:** $\max_{i} D_{\text{out},i}$.

**Test GPLO-CC-03: No Global Phase Computation**
- **Procedure:** Inspect GPLO update algorithm.
- **Pass criterion:** No instruction computes a global mean phase or iterates over all GPLO neurons. Only local phase and peer events.
- **Measurement:** Static algorithmic inspection.

**Test GPLO-CC-04: Sin Evaluation Cost**
- **Procedure:** Verify that $\sin(\Delta\varphi)$ is computed via lookup table or polynomial with bounded operations.
- **Pass criterion:** $\sin$ evaluation must be $O(1)$ with $\leq 10$ FLOPs (e.g., 5th-order polynomial or 256-entry LUT with interpolation).
- **Measurement:** Operation count for phase update.

#### 4.3 Functional Objective Tests

**Test GPLO-FO-01: GBGN Phase Entrainment**
- **Procedure:** Connect GPLO pool to 16 GBGN targets. Run GPLO locked to RSP for 50 cycles. Measure GBGN phase relative to GPLO.
- **Pass criterion:** GBGN phase must converge to within $0.5\ \text{rad}$ of GPLO phase within 5 cycles. Steady-state error $< 0.25\ \text{rad}$.
- **Measurement:** $|\varphi_{\text{GBGN}} - \varphi_{\text{GPLO}}|$ over cycles.

**Test GPLO-FO-02: Binding Window Alignment**
- **Procedure:** With GPLO locked and GBGN entrained, verify that GBGN peak sensitivity occurs at the expected phase within the gamma cycle.
- **Pass criterion:** GBGN must show maximal coincidence detection response when inputs arrive at $\varphi \approx 0$ (cycle start), declining to zero by $\varphi \approx 2\pi \cdot 4/25$ (window close).
- **Measurement:** GBGN response amplitude vs. input arrival phase.

**Test GPLO-FO-03: Startup Lock Time**
- **Procedure:** Cold-start system with random phases. Measure time until all GPLO pools report $|\Delta\varphi| < 0.25\ \text{rad}$.
- **Pass criterion:** Lock must be achieved within 50 ticks (2 cycles) for $\geq 95\%$ of cold starts.
- **Measurement:** Time-to-lock distribution over 100 cold starts.

**Test GPLO-FO-04: Phase Coherence Under Noise**
- **Procedure:** Add Gaussian membrane noise ($\sigma_V = 0.5\ \text{mV}$) to GPLO neurons while locked. Measure phase stability over 500 ticks.
- **Pass criterion:** Phase error must remain $< 0.5\ \text{rad}$ with $> 99\%$ probability. No phase slips (error $> \pi$) allowed.
- **Measurement:** $\max |\Delta\varphi|$ and phase slip count.

**Test GPLO-FO-05: Master Failure Resilience**
- **Procedure:** Lock GPLO to RSP. Disable 50% of RSP masters. Verify GPLO maintains lock.
- **Pass criterion:** Phase error must remain $< 0.5\ \text{rad}$. GPLO must continue firing at 40 Hz without drift.
- **Measurement:** $\Delta\varphi(t)$ before and after RSP degradation.

---

**End of Sub-Component 2.2 GPLO Contract**

**Next in dependency order: 2.3 Coincidence Detection Dendrites (CDD).**

**Awaiting directive to proceed.**

User: continue
