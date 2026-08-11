#include "learning/learning_controller.h"
#include "learning/sparse_codebook.h"
#include "types.h"
#include "constants.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>

namespace fs = std::filesystem;
using namespace genesis;

namespace {

struct Result {
    std::string id;
    bool passed;
    double value;
    std::string detail;
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

uint64_t hash_bytes(uint64_t hash, const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

template <typename T>
uint64_t hash_value(uint64_t hash, const T& value) {
    return hash_bytes(hash, &value, sizeof(T));
}

size_t rss_kb() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream input(line.substr(6));
            size_t value = 0;
            input >> value;
            return value;
        }
    }
    return 0;
}

struct TransitionModel {
    LearningController controller;
    SynapseBlock synapses;
    NeuronBlock neurons;
    uint64_t tick = 0;
    uint32_t pre_count = 2;
    uint32_t post_count = 2;

    explicit TransitionModel(const LearningConfig& config, float initial_weight = 0.05f)
        : controller(config) {
        neurons.resize(pre_count);
        neurons.plasticity_scale.assign(pre_count, 1.0f);
        synapses.resize(4);
        // Two candidate postsynaptic outcomes for each presynaptic context.
        synapses.pre_indices = {0, 0, 1, 1};
        synapses.post_indices = {0, 1, 0, 1};
        synapses.weights.assign(4, initial_weight);
        synapses.eligibility_traces.assign(4, 0.0f);
        synapses.synapse_flags.assign(4, 0);
        controller.initialize(pre_count, synapses.weights.size());
    }

    void event(uint32_t pre, uint32_t post, float reward, bool learning = true) {
        LearningSignal signal;
        signal.reward = reward;
        signal.task_relevance = (reward != 0.0f && learning) ? 1.0f : 0.0f;
        signal.executive_permission = (reward != 0.0f && learning) ? 1.0f : 0.0f;
        signal.tick = tick;
        controller.apply_modulation(signal);
        for (size_t sid = 0; sid < synapses.weights.size(); ++sid) {
            if (synapses.pre_indices[sid] == pre) controller.on_pre_spike(static_cast<uint32_t>(sid), synapses, tick);
        }
        controller.on_post_spike(post, synapses, tick + 1);
        controller.update(synapses, neurons, tick + 1);
        ++tick;
    }

    void idle(int ticks = 25) {
        LearningSignal neutral;
        neutral.tick = tick;
        controller.apply_modulation(neutral);
        for (int i = 0; i < ticks; ++i) {
            controller.update(synapses, neurons, ++tick);
        }
        controller.reset_traces(synapses);
    }

    float probability(uint32_t pre, uint32_t post) const {
        float total = 0.0f;
        float target = 0.0f;
        for (size_t sid = 0; sid < synapses.weights.size(); ++sid) {
            if (synapses.pre_indices[sid] != pre || controller.is_synapse_disabled(synapses, sid)) continue;
            total += synapses.weights[sid];
            if (synapses.post_indices[sid] == post) target += synapses.weights[sid];
        }
        return total > 0.0f ? target / total : 0.0f;
    }

    uint64_t hash() const {
        uint64_t result = 1469598103934665603ULL;
        result = hash_value(result, tick);
        for (float weight : synapses.weights) result = hash_value(result, weight);
        for (float trace : synapses.eligibility_traces) result = hash_value(result, trace);
        return result;
    }
};

std::vector<uint8_t> pattern(size_t class_id, size_t width = 64) {
    std::vector<uint8_t> result(width, 0);
    for (size_t i = 0; i < 8; ++i) result[(class_id * 17 + i * 5) % width] = 1;
    return result;
}

void flip_noise(std::vector<uint8_t>& value, uint64_t seed, size_t flips) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> index(0, value.size() - 1);
    for (size_t i = 0; i < flips; ++i) {
        const size_t bit = index(rng);
        value[bit] = static_cast<uint8_t>(!value[bit]);
    }
}

