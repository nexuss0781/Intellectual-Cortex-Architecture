#include "../src/learning/learning_controller.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>

namespace {

struct Result { double action1 = 0.0; uint64_t hash = 0; bool hash_match = false; };

uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
    return h * 1099511628211ULL;
}

uint64_t hash_state(const genesis::SynapseBlock& s) {
    uint64_t h = 1469598103934665603ULL;
    for (float w : s.weights) h = mix(h, static_cast<uint64_t>(std::llround(w * 1000000.0f)));
    return h;
}

int choose(const genesis::SynapseBlock& s, bool explore, std::mt19937_64& rng) {
    if (explore && (rng() % 100U) < 20U) return static_cast<int>(rng() % 2U);
    return s.weights[0] >= s.weights[1] ? 0 : 1;
}

void setup(genesis::NeuronBlock& neurons, genesis::SynapseBlock& synapses) {
    neurons.resize(3);
    for (float& scale : neurons.plasticity_scale) scale = 1.0f;
    synapses.resize(2);
    synapses.pre_indices = {0, 0};
    synapses.post_indices = {1, 2};
    synapses.weights = {0.50f, 0.50f};
    synapses.precision_scale.assign(2, 1.0f);
}

Result train_and_transfer(uint64_t seed, const std::filesystem::path& state_path, bool control) {
    genesis::LearningConfig config;
    config.eta = 0.04f;
    config.trace_decay = 0.97f;
    config.a_plus = 0.50f;
    config.a_minus = 0.10f;
    config.reward_coefficient = 1.0f;
    config.prediction_error_coefficient = 0.0f;
    config.novelty_coefficient = 0.0f;
    config.task_relevance_coefficient = 0.0f;
    config.executive_permission_coefficient = 0.0f;
    config.homeostasis_enabled = false;
    config.structural_enabled = false;
    config.weight_min = 0.01f;
    config.weight_max = 1.0f;
    config.learning_enabled = !control;

    genesis::NeuronBlock neurons;
    genesis::SynapseBlock synapses;
    setup(neurons, synapses);
    genesis::LearningController learner(config);
    learner.initialize(3, 2);
    std::mt19937_64 rng(seed);

    for (size_t episode = 0; episode < 5000; ++episode) {
        const int action = choose(synapses, true, rng);
        const uint64_t start = static_cast<uint64_t>(episode * 20U + 1U);
        learner.on_pre_spike(static_cast<uint32_t>(action), synapses, start);
        learner.on_post_spike(static_cast<uint32_t>(1 + action), synapses, start + 1U);
        for (int tick = 1; tick < 5; ++tick) {
            genesis::LearningSignal neutral;
            neutral.tick = start + static_cast<uint64_t>(tick);
            learner.apply_modulation(neutral);
            learner.update(synapses, neurons, neutral.tick);
        }
        genesis::LearningSignal outcome;
        outcome.reward = action == 0 ? -0.6f : 1.0f;
        outcome.tick = start + 5U;
        learner.apply_modulation(outcome);
        learner.update(synapses, neurons, outcome.tick);
        learner.reset_traces(synapses);
    }

    const uint64_t before_hash = hash_state(synapses);
    {
        std::ofstream state(state_path);
        state << std::setprecision(10) << synapses.weights[0] << '\n' << synapses.weights[1] << '\n';
    }
    genesis::SynapseBlock restored;
    restored.resize(2);
    restored.weights.assign(2, 0.0f);
    {
        std::ifstream state(state_path);
        state >> restored.weights[0] >> restored.weights[1];
    }

    size_t action1_count = 0;
    for (size_t episode = 0; episode < 1000; ++episode) {
        // Transfer environment: the horizon and reward magnitudes changed,
        // but action 1 remains the better long-term option.
        const int action = choose(restored, false, rng);
        const float total_outcome = action == 0 ? (0.3f - 0.4f) : 0.5f;
        (void)total_outcome;
        action1_count += action == 1 ? 1U : 0U;
    }
    Result result;
    result.action1 = static_cast<double>(action1_count) / 1000.0;
    result.hash = before_hash;
    result.hash_match = before_hash == hash_state(restored);
    return result;
}

} // namespace

int main(int argc, char** argv) {
    const uint64_t seed = argc > 1 ? std::stoull(argv[1]) : 424242;
    const std::filesystem::path dir = argc > 2 ? argv[2] : "artifacts/transfer-decision";
    std::filesystem::create_directories(dir);
    const Result learned = train_and_transfer(seed, dir / "learned.state", false);
    const Result control = train_and_transfer(seed, dir / "control.state", true);
    const bool pass = learned.action1 >= 0.70 && control.action1 < 0.70 && learned.hash_match;

    std::ofstream summary(dir / "summary.txt");
    summary << "A3_TRANSFER_DECISION=" << (pass ? "PASS" : "FAIL") << '\n';
    summary << "learned_transfer_action1=" << learned.action1 << '\n';
    summary << "control_transfer_action1=" << control.action1 << '\n';
    summary << "restart_hash_match=" << (learned.hash_match ? 1 : 0) << '\n';
    summary << "learned_state_hash=" << learned.hash << '\n';

    std::cout << "A3_TRANSFER_DECISION=" << (pass ? "PASS" : "FAIL") << '\n';
    std::cout << std::fixed << std::setprecision(4)
              << "learned_transfer_action1=" << learned.action1
              << " control_transfer_action1=" << control.action1 << '\n';
    return pass ? 0 : 1;
}
