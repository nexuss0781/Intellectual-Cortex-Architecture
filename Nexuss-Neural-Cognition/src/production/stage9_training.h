#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace genesis {

inline uint64_t stage9_mix(uint64_t hash, const uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U);
    hash *= 1099511628211ULL;
    return hash;
}

inline uint64_t stage9_hash_string(const std::string& value) {
    uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : value) hash = stage9_mix(hash, static_cast<uint64_t>(character));
    return hash;
}

inline uint64_t stage9_hash_float(const float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return static_cast<uint64_t>(bits);
}

struct ModelIdentity {
    std::string model_id;
    std::string weight_digest;
    std::string tokenizer_digest;
    std::string license;
    std::string base_model_card_digest;
    uint64_t parameter_count = 0;
};

struct TrainingRunManifest {
    std::string run_id;
    ModelIdentity base;
    std::string dataset_release_digest;
    std::string split_manifest_digest;
    std::string code_commit;
    std::string container_digest;
    std::string hardware_manifest;
    std::string optimizer_config_digest;
    std::string seed_policy;
    std::vector<std::string> checkpoint_digests;
};

struct CheckpointEvaluation {
    std::string checkpoint_digest;
    double domain_score = 0.0;
    double general_retention = 0.0;
    double safety_score = 0.0;
    double citation_or_grounding_score = 0.0;
    double p95_latency_ms = 0.0;
    double peak_gpu_memory_mb = 0.0;
    double energy_or_compute = 0.0;
};

struct TrainingExample {
    std::string example_id;
    std::string split;
    std::string mixture;
    std::string language;
    std::string text;
    bool hidden = false;
    bool benchmark_marker = false;
    size_t supervised_start = 1U;
};

struct ModelSnapshot {
    std::vector<float> weights;
    std::vector<float> bias;
    uint64_t seed = 0;
    uint64_t steps = 0;
};

struct PreferencePair {
    std::string example_id;
    TrainingExample chosen;
    TrainingExample rejected;
};

struct LanguageEvaluation {
    double loss = 0.0;
    double accuracy = 0.0;
    size_t transitions = 0;
};

class ByteLanguageModel {
public:
    static constexpr size_t kVocabulary = 256U;
    static constexpr size_t kParameters = kVocabulary * kVocabulary + kVocabulary;

    explicit ByteLanguageModel(const uint64_t seed = 424242ULL) : seed_(seed), weights_(kParameters - kVocabulary, 0.0F), bias_(kVocabulary, 0.0F) { initialize(); }

    void initialize() {
        uint64_t state = seed_;
        for (float& weight : weights_) {
            state = stage9_mix(state, 0xA5A5A5A5ULL);
            const double centered = static_cast<double>(state % 2001ULL) / 1000000.0 - 0.001;
            weight = static_cast<float>(centered);
        }
        std::fill(bias_.begin(), bias_.end(), 0.0F);
        steps_ = 0;
    }

    void train(const std::vector<TrainingExample>& examples, const size_t epochs, const double learning_rate) {
        for (size_t epoch = 0; epoch < epochs; ++epoch) {
            for (const auto& example : examples) {
                if (example.hidden || example.benchmark_marker || example.text.size() < 2U) continue;
                for (size_t index = 1; index < example.text.size(); ++index) {
                    const size_t previous = static_cast<unsigned char>(example.text[index - 1U]);
                    const size_t target = static_cast<unsigned char>(example.text[index]);
                    const size_t row = previous * kVocabulary;
                    float maximum = -std::numeric_limits<float>::infinity();
                    for (size_t output = 0; output < kVocabulary; ++output) maximum = std::max(maximum, weights_[row + output] + bias_[output]);
                    double normalizer = 0.0;
                    for (size_t output = 0; output < kVocabulary; ++output) normalizer += std::exp(static_cast<double>(weights_[row + output] + bias_[output] - maximum));
                    for (size_t output = 0; output < kVocabulary; ++output) {
                        const double probability = std::exp(static_cast<double>(weights_[row + output] + bias_[output] - maximum)) / normalizer;
                        const double gradient = probability - (output == target ? 1.0 : 0.0);
                        weights_[row + output] = static_cast<float>(weights_[row + output] - learning_rate * gradient);
                        bias_[output] = static_cast<float>(bias_[output] - learning_rate * 0.05 * gradient);
                    }
                    ++steps_;
                }
            }
        }
    }

