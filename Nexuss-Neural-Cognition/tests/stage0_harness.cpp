#include "intellectual/uin_engine.h"
#include "intellectual/uin_constants.h"
#include "types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
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
using genesis::NeuronBlock;
using genesis::SynapseBlock;
using genesis::intellectual::IntellectualSynapse;
using genesis::intellectual::NeuronType;
using genesis::intellectual::SynapseType;
using genesis::intellectual::UINConstants;
using genesis::intellectual::UINEngine;
using genesis::intellectual::V_REST_MV;
using genesis::intellectual::TAU_MEMBRANE_MS;
using genesis::intellectual::R_MEGOHM;
using genesis::intellectual::E_EXC_MV;
using genesis::intellectual::E_INH_MV;
using genesis::intellectual::E_BIND_MV;
using genesis::intellectual::THETA_BASE_MV;
using genesis::intellectual::REFRACTORY_TICKS;

namespace {

struct TestResult {
    std::string id;
    bool passed;
    double value;
    std::string detail;
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

uint64_t mix_hash(uint64_t hash, const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

template <typename T>
uint64_t mix_value(uint64_t hash, const T& value) {
    return mix_hash(hash, &value, sizeof(T));
}

uint64_t run_deterministic_trace(uint64_t seed, uint32_t ticks = 4000) {
    UINEngine engine;
    NeuronBlock neurons;
    constexpr uint32_t count = 32;
    neurons.resize(count);
    engine.initialize_pool(neurons, 0, count, static_cast<uint8_t>(NeuronType::CI));

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<float> excitation(0.0f, 0.45f);
    std::uniform_real_distribution<float> inhibition(0.0f, 0.18f);
    std::uniform_real_distribution<float> binding(0.0f, 0.12f);
    uint64_t hash = 1469598103934665603ULL;

    for (uint32_t tick = 0; tick < ticks; ++tick) {
        for (uint32_t i = 0; i < count; ++i) {
            neurons.g_exc[i] = excitation(rng);
            neurons.g_inh[i] = inhibition(rng);
            neurons.g_bind[i] = binding(rng);
        }
        engine.step_kernel(neurons, count, 1.0f);
        hash = mix_value(hash, tick);
        for (uint32_t i = 0; i < count; ++i) {
            hash = mix_value(hash, neurons.membrane_potential[i]);
            hash = mix_value(hash, neurons.g_exc[i]);
            hash = mix_value(hash, neurons.g_inh[i]);
            hash = mix_value(hash, neurons.g_bind[i]);
            const bool fired = neurons.has_fired[i];
            hash = mix_value(hash, fired);
            hash = mix_value(hash, neurons.refractory_timer[i]);
            hash = mix_value(hash, neurons.stpd_trace[i]);
        }
    }
    return hash;
}

size_t read_rss_kb() {
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
    // Linux reports ru_maxrss in kilobytes.
    return static_cast<size_t>(usage.ru_maxrss);
}

double estimate_mb(size_t neurons, size_t synapses) {
    return (static_cast<double>(neurons) * 88.0 +
            static_cast<double>(synapses) * 32.0) / (1024.0 * 1024.0) * 1.15;
}

int parse_int_arg(int argc, char** argv, const std::string& name, int fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) return std::stoi(argv[i + 1]);
    }
    return fallback;
}

std::string parse_string_arg(int argc, char** argv, const std::string& name,
                             const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) return argv[i + 1];
    }
    return fallback;
}

