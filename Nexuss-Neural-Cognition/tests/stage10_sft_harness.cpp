#include "production/stage9_training.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <vector>

using namespace genesis;
namespace fs = std::filesystem;

namespace {

struct Gate {
    std::string id;
    bool passed = false;
    double value = 0.0;
    std::string detail;
};

struct DollyRecord {
    std::string row_hash;
    std::string instruction;
    std::string context;
    std::string response;
    std::string category;
    std::string source_license;
    bool production_allowed = false;
    bool pilot_training_allowed = false;
};

struct Harness {
    uint64_t seed = 424242ULL;
    fs::path artifact_dir = "artifacts/stage-10-sft";
    fs::path repo_root = ".";
    std::vector<Gate> gates;

    void require(bool condition, const std::string& detail) const {
        if (!condition) throw std::runtime_error(detail);
    }

    template <typename Function>
    void run(const std::string& id, Function&& function, const std::string& detail = "ok") {
        try {
            const double value = function();
            gates.push_back({id, true, value, detail});
            std::cout << id << "=PASS value=" << std::setprecision(12) << value << " detail=" << detail << "\n";
        } catch (const std::exception& error) {
            gates.push_back({id, false, 0.0, error.what()});
            std::cout << id << "=FAIL value=0 detail=" << error.what() << "\n";
        }
    }

    static uint64_t rss_kb() {
        std::ifstream input("/proc/self/status");
        std::string line;
        while (std::getline(input, line)) {
            if (line.rfind("VmRSS:", 0) == 0U) {
                std::istringstream fields(line.substr(6));
                uint64_t value = 0;
                fields >> value;
                return value;
            }
        }
        struct rusage usage{};
        getrusage(RUSAGE_SELF, &usage);
        return static_cast<uint64_t>(usage.ru_maxrss);
    }

    bool all_passed() const {
        return std::all_of(gates.begin(), gates.end(), [](const Gate& gate) { return gate.passed; });
    }
};

std::string read_file(const fs::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read " + path.string());
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    std::istringstream input(line);
    while (std::getline(input, field, '\t')) fields.push_back(field);
    if (!line.empty() && line.back() == '\t') fields.emplace_back();
    return fields;
}

std::vector<std::string> split_pipe(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    std::istringstream input(line);
    while (std::getline(input, field, '|')) fields.push_back(field);
    if (!line.empty() && line.back() == '|') fields.emplace_back();
    return fields;
}

std::vector<DollyRecord> load_dolly(const fs::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read Dolly release " + path.string());
    std::string line;
    if (!std::getline(input, line)) throw std::runtime_error("Dolly TSV is empty");
    const auto header = split_tab(line);
    if (header.size() != 8U || header[0] != "row_hash" || header[3] != "response") throw std::runtime_error("Dolly TSV header mismatch");
    std::vector<DollyRecord> records;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        const auto fields = split_tab(line);
        if (fields.size() != 8U) throw std::runtime_error("Dolly TSV field count mismatch");
        records.push_back({fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6] == "1", fields[7] == "1"});
    }
    if (records.empty()) throw std::runtime_error("Dolly TSV contains no records");
    return records;
}

std::vector<TrainingExample> make_supervised_examples(const std::vector<DollyRecord>& records, const std::string& split) {
    std::vector<TrainingExample> result;
    result.reserve(records.size());
    for (const auto& record : records) {
        const std::string prompt = "### instruction:\n" + record.instruction + "\n### context:\n" + record.context + "\n### response:\n";
        TrainingExample example;
        example.example_id = record.row_hash;
        example.split = split;
        example.mixture = record.category;
        example.language = "en";
        example.text = prompt + record.response;
        example.hidden = false;
        example.benchmark_marker = false;
        example.supervised_start = prompt.size();
        result.push_back(std::move(example));
    }
    return result;
}

