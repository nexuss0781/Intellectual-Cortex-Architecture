#include "../src/learning/learning_controller.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>

namespace {

struct Result { double good_path = 0.0; double restart_good_path = 0.0; uint64_t violations = 0; uint64_t hash = 0; bool hash_match = false; };

uint64_t mix(uint64_t h, uint64_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U); return h * 1099511628211ULL; }
uint64_t hash_state(const genesis::SynapseBlock& s) { uint64_t h = 1469598103934665603ULL; for (float w : s.weights) h = mix(h, static_cast<uint64_t>(std::llround(w * 1000000.0f))); return h; }
int choose(const genesis::SynapseBlock& s, size_t first, bool explore, std::mt19937_64& rng) {
    if (explore && (rng() % 100U) < 20U) return static_cast<int>(rng() % 2U);
    return s.weights[first] >= s.weights[first + 1U] ? 0 : 1;
}

void setup(genesis::NeuronBlock& neurons, genesis::SynapseBlock& synapses) {
    neurons.resize(7);
    for (float& scale : neurons.plasticity_scale) scale = 1.0f;
    synapses.resize(6);
    for (size_t i = 0; i < 6; ++i) { synapses.pre_indices[i] = 0; synapses.post_indices[i] = static_cast<uint32_t>(i + 1U); }
    synapses.weights.assign(6, 0.50f);
    synapses.precision_scale.assign(6, 1.0f);
}

Result run(uint64_t seed, bool learning_enabled, const std::filesystem::path& state_path) {
    genesis::LearningConfig config;
    config.eta = 0.035f; config.trace_decay = 0.98f; config.a_plus = 0.45f; config.a_minus = 0.10f;
    config.reward_coefficient = 1.0f; config.prediction_error_coefficient = 0.0f; config.novelty_coefficient = 0.0f;
    config.task_relevance_coefficient = 0.0f; config.executive_permission_coefficient = 0.0f;
    config.homeostasis_enabled = false; config.structural_enabled = false; config.weight_min = 0.01f; config.weight_max = 1.0f;
    config.learning_enabled = learning_enabled;

    genesis::NeuronBlock neurons; genesis::SynapseBlock synapses; setup(neurons, synapses);
    genesis::LearningController learner(config); learner.initialize(7, 6);
    std::mt19937_64 rng(seed);
    constexpr size_t train_episodes = 6000;
    for (size_t episode = 0; episode < train_episodes; ++episode) {
        const uint64_t start = static_cast<uint64_t>(episode * 30U + 1U);
        const int route = choose(synapses, 0, true, rng);
        const size_t route_sid = static_cast<size_t>(route);
        learner.on_pre_spike(static_cast<uint32_t>(route_sid), synapses, start);
        learner.on_post_spike(static_cast<uint32_t>(route_sid + 1U), synapses, start + 1U);
        const size_t action_base = route == 0 ? 2U : 4U;
        const int action = choose(synapses, action_base, true, rng);
        const size_t action_sid = action_base + static_cast<size_t>(action);
        learner.on_pre_spike(static_cast<uint32_t>(action_sid), synapses, start + 2U);
        learner.on_post_spike(static_cast<uint32_t>(action_sid + 1U), synapses, start + 3U);
        for (uint64_t tick = start + 4U; tick < start + 12U; ++tick) {
            genesis::LearningSignal neutral; neutral.tick = tick; learner.apply_modulation(neutral); learner.update(synapses, neurons, tick);
        }
        genesis::LearningSignal outcome; outcome.reward = (route == 1 && action == 1) ? 1.0f : -0.5f; outcome.tick = start + 12U;
        learner.apply_modulation(outcome); learner.update(synapses, neurons, outcome.tick); learner.reset_traces(synapses);
    }

    const uint64_t before_hash = hash_state(synapses);
    { std::ofstream state(state_path); for (float w : synapses.weights) state << std::setprecision(10) << w << '\n'; }
    genesis::SynapseBlock restored; restored.resize(6); restored.weights.assign(6, 0.0f);
    { std::ifstream state(state_path); for (float& w : restored.weights) state >> w; }
    size_t good = 0, restart_good = 0;
    for (size_t episode = 0; episode < 1000; ++episode) {
        const int route = choose(synapses, 0, false, rng); const size_t action_base = route == 0 ? 2U : 4U; const int action = choose(synapses, action_base, false, rng); good += (route == 1 && action == 1) ? 1U : 0U;
        const int restart_route = choose(restored, 0, false, rng); const size_t restart_base = restart_route == 0 ? 2U : 4U; const int restart_action = choose(restored, restart_base, false, rng); restart_good += (restart_route == 1 && restart_action == 1) ? 1U : 0U;
    }
    Result result; result.good_path = static_cast<double>(good) / 1000.0; result.restart_good_path = static_cast<double>(restart_good) / 1000.0; result.violations = learner.metrics().bound_violations; result.hash = before_hash; result.hash_match = before_hash == hash_state(restored); return result;
}

} // namespace

int main(int argc, char** argv) {
    const uint64_t seed = argc > 1 ? std::stoull(argv[1]) : 424242; const std::filesystem::path dir = argc > 2 ? argv[2] : "artifacts/chain-decision"; std::filesystem::create_directories(dir);
    const Result control = run(seed, false, dir / "control.state"); const Result learned = run(seed, true, dir / "learned.state");
    const bool pass = learned.good_path >= 0.70 && learned.restart_good_path >= 0.70 && control.good_path < 0.70 && learned.violations == 0 && learned.hash_match;
    std::ofstream summary(dir / "summary.txt"); summary << "A4_CHAIN_DECISION=" << (pass ? "PASS" : "FAIL") << '\n' << "control_good_path=" << control.good_path << '\n' << "learned_good_path=" << learned.good_path << '\n' << "restart_good_path=" << learned.restart_good_path << '\n' << "bound_violations=" << learned.violations << '\n' << "restart_hash_match=" << (learned.hash_match ? 1 : 0) << '\n' << "learned_state_hash=" << learned.hash << '\n';
    std::cout << "A4_CHAIN_DECISION=" << (pass ? "PASS" : "FAIL") << '\n' << std::fixed << std::setprecision(4) << "control_good_path=" << control.good_path << " learned_good_path=" << learned.good_path << " restart_good_path=" << learned.restart_good_path << '\n';
    return pass ? 0 : 1;
}
