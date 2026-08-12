#include "../src/learning/learning_controller.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

namespace {

struct RunResult {
    double train_accuracy = 0.0;
    double heldout_accuracy = 0.0;
    double improvement = 0.0;
    float w_a0 = 0.0f;
    float w_a1 = 0.0f;
    float w_b0 = 0.0f;
    float w_b1 = 0.0f;
    uint64_t bound_violations = 0;
    uint64_t state_hash = 0;
    double restart_accuracy = 0.0;
    bool restart_hash_match = false;
};

uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
    return h * 1099511628211ULL;
}

uint64_t state_hash(const genesis::SynapseBlock& s) {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < s.weights.size(); ++i) {
        h = mix(h, static_cast<uint64_t>(std::llround(s.weights[i] * 1000000.0f)));
        h = mix(h, s.eligibility_traces[i] == 0.0f ? 0ULL : 1ULL);
    }
    return h;
}

int choose_action(const genesis::SynapseBlock& s, int context, bool explore, std::mt19937_64& rng) {
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    if (explore && unit(rng) < 0.20) {
        std::uniform_int_distribution<int> action(0, 1);
        return action(rng);
    }
    const size_t first = context == 0 ? 0U : 2U;
    const size_t second = first + 1U;
    return s.weights[first] >= s.weights[second] ? 0 : 1;
}

RunResult run(uint64_t seed, bool learning_enabled, const std::filesystem::path& output) {
    constexpr size_t neurons = 4;
    constexpr size_t synapses = 4;
    genesis::LearningConfig config;
    config.eta = 0.05f;
    config.trace_decay = 0.90f;
    config.a_plus = 0.40f;
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
    config.validate();

    genesis::NeuronBlock neuron_state;
    neuron_state.resize(neurons);
    for (auto& value : neuron_state.plasticity_scale) value = 1.0f;

    genesis::SynapseBlock synapse_state;
    synapse_state.resize(synapses);
    synapse_state.pre_indices = {0, 0, 1, 1};
    synapse_state.post_indices = {2, 3, 2, 3};
    synapse_state.weights = {0.50f, 0.50f, 0.50f, 0.50f};
    synapse_state.precision_scale.assign(synapses, 1.0f);

    genesis::LearningController controller(config);
    controller.initialize(neurons, synapses);
    std::mt19937_64 rng(seed);
    size_t train_correct = 0;
    constexpr size_t train_trials = 4000;

    for (size_t trial = 0; trial < train_trials; ++trial) {
        const int context = static_cast<int>(trial % 2U);
        const int action = choose_action(synapse_state, context, true, rng);
        const size_t sid = (context == 0 ? 0U : 2U) + static_cast<size_t>(action);
        const int target = context;
        const bool correct = action == target;
        train_correct += correct ? 1U : 0U;
        const uint64_t tick = static_cast<uint64_t>(trial * 3U + 1U);
        controller.on_pre_spike(static_cast<uint32_t>(sid), synapse_state, tick);
        controller.on_post_spike(static_cast<uint32_t>(2U + action), synapse_state, tick + 1U);
        genesis::LearningSignal signal;
        signal.reward = correct ? 1.0f : -1.0f;
        signal.tick = tick + 1U;
        controller.apply_modulation(signal);
        controller.update(synapse_state, neuron_state, tick + 1U);
        controller.reset_traces(synapse_state);
    }

    size_t heldout_correct = 0;
    constexpr size_t heldout_trials = 1000;
    for (size_t trial = 0; trial < heldout_trials; ++trial) {
        const int context = static_cast<int>(trial % 2U);
        const int action = choose_action(synapse_state, context, false, rng);
        heldout_correct += action == context ? 1U : 0U;
    }

    RunResult result;
    result.train_accuracy = static_cast<double>(train_correct) / static_cast<double>(train_trials);
    result.heldout_accuracy = static_cast<double>(heldout_correct) / static_cast<double>(heldout_trials);

    const std::filesystem::path state_path = output.string() + ".state";
    {
        std::ofstream state(state_path);
        for (const float weight : synapse_state.weights) state << std::setprecision(10) << weight << '\n';
    }
    genesis::SynapseBlock restored;
    restored.resize(synapses);
    restored.pre_indices = synapse_state.pre_indices;
    restored.post_indices = synapse_state.post_indices;
    restored.weights.assign(synapses, 0.0f);
    {
        std::ifstream state(state_path);
        for (float& weight : restored.weights) state >> weight;
    }
    size_t restart_correct = 0;
    for (size_t trial = 0; trial < heldout_trials; ++trial) {
        const int context = static_cast<int>(trial % 2U);
        const int action = choose_action(restored, context, false, rng);
        restart_correct += action == context ? 1U : 0U;
    }
    result.restart_accuracy = static_cast<double>(restart_correct) / static_cast<double>(heldout_trials);
    result.restart_hash_match = state_hash(restored) == state_hash(synapse_state);
    result.w_a0 = synapse_state.weights[0];
    result.w_a1 = synapse_state.weights[1];
    result.w_b0 = synapse_state.weights[2];
    result.w_b1 = synapse_state.weights[3];
    result.bound_violations = controller.metrics().bound_violations;
    result.state_hash = state_hash(synapse_state);

    std::ofstream csv(output);
    csv << "seed,condition,train_accuracy,heldout_accuracy,restart_accuracy,restart_hash_match,improvement,w_a0,w_a1,w_b0,w_b1,bound_violations,state_hash\n";
    csv << seed << ',' << (learning_enabled ? "learning" : "control") << ','
        << std::setprecision(10) << result.train_accuracy << ',' << result.heldout_accuracy << ','
        << result.restart_accuracy << ',' << (result.restart_hash_match ? 1 : 0) << ',' << result.improvement << ','
        << result.w_a0 << ',' << result.w_a1 << ',' << result.w_b0 << ',' << result.w_b1 << ','
        << result.bound_violations << ',' << result.state_hash << '\n';
    return result;
}

} // namespace

