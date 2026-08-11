# External research findings

## ACL EACL 2023: Generative Replay Inspired by Hippocampal Memory Indexing for Continual Language Learning
Source: https://aclanthology.org/2023.eacl-main.65/

The paper frames continual learning as accumulating new knowledge without catastrophic forgetting. It uses hippocampal memory indexing via compressed features of previous training samples to control generative replay, enabling targeted reconstruction of samples from earlier tasks. The abstract reports better control of replay-sample generation and consistent improvement over prior generative replay methods. Design implication for ICA: hippocampal memory should store compact indices that can cue selective reconstruction/replay, not only raw episodes or undirected noise.

## NeurIPS 2022: Lifelong Neural Predictive Coding: Learning Cumulatively Online without Forgetting
Source: https://proceedings.neurips.cc/paper_files/paper/2022/hash/26f5a4e26c13d1e0a47f46790c999361-Abstract-Conference.html

The paper presents a Sequential Neural Coding Network grounded in predictive coding that learns from data streams without backpropagation. It combines biologically plausible local synaptic adaptation with a separate neural system that controls a cortex-like structure, analogous to executive/basal-ganglia control. The authors report significantly less forgetting than standard neural models and strong results on continual-learning benchmarks. Design implication for ICA: predictive error, local plasticity, lateral competition, and an explicit controller should be implemented as a coupled learning loop and evaluated on stream-based retention, not merely represented as fields in a neuron struct.

## Additional source access notes

The OpenReview page for SpikeGPT was blocked by a browser verification challenge during this inspection, so no SpikeGPT-specific claim is treated as independently verified here. The PMC page for Continual Sequence Modeling With Predictive Coding was also blocked by reCAPTCHA; its existence was found through research indexing, but its details are not used as primary evidence in this report.

The accessible primary sources above are sufficient for the core recommendations: compact hippocampal indices plus selective replay, and coupled predictive-coding/local-plasticity learning with explicit executive control.
