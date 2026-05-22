#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>

namespace genesis {
namespace intellectual {

// =============================================================================
// CONSTANTS: Universal Intellectual Neuron (UIN) Parameters
// =============================================================================

// Time constants (ms)
constexpr float TAU_EXC_MS      = 5.0f;    // Excitatory conductance decay
constexpr float TAU_INH_MS      = 10.0f;   // Inhibitory conductance decay
constexpr float TAU_BIND_MS     = 20.0f;   // Binding conductance decay
constexpr float TAU_MEMBRANE_MS = 20.0f;   // Membrane time constant
constexpr float TAU_SLOW_MS     = 200.0f;  // Slow activation gate (default)
constexpr float TAU_THETA_MS    = 100.0f;  // Dynamic threshold adaptation
constexpr float TAU_TRACE_MS    = 20.0f;   // STDP eligibility trace
constexpr float TAU_PRECISION_MS = 50.0f;  // Precision weight adaptation

// Reversal potentials (mV)
constexpr float E_EXC_MV  = 0.0f;
constexpr float E_INH_MV  = -75.0f;
constexpr float E_BIND_MV = 0.0f;

// Resting and threshold potentials (mV)
constexpr float V_REST_MV    = -70.0f;
constexpr float V_RESET_MV   = -75.0f;
constexpr float THETA_BASE_MV = -55.0f;

// Resistance (MΩ)
constexpr float R_MEGOHM = 1.0f;

// Spike-triggered increments
constexpr float THETA_SPIKE_INCREMENT_MV = 2.0f;  // β in spec
constexpr float SLOW_SPIKE_INCREMENT     = 0.3f;  // α in spec
constexpr float TRACE_SPIKE_INCREMENT    = 0.1f;  // A₊ in spec

// Refractory period (ticks)
constexpr int REFRACTORY_TICKS = 5;

// Oscillation frequency (Hz) - Gamma band
constexpr float GAMMA_FREQ_HZ = 40.0f;
constexpr float TWO_PI        = 6.283185307179586f;

// Decision boundaries for Executive Controller (mV)
constexpr float EC_UPPER_BOUND_MV = -45.0f;
constexpr float EC_LOWER_BOUND_MV = -85.0f;

// Prediction error threshold
constexpr float PREDICTION_ERROR_THRESHOLD = 0.5f;

// =============================================================================
// Neuron Class Type IDs
// =============================================================================
enum class NeuronType : uint8_t {
    CI = 0,  // Core Integrator - Default computation
    AS = 1,  // Attractor Sustainer - Working memory
    PM = 2,  // Precision Modulator - Attentional gain
    PU = 3,  // Prediction Unit - Predictive coding error
    BG = 4,  // Binding Gate - VSA tensor-product binding
    EC = 5   // Executive Controller - Decision making
};

// =============================================================================
// Synapse Class Types (encoded in tag[0] bits [0:2])
// =============================================================================
enum class SynapseType : uint8_t {
    FEEDFORWARD    = 0,  // Standard feedforward connection
    RECURRENT      = 1,  // Recurrent collateral
    TOP_DOWN       = 2,  // Top-down prediction
    LATERAL_INH    = 3,  // Lateral inhibition
    PRECISION_GATE = 4,  // Precision modulation gate
    BINDING_PAIR   = 5   // Multiplicative binding pair
};

// =============================================================================
// Synapse Tag Bit Flags
// =============================================================================
constexpr uint8_t TAG_PLASTICITY_ENABLED_BIT = 3;
constexpr uint8_t TAG_STRUCTURAL_PRUNABLE_BIT = 4;

inline bool is_plasticity_enabled(uint8_t tag_byte) {
    return (tag_byte >> TAG_PLASTICITY_ENABLED_BIT) & 0x01;
}

inline bool is_structural_prunable(uint8_t tag_byte) {
    return (tag_byte >> TAG_STRUCTURAL_PRUNABLE_BIT) & 0x01;
}

// =============================================================================
// Precomputed Decay Factors (for O(1) updates)
// =============================================================================
struct UINConstants {
    float exc_decay;      // exp(-dt/TAU_EXC_MS)
    float inh_decay;      // exp(-dt/TAU_INH_MS)
    float bind_decay;     // exp(-dt/TAU_BIND_MS)
    float membrane_decay; // exp(-dt/TAU_MEMBRANE_MS)
    float slow_decay;     // exp(-dt/TAU_SLOW_MS)
    float theta_decay;    // exp(-dt/TAU_THETA_MS)
    float trace_decay;    // exp(-dt/TAU_TRACE_MS)
    float precision_decay;// exp(-dt/TAU_PRECISION_MS)
    float omega_dt;       // 2π * f * dt (phase rotation)
    
    // Class-specific constants for AS neurons
    float slow_decay_as;  // exp(-dt/500ms) for Attractor Sustainers
    float theta_increment_as; // Reduced θ increment for AS (0.5 mV vs 2.0 mV)
    
    UINConstants(float dt = 1.0f) {
        exc_decay       = std::exp(-dt / TAU_EXC_MS);
        inh_decay       = std::exp(-dt / TAU_INH_MS);
        bind_decay      = std::exp(-dt / TAU_BIND_MS);
        membrane_decay  = std::exp(-dt / TAU_MEMBRANE_MS);
        slow_decay      = std::exp(-dt / TAU_SLOW_MS);
        theta_decay     = std::exp(-dt / TAU_THETA_MS);
        trace_decay     = std::exp(-dt / TAU_TRACE_MS);
        precision_decay = std::exp(-dt / TAU_PRECISION_MS);
        omega_dt        = TWO_PI * GAMMA_FREQ_HZ * dt;
        
        // AS-specific
        slow_decay_as   = std::exp(-dt / 500.0f);
        theta_increment_as = 0.5f;
    }
};

// Global constants instance (initialized once at startup)
extern UINConstants g_uin_constants;

void initialize_uin_constants(float dt = 1.0f);

} // namespace intellectual
} // namespace genesis