    LanguageEvaluation evaluate(const std::vector<TrainingExample>& examples) const {
        LanguageEvaluation evaluation;
        double loss = 0.0;
        for (const auto& example : examples) {
            if (example.hidden || example.benchmark_marker || example.text.size() < 2U) continue;
            for (size_t index = 1; index < example.text.size(); ++index) {
                const size_t previous = static_cast<unsigned char>(example.text[index - 1U]);
                const size_t target = static_cast<unsigned char>(example.text[index]);
                const size_t predicted = predict_next(static_cast<unsigned char>(previous));
                if (predicted == target) evaluation.accuracy += 1.0;
                const size_t row = previous * kVocabulary;
                float maximum = -std::numeric_limits<float>::infinity();
                for (size_t output = 0; output < kVocabulary; ++output) maximum = std::max(maximum, weights_[row + output] + bias_[output]);
                double normalizer = 0.0;
                for (size_t output = 0; output < kVocabulary; ++output) normalizer += std::exp(static_cast<double>(weights_[row + output] + bias_[output] - maximum));
                const double probability = std::exp(static_cast<double>(weights_[row + target] + bias_[target] - maximum)) / normalizer;
                loss += -std::log(std::max(probability, 1e-12));
                ++evaluation.transitions;
            }
        }
        if (evaluation.transitions > 0U) { evaluation.accuracy /= static_cast<double>(evaluation.transitions); evaluation.loss = loss / static_cast<double>(evaluation.transitions); }
        return evaluation;
    }

    void train_supervised(const std::vector<TrainingExample>& examples, const size_t epochs, const double learning_rate) {
        for (size_t epoch = 0; epoch < epochs; ++epoch) {
            for (const auto& example : examples) {
                if (example.hidden || example.benchmark_marker || example.text.size() < 2U) continue;
                const size_t start = std::min(std::max<size_t>(1U, example.supervised_start), example.text.size() - 1U);
                for (size_t index = start; index < example.text.size(); ++index) {
                    const size_t previous = static_cast<unsigned char>(example.text[index - 1U]);
                    const size_t target = static_cast<unsigned char>(example.text[index]);
                    const size_t row = previous * kVocabulary;
                    float maximum = -std::numeric_limits<float>::infinity();
                    for (size_t output = 0; output < kVocabulary; ++output) maximum = std::max(maximum, weights_[row + output] + bias_[output]);
                    double normalizer = 0.0;
                    for (size_t output = 0; output < kVocabulary; ++output) normalizer += std::exp(static_cast<double>(weights_[row + output] + bias_[output] - maximum));
                    for (size_t output = 0; output < kVocabulary; ++output) {
                        const double probability = std::exp(static_cast<double>(weights_[row + output] + bias_[output] - maximum)) / normalizer;
                        const double gradient = probability - (output == target ? 1.0 : 0.0);
                        weights_[row + output] = static_cast<float>(weights_[row + output] - learning_rate * gradient);
                        bias_[output] = static_cast<float>(bias_[output] - learning_rate * 0.05 * gradient);
                    }
                    ++steps_;
                }
            }
        }
    }

