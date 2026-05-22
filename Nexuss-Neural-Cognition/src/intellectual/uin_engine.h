#pragma once

#include "uin_constants.h"
#include "uin_state.h"
#include "../types.h"  // For NeuronBlock, SynapseBlock
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace genesis {
namespace intellectual {

// =============================================================================
// Universal Intellectual Neuron (UIN) Engine
// 
// Implements the O(1) per-neuron update kernel as specified in §4
// All 6 neuron classes share this single kernel with type-specific branches
// =============================================================================
class UINEngine {
public:
    UINEngine() = default;
    ~UINEngine() = default;
    
    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------
    
    /// Initialize a pool of neurons with a specific type_id
    /// @param neurons Reference to the NeuronBlock (SoA structure)
    /// @param offset Starting index in the neuron array
    /// @param count Number of neurons to initialize
    /// @param type_id Neuron class type (0-5)
    void initialize_pool(NeuronBlock& neurons, uint32_t offset, uint32_t count, uint8_t type_id);
    
    /// Mark neurons as intellectual pool (sets FLAG_INTELLECTUAL_POOL)
    void mark_as_intellectual(NeuronBlock& neurons, uint32_t offset, uint32_t count);
    
    // -------------------------------------------------------------------------
    // Per-tick Update Kernel (O(N) total, O(1) per neuron)
    // -------------------------------------------------------------------------
    
    /// Execute the universal update kernel for all intellectual neurons
    /// @param neurons Reference to NeuronBlock containing all neuron state
    /// @param num_neurons Total number of neurons to process
    /// @param dt Time step in ms (default 1.0)
    void step_kernel(NeuronBlock& neurons, uint32_t num_neurons, float dt = 1.0f);
    
    // -------------------------------------------------------------------------
    // Synaptic Event Delivery (O(1) per event)
    // -------------------------------------------------------------------------
    
    /// Deliver a spike through an intellectual synapse
    /// @param syn Pointer to the synapse structure
    /// @param neurons Target neuron block
    /// @param pre_rate Presynaptic firing rate (approximated by s_slow)
    void deliver_spike(const IntellectualSynapse* syn, NeuronBlock& neurons, float pre_rate = 1.0f);
    
    /// Deliver batch of spikes (optimized for cache coherence)
    /// @param synapses Array of synapse indices to process
    /// @param syn_block Full synapse block
    /// @param neurons Target neuron block
    /// @param num_events Number of synaptic events
    void deliver_spike_batch(const uint32_t* synapse_indices, 
                             const SynapseBlock& syn_block,
                             NeuronBlock& neurons,
                             uint32_t num_events);
    
    // -------------------------------------------------------------------------
    // Type Queries
    // -------------------------------------------------------------------------
    
    /// Get neuron type_id from NeuronBlock
    uint8_t get_type(const NeuronBlock& neurons, uint32_t idx) const;
    
    /// Check if neuron belongs to intellectual pool
    bool is_intellectual(const NeuronBlock& neurons, uint32_t idx) const;
    
    /// Get neuron state as IntellectualNeuronState (zero-copy view)
    /// Note: This requires the NeuronBlock to have intellectual overlay arrays
    IntellectualNeuronState get_neuron_state(const NeuronBlock& neurons, uint32_t idx) const;
    
    // -------------------------------------------------------------------------
    // Memory Audit Helpers
    // -------------------------------------------------------------------------
    
    /// Verify memory layout constraints
    static inline constexpr size_t get_neuron_state_size() { return sizeof(IntellectualNeuronState); }
    static inline constexpr size_t get_synapse_size() { return sizeof(IntellectualSynapse); }
    static inline constexpr size_t get_neuron_alignment() { return alignof(IntellectualNeuronState); }
    
private:
    // -------------------------------------------------------------------------
    // Internal kernel helpers (all O(1))
    // -------------------------------------------------------------------------
    
    /// Update conductance decay (§4.2)
    inline void update_conductances(float& g_exc, float& g_inh, float& g_bind) const;
    
    /// Compute synaptic current (§4.3)
    inline float compute_synaptic_current(float V, float g_exc, float g_inh, float g_bind) const;
    
    /// Update membrane potential (§4.4)
    inline void update_membrane(float& V, float I_syn, float dt) const;
    
    /// Update slow gate (§4.5)
    inline void update_slow_gate(float& s_slow, bool spiked_last_tick, uint16_t type_id, float dt) const;
    
    /// Update dynamic threshold (§4.6)
    inline void update_threshold(float& theta_dyn, bool spiked_last_tick, uint16_t type_id, float dt) const;
    
    /// Update phase rotation (§4.7)
    inline void update_phase(float& phi, float dt) const;
    
    /// Compute prediction error (§4.8) - for PU neurons
    inline void compute_prediction_error(float& epsilon, float s_slow, float mu_pred, float precision) const;
    
    /// Update precision weight (§4.9) - for PM neurons
    inline void update_precision(float& precision, float target_precision, float dt) const;
    
    /// Update eligibility trace (§4.10)
    inline void update_trace(float& trace, bool spiked) const;
    
    /// Check firing condition and emit spike (§4.11)
    /// @return true if neuron fired
    inline bool check_and_fire(float& V, float& theta_dyn, uint32_t& spike_timer, 
                               float& trace, uint16_t type_id) const;
    
    // -------------------------------------------------------------------------
    // Constants (precomputed)
    // -------------------------------------------------------------------------
    UINConstants constants_;
    bool constants_initialized_ = false;
    
    void ensure_constants_initialized(float dt = 1.0f);
};

// =============================================================================
// Inline Implementations of O(1) Kernel Helpers
// =============================================================================

inline void UINEngine::ensure_constants_initialized(float dt) {
    if (!constants_initialized_) {
        constants_ = UINConstants(dt);
        constants_initialized_ = true;
    }
}

inline void UINEngine::update_conductances(float& g_exc, float& g_inh, float& g_bind) const {
    g_exc *= constants_.exc_decay;
    g_inh *= constants_.inh_decay;
    g_bind *= constants_.bind_decay;
}

inline float UINEngine::compute_synaptic_current(float V, float g_exc, float g_inh, float g_bind) const {
    // I_syn = g_exc(V - E_exc) + g_inh(V - E_inh) + g_bind(V - E_bind)
    return g_exc * (V - E_EXC_MV) + 
           g_inh * (V - E_INH_MV) + 
           g_bind * (V - E_BIND_MV);
}

inline void UINEngine::update_membrane(float& V, float I_syn, float dt) const {
    // V ← V + (dt/τₘ)(-(V - V_rest) + Rₘ·I_syn)
    const float dv = (dt / TAU_MEMBRANE_MS) * (-(V - V_REST_MV) + R_MEGOHM * I_syn);
    V += dv;
}

inline void UINEngine::update_slow_gate(float& s_slow, bool spiked_last_tick, 
                                        uint16_t type_id, float dt) const {
    // s_slow ← s_slow + (dt/τₛ)(-s_slow + α·𝕀_spike(t-1))
    const float alpha = SLOW_SPIKE_INCREMENT;
    const float target = spiked_last_tick ? alpha : 0.0f;
    
    // Use AS-specific decay for Attractor Sustainers
    const float decay = is_attractor_sustainer(type_id) ? 
                        constants_.slow_decay_as : constants_.slow_decay;
    
    s_slow = s_slow * decay + target * (1.0f - decay);
}

inline void UINEngine::update_threshold(float& theta_dyn, bool spiked_last_tick,
                                        uint16_t type_id, float dt) const {
    // θ_dyn ← θ_dyn + (dt/τ_θ)(-(θ_dyn - θ_base) + β·𝕀_spike(t-1))
    const float beta = is_attractor_sustainer(type_id) ? 
                       constants_.theta_increment_as : THETA_SPIKE_INCREMENT_MV;
    const float increment = spiked_last_tick ? beta : 0.0f;
    
    const float decay_factor = (dt / TAU_THETA_MS);
    theta_dyn += decay_factor * (-(theta_dyn - THETA_BASE_MV) + increment);
}

inline void UINEngine::update_phase(float& phi, float dt) const {
    // φ ← φ + ω·dt (mod 2π)
    phi += constants_.omega_dt;
    // Use fmod for proper modulo operation (handles large omega_dt values)
    phi = std::fmod(phi, TWO_PI);
    if (phi < 0) phi += TWO_PI;  // Ensure positive range [0, 2π)
}

inline void UINEngine::compute_prediction_error(float& epsilon, float s_slow, 
                                                 float mu_pred, float precision) const {
    // ε ← precision · (s_slow - μ_pred)
    epsilon = precision * (s_slow - mu_pred);
}

inline void UINEngine::update_precision(float& precision, float target_precision, float dt) const {
    // precision ← precision + (dt/τ_π)(-precision + π_input)
    const float decay_factor = (dt / TAU_PRECISION_MS);
    precision += decay_factor * (-precision + target_precision);
}

inline void UINEngine::update_trace(float& trace, bool spiked) const {
    // trace ← trace · exp(-dt/τ_trace) + A₊·𝕀_spike
    trace *= constants_.trace_decay;
    if (spiked) {
        trace += TRACE_SPIKE_INCREMENT;
    }
}

inline bool UINEngine::check_and_fire(float& V, float& theta_dyn, uint32_t& spike_timer,
                                       float& trace, uint16_t type_id) const {
    // Executive Controller special handling for drift-diffusion
    if (is_executive_controller(type_id)) {
        // EC uses dual thresholds (upper and lower bounds)
        if (V >= EC_UPPER_BOUND_MV) {
            // "Go" decision - Option A
            V = V_RESET_MV;
            spike_timer = REFRACTORY_TICKS;
            trace += TRACE_SPIKE_INCREMENT;
            return true;
        } else if (V <= EC_LOWER_BOUND_MV) {
            // "No-go" decision - Option B
            V = V_RESET_MV;
            spike_timer = REFRACTORY_TICKS;
            trace += TRACE_SPIKE_INCREMENT;
            return true;
        }
        return false;
    }
    
    // Standard firing condition for all other types
    if (V >= theta_dyn) {
        V = V_RESET_MV;
        spike_timer = REFRACTORY_TICKS;
        trace += TRACE_SPIKE_INCREMENT;
        return true;
    }
    return false;
}

} // namespace intellectual
} // namespace genesis