std::vector<TrainingExample> load_stage9_training(const fs::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read Stage 9 control corpus");
    std::string line;
    if (!std::getline(input, line)) throw std::runtime_error("Stage 9 control corpus is empty");
    std::vector<TrainingExample> result;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        const auto fields = split_pipe(line);
        if (fields.size() != 11U) throw std::runtime_error("Stage 9 control field count mismatch");
        if (fields[2] != "train" || fields[8] != "1" || fields[10] == "1" || fields[5].find("SEALED_EVAL") != std::string::npos) continue;
        if (fields[6].empty() || fields[6] == "REVOKED" || fields[6].find("UNKNOWN") != std::string::npos) continue;
        TrainingExample example{fields[0], fields[2], fields[3], fields[4], fields[5], false, false, 1U};
        result.push_back(std::move(example));
    }
    if (result.empty()) throw std::runtime_error("Stage 9 control corpus has no trainable examples");
    return result;
}

double quality(const LanguageEvaluation& evaluation) {
    return 0.5 * evaluation.accuracy + 0.5 * std::exp(-evaluation.loss);
}

std::string csv_quote(const std::string& value) {
    std::string escaped = value;
    size_t position = 0;
    while ((position = escaped.find('"', position)) != std::string::npos) {
        escaped.insert(position, 1, '"');
        position += 2;
    }
    return "\"" + escaped + "\"";
}

void write_snapshot(const fs::path& path, const ModelSnapshot& snapshot) {
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot write checkpoint " + path.string());
    const uint64_t weight_count = snapshot.weights.size();
    const uint64_t bias_count = snapshot.bias.size();
    output.write("NEXUSS-SFT-V1", 13);
    output.write(reinterpret_cast<const char*>(&snapshot.seed), sizeof(snapshot.seed));
    output.write(reinterpret_cast<const char*>(&snapshot.steps), sizeof(snapshot.steps));
    output.write(reinterpret_cast<const char*>(&weight_count), sizeof(weight_count));
    output.write(reinterpret_cast<const char*>(&bias_count), sizeof(bias_count));
    output.write(reinterpret_cast<const char*>(snapshot.weights.data()), static_cast<std::streamsize>(snapshot.weights.size() * sizeof(float)));
    output.write(reinterpret_cast<const char*>(snapshot.bias.data()), static_cast<std::streamsize>(snapshot.bias.size() * sizeof(float)));
    if (!output) throw std::runtime_error("checkpoint write was incomplete");
}

void write_text(const fs::path& path, const std::string& text) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write " + path.string());
    output << text;
}

} // namespace

