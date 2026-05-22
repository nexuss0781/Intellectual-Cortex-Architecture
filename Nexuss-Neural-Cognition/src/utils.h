#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace genesis {
namespace utils {

// -----------------------------------------------------------------------------
// 1. Numerical Stability & Safety Tools
// -----------------------------------------------------------------------------

// Ensures a float is valid. If NaN or Inf, resets to a safe fallback (usually 0 or Rest Potential).
// This prevents "poisoning" the network if one neuron explodes.
inline float sanitize(float value, float fallback = 0.0f) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return value;
}

// Hard clamping wrapper using C++17 std::clamp.
// Critical for keeping Weights [W_MIN, W_MAX] and ATP [0, 1].
inline float clip(float n, float lower, float upper) {
    return std::clamp(n, lower, upper);
}

// -----------------------------------------------------------------------------
// 2. Deterministic Random Number Generator (Xorshift32)
// -----------------------------------------------------------------------------
// We need a strictly deterministic RNG to ensure that if we run the simulation 
// twice with the same seed, we get the exact same evolutionary result.
// Standard std::rand is not guaranteed to be consistent across platforms/compilers.
// Xorshift32 is chosen for maximum speed with sufficient statistical quality for physics noise.
struct FastRNG {
    uint32_t state;

    explicit FastRNG(uint32_t seed) {
        // Avoid 0 seed, which locks Xorshift
        state = (seed == 0) ? 0xDEADBEEF : seed;
    }

    // Generates a float in range [0.0, 1.0]
    // Uses bit masking for speed instead of expensive division by RAND_MAX
    float next_float() {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        
        // Mask to 23 bits (mantissa of float) and normalize
        // 0x007FFFFF is the mantissa mask for IEEE 754 float
        // Dividing by 8388608.0f (2^23) normalizes to [0,1]
        // This acts as a standard uniform distribution approximation
        constexpr float INV_MANTISSA = 1.0f / 8388608.0f;
        return (x & 0x007FFFFF) * INV_MANTISSA;
    }

    // Generates integer in range [min, max]
    int next_int(int min, int max) {
        if (min >= max) return min;
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        
        // Simple modulo is acceptable for biological connectivity noise
        return min + (x % (max - min + 1));
    }
};

} // namespace utils
} // namespace genesis