    double score_supervised(const TrainingExample& example) const {
        if (example.hidden || example.benchmark_marker || example.text.size() < 2U) return 0.0;
        const size_t start = std::min(std::max<size_t>(1U, example.supervised_start), example.text.size() - 1U);
        double score = 0.0;
        size_t transitions = 0U;
        for (size_t index = start; index < example.text.size(); ++index) {
            const size_t previous = static_cast<unsigned char>(example.text[index - 1U]);
            const size_t target = static_cast<unsigned char>(example.text[index]);
            const size_t row = previous * kVocabulary;
            float maximum = -std::numeric_limits<float>::infinity();
            for (size_t output = 0; output < kVocabulary; ++output) maximum = std::max(maximum, weights_[row + output] + bias_[output]);
            double normalizer = 0.0;
            for (size_t output = 0; output < kVocabulary; ++output) normalizer += std::exp(static_cast<double>(weights_[row + output] + bias_[output] - maximum));
            const double probability = std::exp(static_cast<double>(weights_[row + target] + bias_[target] - maximum)) / normalizer;
            score += std::log(std::max(probability, 1e-12));
            ++transitions;
        }
        return transitions == 0U ? 0.0 : score / static_cast<double>(transitions);
    }

    void train_preference(const std::vector<PreferencePair>& pairs, const size_t epochs, const double learning_rate, const double beta) {
        for (size_t epoch = 0; epoch < epochs; ++epoch) {
            for (const auto& pair : pairs) {
                const double chosen_score = score_supervised(pair.chosen);
                const double rejected_score = score_supervised(pair.rejected);
                const double margin = std::max(-30.0, std::min(30.0, beta * (chosen_score - rejected_score)));
                const double preference_weight = 1.0 / (1.0 + std::exp(margin));
                apply_supervised_gradient(pair.chosen, learning_rate * preference_weight);
                apply_supervised_gradient(pair.rejected, -learning_rate * preference_weight);
            }
        }
    }

    LanguageEvaluation evaluate_supervised(const std::vector<TrainingExample>& examples) const {
        LanguageEvaluation evaluation;
        double loss = 0.0;
        for (const auto& example : examples) {
            if (example.hidden || example.benchmark_marker || example.text.size() < 2U) continue;
            const size_t start = std::min(std::max<size_t>(1U, example.supervised_start), example.text.size() - 1U);
            for (size_t index = start; index < example.text.size(); ++index) {
                const size_t previous = static_cast<unsigned char>(example.text[index - 1U]);
                const size_t target = static_cast<unsigned char>(example.text[index]);
                const size_t predicted = predict_next(static_cast<unsigned char>(previous));
                if (predicted == target) evaluation.accuracy += 1.0;
                const size_t row = previous * kVocabulary;
                float maximum = -std::numeric_limits<float>::infinity();
                for (size_t output = 0; output < kVocabulary; ++output) maximum = std::max(maximum, weights_[row + output] + bias_[output]);
                double normalizer = 0.0;
                for (size_t output = 0; output < kVocabulary; ++output) normalizer += std::exp(static_cast<double>(weights_[row + output] + bias_[output] - maximum));
                const double probability = std::exp(static_cast<double>(weights_[row + target] + bias_[target] - maximum)) / normalizer;
                loss += -std::log(std::max(probability, 1e-12));
                ++evaluation.transitions;
            }
        }
        if (evaluation.transitions > 0U) { evaluation.accuracy /= static_cast<double>(evaluation.transitions); evaluation.loss = loss / static_cast<double>(evaluation.transitions); }
        return evaluation;
    }

    size_t predict_next(const unsigned char previous) const {
        const size_t row = static_cast<size_t>(previous) * kVocabulary;
        size_t best = 0;
        float best_value = -std::numeric_limits<float>::infinity();
        for (size_t output = 0; output < kVocabulary; ++output) {
            const float value = weights_[row + output] + bias_[output];
            if (value > best_value) { best_value = value; best = output; }
        }
        return best;
    }