int run_memory_probe(int argc, char** argv) {
    const size_t neurons = static_cast<size_t>(parse_int_arg(argc, argv, "--memory-scale", 1000));
    const size_t synapses_per_neuron = static_cast<size_t>(parse_int_arg(argc, argv, "--synapses-per-neuron", 5));
    require(neurons > 0 && synapses_per_neuron > 0, "memory probe requires positive dimensions");
    const size_t synapses = neurons * synapses_per_neuron;

    NeuronBlock neuron_block;
    SynapseBlock synapse_block;
    neuron_block.resize(neurons);
    synapse_block.resize(synapses);

    // Touch the allocated arrays so resident memory reflects committed pages.
    for (size_t i = 0; i < neurons; ++i) {
        neuron_block.membrane_potential[i] = V_REST_MV;
        neuron_block.g_exc[i] = 0.01f;
        neuron_block.g_inh[i] = 0.01f;
        neuron_block.neuron_flags[i] = 0;
    }
    for (size_t i = 0; i < synapses; ++i) {
        synapse_block.pre_indices[i] = static_cast<uint32_t>(i % neurons);
        synapse_block.post_indices[i] = static_cast<uint32_t>((i + 1) % neurons);
        synapse_block.weights[i] = 0.01f;
        synapse_block.binding_tag[i] = static_cast<uint64_t>(i);
    }

    const size_t rss_kb = read_rss_kb();
    std::cout << "memory_probe," << neurons << ',' << synapses << ',' << rss_kb
              << ',' << std::fixed << std::setprecision(4)
              << estimate_mb(neurons, synapses) << '\n';
    return rss_kb == 0 ? 2 : 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (std::find(argv, argv + argc, std::string("--memory-probe")) != argv + argc) {
            return run_memory_probe(argc, argv);
        }

        const uint64_t seed = static_cast<uint64_t>(std::stoull(
            parse_string_arg(argc, argv, "--seed", "424242")));
        const fs::path artifact_dir = parse_string_arg(argc, argv, "--artifact-dir", "artifacts/stage-0");
        fs::create_directories(artifact_dir);

        std::vector<TestResult> results;
        auto run = [&results](const std::string& id, const auto& fn) {
            try {
                const double value = fn();
                results.push_back({id, true, value, "ok"});
            } catch (const std::exception& ex) {
                results.push_back({id, false, 0.0, ex.what()});
            }
        };

        run("S0-PHY-01", [seed]() {
            UINEngine engine;
            NeuronBlock neurons;
            neurons.resize(1);
            engine.initialize_pool(neurons, 0, 1, static_cast<uint8_t>(NeuronType::CI));
            std::mt19937_64 rng(seed + 101);
            std::uniform_real_distribution<float> conductance(0.01f, 0.50f);
            float minimum_delta = std::numeric_limits<float>::max();
            for (int trial = 0; trial < 10000; ++trial) {
                neurons.membrane_potential[0] = V_REST_MV;
                neurons.g_exc[0] = conductance(rng);
                neurons.g_inh[0] = 0.0f;
                neurons.g_bind[0] = 0.0f;
                neurons.refractory_timer[0] = 0;
                neurons.has_fired[0] = false;
                engine.step_kernel(neurons, 1);
                const float delta = neurons.membrane_potential[0] - V_REST_MV;
                require(delta > 0.0f, "positive excitation did not depolarize");
                require(!neurons.has_fired[0], "direction test crossed threshold");
                minimum_delta = std::min(minimum_delta, delta);
            }
            return static_cast<double>(minimum_delta);
        });

        run("S0-PHY-02", [seed]() {
            UINEngine engine;
            NeuronBlock neurons;
            neurons.resize(1);
            engine.initialize_pool(neurons, 0, 1, static_cast<uint8_t>(NeuronType::CI));
            std::mt19937_64 rng(seed + 202);
            std::uniform_real_distribution<float> conductance(0.01f, 0.50f);
            float minimum_delta = std::numeric_limits<float>::max();
            for (int trial = 0; trial < 10000; ++trial) {
                neurons.membrane_potential[0] = V_REST_MV;
                neurons.g_exc[0] = 0.0f;
                neurons.g_inh[0] = conductance(rng);
                neurons.g_bind[0] = 0.0f;
                neurons.refractory_timer[0] = 0;
                neurons.has_fired[0] = false;
                engine.step_kernel(neurons, 1);
                const float delta = V_REST_MV - neurons.membrane_potential[0];
                require(delta > 0.0f, "positive inhibition did not hyperpolarize");
                require(!neurons.has_fired[0], "direction test unexpectedly fired");
                minimum_delta = std::min(minimum_delta, delta);
            }
            return static_cast<double>(minimum_delta);
        });

        run("S0-PHY-03", [seed]() {
            UINEngine engine;
            NeuronBlock neurons;
            neurons.resize(1);
            engine.initialize_pool(neurons, 0, 1, static_cast<uint8_t>(NeuronType::CI));
            std::mt19937_64 rng(seed);
            std::uniform_real_distribution<float> voltage(-70.0f, -65.0f);
            std::uniform_real_distribution<float> conductance(0.0f, 0.20f);
            double worst_error = 0.0;
            UINConstants constants(1.0f);
            for (int i = 0; i < 100; ++i) {
                const float v = voltage(rng);
                const float ge = conductance(rng);
                const float gi = conductance(rng);
                const float gb = conductance(rng);
                neurons.membrane_potential[0] = v;
                neurons.g_exc[0] = ge;
                neurons.g_inh[0] = gi;
                neurons.g_bind[0] = gb;
                neurons.has_fired[0] = false;
                const float ge_decay = ge * constants.exc_decay;
                const float gi_decay = gi * constants.inh_decay;
                const float gb_decay = gb * constants.bind_decay;
                const float current = ge_decay * (E_EXC_MV - v) +
                                      gi_decay * (E_INH_MV - v) +
                                      gb_decay * (E_BIND_MV - v);
                const float expected = v + (1.0f / TAU_MEMBRANE_MS) *
                    (-(v - V_REST_MV) + R_MEGOHM * current);
                engine.step_kernel(neurons, 1);
                require(!neurons.has_fired[0], "magnitude case unexpectedly fired");
                const double error = std::abs(static_cast<double>(neurons.membrane_potential[0]) - expected);
                worst_error = std::max(worst_error, error);
                require(error <= 1e-5, "membrane equation mismatch");
            }
            return worst_error;
        });

        run("S0-PHY-04", []() {
            UINEngine engine;
            NeuronBlock neurons;
            neurons.resize(1);
            engine.initialize_pool(neurons, 0, 1, static_cast<uint8_t>(NeuronType::CI));
            const float decay = 1.0f - 1.0f / TAU_MEMBRANE_MS;
            neurons.membrane_potential[0] = (THETA_BASE_MV - V_REST_MV * (1.0f / TAU_MEMBRANE_MS)) / decay;
            engine.step_kernel(neurons, 1);
            require(neurons.has_fired[0], "threshold boundary did not use documented >= policy");
            return 1.0;
        });

        run("S0-PHY-05", []() {
            UINEngine engine;
            NeuronBlock neurons;
            neurons.resize(1);
            engine.initialize_pool(neurons, 0, 1, static_cast<uint8_t>(NeuronType::CI));
            neurons.membrane_potential[0] = -50.0f;
            engine.step_kernel(neurons, 1);
            require(neurons.has_fired[0], "initial threshold spike missing");
            require(neurons.refractory_timer[0] == REFRACTORY_TICKS,
                    "refractory timer was not initialized");
            for (int i = 0; i < REFRACTORY_TICKS; ++i) {
                neurons.membrane_potential[0] = -50.0f;
                engine.step_kernel(neurons, 1);
                require(!neurons.has_fired[0], "spike occurred during refractory period");
            }
            require(neurons.refractory_timer[0] == 0, "refractory timer did not reach zero");
            neurons.membrane_potential[0] = -50.0f;
            engine.step_kernel(neurons, 1);
            require(neurons.has_fired[0], "firing did not resume after refractory period");
            return 1.0;
        });

        run("S0-PHY-06", []() {
            UINEngine engine;
            NeuronBlock neurons;
            neurons.resize(1);
            engine.initialize_pool(neurons, 0, 1, static_cast<uint8_t>(NeuronType::CI));
            neurons.g_exc[0] = 0.50f;
            neurons.g_inh[0] = 0.30f;
            neurons.g_bind[0] = 0.20f;
            UINConstants constants(1.0f);
            double worst_error = 0.0;
            for (int tick = 1; tick <= 1000; ++tick) {
                neurons.membrane_potential[0] = V_REST_MV;
                engine.step_kernel(neurons, 1);
                const double expected_exc = 0.50 * std::pow(constants.exc_decay, tick);
                const double error = std::abs(static_cast<double>(neurons.g_exc[0]) - expected_exc);
                worst_error = std::max(worst_error, error);
                require(error <= 1e-5 * std::max(1.0, expected_exc), "excitatory decay mismatch");
            }
            return worst_error;
        });

        run("S0-PHY-07", []() {
            UINEngine engine;
            NeuronBlock neurons;
            constexpr uint32_t count = 8;
            neurons.resize(count);
            engine.initialize_pool(neurons, 0, count, static_cast<uint8_t>(NeuronType::CI));
            std::mt19937_64 rng(20260101);
            std::uniform_real_distribution<float> drive(0.0f, 0.55f);
            for (uint32_t tick = 0; tick < 1'000'000; ++tick) {
                for (uint32_t i = 0; i < count; ++i) {
                    neurons.g_exc[i] = drive(rng);
                    neurons.g_inh[i] = drive(rng) * 0.35f;
                    neurons.g_bind[i] = drive(rng) * 0.20f;
                }
                engine.step_kernel(neurons, count);
                for (uint32_t i = 0; i < count; ++i) {
                    require(std::isfinite(neurons.membrane_potential[i]), "V became non-finite");
                    require(std::isfinite(neurons.g_exc[i]) && std::isfinite(neurons.g_inh[i]) &&
                            std::isfinite(neurons.g_bind[i]), "conductance became non-finite");
                    require(std::isfinite(neurons.stpd_trace[i]), "trace became non-finite");
                    require(neurons.membrane_potential[i] > -10000.0f &&
                            neurons.membrane_potential[i] < 10000.0f,
                            "V escaped bounded stress range");
                }
            }
            return 1'000'000.0;
        });

        run("S0-DET-01", [seed, artifact_dir]() {
            const uint64_t a = run_deterministic_trace(seed);
            const uint64_t b = run_deterministic_trace(seed);
            const uint64_t c = run_deterministic_trace(seed);
            require(a == b && b == c, "same-seed traces differ");
            std::ofstream out(artifact_dir / "same_seed_hashes.csv");
            out << "run,hash\n1," << a << "\n2," << b << "\n3," << c << "\n";
            return static_cast<double>(a);
        });

        run("S0-DET-02", [seed]() {
            const uint64_t baseline = run_deterministic_trace(seed);
            int different = 0;
            for (int i = 1; i <= 20; ++i) {
                if (run_deterministic_trace(seed + static_cast<uint64_t>(i)) != baseline) ++different;
            }
            require(different >= 19, "distinct seeds did not produce distinct traces");
            return static_cast<double>(different) / 20.0;
        });

        run("S0-SCHEMA-01", []() {
            NeuronBlock legacy;
            legacy.resize(2);
            require(!legacy.has_uin_overlay(), "fresh NeuronBlock unexpectedly uses UIN overlay");
            UINEngine engine;
            engine.initialize_pool(legacy, 0, 2, static_cast<uint8_t>(NeuronType::CI));
            require(legacy.has_uin_overlay(), "UIN pool did not activate overlay schema");
            require(legacy.state_schema.compatible_with(genesis::StateSchema::uin_overlay()),
                    "UIN state schema is incompatible with expected contract");
            return 1.0;
        });

        run("S0-ERR-01", []() {
            UINEngine engine;
            NeuronBlock neurons;
            neurons.resize(4);
            bool caught_init = false;
            bool caught_step = false;
            bool caught_get = false;
            bool caught_null = false;
            bool caught_post = false;
            try { engine.initialize_pool(neurons, 3, 2, 0); } catch (const std::out_of_range&) { caught_init = true; }
            engine.initialize_pool(neurons, 0, 4, 0);
            try { engine.step_kernel(neurons, 5); } catch (const std::out_of_range&) { caught_step = true; }
            try { (void)engine.get_type(neurons, 4); } catch (const std::out_of_range&) { caught_get = true; }
            try { engine.deliver_spike(nullptr, neurons); } catch (const std::invalid_argument&) { caught_null = true; }
            IntellectualSynapse bad;
            bad.post_id = 4;
            try { engine.deliver_spike(&bad, neurons); } catch (const std::out_of_range&) { caught_post = true; }
            require(caught_init && caught_step && caught_get && caught_null && caught_post,
                    "one or more invalid-input paths were silent");
            return 5.0;
        });

        std::ofstream metrics(artifact_dir / "stage0_metrics.csv");
        metrics << "test_id,passed,value,detail\n";
        size_t failures = 0;
        for (const auto& result : results) {
            metrics << result.id << ',' << (result.passed ? 1 : 0) << ','
                    << std::setprecision(12) << result.value << ",\""
                    << result.detail << "\"\n";
            if (!result.passed) ++failures;
        }
        metrics.close();

        std::ofstream summary(artifact_dir / "stage0_summary.txt");
        summary << "seed=" << seed << '\n';
        summary << "tests=" << results.size() << '\n';
        summary << "failures=" << failures << '\n';
        summary << "same_seed_hash=" << run_deterministic_trace(seed) << '\n';
        summary.close();

        for (const auto& result : results) {
            std::cout << result.id << '=' << (result.passed ? "PASS" : "FAIL")
                      << " value=" << result.value << " detail=" << result.detail << '\n';
        }
        std::cout << "STAGE0_HARNESS=" << (failures == 0 ? "PASS" : "FAIL") << '\n';
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "STAGE0_HARNESS=ERROR " << ex.what() << '\n';
        return 2;
    }
}
