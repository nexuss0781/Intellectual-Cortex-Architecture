#pragma once

#include "uin_constants.h"
#include <cstdint>
#include <cstddef>

namespace genesis {
namespace intellectual {

// =============================================================================
// IntellectualNeuronState: 64-byte state vector for UIN
// Aligned to 32 bytes for AVX2 SIMD efficiency
// =============================================================================
struct alignas(32) IntellectualNeuronState {
    // === Cache Line 1 (32 bytes) ===
    float V;              // [ 0- 3] Membrane potential (mV)
    float I_syn;          // [ 4- 7] Total synaptic current (nA)
    float g_exc;          // [ 8-11] Excitatory conductance (nS)
    float g_inh;          // [12-15] Inhibitory conductance (nS)
    float g_bind;         // [16-19] Multiplicative binding conductance
    float theta_dyn;      // [20-23] Adaptive threshold (mV)
    float s_slow;         // [24-27] Slow activation gate [0,1]
    float phi;            // [28-31] Oscillatory phase [0, 2π)
    
    // === Cache Line 2 (32 bytes) ===
    float mu_pred;        // [32-35] Top-down prediction (rate code)
    float epsilon;        // [36-39] Local prediction error
    float precision;      // [40-43] Precision weight (attentional gain)
    uint32_t spike_timer; // [44-47] Refractory countdown (ticks)
    float trace;          // [48-51] Eligibility / STDP trace
    uint16_t type_id;     // [52-53] Neuron class (0-5)
    uint16_t flags;       // [54-55] Mode bits (broadcast, learn, inhibit)
    uint8_t  reserved[8]; // [56-63] Padding / substrate compatibility
    
    // Default constructor - initializes to resting state
    IntellectualNeuronState() :
        V(-70.0f),
        I_syn(0.0f),
        g_exc(0.0f),
        g_inh(0.0f),
        g_bind(0.0f),
        theta_dyn(-55.0f),
        s_slow(0.0f),
        phi(0.0f),
        mu_pred(0.0f),
        epsilon(0.0f),
        precision(1.0f),
        spike_timer(0),
        trace(0.0f),
        type_id(0),
        flags(0)
    {
        for (int i = 0; i < 8; ++i) reserved[i] = 0;
    }
};

// Compile-time size verification (outside struct definition)
static_assert(sizeof(IntellectualNeuronState) == 64, 
              "IntellectualNeuronState must be exactly 64 bytes");
static_assert(alignof(IntellectualNeuronState) >= 32,
              "IntellectualNeuronState must be 32-byte aligned for SIMD");

// =============================================================================
// IntellectualSynapse: 32-byte synapse structure
// =============================================================================
struct alignas(8) IntellectualSynapse {
    uint32_t post_id;     // [ 0- 3] Postsynaptic index
    float    w;           // [ 4- 7] Efficacy (nS or dimensionless)
    float    delay;       // [ 8-11] Axonal delay (ticks, max 255)
    float    eligibility; // [12-15] Eligibility trace (RL / consolidation)
    float    pred_coeff;  // [16-19] Top-down prediction weight (for PU targets)
    float    precision;   // [20-23] Precision scaling (for PM targets)
    uint8_t  tag[8];      // [24-31] Binding key / semantic label / type flags
    
    // Default constructor
    IntellectualSynapse() :
        post_id(0),
        w(0.0f),
        delay(1.0f),
        eligibility(0.0f),
        pred_coeff(0.0f),
        precision(1.0f)
    {
        for (int i = 0; i < 8; ++i) tag[i] = 0;
    }
    
    // Get synapse type from tag[0] bits [0:2]
    inline SynapseType get_type() const {
        return static_cast<SynapseType>(tag[0] & 0x07);
    }
    
    // Set synapse type in tag[0] bits [0:2]
    inline void set_type(SynapseType type) {
        tag[0] = (tag[0] & 0xF8) | (static_cast<uint8_t>(type) & 0x07);
    }
    
    // Check if plasticity is enabled
    inline bool is_plastic() const {
        return is_plasticity_enabled(tag[0]);
    }
    
    // Check if structurally prunable
    inline bool is_prunable() const {
        return is_structural_prunable(tag[0]);
    }
};

// Compile-time size verification (outside struct definition)
static_assert(sizeof(IntellectualSynapse) == 32,
              "IntellectualSynapse must be exactly 32 bytes");

// =============================================================================
// Flag bit definitions for IntellectualNeuronState.flags
// =============================================================================
constexpr uint16_t FLAG_BROADCAST_ENABLED = 0x0001;
constexpr uint16_t FLAG_LEARNING_ENABLED  = 0x0002;
constexpr uint16_t FLAG_INHIBITED         = 0x0004;
constexpr uint16_t FLAG_INTELLECTUAL_POOL = 0x0008;  // Marks neuron as intellectual (not sensory/survival)

// =============================================================================
// Helper functions for type checking
// =============================================================================
inline bool is_intellectual_type(uint16_t type_id) {
    return type_id <= 5;  // CI=0 through EC=5
}

inline bool is_attractor_sustainer(uint16_t type_id) {
    return type_id == static_cast<uint16_t>(NeuronType::AS);
}

inline bool is_precision_modulator(uint16_t type_id) {
    return type_id == static_cast<uint16_t>(NeuronType::PM);
}

inline bool is_prediction_unit(uint16_t type_id) {
    return type_id == static_cast<uint16_t>(NeuronType::PU);
}

inline bool is_binding_gate(uint16_t type_id) {
    return type_id == static_cast<uint16_t>(NeuronType::BG);
}

inline bool is_executive_controller(uint16_t type_id) {
    return type_id == static_cast<uint16_t>(NeuronType::EC);
}

} // namespace intellectual
} // namespace genesis