int main(int argc, char** argv) {
    uint64_t seed = 424242;
    std::filesystem::path artifact_dir = "artifacts/experience-decision";
    if (argc > 1) seed = std::stoull(argv[1]);
    if (argc > 2) artifact_dir = argv[2];
    std::filesystem::create_directories(artifact_dir);

    const RunResult control = run(seed, false, artifact_dir / "control.csv");
    const RunResult learning = run(seed, true, artifact_dir / "learning.csv");
    const double improvement = learning.heldout_accuracy - control.heldout_accuracy;
    const bool pass = learning.heldout_accuracy >= 0.70 && improvement >= 0.15 && learning.bound_violations == 0 && learning.restart_accuracy >= 0.70 && learning.restart_hash_match;

    std::ofstream summary(artifact_dir / "summary.txt");
    summary << "A1_EXPERIENCE_DECISION=" << (pass ? "PASS" : "FAIL") << '\n';
    summary << "control_heldout_accuracy=" << control.heldout_accuracy << '\n';
    summary << "learning_heldout_accuracy=" << learning.heldout_accuracy << '\n';
    summary << "improvement=" << improvement << '\n';
    summary << "learning_state_hash=" << learning.state_hash << '\n';
    summary << "restart_accuracy=" << learning.restart_accuracy << '\n';
    summary << "restart_hash_match=" << (learning.restart_hash_match ? 1 : 0) << '\n';
    summary << "bound_violations=" << learning.bound_violations << '\n';

    std::cout << "A1_EXPERIENCE_DECISION=" << (pass ? "PASS" : "FAIL") << '\n';
    std::cout << std::fixed << std::setprecision(4)
              << "control_heldout=" << control.heldout_accuracy
              << " learning_heldout=" << learning.heldout_accuracy
              << " restart_heldout=" << learning.restart_accuracy
              << " improvement=" << improvement << '\n';
    std::cout << "weights learning=[" << learning.w_a0 << ',' << learning.w_a1 << ','
              << learning.w_b0 << ',' << learning.w_b1 << "]\n";
    return pass ? 0 : 1;
}
