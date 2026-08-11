#pragma once

#include <cstdint>

namespace genesis {

// Versioned meaning contract for fields that are shared by the legacy substrate
// and the Universal Intellectual Neuron overlay. This is intentionally small
// and stored once per NeuronBlock rather than once per neuron.
struct StateSchema {
    static constexpr uint32_t CURRENT_VERSION = 1;

    enum class FieldMeaning : uint8_t {
        ENERGY = 0,
        THETA_DYN = 1,
        RECOVERY = 2,
        PHASE = 3,
        RATE = 4,
        S_SLOW = 5
    };

    uint32_t version = CURRENT_VERSION;
    bool uses_uin_overlay = false;
    FieldMeaning atp_meaning = FieldMeaning::ENERGY;
    FieldMeaning recovery_meaning = FieldMeaning::RECOVERY;
    FieldMeaning rate_meaning = FieldMeaning::RATE;

    static StateSchema legacy() {
        return StateSchema{};
    }

    static StateSchema uin_overlay() {
        StateSchema schema;
        schema.uses_uin_overlay = true;
        schema.atp_meaning = FieldMeaning::THETA_DYN;
        schema.recovery_meaning = FieldMeaning::PHASE;
        schema.rate_meaning = FieldMeaning::S_SLOW;
        return schema;
    }

    bool compatible_with(const StateSchema& expected) const {
        return version == expected.version &&
               uses_uin_overlay == expected.uses_uin_overlay &&
               atp_meaning == expected.atp_meaning &&
               recovery_meaning == expected.recovery_meaning &&
               rate_meaning == expected.rate_meaning;
    }
};

} // namespace genesis
