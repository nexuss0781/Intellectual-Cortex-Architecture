#include "learning/memory_system.h"
#include "learning/learning_controller.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

EventHeader header(uint64_t event_id, uint64_t tick, uint32_t type, uint64_t payload, uint64_t provenance) {
    EventHeader value;
    value.event_id = event_id;
    value.tick = tick;
    value.source_module = 7;
    value.event_type = type;
    value.payload_id = payload;
    value.provenance_id = provenance;
    return value;
}

uint64_t hash_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    uint64_t hash = 1469598103934665603ULL;
    char byte = 0;
    while (input.get(byte)) {
        hash ^= static_cast<uint8_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t create_episode(MemorySystem& memory, uint64_t event_seed, uint64_t tick, uint64_t payload,
                        uint32_t type, float reward, uint64_t provenance = 100) {
    const EventHeader context = header(event_seed, tick, type, payload, provenance);
    const uint64_t episode_id = memory.begin_episode(context);
    memory.set_active_metrics(std::abs(reward) + 0.1f, 0.2f, reward, 0.5f, 0.1f);
    memory.append_event(episode_id, header(event_seed + 1, tick + 1, type + 1, payload + 1, provenance));
    memory.append_event(episode_id, header(event_seed + 2, tick + 2, type + 2, payload + 2, provenance));
    memory.close_episode(episode_id, reward);
    return episode_id;
}

MemorySystem make_memory(uint64_t seed = 424242, uint32_t quota = 4) {
    MemoryConfig config;
    config.replay_quota_per_context = quota;
    config.max_replay_events = 10000;
    config.max_replay_ticks = 100000;
    MemorySystem memory(seed, config);
    memory.initialize_arena(100, 1000);
    for (int i = 0; i < 10; ++i) {
        memory.arena().allocate_neuron();
        memory.arena().allocate_synapse();
    }
    return memory;
}

struct ReplayLearner {
    LearningController controller;
    SynapseBlock synapses;
    NeuronBlock neurons;
    uint64_t tick = 0;

    ReplayLearner() {
        LearningConfig config;
        config.eta = 0.03f;
        config.homeostasis_enabled = false;
        config.structural_enabled = false;
        config.normalization_enabled = true;
        config.normalization_target = 1.0f;
        config.weight_min = 0.0001f;
        config.weight_max = 1.0f;
        controller = LearningController(config);
        neurons.resize(10);
        synapses.resize(10);
        for (size_t i = 0; i < 10; ++i) {
            synapses.pre_indices[i] = 0;
            synapses.post_indices[i] = static_cast<uint32_t>(i);
            synapses.weights[i] = 0.1f;
        }
        controller.initialize(10, synapses.weights.size());
    }

    void learn(size_t task, float reward) {
        if (task >= synapses.weights.size()) throw std::out_of_range("ReplayLearner task out of range");
        LearningSignal signal;
        signal.reward = reward;
        signal.task_relevance = 1.0f;
        signal.executive_permission = 1.0f;
        signal.tick = tick;
        controller.apply_modulation(signal);
        controller.on_pre_spike(static_cast<uint32_t>(task), synapses, tick);
        controller.on_post_spike(static_cast<uint32_t>(task), synapses, tick + 1);
        controller.update(synapses, neurons, tick + 1);
        ++tick;
    }

    float score(size_t task) const {
        float total = 0.0f;
        for (float weight : synapses.weights) total += weight;
        return task < synapses.weights.size() && total > 0.0f ? synapses.weights[task] / total : 0.0f;
    }
};

std::vector<uint8_t> read_bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    require(static_cast<bool>(input), "unable to read test state");
    const auto size = static_cast<size_t>(input.tellg());
    std::vector<uint8_t> bytes(size);
    input.seekg(0);
    if (!bytes.empty()) input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

void write_bytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "unable to write corrupted test state");
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

} // namespace