double run_ablation_selectivity(LearningConfig config, bool reward_selective) {
    TransitionModel model(config);
    for (int i = 0; i < 2000; ++i) {
        model.event(0, 0, 1.0f);
        model.idle();
        model.event(1, 1, reward_selective ? 0.0f : 1.0f);
        model.idle();
    }
    return static_cast<double>(model.probability(0, 0) - model.probability(1, 1)) * 100.0;
}

uint64_t run_deterministic_scenario(uint64_t seed) {
    LearningConfig config;
    config.eta = 0.01f;
    config.homeostasis_enabled = false;
    TransitionModel model(config);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> transition(0, 1);
    for (int i = 0; i < 10000; ++i) {
        const uint32_t pre = static_cast<uint32_t>(transition(rng));
        const uint32_t post = (pre == 0) ? 1U : 0U;
        model.event(pre, post, pre == 0 ? 1.0f : 0.0f);
    }
    return model.hash();
}

int parse_int_arg(int argc, char** argv, const std::string& name, int fallback) {
    for (int i = 1; i + 1 < argc; ++i) if (argv[i] == name) return std::stoi(argv[i + 1]);
    return fallback;
}

std::string parse_string_arg(int argc, char** argv, const std::string& name, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) if (argv[i] == name) return argv[i + 1];
    return fallback;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const uint64_t seed = static_cast<uint64_t>(std::stoull(parse_string_arg(argc, argv, "--seed", "424242")));
        const fs::path artifact_dir = parse_string_arg(argc, argv, "--artifact-dir", "artifacts/stage-1");
        fs::create_directories(artifact_dir);
        std::vector<Result> results;

        auto run = [&results](const std::string& id, const auto& fn) {
            try {
                const double value = fn();
                results.push_back({id, true, value, "ok"});
            } catch (const std::exception& ex) {
                results.push_back({id, false, 0.0, ex.what()});
            }
        };

        run("L1-UNIT-00", []() {
            LearningConfig invalid;
            invalid.weight_min = 1.0f;
            invalid.weight_max = 0.0f;
            bool caught = false;
            try { LearningController controller(invalid); } catch (const std::invalid_argument&) { caught = true; }
            require(caught, "invalid LearningConfig was accepted");
            return 1.0;
        });

        run("L1-UNIT-01", []() {
            LearningConfig config;
            config.homeostasis_enabled = false;
            TransitionModel causal(config);
            causal.event(0, 1, 1.0f);
            const float causal_trace = causal.synapses.eligibility_traces[1];
            TransitionModel anti(config);
            anti.controller.on_post_spike(1, 1);
            anti.controller.apply_modulation(LearningSignal{});
            anti.controller.on_pre_spike(1, anti.synapses, 2);
            anti.controller.update(anti.synapses, anti.neurons, 2);
            const float anti_trace = anti.synapses.eligibility_traces[1];
            require(causal_trace > 0.0f, "causal trace was not positive");
            require(anti_trace <= 0.0f, "anti-causal trace was positive");
            require(causal_trace >= std::abs(anti_trace) * 5.0f, "causal update did not exceed anti-causal update by 5x");
            return static_cast<double>(causal_trace);
        });

        run("L1-UNIT-02", []() {
            LearningConfig config;
            config.homeostasis_enabled = false;
            TransitionModel model(config);
            model.controller.on_post_spike(1, 1);
            model.controller.apply_modulation(LearningSignal{});
            model.controller.on_pre_spike(1, model.synapses, 2);
            model.controller.update(model.synapses, model.neurons, 2);
            require(model.synapses.eligibility_traces[1] <= 0.0f, "anti-causal-only stream produced positive trace");
            return static_cast<double>(model.synapses.eligibility_traces[1]);
        });

        run("L1-UNIT-03", []() {
            LearningConfig config;
            config.homeostasis_enabled = false;
            config.learning_enabled = false;
            TransitionModel model(config);
            model.synapses.eligibility_traces[0] = 1.0f;
            double worst = 0.0;
            for (int tick = 1; tick <= 1000; ++tick) {
                model.controller.update(model.synapses, model.neurons, tick);
                const double expected = std::pow(static_cast<double>(config.trace_decay), tick);
                const double error = std::abs(model.synapses.eligibility_traces[0] - expected);
                worst = std::max(worst, error);
                require(error <= 1e-5 * std::max(1.0, expected), "eligibility decay mismatch");
            }
            return worst;
        });

        run("L1-UNIT-04", []() {
            LearningConfig config;
            config.homeostasis_enabled = false;
            TransitionModel model(config);
            const auto before = model.synapses.weights;
            for (int i = 0; i < 100; ++i) model.event(0, 1, 0.0f);
            for (size_t i = 0; i < before.size(); ++i) require(model.synapses.weights[i] == before[i], "neutral modulation changed a weight");
            return 1.0;
        });

        run("L1-UNIT-05", []() {
            LearningConfig config;
            config.eta = 0.02f;
            config.homeostasis_enabled = false;
            TransitionModel full(config);
            TransitionModel plain(config);
            for (int i = 0; i < 10000; ++i) {
                full.event(0, 0, 1.0f);
                full.idle();
                full.event(1, 1, 0.0f);
                full.idle();
                plain.event(0, 0, 1.0f);
                plain.idle();
                plain.event(1, 1, 1.0f);
                plain.idle();
            }
            const float full_selectivity = full.probability(0, 0) - full.probability(1, 1);
            const float plain_selectivity = plain.probability(0, 0) - plain.probability(1, 1);
            require(full_selectivity * 100.0f >= 20.0f, "rewarded transition did not separate by 20 percentage points: full_p00=" + std::to_string(full.probability(0, 0)) + " full_p11=" + std::to_string(full.probability(1, 1)) + " plain_p00=" + std::to_string(plain.probability(0, 0)) + " plain_p11=" + std::to_string(plain.probability(1, 1)));
            require(full_selectivity > plain_selectivity + 0.10f, "reward modulation did not beat plain STDP: full=" + std::to_string(full_selectivity) + " plain=" + std::to_string(plain_selectivity));
            return static_cast<double>((full_selectivity - plain_selectivity) * 100.0f);
        });

        run("L1-UNIT-06", [seed]() {
            LearningConfig config;
            config.eta = 0.2f;
            config.homeostasis_enabled = true;
            TransitionModel model(config);
            std::mt19937_64 rng(seed);
            std::uniform_real_distribution<float> reward(-1.0f, 1.0f);
            for (int i = 0; i < 20000; ++i) {
                model.event(static_cast<uint32_t>(i % 2), static_cast<uint32_t>((i + 1) % 2), reward(rng));
                model.controller.observe_population_spikes(static_cast<size_t>(i % 3), 2);
            }
            const auto& metrics = model.controller.metrics();
            for (float weight : model.synapses.weights) require(std::isfinite(weight) && weight >= config.weight_min && weight <= config.weight_max, "weight escaped bounds");
            require(std::isfinite(model.controller.homeostatic_gain()), "homeostatic gain became non-finite");
            require(metrics.bound_violations == 0, "controller reported bound violations");
            return static_cast<double>(metrics.clipped_updates);
        });

        run("L1-UNIT-07", []() {
            LearningConfig config;
            config.target_rate_hz = 50.0f;
            config.homeostatic_rate = 0.05f;
            LearningController controller(config);
            controller.initialize(100, 1);
            for (int tick = 0; tick < 600; ++tick) {
                const size_t fired = static_cast<size_t>(std::lround(100.0 * controller.homeostatic_gain() * 0.05));
                controller.observe_population_spikes(std::min<size_t>(100, fired), 100);
            }
            const float rate = controller.metrics().mean_firing_rate_hz;
            require(rate >= 37.5f && rate <= 62.5f, "homeostatic rate did not stabilize within +/-25%");
            return rate;
        });

        run("L1-UNIT-08", []() {
            LearningConfig config;
            config.homeostasis_enabled = false;
            config.learning_enabled = false;
            config.prune_patience_ticks = 5;
            config.prune_threshold = 0.001f;
            TransitionModel model(config, 0.0001f);
            model.synapses.synapse_flags.assign(4, LEARNING_SYNAPSE_FLAG_PRUNABLE);
            for (int tick = 0; tick < 4; ++tick) {
                model.controller.update(model.synapses, model.neurons, tick);
                require(model.controller.structural_proposals().empty(), "synapse proposed before patience interval");
            }
            model.controller.update(model.synapses, model.neurons, 4);
            require(model.controller.structural_proposals().size() == 4, "all qualifying synapses did not produce proposals");
            const uint32_t old_pre = model.synapses.pre_indices[0];
            const uint32_t old_post = model.synapses.post_indices[0];
            const size_t applied = model.controller.apply_structural_proposals(model.synapses);
            require(applied == 4, "structural proposals were not applied");
            require(model.synapses.pre_indices[0] == old_pre && model.synapses.post_indices[0] == old_post, "stable synapse IDs changed");
            require(model.controller.is_synapse_disabled(model.synapses, 0), "pruned synapse was not disabled");
            return static_cast<double>(applied);
        });

        run("L1-UNIT-09", []() {
            LearningConfig config;
            config.normalization_enabled = true;
            config.normalization_target = 1.0f;
            config.homeostasis_enabled = false;
            TransitionModel model(config, 0.8f);
            model.event(0, 0, 1.0f);
            float total = model.synapses.weights[0] + model.synapses.weights[1];
            require(total <= 1.00001f, "normalization target was exceeded");
            return static_cast<double>(total);
        });

        run("L1-INT-01", []() {
            LearningConfig config;
            config.eta = 0.02f;
            config.homeostasis_enabled = false;
            TransitionModel model(config);
            for (int i = 0; i < 5000; ++i) {
                model.event(0, 1, 1.0f);
                model.idle();
                if (i % 3 == 0) {
                    model.event(1, 0, 0.0f);
                    model.idle();
                }
            }
            require(model.probability(0, 1) >= 0.75f, "held-out temporal association accuracy below 75%: p_target=" + std::to_string(model.probability(0, 1)) + " p_false=" + std::to_string(model.probability(0, 0)) + " w00=" + std::to_string(model.synapses.weights[0]) + " w01=" + std::to_string(model.synapses.weights[1]));
            require(model.probability(0, 0) <= 0.20f, "false prediction rate above 20%: p_false=" + std::to_string(model.probability(0, 0)));
            return static_cast<double>(model.probability(0, 1));
        });

        run("L1-INT-02", []() {
            LearningConfig config;
            config.eta = 0.02f;
            config.homeostasis_enabled = false;
            TransitionModel model(config);
            for (int i = 0; i < 5000; ++i) {
                model.event(0, 0, 1.0f);
                model.idle();
            }
            const float before = model.probability(1, 1);
            model.idle(50);
            for (int i = 0; i < 5000; ++i) {
                model.event(1, 1, 1.0f);
                model.idle();
            }
            const float after = model.probability(1, 1);
            require(after >= 0.70f, "new distribution did not reach 70% within 5000 events: before=" + std::to_string(before) + " after=" + std::to_string(after));
            require(before < 0.70f, "nonstationary baseline was already solved: before=" + std::to_string(before));
            return static_cast<double>(after);
        });

        run("L1-INT-03", []() {
            LearningConfig config;
            config.eta = 0.02f;
            config.homeostasis_enabled = false;
            TransitionModel model(config);
            for (int i = 0; i < 5000; ++i) {
                model.event(0, 0, 1.0f);
                model.idle();
            }
            const float pre_switch = model.probability(0, 0);
            model.idle(50);
            for (int i = 0; i < 5000; ++i) {
                model.event(1, 1, 1.0f);
                model.idle();
            }
            const float post_switch = model.probability(0, 0);
            require(post_switch >= 0.80f * pre_switch, "old distribution was forgotten after a disjoint switch: pre=" + std::to_string(pre_switch) + " post=" + std::to_string(post_switch));
            return static_cast<double>(post_switch / pre_switch);
        });

        run("L1-INT-04", [seed]() {
            SparseCodebook codebook(4, 64, 8);
            for (size_t class_id = 0; class_id < 4; ++class_id) {
                for (int repeat = 0; repeat < 100; ++repeat) {
                    auto sample = pattern(class_id);
                    flip_noise(sample, seed + class_id * 1000 + repeat, 2);
                    codebook.learn(class_id, sample);
                }
            }
            size_t correct = 0;
            for (size_t class_id = 0; class_id < 4; ++class_id) {
                for (int repeat = 0; repeat < 100; ++repeat) {
                    auto sample = pattern(class_id);
                    flip_noise(sample, seed + class_id * 1000 + repeat + 10000, 2);
                    if (codebook.decode(sample) == class_id) ++correct;
                }
            }
            const double accuracy = static_cast<double>(correct) / 400.0;
            require(accuracy >= 0.85, "sparse noisy-pattern separation below 85%");
            return accuracy;
        });

        run("L1-OPS-01", [seed]() {
            const uint64_t a = run_deterministic_scenario(seed);
            const uint64_t b = run_deterministic_scenario(seed);
            const uint64_t c = run_deterministic_scenario(seed);
            require(a == b && b == c, "same-seed Stage 1 results differ");
            return static_cast<double>(a);
        });

        run("L1-OPS-02", []() {
            constexpr size_t synapse_count = 100000;
            LearningConfig maintenance_config;
            maintenance_config.learning_enabled = false;
            maintenance_config.homeostasis_enabled = false;
            maintenance_config.structural_enabled = false;
            LearningConfig learning_config = maintenance_config;
            learning_config.learning_enabled = true;

            SynapseBlock maintenance_synapses;
            SynapseBlock learning_synapses;
            maintenance_synapses.resize(synapse_count);
            learning_synapses.resize(synapse_count);
            for (size_t sid = 0; sid < synapse_count; ++sid) {
                maintenance_synapses.pre_indices[sid] = 0;
                maintenance_synapses.post_indices[sid] = 1;
                maintenance_synapses.weights[sid] = 0.05f;
                maintenance_synapses.eligibility_traces[sid] = 0.1f;
                learning_synapses.pre_indices[sid] = 0;
                learning_synapses.post_indices[sid] = 1;
                learning_synapses.weights[sid] = 0.05f;
                learning_synapses.eligibility_traces[sid] = 0.1f;
            }
            NeuronBlock maintenance_neurons;
            NeuronBlock learning_neurons;
            maintenance_neurons.resize(2);
            learning_neurons.resize(2);
            LearningController maintenance(maintenance_config);
            LearningController learning(learning_config);
            maintenance.initialize(2, synapse_count);
            learning.initialize(2, synapse_count);
            for (size_t sid = 0; sid < synapse_count; ++sid) learning.on_pre_spike(static_cast<uint32_t>(sid), learning_synapses, 0);
            learning.on_post_spike(1, learning_synapses, 1);
            LearningSignal reward;
            reward.reward = 1.0f;
            learning.apply_modulation(reward);

            const auto learning_start = std::chrono::steady_clock::now();
            for (int tick = 0; tick < 100; ++tick) learning.update(learning_synapses, learning_neurons, tick);
            const auto learning_end = std::chrono::steady_clock::now();
            const double learning_ms = std::chrono::duration<double, std::milli>(learning_end - learning_start).count();

            const auto maintenance_start = std::chrono::steady_clock::now();
            for (int tick = 0; tick < 100; ++tick) maintenance.update(maintenance_synapses, maintenance_neurons, tick);
            const auto maintenance_end = std::chrono::steady_clock::now();
            const double maintenance_ms = std::chrono::duration<double, std::milli>(maintenance_end - maintenance_start).count();
            const double ratio = learning_ms / std::max(1e-9, maintenance_ms);
            // This wall-clock gate is intentionally tolerant of sequential workflow load and
            // scheduler variance; it still rejects a pathological >3x learning overhead.
            constexpr double max_learning_overhead_ratio = 3.0;
            require(maintenance_ms > 0.0 && ratio <= max_learning_overhead_ratio, "learning overhead exceeded 3.0x maintenance: learning_ms=" + std::to_string(learning_ms) + " maintenance_ms=" + std::to_string(maintenance_ms) + " ratio=" + std::to_string(ratio));
            return ratio;
        });

        run("L1-OPS-03", []() {
            LearningConfig config;
            config.homeostasis_enabled = false;
            config.structural_enabled = false;
            LearningController controller(config);
            const size_t before = rss_kb();
            controller.initialize(270000, 1000000);
            SynapseBlock synapses;
            NeuronBlock neurons;
            synapses.resize(1000000);
            neurons.resize(270000);
            controller.update(synapses, neurons, 1);
            const size_t after = rss_kb();
            require(after >= before, "RSS accounting regressed");
            require(after < 512000, "Stage 1 auxiliary state exceeded 512 MB resident budget");
            return static_cast<double>(after - before);
        });

        // Evidence telemetry: deterministic learning curves, rate trace,
        // distributions, and event counters are written independently of the
        // pass/fail table so later stages can consume the measurements.
        LearningConfig telemetry_config;
        telemetry_config.eta = 0.02f;
        telemetry_config.homeostasis_enabled = true;
        TransitionModel telemetry_model(telemetry_config);
        std::ofstream curves(artifact_dir / "task_curves.csv");
        curves << "task,event,score_a,score_b\n";
        for (int event = 1; event <= 5000; ++event) {
            telemetry_model.event(0, 1, 1.0f);
            telemetry_model.idle();
            if (event % 500 == 0) {
                curves << "temporal_association," << event << ','
                       << telemetry_model.probability(0, 1) << ','
                       << telemetry_model.probability(0, 0) << "\n";
            }
        }
        TransitionModel nonstationary(telemetry_config);
        for (int event = 1; event <= 5000; ++event) {
            nonstationary.event(0, 0, 1.0f);
            nonstationary.idle();
            if (event % 500 == 0) curves << "nonstationary_A," << event << ',' << nonstationary.probability(0, 0) << ',' << nonstationary.probability(1, 1) << "\n";
        }
        for (int event = 1; event <= 5000; ++event) {
            nonstationary.event(1, 1, 1.0f);
            nonstationary.idle();
            if (event % 500 == 0) curves << "nonstationary_B," << (5000 + event) << ',' << nonstationary.probability(0, 0) << ',' << nonstationary.probability(1, 1) << "\n";
        }
        curves.close();

        std::ofstream rate_trace(artifact_dir / "firing_rate_trace.csv");
        rate_trace << "tick,rate_hz,homeostatic_gain\n";
        LearningController rate_controller;
        rate_controller.initialize(100, 1);
        for (int tick = 0; tick < 600; ++tick) {
            const size_t fired = static_cast<size_t>(std::lround(100.0 * rate_controller.homeostatic_gain() * 0.05));
            rate_controller.observe_population_spikes(std::min<size_t>(100, fired), 100);
            if (tick % 10 == 0) rate_trace << tick << ',' << rate_controller.metrics().mean_firing_rate_hz << ',' << rate_controller.homeostatic_gain() << "\n";
        }
        rate_trace.close();

        std::ofstream histograms(artifact_dir / "weight_trace_histogram.csv");
        histograms << "quantity,bucket,count\n";
        std::vector<size_t> weight_buckets(10, 0);
        std::vector<size_t> trace_buckets(10, 0);
        for (float weight : telemetry_model.synapses.weights) {
            const size_t bucket = std::min<size_t>(9, static_cast<size_t>(std::max(0.0f, weight) * 10.0f));
            ++weight_buckets[bucket];
        }
        for (float trace : telemetry_model.synapses.eligibility_traces) {
            const float normalized = std::min(0.999999f, std::max(0.0f, (trace + 1.0f) * 0.5f));
            ++trace_buckets[static_cast<size_t>(normalized * 10.0f)];
        }
        for (size_t i = 0; i < 10; ++i) {
            histograms << "weight," << i << ',' << weight_buckets[i] << "\n";
            histograms << "trace," << i << ',' << trace_buckets[i] << "\n";
        }
        histograms.close();

        const auto& telemetry_metrics = telemetry_model.controller.metrics();
        std::ofstream event_counts(artifact_dir / "event_counts.csv");
        event_counts << "metric,value\n"
                     << "pre_spike_events," << telemetry_metrics.pre_spike_events << "\n"
                     << "post_spike_events," << telemetry_metrics.post_spike_events << "\n"
                     << "causal_events," << telemetry_metrics.causal_events << "\n"
                     << "anti_causal_events," << telemetry_metrics.anti_causal_events << "\n"
                     << "weight_updates," << telemetry_metrics.weight_updates << "\n"
                     << "clipped_updates," << telemetry_metrics.clipped_updates << "\n"
                     << "structural_proposals," << telemetry_metrics.structural_proposals << "\n"
                     << "bound_violations," << telemetry_metrics.bound_violations << "\n";
        event_counts.close();

        std::ofstream ablations(artifact_dir / "ablation.csv");
        ablations << "ablation,selectivity_percentage_points\n";
        LearningConfig ablation_config;
        ablation_config.homeostasis_enabled = false;
        ablations << "no_learning," << run_ablation_selectivity([&] { auto c = ablation_config; c.learning_enabled = false; return c; }(), true) << "\n";
        ablations << "plain_stdp," << run_ablation_selectivity(ablation_config, false) << "\n";
        ablations << "stdp_plus_reward," << run_ablation_selectivity(ablation_config, true) << "\n";
        ablations << "full_three_factor," << run_ablation_selectivity(LearningConfig{}, true) << "\n";
        ablations << "full_without_homeostasis," << run_ablation_selectivity([&] { auto c = LearningConfig{}; c.homeostasis_enabled = false; return c; }(), true) << "\n";
        ablations << "full_without_structural," << run_ablation_selectivity([&] { auto c = LearningConfig{}; c.structural_enabled = false; return c; }(), true) << "\n";
        ablations.close();

        std::ofstream metrics(artifact_dir / "stage1_metrics.csv");
        metrics << "test_id,passed,value,detail\n";
        size_t failures = 0;
        for (const auto& result : results) {
            metrics << result.id << ',' << (result.passed ? 1 : 0) << ','
                    << std::setprecision(12) << result.value << ",\""
                    << result.detail << "\"\n";
            if (!result.passed) ++failures;
        }
        metrics.close();

        std::ofstream summary(artifact_dir / "stage1_summary.txt");
        summary << "seed=" << seed << '\n';
        summary << "tests=" << results.size() << '\n';
        summary << "failures=" << failures << '\n';
        summary << "deterministic_hash=" << run_deterministic_scenario(seed) << '\n';
        summary.close();

        for (const auto& result : results) {
            std::cout << result.id << '=' << (result.passed ? "PASS" : "FAIL")
                      << " value=" << result.value << " detail=" << result.detail << '\n';
        }
        std::cout << "STAGE1_HARNESS=" << (failures == 0 ? "PASS" : "FAIL") << '\n';
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "STAGE1_HARNESS=ERROR " << ex.what() << '\n';
        return 2;
    }
}
