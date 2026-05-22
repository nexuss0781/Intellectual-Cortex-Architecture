# Project Genesis: Phase I - The Bio-Digital Substrate

**Version:** 1.0.0 (Implementation of v2.0.2 Spec)  
**Status:** Completed & Validated  
**Engine:** C++17 Custom Physics Engine (No ML Frameworks)

---

## 1. Overview
Phase I establishes the **Bio-Digital Substrate**, the physical layer of the Artificial General Intelligence. Unlike traditional AI which relies on global loss functions and backpropagation, this engine simulates the local laws of physics and biology. It provides a deterministic, energetically constrained environment where intelligence can grow.

**Core Philosophy:**
*   **0% Machine Learning:** No tensors, no autograd, no PyTorch/TensorFlow.
*   **100% Biology:** Dynamics are driven by differential equations modeling membrane potential, metabolic energy (ATP), and synaptic plasticity.
*   **Plasticity First:** The network is not "trained"; it rewires itself in real-time based on causal input patterns (STDP) and global reward signals (Dopamine).

---

## 2. The Biological Physics Spec

The engine operates on a discrete time step of **1.0 ms**. All constants are biologically tuned to this tick.

### 2.1 Neuron Model: Leaky Integrate-and-Fire (LIF)
Neurons are state machines that integrate voltage inputs and fire only when specific conditions are met.

*   **Resting Potential:** -70 mV
*   **Threshold:** -55 mV
*   **Refractory Period:** 5 ms (Absolute dead time after firing)
*   **Voltage Decay:** $\tau = 20ms$ (Decay Factor: 0.9512)

**Update Logic:**
$$V(t+1) = V_{rest} + (V(t) - V_{rest}) \cdot e^{-dt/\tau} + \sum I_{syn}$$

### 2.2 Metabolic Constraints (Energy Economy)
The system enforces a strict energy budget to prevent runaway excitation (epilepsy) and force efficient sparse coding.

*   **ATP Cap:** 1.0 (100%)
*   **Spike Cost:** 0.10 (10% energy per spike)
*   **Refill Rate:** 0.01 per tick
*   **Gating:** Neurons cannot fire if $ATP < 0.10$.

*Behavior:* At maximum continuous firing, a neuron depletes its energy reserves in ~50ms and is forced into silence (fatigue) until it rests.

### 2.3 Synaptic Plasticity: Three-Factor STDP
Learning follows the Hebbian principle ("Fire together, wire together"), gated by a global reward signal.

**1. Eligibility Traces (The "Tag"):**
When neurons fire, they leave a chemical trace at the synapse marking it for potential change.
*   **Pre-before-Post (Causal):** Trace increases ($+0.10$).
*   **Post-before-Pre (Anti-Causal):** Trace decreases ($-0.12$).
*   **Trace Decay:** $\tau = 100ms$.

**2. Dopamine Gating (The "Print"):**
Weights only change when the Eligibility Trace coincides with Dopamine.
$$\Delta W = \text{Dopamine}(t) \cdot \text{Eligibility}(t)$$

---

## 3. Technical Architecture

The engine is built for maximum cache efficiency and deterministic execution.

### 3.1 Data-Oriented Design
We utilize a **Struct-of-Arrays (SoA)** layout instead of Array-of-Structs (AoS). This ensures that when the physics loop processes voltages, it loads contiguous memory into the CPU cache, enabling SIMD auto-vectorization.

**Memory Layout (Aligned 64-byte):**