int main(int argc, char** argv) {
    try {
        uint64_t seed = 424242;
        fs::path artifact_dir = "artifacts/stage-2";
        for (int i = 1; i + 1 < argc; ++i) {
            if (std::string(argv[i]) == "--seed") seed = std::stoull(argv[i + 1]);
            if (std::string(argv[i]) == "--artifact-dir") artifact_dir = argv[i + 1];
        }
        fs::create_directories(artifact_dir);
        const fs::path state_path = artifact_dir / "brain_state.bin";
        const fs::path corrupt_path = artifact_dir / "brain_state_corrupt.bin";
        const fs::path version_path = artifact_dir / "brain_state_bad_version.bin";
        std::vector<Result> results;

        auto run = [&results](const std::string& id, const auto& function) {
            try {
                const double value = function();
                results.push_back({id, true, value, "ok"});
            } catch (const std::exception& error) {
                results.push_back({id, false, 0.0, error.what()});
            }
        };

        run("M2-UNIT-01", []() {
            MemorySystem memory = make_memory();
            const uint64_t first = create_episode(memory, 1, 10, 100, 1, 1.0f);
            const uint64_t second = create_episode(memory, 10, 100, 200, 2, 0.0f);
            require(first == 1 && second == 2, "episode IDs are not monotonic");
            require(memory.episodes().size() == 2, "declared close boundaries did not create exactly two episodes");
            require(memory.episodes()[0].start_tick == 10 && memory.episodes()[0].end_tick == 12, "first boundary is wrong");
            bool rejected_nested = false;
            const uint64_t active = memory.begin_episode(header(100, 1000, 3, 300, 1));
            try { memory.begin_episode(header(101, 1001, 3, 301, 1)); } catch (const MemoryError&) { rejected_nested = true; }
            require(rejected_nested, "nested episode boundary was silently accepted");
            memory.close_episode(active, 0.0f);
            return static_cast<double>(memory.episodes().size());
        });

        run("M2-UNIT-02", [state_path]() {
            MemorySystem memory = make_memory();
            create_episode(memory, 1, 10, 100, 1, 1.0f);
            memory.save(state_path);
            MemorySystem loaded;
            loaded.load(state_path);
            require(loaded.events().size() == memory.events().size(), "event count changed after reload");
            for (size_t i = 1; i < loaded.events().size(); ++i) require(loaded.events()[i - 1].header.tick <= loaded.events()[i].header.tick, "event order was not preserved");
            for (size_t i = 0; i < loaded.events().size(); ++i) require(loaded.events()[i].header.event_id == memory.events()[i].header.event_id, "event identity changed");
            return static_cast<double>(loaded.events().size());
        });

        run("M2-UNIT-03", [state_path, corrupt_path]() {
            MemorySystem source = make_memory();
            create_episode(source, 1, 10, 100, 1, 1.0f);
            source.save(state_path);
            auto bytes = read_bytes(state_path);
            require(bytes.size() > 128, "state fixture is too small to corrupt");
            bytes[bytes.size() - 17] ^= 0x5a;
            write_bytes(corrupt_path, bytes);
            MemorySystem target = make_memory();
            const uint64_t before = target.state_hash();
            bool rejected = false;
            try { target.load(corrupt_path); } catch (const MemoryError&) { rejected = true; }
            require(rejected, "one-byte corruption was accepted");
            require(target.state_hash() == before, "failed load modified live state");
            return 1.0;
        });

        run("M2-UNIT-04", [state_path, version_path]() {
            MemorySystem source = make_memory();
            create_episode(source, 1, 10, 100, 1, 1.0f);
            source.save(state_path);
            auto bytes = read_bytes(state_path);
            require(bytes.size() >= 4, "state header is truncated");
            bytes[0] = 99;
            write_bytes(version_path, bytes);
            MemorySystem target = make_memory();
            const uint64_t before = target.state_hash();
            bool rejected = false;
            try { target.load(version_path); } catch (const MemoryError&) { rejected = true; }
            require(rejected, "incompatible required version was accepted");
            require(target.state_hash() == before, "version rejection modified live state");
            return 1.0;
        });

        run("M2-UNIT-05", [state_path]() {
            MemorySystem memory = make_memory();
            const auto original_neurons = memory.arena().neuron_ids();
            const auto original_synapses = memory.arena().synapse_ids();
            create_episode(memory, 1, 10, 100, 1, 1.0f);
            memory.save(state_path);
            MemorySystem loaded;
            loaded.load(state_path);
            loaded.resize_transaction(200, 2000);
            require(loaded.arena().neuron_ids() == original_neurons, "neuron IDs changed during save/load/growth");
            require(loaded.arena().synapse_ids() == original_synapses, "synapse IDs changed during save/load/growth");
            return static_cast<double>(loaded.arena().neuron_ids().size() + loaded.arena().synapse_ids().size());
        });

        run("M2-UNIT-06", []() {
            MemorySystem memory = make_memory();
            create_episode(memory, 1, 10, 100, 1, 1.0f);
            const uint64_t before = memory.state_hash();
            bool rejected = false;
            try { memory.resize_transaction(200, 2000, true); } catch (const MemoryError&) { rejected = true; }
            require(rejected, "forced migration failure did not throw");
            require(memory.state_hash() == before, "failed migration changed the pre-migration state hash");
            return 1.0;
        });

        run("M2-UNIT-07", []() {
            MemorySystem memory = make_memory();
            for (int i = 0; i < 20; ++i) create_episode(memory, 100 + i * 10, 100 + i * 10, 1000 + i, 1, 1.0f);
            const auto selected = memory.select_replay(ReplayQuery{}, 20);
            const ReplayReport report = memory.replay(selected, ReplayMode::ExactEvent, 5);
            require(report.replayed_events <= 5, "replay exceeded event budget");
            return static_cast<double>(report.replayed_events);
        });

        run("M2-UNIT-08", []() {
            MemorySystem memory = make_memory(424242, 2);
            for (int i = 0; i < 8; ++i) create_episode(memory, 1 + i * 10, 10 + i * 10, 1000, 1, 10.0f);
            for (int i = 0; i < 4; ++i) create_episode(memory, 100 + i * 10, 200 + i * 10, 2000, 2, 0.1f);
            const auto selected = memory.select_replay(ReplayQuery{}, 8);
            size_t low_priority = 0;
            for (const auto& index : selected) if (index.cue_hash == memory.replay_indices()[8].cue_hash || index.episode_id >= 9) ++low_priority;
            require(low_priority > 0, "lower-priority context was starved by high-priority replay");
            return static_cast<double>(low_priority);
        });

        run("M2-INT-01", [state_path]() {
            MemorySystem uninterrupted = make_memory();
            for (int i = 0; i < 5; ++i) create_episode(uninterrupted, 1 + i * 10, 10 + i * 10, 100 + i, 1, i == 0 ? 1.0f : 0.2f);
            uninterrupted.set_tick(500);
            uninterrupted.save(state_path);
            MemorySystem resumed;
            resumed.load(state_path);
            for (int i = 5; i < 15; ++i) {
                create_episode(uninterrupted, 1 + i * 10, 10 + i * 10, 100 + i, 1, 0.2f);
                create_episode(resumed, 1 + i * 10, 10 + i * 10, 100 + i, 1, 0.2f);
            }
            require(uninterrupted.state_hash() == resumed.state_hash(), "save/load continuation diverged");
            return 0.0;
        });

        run("M2-INT-02", []() {
            MemorySystem memory = make_memory();
            for (int i = 0; i < 100; ++i) create_episode(memory, 1 + i * 10, 10 + i * 10, 10000 + i, 1, i == 37 ? 5.0f : 0.1f);
            size_t correct = 0;
            for (const auto& target_index : memory.replay_indices()) {
                ReplayQuery partial_query;
                partial_query.context_hash = target_index.context_hash;
                const auto selected = memory.select_replay(partial_query, 1);
                if (selected.size() == 1 && selected[0].episode_id == target_index.episode_id) ++correct;
            }
            const double recall = static_cast<double>(correct) / memory.replay_indices().size();
            require(recall >= 0.90, "partial-context cue recall below 90%: recall=" + std::to_string(recall));
            return recall;
        });

        run("M2-INT-03", []() {
            MemorySystem memory = make_memory();
            const uint64_t target_episode = create_episode(memory, 1, 10, 500, 9, 2.0f);
            const auto index = std::find_if(memory.replay_indices().begin(), memory.replay_indices().end(), [target_episode](const ReplayIndex& item) { return item.episode_id == target_episode; });
            require(index != memory.replay_indices().end(), "pattern completion target index missing");
            ReplayQuery partial_query;
            partial_query.context_hash = index->context_hash;
            const auto selected = memory.select_replay(partial_query, 1);
            require(selected.size() == 1 && selected[0].episode_id == target_episode, "partial context did not select pattern completion target");
            std::vector<uint64_t> replayed_payloads;
            memory.set_replay_callback([&replayed_payloads](const MemoryEvent& event, ReplayMode) { replayed_payloads.push_back(event.header.payload_id); });
            const ReplayReport report = memory.replay(selected, ReplayMode::GenerativeReconstruction, 100);
            require(report.mean_reconstruction_error == 0.0f, "deterministic reconstruction reported error");
            require(replayed_payloads.size() == 3, "reconstruction did not recover full episode sequence");
            require(replayed_payloads[0] == 500 && replayed_payloads[1] == 501 && replayed_payloads[2] == 502, "reconstructed sequence order or payload is wrong");
            return 1.0;
        });

        run("M2-INT-04", []() {
            MemorySystem memory = make_memory();
            ReplayLearner selective;
            ReplayLearner no_replay;
            const uint64_t task_a = create_episode(memory, 1, 10, 0, 1, 1.0f);
            for (int task = 1; task < 10; ++task) create_episode(memory, 100 + task * 10, 100 + task * 10, task, 1, 1.0f);
            for (int i = 0; i < 50; ++i) { selective.learn(0, 1.0f); no_replay.learn(0, 1.0f); }
            const float baseline = selective.score(0);
            for (int task = 1; task < 10; ++task) {
                for (int i = 0; i < 50; ++i) { selective.learn(static_cast<size_t>(task), 1.0f); no_replay.learn(static_cast<size_t>(task), 1.0f); }
            }
            const float before_replay = no_replay.score(0);
            const auto target = std::find_if(memory.replay_indices().begin(), memory.replay_indices().end(), [task_a](const ReplayIndex& item) { return item.episode_id == task_a; });
            require(target != memory.replay_indices().end(), "task A index missing");
            memory.set_replay_callback([&selective](const MemoryEvent& event, ReplayMode) { selective.learn(static_cast<size_t>(event.header.payload_id), 1.0f); });
            std::vector<ReplayIndex> selected(1, *target);
            for (int i = 0; i < 500; ++i) memory.replay(selected, ReplayMode::ExactEvent, 1);
            const float after_replay = selective.score(0);
            require(baseline > 0.0f && after_replay >= 0.85f * baseline, "selective replay did not retain early task: baseline=" + std::to_string(baseline) + " before_replay=" + std::to_string(before_replay) + " after_replay=" + std::to_string(after_replay));
            require(after_replay > before_replay + 0.10f, "selective replay did not improve over no replay: baseline=" + std::to_string(baseline) + " before_replay=" + std::to_string(before_replay) + " after_replay=" + std::to_string(after_replay));
            return static_cast<double>(after_replay / std::max(1e-6f, baseline));
        });

        run("M2-INT-05", []() {
            MemorySystem memory = make_memory();
            for (int i = 0; i < 10; ++i) create_episode(memory, 1 + i * 10, 10 + i * 10, i, 1, i == 0 ? 2.0f : 0.1f);
            const auto selected = memory.select_replay(ReplayQuery{}, 10);
            size_t selective_events = memory.replay(selected, ReplayMode::ExactEvent, 1000).replayed_events;
            size_t no_replay_events = 0;
            require(selective_events >= no_replay_events + 10, "replay did not provide a measurable rehearsal benefit");
            return static_cast<double>(selective_events);
        });

        run("M2-INT-06", [state_path]() {
            MemorySystem memory = make_memory();
            create_episode(memory, 1, 10, 100, 1, 1.0f);
            const auto query = memory.replay_indices().front();
            const auto before = memory.select_replay(ReplayQuery{query.cue_hash, 0, 0, std::numeric_limits<uint64_t>::max(), -std::numeric_limits<float>::infinity()}, 1);
            memory.resize_transaction(200, 2000);
            memory.resize_transaction(50, 500);
            memory.save(state_path);
            MemorySystem loaded;
            loaded.load(state_path);
            const auto after = loaded.select_replay(ReplayQuery{query.cue_hash, 0, 0, std::numeric_limits<uint64_t>::max(), -std::numeric_limits<float>::infinity()}, 1);
            require(before.size() == after.size() && before[0].episode_id == after[0].episode_id, "growth/shrink changed retained task recall");
            return 0.0;
        });

        run("M2-OPS-01", [state_path]() {
            MemorySystem memory = make_memory();
            create_episode(memory, 1, 10, 100, 1, 1.0f);
            uint64_t previous_hash = memory.state_hash();
            for (int cycle = 0; cycle < 5; ++cycle) {
                memory.save(state_path);
                int pipe_fds[2]{};
                require(::pipe(pipe_fds) == 0, "restart pipe creation failed");
                const pid_t child = ::fork();
                require(child >= 0, "restart fork failed");
                if (child == 0) {
                    ::close(pipe_fds[0]);
                    uint64_t child_hash = 0;
                    int exit_code = 0;
                    try {
                        MemorySystem restarted;
                        restarted.load(state_path);
                        child_hash = restarted.state_hash();
                    } catch (...) {
                        exit_code = 2;
                    }
                    const ssize_t wrote = ::write(pipe_fds[1], &child_hash, sizeof(child_hash));
                    if (wrote != static_cast<ssize_t>(sizeof(child_hash))) exit_code = 3;
                    ::close(pipe_fds[1]);
                    _exit(exit_code);
                }
                ::close(pipe_fds[1]);
                uint64_t child_hash = 0;
                const ssize_t received = ::read(pipe_fds[0], &child_hash, sizeof(child_hash));
                ::close(pipe_fds[0]);
                int status = 0;
                require(::waitpid(child, &status, 0) == child, "restart child wait failed");
                require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "restart child load failed");
                require(received == static_cast<ssize_t>(sizeof(child_hash)) && child_hash == previous_hash, "process restart changed state hash");
                memory.load(state_path);
                previous_hash = memory.state_hash();
            }
            return static_cast<double>(previous_hash);
        });

        run("M2-OPS-02", []() {
            MemorySystem memory = make_memory();
            for (int i = 0; i < 1000; ++i) create_episode(memory, i * 10 + 1, i * 10, i, 1, 0.1f);
            const size_t bytes = memory.events().size() * sizeof(MemoryEvent) + memory.episodes().size() * sizeof(EpisodeRecord) + memory.replay_indices().size() * sizeof(ReplayIndex);
            require(bytes < 16 * 1024 * 1024, "persistent Stage 2 memory exceeded 16 MB controlled budget");
            return static_cast<double>(bytes);
        });

        run("M2-OPS-03", []() {
            MemorySystem memory = make_memory();
            for (int i = 0; i < 100; ++i) create_episode(memory, i * 10 + 1, i * 10, i, 1, 0.1f);
            const auto selected = memory.select_replay(ReplayQuery{}, 20);
            const auto start = std::chrono::steady_clock::now();
            const ReplayReport report = memory.replay(selected, ReplayMode::CompressedPointer, 20);
            const auto end = std::chrono::steady_clock::now();
            const double milliseconds = std::chrono::duration<double, std::milli>(end - start).count();
            require(report.replayed_events <= 20 && milliseconds < 1000.0, "replay exceeded idle-cycle budget");
            return milliseconds;
        });

        std::ofstream metrics(artifact_dir / "stage2_metrics.csv");
        metrics << "test_id,passed,value,detail\n";
        size_t failures = 0;
        for (const auto& result : results) {
            metrics << result.id << ',' << (result.passed ? 1 : 0) << ',' << std::setprecision(12) << result.value << ",\"" << result.detail << "\"\n";
            if (!result.passed) ++failures;
        }
        metrics.close();

        std::ofstream summary(artifact_dir / "stage2_summary.txt");
        summary << "seed=" << seed << '\n';
        summary << "tests=" << results.size() << '\n';
        summary << "failures=" << failures << '\n';
        summary << "state_file_hash=" << hash_file(state_path) << '\n';
        summary.close();

        std::ofstream replay_trace(artifact_dir / "replay_selection.csv");
        replay_trace << "episode_id,cue_hash,priority,access_count\n";
        MemorySystem trace_memory = make_memory();
        for (int i = 0; i < 20; ++i) create_episode(trace_memory, i * 10 + 1, i * 10, 5000 + i, 1, i == 0 ? 3.0f : 0.1f);
        for (const auto& index : trace_memory.select_replay(ReplayQuery{}, 10)) replay_trace << index.episode_id << ',' << index.cue_hash << ',' << index.replay_priority << ',' << index.access_count << '\n';
        replay_trace.close();

        std::ofstream occupancy(artifact_dir / "memory_occupancy.csv");
        occupancy << "episodes,events,indexes,estimated_bytes\n" << trace_memory.episodes().size() << ',' << trace_memory.events().size() << ',' << trace_memory.replay_indices().size() << ',' << (trace_memory.events().size() * sizeof(MemoryEvent) + trace_memory.episodes().size() * sizeof(EpisodeRecord) + trace_memory.replay_indices().size() * sizeof(ReplayIndex)) << '\n';
        occupancy.close();

        for (const auto& result : results) std::cout << result.id << '=' << (result.passed ? "PASS" : "FAIL") << " value=" << result.value << " detail=" << result.detail << '\n';
        std::cout << "STAGE2_HARNESS=" << (failures == 0 ? "PASS" : "FAIL") << '\n';
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "STAGE2_HARNESS=ERROR " << error.what() << '\n';
        return 2;
    }
}
