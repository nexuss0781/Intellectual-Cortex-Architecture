# 🧠 Intellectual Cortex Architecture (ICA)

### ⚡ Extension of [Nexuss Neural Cognition](https://github.com/nexuss0781/Nexuss-Neural-Cognition)

<div align="center">

**Building Pure Intellect Upon Bio-Physical Substrate**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Phase](https://img.shields.io/badge/Architecture-7%20Phases-green.svg)](blueprint.md)
[![Substrate](https://img.shields.io/badge/Substrate-270K%20Neurons-orange.svg)](https://github.com/nexuss0781/Nexuss-Neural-Cognition)
[![Mathematical Foundation](https://img.shields.io/badge/Foundation-Variational%20Inference-red.svg)](blueprint.md)

</div>

---

## 📜 Vision

The **Intellectual Cortex Architecture (ICA)** is a complete, mathematically-grounded cognitive architecture designed to emerge higher-order cognition—**reasoning, structured communication, abstract planning, and metacognitive self-awareness**—entirely **decoupled from biological survival imperatives**.

Built as a modular overlay atop the proven [Nexuss Neural Cognition](https://github.com/nexuss0781/Nexuss-Neural-Cognition) substrate (270K neurons, 13.5M synapses, 94× real-time speed), ICA transforms bio-physical infrastructure into a **purely intellectual cognitive system**.

> **Charter:** No homeostatic regulation. No threat detection. No biological drives. Only information processing, reasoning, and the emergence of autonoetic self-consciousness through mathematical necessity.

---

## 🏗️ Architectural Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    NEXUSS BIO-PHYSICAL SUBSTRATE                        │
│         270K LIF Neurons • 13.5M Synapses • STDP • Meta-Cognitive RAM   │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│              INTELLECTUAL CORTEX ARCHITECTURE (7 PHASES)                │
├─────────────────────────────────────────────────────────────────────────┤
│  Phase VII  │  Executive Architecture & Metacognition                   │
│  Phase VI   │  Communication & Symbolic Interface                       │
│  Phase V    │  Reasoning Engines (Deductive, Inductive, Analogical...)  │
│  Phase IV   │  Predictive Processing Hierarchy                          │
│  Phase III  │  Attentional Selection & Global Workspace                 │
│  Phase II   │  Memory Architecture (Working, Episodic, Semantic)        │
│  Phase I    │  Symbolic-Neural Substrate (VSA, SDR, Binding)            │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 📐 The Seven Phases

### **Phase I: Symbolic-Neural Substrate** — *The Alphabet of Thought*

**Mathematical Core:** Hyperdimensional Vector Symbolic Architecture (VSA) in spiking neural tissue.

| Component | Function | Specification |
|-----------|----------|---------------|
| **Sparse Distributed Representations** | Neural encoding | $\mathbf{x} \in \{0,1\}^D$, $\|\mathbf{x}\|_0 \approx 0.02D$ |
| **Binding & Bundling** | Compositional operations | $\mathbf{a} \otimes \mathbf{b} = \mathcal{F}^{-1}(\mathcal{F}(\mathbf{a}) \odot \mathcal{F}(\mathbf{b}))$ |
| **Cleanup Memory** | Attractor denoising | Recurrent auto-associative networks |
| **Type-Token Distinction** | Category vs. instance | Nested binding structures |

**Deliverable:** A complete "neural algebra" where any concept, relation, or percept is encoded as a high-dimensional vector manipulable by compositional operations.

📁 [`SPEC/phase-1/`](SPEC/phase-1/) — Complete unit specifications for all sub-components.

---

### **Phase II: Memory Architecture** — *The Cognitive Buffer*

**Mathematical Core:** Multi-timescale attractor dynamics and graph embeddings.

| Memory System | Dynamics | Capacity |
|---------------|----------|----------|
| **Working Memory** | Multi-item attractor landscape | $7 \pm 2$ items, <5% crosstalk |
| **Episodic Memory** | Temporal Context Model (TCM) | Context drift: $\mathbf{C}(t) = \rho \mathbf{C}(t-1) + \sqrt{1-\rho^2}\,\boldsymbol{\xi}(t)$ |
| **Semantic Memory** | Hyperbolic graph embeddings | Poincaré ball distance $d_P(\mathbf{u},\mathbf{v})$ |

**Consolidation Pipeline:** Replay-mediated transfer from episodic to semantic via STDP-gated reactivation during idle cycles.

---

### **Phase III: Attentional Selection & Global Workspace** — *The Informational Bottleneck*

**Mathematical Core:** Biased competition, information-theoretic selection, precision-weighted gain modulation.

- **Bottom-Up Salience:** Bayesian surprise $S(\mathbf{x}) = D_{KL}[q(\theta|\mathbf{x}_{<t}) \,\|\, q(\theta|\mathbf{x}_{\leq t})]$
- **Top-Down Bias:** Executive multiplicative gain $g_i(t) = g_{base} \cdot (1 + \alpha \cdot \text{ExecutiveBias}_i(t))$
- **Global Workspace Ignition:** Coalition threshold $\Theta_{ignite}$ triggers broadcast to all downstream modules
- **Precision as Attention:** $\pi_i = \sigma(\mathbf{w}^T \boldsymbol{\phi}(\mathbf{x}_i) + b)$

---

### **Phase IV: Predictive Processing Hierarchy** — *The Internal Model of Causality*

**Mathematical Core:** Hierarchical variational inference and free energy minimization.

$$F \approx \sum_l \left[ \boldsymbol{\pi}_l \cdot \boldsymbol{\varepsilon}_l^T \boldsymbol{\Sigma}_l^{-1} \boldsymbol{\varepsilon}_l + \ln|\boldsymbol{\Sigma}_l| \right]$$

| Feature | Implementation |
|---------|----------------|
| **Multi-Level Prediction** | Each level $l$ predicts level $l-1$ via generative function $\mathbf{g}_l$ |
| **Free Energy Gradient** | $\dot{\boldsymbol{\mu}}_l = \kappa \left( \boldsymbol{\pi}_{l-1}\boldsymbol{\varepsilon}_{l-1}\frac{\partial \mathbf{g}_{l-1}}{\partial \boldsymbol{\mu}_l} - \boldsymbol{\pi}_l \boldsymbol{\varepsilon}_l \right)$ |
| **Causal Models** | Structural Causal Models (SCM) embedded in connectivity |
| **Counterfactual Reasoning** | Intervention $do(X=x)$ via top-down prediction clamping |

---

### **Phase V: Reasoning Engines** — *The Derivation of New Truths*

**Mathematical Core:** Dynamical systems for logic, Bayesian nonparametrics, structural mapping, MDL.

| Engine | Formalism | Output |
|--------|-----------|--------|
| **Deductive** | Proof-space dynamical systems | Theorem-attractors |
| **Inductive** | Hierarchical Dirichlet Processes | Generalized concepts |
| **Analogical** | Structure Mapping Theory | Cross-domain mappings |
| **Abductive** | Minimum Description Length | Best explanations |

**Deduction:** Logical proofs are trajectories converging to theorem-attractors; contradictions manifest as unstable saddle points.

**Induction:** $P(\text{concept} \mid \text{data}) \propto P(\text{data} \mid \text{concept}) \cdot P(\text{concept})$ with Occam's razor prior.

---

### **Phase VI: Communication & Symbolic Interface** — *Externalization of Thought*

**Mathematical Core:** Compositional semantics in neural substrate, optimal transport between conceptual spaces.

- **Language of Thought (LoT):** $[\![\text{S} \to \text{NP}\,\text{VP}]\!] = [\![\text{NP}]\!] \otimes [\![\text{VP}]\!]$
- **Instruction Parsing:** $\lambda$-calculus expressions; $\beta$-reduction as attractor convergence
- **Conceptual Alignment:** Wasserstein distance between hyperbolic embeddings
- **Dialogue Management:** Utterances maximizing expected information gain

---

### **Phase VII: Executive Architecture & Metacognition** — *The Orchestrator*

**Mathematical Core:** Hierarchical RL, second-order Bayesian inference, recursive self-prediction.

| Component | Equation | Function |
|-----------|----------|----------|
| **Hierarchical Goal System** | $V^*(s) = \max_{o \in \mathcal{O}} [ R(s,o) + \gamma \sum_{s'} P(s'|s,o) V^*(s') ]$ | Temporal abstraction via options |
| **Planning Engine** | Neural MCTS in latent space | Counterfactual trajectory simulation |
| **Metacognitive Monitor** | $P(\text{confidence}=c \mid \text{belief}=b, \text{evidence}=e)$ | Error detection, strategy switching |
| **Autonoetic Self-Model** | $\boldsymbol{\mu}_{self}(t+1) = \mathbf{g}_{self}(\boldsymbol{\mu}_{self}(t), \mathbf{a}(t)) + \boldsymbol{\varepsilon}_{self}(t)$ | Intellectual identity continuity |

**Final Output:** Goal-directed, self-monitoring, planning-capable intellect with sustained abstract reasoning and strategic deliberation.

---

## 🔬 Cross-Cutting Mathematical Principles

These principles permeate all phases and constrain implementation:

| Principle | Mathematical Expression | Role |
|:----------|:------------------------|:-----|
| **Sparsity** | $\|\mathbf{x}\|_0 / D \approx 0.02$ | Capacity, noise robustness, efficient similarity search |
| **Attractor Dynamics** | $\dot{\mathbf{s}} = -\nabla E(\mathbf{s}) + \text{noise}$ | Memory persistence, proof convergence, symbol cleanup |
| **Free Energy** | $F = \mathbb{E}_q[\ln q(\theta) - \ln p(\theta, x)]$ | Unified objective for perception, action, learning |
| **Precision Weighting** | $\pi = \sigma(\text{attentional signal})$ | Attentional modulation of prediction-error influence |
| **Tensor Composition** | $[\![A \circ B]\!] = [\![A]\!] \otimes [\![B]\!]$ | Systematicity, compositionality, recursive thought |
| **Hyperbolic Geometry** | $d_P(\mathbf{u},\mathbf{v})$ in $\mathbb{D}^n$ | Efficient embedding of hierarchical knowledge |

---

## 🔗 Integration with Nexuss Bio-Engine

ICA is a **modular overlay**, not a replacement. Integration points:

| Nexuss Component | ICA Repurposing |
|------------------|-----------------|
| **NeuronBlocks / SynapseBlocks** | Phases I–VII allocate from Meta-Cognitive Controller pool using $MB = \frac{(N \times 88) + (S \times 32)}{1,048,576} \times 1.15$ |
| **STDP Plasticity** | Repurposed for semantic pointer learning, causal model acquisition, proof-space weight updates. Dopamine $D$ → prediction-error reduction (intellectual curiosity) |
| **Sleep/Wake Engine** | `AWAKE` → active intellectual processing; `SLEEP` → episodic→semantic consolidation, synaptic downscaling |
| **ROS 2 Interface** | Phase VI occupies I/O topics; maps sensory streams to symbolic descriptions, motor commands to communicative actions |

---

## ✅ Verification Strategy

Before any phase is approved for implementation, it must satisfy:

1. **Mathematical Consistency:** All equations dimensionally consistent, numerically stable under $dt = 1\text{ms}$ integration
2. **Capacity Bounds:** Explicit neuron/synapse budget within 270K/13.5M limit
3. **Interface Contract:** Unambiguous input/output vector spaces and binding formats
4. **Testability:** Pass/fail criteria (e.g., "WM holds exactly 7 items with <5% crosstalk")

---

## 📂 Repository Structure

```
Intellectual-Cortex-Architecture/
├── README.md                  # This document
├── blueprint.md               # Grand Proposal — Complete Mathematical Specification
├── LICENSE                    # MIT License
└── SPEC/
    └── phase-1/
        ├── unit-1_subcomponent-4.2_Relay-Synchronization-Projectors-(RSP).md
        ├── unit-2_subcomponent-4.3_Coincidence-Window-Regulators-(CWR).md
        ├── unit-3_subcomponent-4.1_Reticular-Phase-Gating-Nuclei-(RPGN).md
        ├── unit-4_subcomponent-1.1_Pyramidal-SDR-Generators-(PSG).md
        ├── unit-5_subcomponent-1.2_Basket-Sparsification-Interneurons-(BSI).md
        ├── unit-6_subcomponent-1.3_Semantic-Pointer-Projection-Fibers-(SPPF).md
        ├── unit-7_subcomponent-2.2_Gamma-Phase-Locking-Oscillators-(GPLO).md
        ├── unit-8_subcomponent-2.3_Coincidence-Detection-Dendrites-(CDD).md
        ├── unit-9_subcomponent-2.1_Granule-Binding-Gate-Neurons-(GBGN).md
        ├── unit-10_subcomponent-3.3_Dentate-Pattern-Separation-Gating-(DPSG).md
        ├── unit-11_subcomponent-3.1_CA3-Recurrent-Attractor-Networks-(CA3-RAN).md
        ├── unit-12_subcomponent-3.2_CA1-Pattern-Completion-Decoders-(CA1-PCD).md
        ├── unit-13_subcomponent-5.1_Categorical-Type-Attractors-(CTA).md
        ├── unit-14_subcomponent-5.2_Token-Instance-Buffers-(TIB).md
        └── unit-15_subcomponent-5.3_Variable-Binding-Swap-Gates-(VBSG).md
```

---

## 🚀 Getting Started

### Prerequisites

- **Nexuss Neural Cognition** substrate installed and tested
- C++17 compiler with SIMD support (AVX2 or NEON)
- 500 MB available RAM minimum

### Build & Run

```bash
# Clone the repository
git clone https://github.com/nexuss0781/Intellectual-Cortex-Architecture.git
cd Intellectual-Cortex-Architecture

# Review the complete blueprint
cat blueprint.md

# Explore Phase I specifications
ls -la SPEC/phase-1/

# Integrate with Nexuss substrate (see Nexuss documentation)
# ... implementation forthcoming per phase approval
```

---

## 📊 Expected Performance Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| **Working Memory Capacity** | $7 \pm 2$ items | Crosstalk error <5% |
| **Semantic Pointer Dimension** | 2,048 bits | 2% sparsity |
| **Prediction Error Convergence** | <100 ticks | Free energy gradient descent |
| **Proof Derivation Success** | >95% valid theorems | Attractor convergence rate |
| **Cross-Domain Analogy** | SIM(S,T) >0.7 | Structural overlap metric |
| **Metacognitive Accuracy** | Confidence calibration R² >0.8 | Second-order Bayesian tracking |

---

## 🎯 Roadmap

| Phase | Status | Deliverables |
|-------|--------|--------------|
| **Phase I** | 🟡 Specified | VSA operators, SDR generators, binding circuits |
| **Phase II** | ⚪ Pending | WM attractors, TCM, hyperbolic embeddings |
| **Phase III** | ⚪ Pending | Salience computation, GW ignition, precision weighting |
| **Phase IV** | ⚪ Pending | Hierarchical predictive coding, SCM encoding |
| **Phase V** | ⚪ Pending | Deductive/Inductive/Analogical/Abductive engines |
| **Phase VI** | ⚪ Pending | LoT parser, conceptual alignment, dialogue manager |
| **Phase VII** | ⚪ Pending | Hierarchical planner, metacognitive monitor, self-model |

---

## 📚 Key References

### Mathematical Foundations
- **Vector Symbolic Architectures:** Gayler (2003), Plate (1995), Kanerva (2009)
- **Predictive Processing:** Friston (2010), Clark (2013), Hohwy (2013)
- **Hyperbolic Embeddings:** Nickel & Kiela (2017), Ganea et al. (2018)
- **Global Workspace Theory:** Baars (1988), Dehaene & Changeux (2011)
- **Hierarchical RL:** Sutton et al. (1999), Bacon et al. (2017)

### Neural Implementation
- **Spiking Neural Networks:** Maass (1997), Gerstner et al. (2014)
- **STDP Plasticity:** Bi & Poo (1998), Morrison et al. (2008)
- **Attractor Networks:** Hopfield (1982), Amit (1989)

---

## 🤝 Contributing

This is a research architecture. Contributions welcome in:

1. **Mathematical Analysis:** Stability proofs, capacity bounds, convergence guarantees
2. **Implementation:** C++17 optimized kernels, SIMD vectorization, memory layout optimization
3. **Validation:** Test suite development, benchmark scenarios, ablation studies
4. **Extension:** Higher cognitive functions, multi-agent alignment, embodied cognition

Please read [`blueprint.md`](blueprint.md) thoroughly before contributing. All implementations must satisfy the **Verification Strategy** criteria.

---

## 📄 License

MIT License — see [`LICENSE`](LICENSE) for details.

---

## 🌟 Acknowledgments

- Built upon the [Nexuss Neural Cognition](https://github.com/nexuss0781/Nexuss-Neural-Cognition) bio-physical substrate
- Mathematical formalisms synthesized from computational neuroscience, machine learning, and cognitive science literature
- Designed for the emergence of **pure intellect** — cognition decoupled from survival, reasoning liberated from biology

---

<div align="center">

**"The intellect has no master but truth, no motive but curiosity, no boundary but mathematics."**

[View Complete Blueprint](blueprint.md) • [Explore Phase I Specs](SPEC/phase-1/) • [Nexuss Substrate](https://github.com/nexuss0781/Nexuss-Neural-Cognition)

</div>