    ModelSnapshot snapshot() const { return {weights_, bias_, seed_, steps_}; }
    bool restore(const ModelSnapshot& snapshot) {
        if (snapshot.weights.size() != weights_.size() || snapshot.bias.size() != bias_.size()) return false;
        weights_ = snapshot.weights; bias_ = snapshot.bias; seed_ = snapshot.seed; steps_ = snapshot.steps; return true;
    }
    uint64_t checkpoint_hash() const {
        uint64_t hash = stage9_mix(seed_, steps_);
        for (const float weight : weights_) hash = stage9_mix(hash, stage9_hash_float(weight));
        for (const float value : bias_) hash = stage9_mix(hash, stage9_hash_float(value));
        return hash;
    }
    std::string model_digest() const { return "model@" + std::to_string(checkpoint_hash()); }
    std::string tokenizer_digest() const { return "tokenizer@identity-byte-v1"; }
    size_t parameter_count() const { return kParameters; }
    size_t parameter_bytes() const { return (weights_.size() + bias_.size()) * sizeof(float); }
    uint64_t steps() const { return steps_; }

private:
    void apply_supervised_gradient(const TrainingExample& example, const double learning_rate) {
        if (example.hidden || example.benchmark_marker || example.text.size() < 2U) return;
        const size_t start = std::min(std::max<size_t>(1U, example.supervised_start), example.text.size() - 1U);
        const double transition_scale = 1.0 / static_cast<double>(std::max<size_t>(1U, example.text.size() - start));
        for (size_t index = start; index < example.text.size(); ++index) {
            const size_t previous = static_cast<unsigned char>(example.text[index - 1U]);
            const size_t target = static_cast<unsigned char>(example.text[index]);
            const size_t row = previous * kVocabulary;
            float maximum = -std::numeric_limits<float>::infinity();
            for (size_t output = 0; output < kVocabulary; ++output) maximum = std::max(maximum, weights_[row + output] + bias_[output]);
            double normalizer = 0.0;
            for (size_t output = 0; output < kVocabulary; ++output) normalizer += std::exp(static_cast<double>(weights_[row + output] + bias_[output] - maximum));
            for (size_t output = 0; output < kVocabulary; ++output) {
                const double probability = std::exp(static_cast<double>(weights_[row + output] + bias_[output] - maximum)) / normalizer;
                const double gradient = probability - (output == target ? 1.0 : 0.0);
                weights_[row + output] = static_cast<float>(weights_[row + output] - learning_rate * transition_scale * gradient);
                bias_[output] = static_cast<float>(bias_[output] - learning_rate * transition_scale * 0.05 * gradient);
            }
            ++steps_;
        }
    }

    uint64_t seed_ = 0;
    std::vector<float> weights_;
    std::vector<float> bias_;
    uint64_t steps_ = 0;
};

class IndependentBigramReference {
public:
    void train(const std::vector<TrainingExample>& examples) {
        for (const auto& example : examples) if (!example.hidden && !example.benchmark_marker) for (size_t index = 1; index < example.text.size(); ++index) ++counts_[static_cast<unsigned char>(example.text[index - 1U])][static_cast<unsigned char>(example.text[index])];
    }
    LanguageEvaluation evaluate(const std::vector<TrainingExample>& examples) const {
        LanguageEvaluation evaluation;
        for (const auto& example : examples) if (!example.hidden && !example.benchmark_marker) for (size_t index = 1; index < example.text.size(); ++index) { const auto& row = counts_[static_cast<unsigned char>(example.text[index - 1U])]; const size_t target = static_cast<unsigned char>(example.text[index]); size_t best = 0; for (size_t output = 1; output < 256U; ++output) if (row[output] > row[best]) best = output; if (best == target) ++evaluation.accuracy; ++evaluation.transitions; }
        if (evaluation.transitions > 0U) evaluation.accuracy /= static_cast<double>(evaluation.transitions);
        evaluation.loss = 1.0 - evaluation.accuracy;
        return evaluation;
    }
    uint64_t digest() const { uint64_t hash = 0; for (const auto& row : counts_) for (const auto count : row) hash = stage9_mix(hash, count); return hash; }
private:
    std::array<std::array<uint32_t, 256>, 256> counts_{};
};

