#include "uin_engine.h"
#include <cstring>  // For memset

namespace genesis {
namespace intellectual {

// =============================================================================
// Initialization Implementation
// =============================================================================

void UINEngine::initialize_pool(NeuronBlock& neurons, uint32_t offset, uint32_t count, uint8_t type_id) {
    ensure_constants_initialized(1.0f);
    
    const uint32_t end = offset + count;
    
    // Verify bounds
    if (end > neurons.membrane_potential.size()) {
        return;  // Out of bounds, silently fail
    }
    
    // Initialize all state arrays for this pool
    for (uint32_t i = offset; i < end; ++i) {
        // Reset membrane potential to resting state
        neurons.membrane_potential[i] = V_REST_MV;
        
        // Set type_id in layer_id
        neurons.layer_id[i] = type_id;
        
        // Reset refractory timer
        neurons.refractory_timer[i] = 0;
        
        // Reset firing state
        neurons.has_fired[i] = false;
        neurons.avg_firing_rate[i] = 0.0f;
        
        // Reset conductances
        neurons.g_exc[i] = 0.0f;
        neurons.g_inh[i] = 0.0f;
        neurons.g_bind[i] = 0.0f;
        
        // Reset prediction and precision
        neurons.mu_pred[i] = 0.0f;
        neurons.epsilon[i] = 0.0f;
        neurons.precision_weight[i] = 1.0f;
        
        // Reset STDP trace
        neurons.stpd_trace[i] = 0.0f;
        
        // Reset phase
        neurons.recovery_variable[i] = 0.0f;
        
        // Reset dynamic threshold to base
        neurons.atp_level[i] = THETA_BASE_MV;
        
        // Mark as intellectual pool via flags
        neurons.neuron_flags[i] = FLAG_INTELLECTUAL_POOL;
        
        // Encode type in plasticity_scale for quick lookup (100 + type_id)
        neurons.plasticity_scale[i] = static_cast<float>(100 + type_id);
    }
}

void UINEngine::mark_as_intellectual(NeuronBlock& neurons, uint32_t offset, uint32_t count) {
    const uint32_t end = offset + count;
    if (end > neurons.membrane_potential.size()) return;
    
    for (uint32_t i = offset; i < end; ++i) {
        neurons.neuron_flags[i] |= FLAG_INTELLECTUAL_POOL;
    }
}

// =============================================================================
// Per-tick Update Kernel Implementation
// =============================================================================

void UINEngine::step_kernel(NeuronBlock& neurons, uint32_t num_neurons, float dt) {
    ensure_constants_initialized(dt);
    
    // First, reset has_fired flags from previous tick and update traces for spikes
    for (uint32_t i = 0; i < num_neurons; ++i) {
        if (!(neurons.neuron_flags[i] & FLAG_INTELLECTUAL_POOL)) {
            continue;
        }
        // Update trace decay based on whether neuron spiked last tick
        if (neurons.has_fired[i]) {
            neurons.stpd_trace[i] += TRACE_SPIKE_INCREMENT;
        }
        neurons.stpd_trace[i] *= constants_.trace_decay;
        // Reset has_fired for this tick
        neurons.has_fired[i] = false;
    }
    
    // Process each neuron with O(1) operations
    for (uint32_t i = 0; i < num_neurons; ++i) {
        // Check if this is an intellectual neuron
        if (!(neurons.neuron_flags[i] & FLAG_INTELLECTUAL_POOL)) {
            // Skip non-intellectual neurons (they use standard bio-engine dynamics)
            continue;
        }
        
        // Skip if in refractory period
        if (neurons.refractory_timer[i] > 0) {
            neurons.refractory_timer[i]--;
            continue;
        }
        
        // Get neuron type
        uint16_t type_id = static_cast<uint16_t>(neurons.layer_id[i]);
        
        // === UIN Universal Kernel (§4) ===
        
        // 4.2 Conductance Decay (Exponential)
        neurons.g_exc[i] *= constants_.exc_decay;
        neurons.g_inh[i] *= constants_.inh_decay;
        neurons.g_bind[i] *= constants_.bind_decay;
        
        // 4.3 Synaptic Current Integration
        float V = neurons.membrane_potential[i];
        float I_syn = compute_synaptic_current(V, neurons.g_exc[i], neurons.g_inh[i], neurons.g_bind[i]);
        
        // 4.4 Membrane Update (LIF)
        update_membrane(V, I_syn, dt);
        neurons.membrane_potential[i] = V;
        
        // 4.5 Slow Gate Update (Attractor Support)
        // Note: spiked_last refers to whether neuron fired in PREVIOUS tick
        // Since we just reset has_fired, we need to track this differently
        // For now, use avg_firing_rate as the rate estimate
        update_slow_gate(neurons.avg_firing_rate[i], false, type_id, dt);
        
        // 4.6 Dynamic Threshold
        update_threshold(neurons.atp_level[i], false, type_id, dt);
        
        // 4.7 Phase Rotation (Binding Synchronization)
        update_phase(neurons.recovery_variable[i], dt);
        
        // 4.8 Prediction Error (for PU neurons, type_id=3)
        if (is_prediction_unit(type_id)) {
            compute_prediction_error(neurons.epsilon[i], neurons.avg_firing_rate[i], 
                                     neurons.mu_pred[i], neurons.precision_weight[i]);
        }
        
        // 4.9 Precision Adaptation (for PM neurons, type_id=2)
        if (is_precision_modulator(type_id)) {
            // Precision is updated via synaptic input, not here
            // Just apply decay
            neurons.precision_weight[i] *= constants_.precision_decay;
        }
        
        // 4.11 Firing Condition
        float& theta_dyn = neurons.atp_level[i];
        uint32_t spike_timer_val = static_cast<uint32_t>(neurons.refractory_timer[i]);
        float& trace = neurons.stpd_trace[i];
        
        if (check_and_fire(V, theta_dyn, spike_timer_val, trace, type_id)) {
            neurons.membrane_potential[i] = V;  // Updated in check_and_fire
            neurons.has_fired[i] = true;
        }
        neurons.refractory_timer[i] = static_cast<int32_t>(spike_timer_val);
    }
}

// =============================================================================
// Synaptic Event Delivery Implementation
// =============================================================================

void UINEngine::deliver_spike(const IntellectualSynapse* syn, NeuronBlock& neurons, float pre_rate) {
    if (!syn || !neurons.membrane_potential.size()) return;
    
    uint32_t post_id = syn->post_id;
    if (post_id >= neurons.membrane_potential.size()) return;
    
    // Get synapse type
    SynapseType type = syn->get_type();
    
    switch (type) {
        case SynapseType::FEEDFORWARD:
        case SynapseType::RECURRENT:
            // Standard synaptic current injection - increment conductance
            neurons.g_exc[post_id] += syn->w * pre_rate;
            break;
            
        case SynapseType::TOP_DOWN:
            // Write directly to mu_pred (prediction field) for PU neurons
            neurons.mu_pred[post_id] += syn->pred_coeff * pre_rate;
            break;
            
        case SynapseType::LATERAL_INH:
            // Inhibitory input - increment inhibitory conductance
            neurons.g_inh[post_id] += syn->w * pre_rate;
            break;
            
        case SynapseType::PRECISION_GATE:
            // Set precision on postsynaptic PM neuron
            neurons.precision_weight[post_id] = std::min(1.0f, std::max(0.0f, syn->precision));
            break;
            
        case SynapseType::BINDING_PAIR:
            // Multiplicative binding - increment binding conductance
            neurons.g_bind[post_id] += syn->w * pre_rate;
            break;
    }
    
    // Update eligibility trace if plasticity enabled
    if (syn->is_plastic()) {
        neurons.stpd_trace[post_id] += TRACE_SPIKE_INCREMENT;
    }
}

void UINEngine::deliver_spike_batch(const uint32_t* synapse_indices,
                                     const SynapseBlock& syn_block,
                                     NeuronBlock& neurons,
                                     uint32_t num_events) {
    for (uint32_t i = 0; i < num_events; ++i) {
        uint32_t syn_idx = synapse_indices[i];
        if (syn_idx >= syn_block.weights.size()) continue;
        
        // Map from legacy SynapseBlock to IntellectualSynapse semantics
        uint32_t post_id = syn_block.post_indices[syn_idx];
        float weight = syn_block.weights[syn_idx];
        
        // Determine synapse type from legacy flags
        SynapseType type = syn_block.is_inhibitory[syn_idx] ? 
                           SynapseType::LATERAL_INH : SynapseType::FEEDFORWARD;
        
        // If intellectual fields are set, use them
        if (syn_idx < syn_block.synapse_type.size()) {
            type = static_cast<SynapseType>(syn_block.synapse_type[syn_idx]);
        }
        
        // Create temporary IntellectualSynapse
        IntellectualSynapse temp_syn;
        temp_syn.post_id = post_id;
        temp_syn.w = weight;
        temp_syn.delay = static_cast<float>(syn_block.delays[syn_idx]);
        temp_syn.eligibility = syn_block.eligibility_traces[syn_idx];
        temp_syn.pred_coeff = (syn_idx < syn_block.pred_coeff.size()) ? 
                              syn_block.pred_coeff[syn_idx] : 1.0f;
        temp_syn.precision = (syn_idx < syn_block.precision_scale.size()) ? 
                             syn_block.precision_scale[syn_idx] : 1.0f;
        temp_syn.set_type(type);
        
        deliver_spike(&temp_syn, neurons, 1.0f);
    }
}

// =============================================================================
// Type Query Implementation
// =============================================================================

uint8_t UINEngine::get_type(const NeuronBlock& neurons, uint32_t idx) const {
    if (idx >= neurons.membrane_potential.size()) return 0;
    return neurons.layer_id[idx];
}

bool UINEngine::is_intellectual(const NeuronBlock& neurons, uint32_t idx) const {
    if (idx >= neurons.membrane_potential.size()) return false;
    return (neurons.neuron_flags[idx] & FLAG_INTELLECTUAL_POOL) != 0;
}

IntellectualNeuronState UINEngine::get_neuron_state(const NeuronBlock& neurons, uint32_t idx) const {
    IntellectualNeuronState state;
    
    if (idx >= neurons.membrane_potential.size()) {
        return state;  // Return default state
    }
    
    // Map from SoA structure to state struct (zero-copy view)
    state.V = neurons.membrane_potential[idx];
    state.I_syn = 0.0f;  // Would need dedicated storage or compute on demand
    state.g_exc = neurons.g_exc[idx];
    state.g_inh = neurons.g_inh[idx];
    state.g_bind = neurons.g_bind[idx];
    state.theta_dyn = neurons.atp_level[idx];
    state.s_slow = neurons.avg_firing_rate[idx];
    state.phi = neurons.recovery_variable[idx];
    state.mu_pred = neurons.mu_pred[idx];
    state.epsilon = neurons.epsilon[idx];
    state.precision = neurons.precision_weight[idx];
    state.spike_timer = static_cast<uint32_t>(neurons.refractory_timer[idx]);
    state.trace = neurons.stpd_trace[idx];
    state.type_id = neurons.layer_id[idx];
    state.flags = neurons.neuron_flags[idx];
    
    return state;
}

} // namespace intellectual
} // namespace genesis