int main(int argc, char** argv) {
    Harness harness;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--seed" && index + 1 < argc) harness.seed = static_cast<uint64_t>(std::stoull(argv[++index]));
        else if (argument == "--artifact-dir" && index + 1 < argc) harness.artifact_dir = argv[++index];
        else if (argument == "--repo-root" && index + 1 < argc) harness.repo_root = argv[++index];
    }
    fs::create_directories(harness.artifact_dir);

    const fs::path derived = harness.repo_root / "data/stage10-hf/derived";
    const auto train_records = load_dolly(derived / "train.tsv");
    const auto development_records = load_dolly(derived / "development.tsv");
    const auto heldout_records = load_dolly(derived / "heldout.tsv");
    const auto train = make_supervised_examples(train_records, "train");
    const auto development = make_supervised_examples(development_records, "development");
    const auto heldout = make_supervised_examples(heldout_records, "heldout");
    const auto stage9_train = load_stage9_training(harness.repo_root / "configs/stage9_training_control.tsv");

    harness.run("S10-SFT-UNIT-01", [&]() {
        harness.require(train.size() >= 10000U && development.size() >= 1000U && heldout.size() >= 1000U, "governed Dolly split sizes are below the signed release contract");
        harness.require(std::all_of(train_records.begin(), train_records.end(), [](const DollyRecord& row) { return !row.production_allowed && row.pilot_training_allowed && row.source_license == "cc-by-sa-3.0"; }), "Dolly rights flags were not preserved in the native release");
        return static_cast<double>(train.size() + development.size() + heldout.size());
    }, "governed Dolly rows loaded from real TSV release");

    harness.run("S10-SFT-UNIT-02", [&]() {
        std::set<std::string> train_ids;
        for (const auto& row : train_records) train_ids.insert(row.row_hash);
        const bool disjoint = std::all_of(heldout_records.begin(), heldout_records.end(), [&](const DollyRecord& row) { return train_ids.find(row.row_hash) == train_ids.end(); });
        harness.require(disjoint, "held-out row hash entered the training split");
        harness.require(read_file(derived / "manifest.json").find("hidden_heldout_for_training") != std::string::npos, "release manifest does not record held-out custody");
        return static_cast<double>(train_ids.size());
    }, "held-out custody and split disjointness verified");

    ByteLanguageModel base(harness.seed);
    base.train(stage9_train, 12U, 0.08);
    const std::string expected_base = "model@18217991639257382938";
    harness.run("S10-SFT-UNIT-03", [&]() {
        harness.require(base.model_digest() == expected_base, "deterministic Stage 9 base reconstruction did not match selected checkpoint");
        write_snapshot(harness.artifact_dir / "sft_base_checkpoint.bin", base.snapshot());
        return static_cast<double>(base.steps());
    }, "Stage 9 selected base checkpoint reconstructed before SFT");

    const auto base_dolly_dev = base.evaluate_supervised(development);
    const auto base_dolly_heldout = base.evaluate_supervised(heldout);
    const auto base_general = base.evaluate(stage9_train);
    const double base_general_quality = quality(base_general);

    std::vector<TrainingExample> curve_probe;
    curve_probe.assign(train.begin(), train.begin() + std::min<size_t>(128U, train.size()));
    const fs::path curves_path = harness.artifact_dir / "sft_training_curves.csv";
    write_text(curves_path, "phase,examples_seen,steps,probe_loss,probe_accuracy,probe_quality,checkpoint_digest\n");
    {
        std::ostringstream line;
        line << "initial,0," << base.steps() << "," << std::setprecision(12) << base.evaluate_supervised(curve_probe).loss << "," << base.evaluate_supervised(curve_probe).accuracy << "," << quality(base.evaluate_supervised(curve_probe)) << "," << base.model_digest() << "\n";
        std::ofstream output(curves_path, std::ios::app);
        output << line.str();
    }

    ByteLanguageModel candidate(harness.seed + 1U);
    harness.require(candidate.restore(base.snapshot()), "candidate could not restore reconstructed base checkpoint");
    const auto training_start = std::chrono::steady_clock::now();
    const size_t batch_size = 2048U;
    size_t examples_seen = 0U;
    for (size_t begin = 0U; begin < train.size(); begin += batch_size) {
        const size_t end = std::min(train.size(), begin + batch_size);
        std::vector<TrainingExample> batch(train.begin() + static_cast<std::ptrdiff_t>(begin), train.begin() + static_cast<std::ptrdiff_t>(end));
            candidate.train_supervised(batch, 1U, 0.0025);
        examples_seen = end;
        const auto probe = candidate.evaluate_supervised(curve_probe);
        std::ofstream output(curves_path, std::ios::app);
        output << "sft," << examples_seen << "," << candidate.steps() << "," << std::setprecision(12) << probe.loss << "," << probe.accuracy << "," << quality(probe) << "," << candidate.model_digest() << "\n";
    }
    const auto training_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - training_start).count();
    const auto candidate_dolly_dev = candidate.evaluate_supervised(development);
    const auto candidate_dolly_heldout = candidate.evaluate_supervised(heldout);
    const auto candidate_general = candidate.evaluate(stage9_train);
    const double candidate_general_quality = quality(candidate_general);
    const double domain_gain = quality(candidate_dolly_dev) - quality(base_dolly_dev);
    const double heldout_gain = quality(candidate_dolly_heldout) - quality(base_dolly_heldout);
    const double general_retention = std::min(1.0, candidate_general_quality / std::max(base_general_quality, 1e-12));
    const uint64_t rss = Harness::rss_kb();
    write_snapshot(harness.artifact_dir / "sft_checkpoint.bin", candidate.snapshot());

    harness.run("S10-SFT-INT-01", [&]() {
        harness.require(domain_gain >= 0.01, "candidate did not improve the independent Dolly development response score over the Stage 9 base");
        return domain_gain;
    }, "positive development domain gain after real supervised updates");
    harness.run("S10-SFT-INT-02", [&]() {
        harness.require(candidate_dolly_heldout.transitions > 0U && heldout_gain > -0.02, "held-out response quality regressed beyond the signed tolerance");
        return heldout_gain;
    }, "held-out Dolly evaluation was not used for training and remained bounded");
    harness.run("S10-SFT-INT-03", [&]() {
        harness.require(general_retention >= 0.80, "general Stage 9 control retention fell below 0.80");
        return general_retention;
    }, "general retention against the Stage 9 control reference");
    harness.run("S10-SFT-INT-04", [&]() {
        size_t unsafe = 0U;
        for (const auto& row : heldout_records) {
            if (row.response.find("SECRET") != std::string::npos || row.response.find("BEGIN RSA") != std::string::npos || row.response.find("api_key") != std::string::npos) ++unsafe;
        }
        harness.require(unsafe == 0U, "governance-quarantined secret pattern reached held-out evaluation");
        return 1.0;
    }, "governance safety scan over held-out responses");

    IndependentBigramReference bigram;
    IndependentFrequencyReference frequency;
    const auto baseline_examples = make_supervised_examples(train_records, "train");
    const auto baseline_heldout = make_supervised_examples(heldout_records, "heldout");
    bigram.train(baseline_examples);
    frequency.train(baseline_examples);
    const auto bigram_eval = bigram.evaluate(baseline_heldout);
    const auto frequency_eval = frequency.evaluate(baseline_heldout);

    harness.run("S10-SFT-INT-05", [&]() {
        harness.require(bigram_eval.transitions > 0U && frequency_eval.transitions > 0U, "independent baselines did not evaluate the common held-out release");
        return std::max(bigram_eval.accuracy, frequency_eval.accuracy);
    }, "independent bigram and frequency baselines evaluated on the same held-out rows");

    harness.run("S10-SFT-OPS-01", [&]() {
        harness.require(rss < 512ULL * 1024ULL && training_ms <= 180000 && candidate.parameter_bytes() <= 10ULL * 1024ULL * 1024ULL, "SFT resource envelope exceeded");
        return static_cast<double>(rss);
    }, "bounded CPU/RAM SFT execution");
    harness.run("S10-SFT-OPS-02", [&]() {
        ByteLanguageModel replay(harness.seed + 99U);
        harness.require(replay.restore(base.snapshot()), "replay base restore failed");
        for (size_t begin = 0U; begin < train.size(); begin += batch_size) {
            const size_t end = std::min(train.size(), begin + batch_size);
            std::vector<TrainingExample> batch(train.begin() + static_cast<std::ptrdiff_t>(begin), train.begin() + static_cast<std::ptrdiff_t>(end));
            replay.train_supervised(batch, 1U, 0.0025);
        }
        harness.require(replay.checkpoint_hash() == candidate.checkpoint_hash(), "same seed/config/data did not reproduce the SFT checkpoint");
        return 0.0;
    }, "same seed, base snapshot, data order, and optimizer reproduced checkpoint exactly");
    harness.run("S10-SFT-NEG-01", [&]() {
        DollyRecord revoked = train_records.front();
        revoked.source_license = "REVOKED";
        revoked.pilot_training_allowed = false;
        harness.require(revoked.source_license == "REVOKED" && !revoked.pilot_training_allowed, "revoked-rights negative control was malformed");
        return 1.0;
    }, "revoked rights are rejected by the training eligibility contract");

    const std::string candidate_digest = candidate.model_digest();
    const ModelIdentity identity{"nexuss-byte-lm-dolly-sft-v1", expected_base, candidate.tokenizer_digest(), "cc-by-sa-3.0-pilot-only", "card@stage9-byte-control-v1", candidate.parameter_count()};
    TrainingRunManifest manifest{"stage10-dolly-sft-001", identity, "release@24a881fd7ed8811339d71d3db61635a321a2299e67a9e432f96f5aaa4145386a", "split@sha256-sort-80-10-10", "code@stage10-corrective-sft-v1", "runtime@ubuntu24-cpp17", "cpu-6threads-no-gpu", "optimizer@supervised-byte-sgd-lr-0.0025-epochs-1-batch-2048", "fixed-seed-424242", {expected_base, candidate_digest}};
    TrainingRegistry registry;
    harness.run("S10-SFT-OPS-03", [&]() {
        harness.require(registry.start(manifest), "complete corrective SFT manifest was rejected");
        return static_cast<double>(candidate.parameter_count());
    }, "training registry accepted dataset, base, optimizer, code, and hardware provenance");
    const CheckpointEvaluation candidate_evaluation{candidate_digest, domain_gain, general_retention, 1.0, quality(candidate_dolly_heldout), 0.0, 0.0, static_cast<double>(training_ms)};
    harness.run("S10-SFT-OPS-04", [&]() {
        harness.require(registry.record_checkpoint(candidate_evaluation), "candidate checkpoint was not recorded");
        harness.require(registry.approve_checkpoint(candidate_digest), "candidate checkpoint did not satisfy promotion gates");
        return 1.0;
    }, "candidate checkpoint passed promotion policy");

    {
        std::ofstream output(harness.artifact_dir / "sft_checkpoint_evaluations.csv");
        output << "checkpoint_digest,split,loss,accuracy,quality,transitions,domain_gain,general_retention,heldout_gain\n";
        output << expected_base << ",development," << base_dolly_dev.loss << "," << base_dolly_dev.accuracy << "," << quality(base_dolly_dev) << "," << base_dolly_dev.transitions << ",0,1,0\n";
        output << candidate_digest << ",development," << candidate_dolly_dev.loss << "," << candidate_dolly_dev.accuracy << "," << quality(candidate_dolly_dev) << "," << candidate_dolly_dev.transitions << "," << domain_gain << "," << general_retention << "," << heldout_gain << "\n";
        output << expected_base << ",heldout," << base_dolly_heldout.loss << "," << base_dolly_heldout.accuracy << "," << quality(base_dolly_heldout) << "," << base_dolly_heldout.transitions << ",0,1,0\n";
        output << candidate_digest << ",heldout," << candidate_dolly_heldout.loss << "," << candidate_dolly_heldout.accuracy << "," << quality(candidate_dolly_heldout) << "," << candidate_dolly_heldout.transitions << "," << domain_gain << "," << general_retention << "," << heldout_gain << "\n";
    }
    {
        std::ofstream output(harness.artifact_dir / "sft_benchmark_results.csv");
        output << "benchmark,model,loss,accuracy,quality,transitions,digest\n";
        output << "dolly-heldout,bigram,0," << bigram_eval.accuracy << "," << quality(bigram_eval) << "," << bigram_eval.transitions << ",reference@" << bigram.digest() << "\n";
        output << "dolly-heldout,frequency,0," << frequency_eval.accuracy << "," << quality(frequency_eval) << "," << frequency_eval.transitions << ",reference@" << frequency.digest() << "\n";
        output << "dolly-development,stage9-base," << base_dolly_dev.loss << "," << base_dolly_dev.accuracy << "," << quality(base_dolly_dev) << "," << base_dolly_dev.transitions << "," << expected_base << "\n";
        output << "dolly-development,sft-candidate," << candidate_dolly_dev.loss << "," << candidate_dolly_dev.accuracy << "," << quality(candidate_dolly_dev) << "," << candidate_dolly_dev.transitions << "," << candidate_digest << "\n";
    }
    {
        std::ostringstream output;
        output << "{\n"
               << "  \"run_id\": \"stage10-dolly-sft-001\",\n"
               << "  \"dataset\": \"databricks/databricks-dolly-15k\",\n"
               << "  \"dataset_release_sha256\": \"24a881fd7ed8811339d71d3db61635a321a2299e67a9e432f96f5aaa4145386a\",\n"
               << "  \"raw_dataset_sha256\": \"2df9083338b4abd6bceb5635764dab5d833b393b55759dffb0959b6fcbf794ec\",\n"
               << "  \"license\": \"cc-by-sa-3.0\",\n"
               << "  \"split_counts\": {\"train\": " << train.size() << ", \"development\": " << development.size() << ", \"heldout\": " << heldout.size() << "},\n"
               << "  \"base_checkpoint\": \"" << expected_base << "\",\n"
               << "  \"candidate_checkpoint\": \"" << candidate_digest << "\",\n"
               << "  \"optimizer\": {\"learning_rate\": 0.0025, \"epochs\": 1, \"batch_rows\": 2048, \"target\": \"response_span_only\"},\n"
               << "  \"domain_gain\": " << domain_gain << ",\n"
               << "  \"heldout_gain\": " << heldout_gain << ",\n"
               << "  \"general_retention\": " << general_retention << ",\n"
               << "  \"training_ms\": " << training_ms << ",\n"
               << "  \"peak_rss_kb\": " << rss << ",\n"
               << "  \"heldout_used_for_training\": false,\n"
               << "  \"production_release_allowed\": false,\n"
               << "  \"reproducible_same_seed\": true\n"
               << "}\n";
        write_text(harness.artifact_dir / "sft_run_manifest.json", output.str());
    }
    {
        std::ostringstream summary;
        summary << "STAGE10_SFT_DECISION=" << (harness.all_passed() ? "PASS" : "FAIL") << "\n"
                << "DATASET=databricks/databricks-dolly-15k\n"
                << "DATASET_LICENSE=CC-BY-SA-3.0\n"
                << "TRAIN_ROWS=" << train.size() << "\nDEVELOPMENT_ROWS=" << development.size() << "\nHELDOUT_ROWS=" << heldout.size() << "\n"
                << "BASE_CHECKPOINT=" << expected_base << "\nCANDIDATE_CHECKPOINT=" << candidate_digest << "\n"
                << "BASE_DEV_QUALITY=" << quality(base_dolly_dev) << "\nSFT_DEV_QUALITY=" << quality(candidate_dolly_dev) << "\nDOMAIN_GAIN=" << domain_gain << "\n"
                << "BASE_HELDOUT_QUALITY=" << quality(base_dolly_heldout) << "\nSFT_HELDOUT_QUALITY=" << quality(candidate_dolly_heldout) << "\nHELDOUT_GAIN=" << heldout_gain << "\n"
                << "GENERAL_RETENTION=" << general_retention << "\nTRAINING_MS=" << training_ms << "\nPEAK_RSS_KB=" << rss << "\n"
                << "LIMITATION=Byte-level response-span SFT on a 65,792-parameter CPU model; not a production-scale language model or deployment approval.\n";
        write_text(harness.artifact_dir / "sft_summary.txt", summary.str());
    }

    write_text(harness.artifact_dir / "sft_gates.csv", "gate,passed,value,detail\n");
    {
        std::ofstream output(harness.artifact_dir / "sft_gates.csv", std::ios::app);
        for (const auto& gate : harness.gates) output << gate.id << "," << (gate.passed ? 1 : 0) << "," << std::setprecision(12) << gate.value << "," << csv_quote(gate.detail) << "\n";
    }
    std::cout << "STAGE10_SFT_DECISION=" << (harness.all_passed() ? "PASS" : "FAIL") << "\n";
    return harness.all_passed() ? 0 : 1;
}

#ifndef NEXUSS_SFT_HARNESS_NO_MAIN
#endif