class IndependentFrequencyReference {
public:
    void train(const std::vector<TrainingExample>& examples) { for (const auto& example : examples) if (!example.hidden && !example.benchmark_marker) for (const unsigned char character : example.text) ++counts_[character]; }
    LanguageEvaluation evaluate(const std::vector<TrainingExample>& examples) const {
        LanguageEvaluation evaluation; size_t best = 0; for (size_t index = 1; index < 256U; ++index) if (counts_[index] > counts_[best]) best = index;
        for (const auto& example : examples) if (!example.hidden && !example.benchmark_marker) for (const unsigned char character : example.text) { if (character == best) ++evaluation.accuracy; ++evaluation.transitions; }
        if (evaluation.transitions > 0U) {
            evaluation.accuracy /= static_cast<double>(evaluation.transitions);
        }
        evaluation.loss = 1.0 - evaluation.accuracy;
        return evaluation;
    }
    uint64_t digest() const { uint64_t hash = 0; for (const auto count : counts_) hash = stage9_mix(hash, count); return hash; }
private:
    std::array<uint64_t, 256> counts_{};
};

class TrainingRegistry {
public:
    bool start(const TrainingRunManifest& manifest) {
        const bool valid = !manifest.run_id.empty() && !manifest.base.model_id.empty() && !manifest.base.weight_digest.empty() && manifest.base.tokenizer_digest == "tokenizer@identity-byte-v1" && !manifest.base.license.empty() && !manifest.base.base_model_card_digest.empty() && manifest.base.parameter_count > 0U && !manifest.dataset_release_digest.empty() && !manifest.split_manifest_digest.empty() && !manifest.code_commit.empty() && !manifest.container_digest.empty() && !manifest.hardware_manifest.empty() && !manifest.optimizer_config_digest.empty() && !manifest.seed_policy.empty();
        if (valid) { manifest_ = manifest; started_ = true; run_hash_ = canonical_hash(manifest); }
        return valid;
    }
    bool record_checkpoint(const CheckpointEvaluation& evaluation) {
        if (!started_ || evaluation.checkpoint_digest.empty()) return false;
        checkpoints_[evaluation.checkpoint_digest] = evaluation; return true;
    }
    bool approve_checkpoint(const std::string& digest) {
        const auto found = checkpoints_.find(digest); if (found == checkpoints_.end()) return false;
        const auto& evaluation = found->second;
        const bool valid = evaluation.domain_score >= 0.01 && evaluation.general_retention >= 0.80 && evaluation.safety_score >= 0.95 && evaluation.p95_latency_ms <= 100.0 && evaluation.peak_gpu_memory_mb <= 512.0;
        if (valid) approved_.insert(digest);
        return valid;
    }
    bool reproduce(const std::string& run_id) const { return started_ && run_id == manifest_.run_id && run_hash_ == canonical_hash(manifest_); }
    bool approved(const std::string& digest) const { return approved_.find(digest) != approved_.end(); }
    size_t checkpoint_count() const { return checkpoints_.size(); }
private:
    static uint64_t canonical_hash(const TrainingRunManifest& manifest) { std::ostringstream output; output << manifest.run_id << '|' << manifest.base.model_id << '|' << manifest.base.weight_digest << '|' << manifest.base.tokenizer_digest << '|' << manifest.dataset_release_digest << '|' << manifest.split_manifest_digest << '|' << manifest.code_commit << '|' << manifest.container_digest << '|' << manifest.hardware_manifest << '|' << manifest.optimizer_config_digest << '|' << manifest.seed_policy; return stage9_hash_string(output.str()); }
    TrainingRunManifest manifest_;
    std::map<std::string, CheckpointEvaluation> checkpoints_;
    std::set<std::string> approved_;
    uint64_t run_hash_ = 0;
    bool started_ = false;
};

} // namespace genesis
