#include "../src/learning/learning_controller.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

namespace {

struct Result {
    double train_accuracy = 0.0;
    double heldout_action1 = 0.0;
    double restart_action1 = 0.0;
    float action0_weight = 0.0f;
    float action1_weight = 0.0f;
    uint64_t violations = 0;
    uint64_t hash = 0;
    bool restart_hash_match = false;
};

uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
    return h * 1099511628211ULL;
}

uint64_t hash_state(const genesis::SynapseBlock& s) {
    uint64_t h = 1469598103934665603ULL;
    for (float weight : s.weights) h = mix(h, static_cast<uint64_t>(std::llround(weight * 1000000.0f)));
    return h;
}

int choose_action(const genesis::SynapseBlock& s, bool explore, std::mt19937_64& rng) {
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    if (explore && unit(rng) < 0.20) return static_cast<int>(rng() % 2U);
    return s.weights[0] >= s.weights[1] ? 0 : 1;
}

Result run(uint64_t seed, int delay, bool learning_enabled, bool shuffled, const std::filesystem::path& csv_path) {
    constexpr size_t neurons = 3;
    constexpr size_t synapses = 2;
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
    config.learning_enabled = learning_enabled;

    genesis::NeuronBlock neurons_state;
    neurons_state.resize(neurons);
    for (float& scale : neurons_state.plasticity_scale) scale = 1.0f;
    genesis::SynapseBlock synapses_state;
    synapses_state.resize(synapses);
    synapses_state.pre_indices = {0, 0};
    synapses_state.post_indices = {1, 2};
    synapses_state.weights = {0.50f, 0.50f};
    synapses_state.precision_scale.assign(synapses, 1.0f);

    genesis::LearningController controller(config);
    controller.initialize(neurons, synapses);
    std::mt19937_64 rng(seed);
    size_t train_good = 0;
    constexpr size_t episodes = 5000;

    for (size_t episode = 0; episode < episodes; ++episode) {
        const int action = choose_action(synapses_state, true, rng);
        const uint64_t start = static_cast<uint64_t>(episode * 20U + 1U);
        controller.on_pre_spike(static_cast<uint32_t>(action), synapses_state, start);
        controller.on_post_spike(static_cast<uint32_t>(1U + action), synapses_state, start + 1U);
        for (int tick = 1; tick < delay; ++tick) {
            genesis::LearningSignal neutral;
            neutral.tick = start + static_cast<uint64_t>(tick);
            controller.apply_modulation(neutral);
            controller.update(synapses_state, neurons_state, neutral.tick);
        }
        bool long_term_good = action == 1;
        float outcome = action == 0 ? -0.6f : 1.0f;
        if (shuffled) outcome = (rng() % 2U == 0U) ? 1.0f : -1.0f;
        genesis::LearningSignal signal;
        signal.reward = outcome;
        signal.prediction_error = 0.0f;
        signal.tick = start + static_cast<uint64_t>(delay);
        controller.apply_modulation(signal);
        controller.update(synapses_state, neurons_state, signal.tick);
        controller.reset_traces(synapses_state);
        train_good += long_term_good ? 1U : 0U;
    }

    size_t action1_count = 0;
    constexpr size_t heldout = 1000;
    for (size_t episode = 0; episode < heldout; ++episode) action1_count += choose_action(synapses_state, false, rng) == 1 ? 1U : 0U;

    const std::filesystem::path state_path = csv_path.string() + ".state";
    {
        std::ofstream state(state_path);
        state << std::setprecision(10) << synapses_state.weights[0] << '\n' << synapses_state.weights[1] << '\n';
    }
    genesis::SynapseBlock restored;
    restored.resize(synapses);
    restored.weights.assign(synapses, 0.0f);
    {
        std::ifstream state(state_path);
        state >> restored.weights[0] >> restored.weights[1];
    }
    size_t restart_action1 = 0;
    for (size_t episode = 0; episode < heldout; ++episode) restart_action1 += choose_action(restored, false, rng) == 1 ? 1U : 0U;

    Result result;
    result.train_accuracy = static_cast<double>(train_good) / static_cast<double>(episodes);
    result.heldout_action1 = static_cast<double>(action1_count) / static_cast<double>(heldout);
    result.restart_action1 = static_cast<double>(restart_action1) / static_cast<double>(heldout);
    result.action0_weight = synapses_state.weights[0];
    result.action1_weight = synapses_state.weights[1];
    result.violations = controller.metrics().bound_violations;
    result.hash = hash_state(synapses_state);
    result.restart_hash_match = hash_state(restored) == result.hash;

    std::ofstream csv(csv_path);
    csv << "seed,delay,learning,shuffled,train_long_term_rate,heldout_action1,restart_action1,restart_hash_match,action0_weight,action1_weight,bound_violations,state_hash\n";
    csv << seed << ',' << delay << ',' << (learning_enabled ? 1 : 0) << ',' << (shuffled ? 1 : 0) << ','
        << result.train_accuracy << ',' << result.heldout_action1 << ',' << result.restart_action1 << ','
        << (result.restart_hash_match ? 1 : 0) << ',' << result.action0_weight << ',' << result.action1_weight << ','
        << result.violations << ',' << result.hash << '\n';
    return result;
}

} // namespace

int main(int argc, char** argv) {
    uint64_t seed = argc > 1 ? std::stoull(argv[1]) : 424242;
    std::filesystem::path dir = argc > 2 ? argv[2] : "artifacts/delayed-decision";
    std::filesystem::create_directories(dir);

    const Result control = run(seed, 5, false, false, dir / "control.csv");
    const Result immediate = run(seed, 1, true, false, dir / "immediate.csv");
    const Result delayed = run(seed, 5, true, false, dir / "delayed.csv");
    const Result shuffled = run(seed, 5, true, true, dir / "shuffled.csv");
    const bool pass = delayed.heldout_action1 >= 0.70 && delayed.restart_action1 >= 0.70 &&
                      control.heldout_action1 < 0.70 && shuffled.heldout_action1 < 0.70 &&
                      delayed.violations == 0 && delayed.restart_hash_match;

    std::ofstream summary(dir / "summary.txt");
    summary << "A2_DELAYED_DECISION=" << (pass ? "PASS" : "FAIL") << '\n';
    summary << "control_action1=" << control.heldout_action1 << '\n';
    summary << "immediate_action1=" << immediate.heldout_action1 << '\n';
    summary << "delayed_action1=" << delayed.heldout_action1 << '\n';
    summary << "delayed_restart_action1=" << delayed.restart_action1 << '\n';
    summary << "shuffled_action1=" << shuffled.heldout_action1 << '\n';
    summary << "delayed_bound_violations=" << delayed.violations << '\n';
    summary << "delayed_restart_hash_match=" << (delayed.restart_hash_match ? 1 : 0) << '\n';

    std::cout << "A2_DELAYED_DECISION=" << (pass ? "PASS" : "FAIL") << '\n';
    std::cout << std::fixed << std::setprecision(4)
              << "control_action1=" << control.heldout_action1
              << " immediate_action1=" << immediate.heldout_action1
              << " delayed_action1=" << delayed.heldout_action1
              << " shuffled_action1=" << shuffled.heldout_action1 << '\n';
    return pass ? 0 : 1;
}
