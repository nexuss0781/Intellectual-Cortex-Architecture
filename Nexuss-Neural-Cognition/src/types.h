#pragma once

#include "constants.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <deque>

namespace genesis {

// -----------------------------------------------------------------------------
// Phase III: Engine State Machine
// -----------------------------------------------------------------------------
enum class EngineState {
    AWAKE, // Normal sensory processing and learning
    SLEEP  // Sensory gating, spontaneous replay, consolidation
};

// -----------------------------------------------------------------------------
// Intellectual Neuron Type IDs (Phase IV+)
// -----------------------------------------------------------------------------
enum class IntellectualNeuronType : uint8_t {
    CI = 0,  // Core Integrator
    AS = 1,  // Attractor Sustainer
    PM = 2,  // Precision Modulator
    PU = 3,  // Prediction Unit
    BG = 4,  // Binding Gate
    EC = 5   // Executive Controller
};

// -----------------------------------------------------------------------------
// 1. Data-Oriented Design: Struct-of-Arrays (SoA)
// -----------------------------------------------------------------------------
struct alignas(64) NeuronBlock {
    // Phase I: Physics State
    std::vector<float>   membrane_potential;   // V (mV)
    std::vector<float>   recovery_variable;    // u (also used for phase phi in UIN)
    std::vector<float>   atp_level;            // Energy (also used for theta_dyn in UIN)
    
    // Phase I: Discrete State
    std::vector<int32_t> refractory_timer;
    
    // Phase I: Event State
    std::vector<bool>    has_fired;
    std::vector<uint64_t> last_spike_time;
    
    // Phase I: Homeostasis
    std::vector<float>   avg_firing_rate;      // Also used for s_slow in UIN
    
    // Phase II: Space & Structure
    std::vector<float>   pos_x;
    std::vector<float>   pos_y;
    std::vector<uint8_t> layer_id;             // Also used for type_id in UIN

    // --- PHASE III EXTENSIONS: MEMORY ---
    std::vector<float>   plasticity_scale;     // Learning rate multiplier

    // --- PHASE IV+: INTELLECTUAL OVERLAY (64-byte state vector fields) ---
    // These arrays provide the full UIN state within the existing memory budget
    // by reusing substrate fields architecturally while adding minimal new arrays
    
    // Conductance arrays for UIN (excitatory, inhibitory, binding)
    std::vector<float>   g_exc;                // Excitatory conductance (nS)
    std::vector<float>   g_inh;                // Inhibitory conductance (nS)
    std::vector<float>   g_bind;               // Multiplicative binding conductance
    
    // Prediction and precision arrays for UIN
    std::vector<float>   mu_pred;              // Top-down prediction (rate code)
    std::vector<float>   epsilon;              // Local prediction error
    std::vector<float>   precision_weight;     // Precision weight (attentional gain)
    
    // STDP trace array
    std::vector<float>   stpd_trace;           // Eligibility / STDP trace
    
    // Flags for intellectual pool membership
    std::vector<uint16_t> neuron_flags;        // Mode bits (broadcast, learn, inhibit, intellectual)

    void resize(size_t n) {
        membrane_potential.resize(n);
        recovery_variable.resize(n);
        atp_level.resize(n);
        refractory_timer.resize(n);
        has_fired.resize(n);
        last_spike_time.resize(n);
        avg_firing_rate.resize(n);
        
        pos_x.resize(n);
        pos_y.resize(n);
        layer_id.resize(n);
        
        // Phase III
        plasticity_scale.resize(n);
        
        // Phase IV+: Intellectual Overlay
        g_exc.resize(n, 0.0f);
        g_inh.resize(n, 0.0f);
        g_bind.resize(n, 0.0f);
        mu_pred.resize(n, 0.0f);
        epsilon.resize(n, 0.0f);
        precision_weight.resize(n, 1.0f);
        stpd_trace.resize(n, 0.0f);
        neuron_flags.resize(n, 0);
    }
};

struct alignas(64) SynapseBlock {
    // Connectivity
    std::vector<uint32_t> pre_indices;
    std::vector<uint32_t> post_indices;
    
    // Dynamics
    std::vector<float>    weights;
    std::vector<float>    eligibility_traces;
    std::vector<bool>     is_inhibitory;

    // Phase II: Time
    std::vector<uint8_t>  delays;
    
    // Phase IV+: Intellectual Synapse Fields (32-byte overlay)
    std::vector<float>    pred_coeff;      // Top-down prediction weight (for PU targets)
    std::vector<float>    precision_scale; // Precision scaling (for PM targets)
    std::vector<uint64_t> binding_tag;     // 8-byte binding key / semantic label
    std::vector<uint8_t>  synapse_type;    // Synapse class (feedforward, recurrent, top-down, etc.)
    std::vector<uint8_t>  synapse_flags;   // Plasticity enabled, structural prunable flags

    void resize(size_t n) {
        pre_indices.resize(n);
        post_indices.resize(n);
        weights.resize(n);
        eligibility_traces.resize(n);
        is_inhibitory.resize(n);
        delays.resize(n);
        
        // Phase IV+: Intellectual Overlay
        pred_coeff.resize(n, 0.0f);
        precision_scale.resize(n, 1.0f);
        binding_tag.resize(n, 0);
        synapse_type.resize(n, 0);
        synapse_flags.resize(n, 0);
    }
};

// -----------------------------------------------------------------------------
// 2. Topology Acceleration (CSR)
// -----------------------------------------------------------------------------
struct TopologyIndex {
    std::vector<uint32_t> outgoing_start;
    std::vector<uint32_t> outgoing_count;

    void resize(size_t n) {
        outgoing_start.resize(n);
        outgoing_count.resize(n);
    }
};

// -----------------------------------------------------------------------------
// 3. Temporal Integration Buffer
// -----------------------------------------------------------------------------
struct PendingSpike {
    uint32_t target_neuron_id;
    float    signal_strength; 
    uint8_t  ticks_remaining;
};

using SpikeDelayQueue = std::deque<PendingSpike>;

// -----------------------------------------------------------------------------
// 4. Global Context
// -----------------------------------------------------------------------------
struct Context {
    uint64_t current_tick = 0;
    uint64_t rng_seed = 0;

    float dopamine      = 0.0f;
    float acetylcholine = 1.0f;
    
    // Phase III: Current Brain State
    EngineState state = EngineState::AWAKE;
    
    uint64_t total_spikes_this_tick = 0;
};

} // namespace genesis