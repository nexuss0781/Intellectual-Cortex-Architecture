#pragma once

#include "predictive_workspace.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace genesis {

enum class PointerType : uint32_t { Unknown = 0, Lexical = 1, Entity = 2, Action = 3, Location = 4, Role = 5, Construction = 6, Scene = 7, Bound = 8, Modifier = 9 };
enum class LexicalCategory : uint32_t { Unknown = 0, Entity = 1, Action = 2, Location = 3, Modifier = 4, Quantity = 5, Temporal = 6, Function = 7 };
enum class SceneRole : uint32_t { Subject = 0, Verb = 1, Object = 2, Location = 3, Time = 4, Modality = 5, Modifier = 6 };

inline uint64_t language_mix(uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

struct SemanticPointer {
    uint64_t pointer_id = 0;
    PointerType type = PointerType::Unknown;
    uint32_t generation = 0;
    uint32_t dimension = 0;
    std::vector<int8_t> values;
    uint64_t checksum = 0;

    bool valid() const { return pointer_id != 0 && dimension == values.size() && dimension > 0 && checksum == compute_checksum(); }

    uint64_t compute_checksum() const {
        uint64_t hash = 1469598103934665603ULL;
        auto mix_byte = [&hash](uint8_t byte) { hash ^= byte; hash *= 1099511628211ULL; };
        for (size_t i = 0; i < sizeof(pointer_id); ++i) mix_byte(static_cast<uint8_t>((pointer_id >> (i * 8)) & 0xffU));
        for (size_t i = 0; i < sizeof(generation); ++i) mix_byte(static_cast<uint8_t>((generation >> (i * 8)) & 0xffU));
        for (size_t i = 0; i < sizeof(dimension); ++i) mix_byte(static_cast<uint8_t>((dimension >> (i * 8)) & 0xffU));
        mix_byte(static_cast<uint8_t>(type));
        for (int8_t value : values) mix_byte(static_cast<uint8_t>(value));
        return hash;
    }
};

struct PointerConfig {
    uint32_t dimension = 2048;
    uint64_t seed = 424242;
    uint32_t corruption_bits = 64;
};

class SemanticPointerAlgebra {
public:
    explicit SemanticPointerAlgebra(const PointerConfig& config = PointerConfig{}) : config_(config) {
        if (config_.dimension == 0 || config_.dimension > 1000000) throw std::invalid_argument("semantic pointer dimension is invalid");
    }

    SemanticPointer atomic(uint64_t stable_id, PointerType type) const {
        if (stable_id == 0 || type == PointerType::Unknown) throw std::invalid_argument("atomic pointer identity is invalid");
        SemanticPointer pointer;
        pointer.pointer_id = stable_id;
        pointer.type = type;
        pointer.generation = 0;
        pointer.dimension = config_.dimension;
        pointer.values.resize(config_.dimension);
        uint64_t state = language_mix(config_.seed ^ stable_id ^ (static_cast<uint64_t>(type) << 48));
        for (uint32_t i = 0; i < config_.dimension; ++i) { state = language_mix(state + i); pointer.values[i] = (state & 1ULL) ? 1 : -1; }
        pointer.checksum = pointer.compute_checksum();
        return pointer;
    }

    SemanticPointer bind(const SemanticPointer& role, const SemanticPointer& filler, bool polymorphic = false) const {
        validate(role); validate(filler);
        if (role.dimension != filler.dimension || (!polymorphic && role.type != PointerType::Role)) throw std::invalid_argument("binding requires a role pointer with matching dimensions");
        SemanticPointer result;
        result.pointer_id = language_mix(role.pointer_id ^ (filler.pointer_id << 1) ^ 0xB1A0DULL);
        result.type = PointerType::Bound;
        result.generation = std::max(role.generation, filler.generation) + 1;
        result.dimension = role.dimension;
        result.values.resize(result.dimension);
        for (size_t i = 0; i < result.values.size(); ++i) result.values[i] = static_cast<int8_t>(role.values[i] * filler.values[i]);
        result.checksum = result.compute_checksum();
        return result;
    }

    SemanticPointer bundle(const std::vector<SemanticPointer>& pointers) const {
        if (pointers.empty()) throw std::invalid_argument("cannot bundle zero pointers");
        for (const auto& pointer : pointers) validate(pointer);
        SemanticPointer result;
        result.pointer_id = language_mix(pointers.front().pointer_id ^ (static_cast<uint64_t>(pointers.size()) << 32) ^ 0xB00D1EULL);
        result.type = PointerType::Scene;
        result.generation = 1;
        result.dimension = pointers.front().dimension;
        result.values.assign(result.dimension, 1);
        for (uint32_t i = 0; i < result.dimension; ++i) {
            int score = 0;
            for (const auto& pointer : pointers) { if (pointer.dimension != result.dimension) throw std::invalid_argument("bundle dimensions differ"); score += pointer.values[i]; }
            result.values[i] = score < 0 ? -1 : 1;
        }
        result.checksum = result.compute_checksum();
        return result;
    }

    SemanticPointer permute(const SemanticPointer& pointer, int shift) const {
        validate(pointer);
        SemanticPointer result = pointer;
        result.pointer_id = language_mix(pointer.pointer_id ^ static_cast<uint64_t>(shift));
        result.generation = pointer.generation + 1;
        result.values.assign(pointer.dimension, 1);
        const int dimension = static_cast<int>(pointer.dimension);
        for (int i = 0; i < dimension; ++i) { int destination = (i + shift) % dimension; if (destination < 0) destination += dimension; result.values[static_cast<size_t>(destination)] = pointer.values[static_cast<size_t>(i)]; }
        result.checksum = result.compute_checksum();
        return result;
    }

    SemanticPointer unbind(const SemanticPointer& structure, const SemanticPointer& role) const {
        validate(structure); validate(role);
        if (structure.dimension != role.dimension) throw std::invalid_argument("unbind dimensions differ");
        SemanticPointer result;
        result.pointer_id = language_mix(structure.pointer_id ^ (role.pointer_id << 1) ^ 0xA11B1DULL);
        result.type = PointerType::Unknown;
        result.generation = structure.generation + 1;
        result.dimension = structure.dimension;
        result.values.resize(result.dimension);
        for (size_t i = 0; i < result.values.size(); ++i) result.values[i] = static_cast<int8_t>(structure.values[i] * role.values[i]);
        result.checksum = result.compute_checksum();
        return result;
    }

    float similarity(const SemanticPointer& left, const SemanticPointer& right) const {
        validate(left); validate(right);
        if (left.dimension != right.dimension) throw std::invalid_argument("similarity dimensions differ");
        float dot = 0.0f;
        for (size_t i = 0; i < left.values.size(); ++i) dot += static_cast<float>(left.values[i] * right.values[i]);
        return dot / static_cast<float>(left.values.size());
    }

    SemanticPointer cleanup(const SemanticPointer& noisy, const std::vector<SemanticPointer>& known) const {
        validate(noisy); if (known.empty()) throw std::invalid_argument("cleanup requires known pointers");
        const SemanticPointer* best = nullptr; float best_score = -2.0f;
        for (const auto& candidate : known) { const float score = similarity(noisy, candidate); if (score > best_score) { best_score = score; best = &candidate; } }
        return *best;
    }

    void validate(const SemanticPointer& pointer) const {
        if (!pointer.valid() || pointer.dimension != config_.dimension) throw std::invalid_argument("semantic pointer checksum, dimension, or metadata is invalid");
        if (pointer.type == PointerType::Unknown && pointer.pointer_id == 0) throw std::invalid_argument("semantic pointer type is invalid");
        for (int8_t value : pointer.values) if (value != -1 && value != 1) throw std::invalid_argument("semantic pointer is not bipolar");
    }

    const PointerConfig& config() const { return config_; }

private:
    PointerConfig config_;
};

struct TokenEvent {
    uint64_t occurrence_id = 0;
    uint64_t lexical_type_id = 0;
    uint64_t episode_id = 0;
    uint32_t position = 0;
    uint32_t channel = 0;
    SparseCode form_code;
    uint64_t context_pointer_id = 0;
    std::string surface;
};

struct SceneGraph {
    uint64_t scene_id = 0;
    std::map<SceneRole, SemanticPointer> roles;
    std::set<SceneRole> missing_roles;
    SemanticPointer semantic_pointer;

    bool has(SceneRole role) const { return roles.find(role) != roles.end(); }
    const SemanticPointer* get(SceneRole role) const { const auto iterator = roles.find(role); return iterator == roles.end() ? nullptr : &iterator->second; }
    uint64_t signature() const {
        uint64_t hash = 1469598103934665603ULL;
        for (const auto& item : roles) hash ^= item.second.pointer_id + (static_cast<uint64_t>(item.first) << 56) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
        return hash;
    }
};

struct ActionCommand {
    uint64_t command_id = 0;
    SemanticPointer action;
    SemanticPointer subject;
    SemanticPointer object;
    SemanticPointer location;
    bool requires_authorization = false;
    bool safe = false;
};

struct GroundedExample {
    uint64_t example_id = 0;
    std::vector<TokenEvent> tokens;
    SceneGraph scene;
    ActionCommand optional_action;
    uint64_t episode_id = 0;
    std::vector<SceneRole> role_alignment;
    float consequence = 0.0f;
};

struct Feedback {
    uint64_t example_id = 0;
    std::vector<TokenEvent> tokens;
    SceneGraph corrected_scene;
    bool positive = false;
};

struct LanguageMetrics {
    uint64_t observed_examples = 0;
    uint64_t lexical_updates = 0;
    uint64_t construction_updates = 0;
    uint64_t corrections = 0;
    uint64_t abstentions = 0;
    float lexical_confidence = 0.0f;
    float construction_confidence = 0.0f;
};

enum class HypothesisProvenance : uint32_t { CONSTRUCTION = 0, EPISODIC = 1, LEXICAL = 2, FALLBACK = 3 };

struct GeneratedResponse {
    std::vector<TokenEvent> tokens;
    HypothesisProvenance provenance = HypothesisProvenance::FALLBACK;
    bool fallback = true;
};

struct LanguageHypothesis {
    uint64_t hypothesis_id = 0;
    SemanticPointer semantic_pointer;
    SceneGraph predicted_scene;
    float confidence = 0.0f;
    HypothesisProvenance provenance = HypothesisProvenance::FALLBACK;
    bool abstained = false;
    bool unsafe = false;
};

struct Construction {
    uint64_t construction_id = 0;
    uint64_t form_signature = 0;
    std::vector<LexicalCategory> categories;
    std::vector<SceneRole> roles;
    std::vector<uint64_t> exemplar_lexical_ids;
    uint32_t support = 0;
    uint32_t contradictions = 0;
    float fit_gain = 0.0f;
    float complexity_penalty = 0.0f;
    float confidence = 0.0f;
    bool promoted = false;
};

struct LexicalSense {
    uint64_t concept_id = 0;
    PointerType concept_type = PointerType::Unknown;
    uint32_t evidence = 0;
    uint32_t contradictions = 0;
    std::vector<uint64_t> supporting_episodes;
};

struct LexicalEntry {
    uint64_t lexical_type_id = 0;
    std::string form;
    LexicalCategory category = LexicalCategory::Unknown;
    SemanticPointer form_pointer;
    uint32_t evidence = 0;
    uint32_t contradictions = 0;
    std::map<uint64_t, LexicalSense> senses;
    float confidence = 0.0f;
};

struct TokenizerManifest {
    uint32_t version = 1;
    std::string mode = "whitespace-lowercase-v1";
    uint64_t vocabulary_hash = 0;
    uint64_t corpus_hash = 0;
};

class TokenEncoder {
public:
    TokenEncoder(const PointerConfig& pointer_config = PointerConfig{}, uint64_t seed = 424242) : algebra_(pointer_config), seed_(seed) {}

    std::vector<std::string> split(const std::string& text) const {
        std::vector<std::string> output; std::string current;
        for (char character : text) {
            if (std::isspace(static_cast<unsigned char>(character))) { if (!current.empty()) { output.push_back(current); current.clear(); } }
            else { current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character)))); }
        }
        if (!current.empty()) output.push_back(current);
        return output;
    }

    std::vector<TokenEvent> encode(const std::string& text, uint64_t episode_id, uint32_t channel = 0) {
        const auto forms = split(text);
        std::vector<TokenEvent> events;
        for (size_t position = 0; position < forms.size(); ++position) {
            const auto iterator = vocabulary_.find(forms[position]);
            const uint64_t lexical_id = iterator == vocabulary_.end() ? register_form(forms[position]) : iterator->second;
            TokenEvent event;
            event.occurrence_id = next_occurrence_id_++;
            event.lexical_type_id = lexical_id;
            event.episode_id = episode_id;
            event.position = static_cast<uint32_t>(position);
            event.channel = channel;
            event.form_code = to_sparse(algebra_.atomic(lexical_id, PointerType::Lexical));
            event.context_pointer_id = language_mix(episode_id ^ static_cast<uint64_t>(position));
            event.surface = forms[position];
            events.push_back(event);
        }
        return events;
    }

    uint64_t register_form(const std::string& form) {
        const auto existing = vocabulary_.find(form);
        if (existing != vocabulary_.end()) return existing->second;
        const uint64_t id = language_mix(seed_ ^ static_cast<uint64_t>(vocabulary_.size() + 1));
        vocabulary_[form] = id; reverse_[id] = form; return id;
    }

    const std::unordered_map<std::string, uint64_t>& vocabulary() const { return vocabulary_; }
    const std::unordered_map<uint64_t, std::string>& reverse_vocabulary() const { return reverse_; }
    const TokenizerManifest& manifest() const { return manifest_; }
    void set_manifest(uint64_t corpus_hash) { manifest_.corpus_hash = corpus_hash; manifest_.vocabulary_hash = vocabulary_hash(); }
    uint64_t vocabulary_hash() const { uint64_t hash = 1469598103934665603ULL; std::vector<std::pair<std::string, uint64_t>> ordered(vocabulary_.begin(), vocabulary_.end()); std::sort(ordered.begin(), ordered.end()); for (const auto& item : ordered) { hash ^= std::hash<std::string>{}(item.first) + item.second; hash *= 1099511628211ULL; } return hash; }

    void serialize(std::ostream& output) const {
        write(output, next_occurrence_id_); write(output, seed_); write_string(output, manifest_.mode); write(output, manifest_.version); write(output, vocabulary_hash()); write(output, manifest_.corpus_hash);
        write(output, static_cast<uint64_t>(vocabulary_.size())); std::vector<std::pair<std::string, uint64_t>> ordered(vocabulary_.begin(), vocabulary_.end()); std::sort(ordered.begin(), ordered.end()); for (const auto& item : ordered) { write_string(output, item.first); write(output, item.second); }
    }
    void deserialize(std::istream& input) {
        read(input, next_occurrence_id_); read(input, seed_); read_string(input, manifest_.mode); read(input, manifest_.version); read(input, manifest_.vocabulary_hash); read(input, manifest_.corpus_hash);
        uint64_t count = 0; read(input, count); if (count > 1000000) throw std::runtime_error("token vocabulary is too large"); vocabulary_.clear(); reverse_.clear();
        for (uint64_t i = 0; i < count; ++i) { std::string form; uint64_t id = 0; read_string(input, form); read(input, id); if (id == 0 || form.empty()) throw std::runtime_error("token vocabulary entry is invalid"); vocabulary_[form] = id; reverse_[id] = form; }
        if (vocabulary_hash() != manifest_.vocabulary_hash) throw std::runtime_error("token vocabulary hash mismatch");
    }

private:
    static SparseCode to_sparse(const SemanticPointer& pointer) { SparseCode code(pointer.dimension); for (size_t i = 0; i < pointer.values.size(); ++i) code.values[i] = static_cast<float>(pointer.values[i]); return code; }
    template <typename T> static void write(std::ostream& output, const T& value) { output.write(reinterpret_cast<const char*>(&value), sizeof(value)); if (!output) throw std::runtime_error("tokenizer write failed"); }
    template <typename T> static void read(std::istream& input, T& value) { input.read(reinterpret_cast<char*>(&value), sizeof(value)); if (!input) throw std::runtime_error("tokenizer read failed"); }
    static void write_string(std::ostream& output, const std::string& value) { write(output, static_cast<uint64_t>(value.size())); output.write(value.data(), static_cast<std::streamsize>(value.size())); }
    static void read_string(std::istream& input, std::string& value) { uint64_t size = 0; read(input, size); if (size > 1000000) throw std::runtime_error("token string is too large"); value.resize(static_cast<size_t>(size)); input.read(value.data(), static_cast<std::streamsize>(size)); if (!input) throw std::runtime_error("token string read failed"); }

    SemanticPointerAlgebra algebra_;
    uint64_t seed_ = 424242;
    uint64_t next_occurrence_id_ = 1;
    std::unordered_map<std::string, uint64_t> vocabulary_;
    std::unordered_map<uint64_t, std::string> reverse_;
    TokenizerManifest manifest_;
};

class LanguageLearner {
public:
    explicit LanguageLearner(const PointerConfig& pointer_config = PointerConfig{}, uint64_t seed = 424242)
        : algebra_(pointer_config), encoder_(pointer_config, seed), seed_(seed) {
        role_pointers_[SceneRole::Subject] = algebra_.atomic(1001, PointerType::Role);
        role_pointers_[SceneRole::Verb] = algebra_.atomic(1002, PointerType::Role);
        role_pointers_[SceneRole::Object] = algebra_.atomic(1003, PointerType::Role);
        role_pointers_[SceneRole::Location] = algebra_.atomic(1004, PointerType::Role);
        role_pointers_[SceneRole::Time] = algebra_.atomic(1005, PointerType::Role);
        role_pointers_[SceneRole::Modality] = algebra_.atomic(1006, PointerType::Role);
        role_pointers_[SceneRole::Modifier] = algebra_.atomic(1007, PointerType::Role);
    }

    void observe(const GroundedExample& example) {
        if (example.tokens.empty() || example.tokens.size() != example.role_alignment.size()) throw std::invalid_argument("grounded example token/role alignment mismatch");
        ++metrics_.observed_examples;
        for (size_t i = 0; i < example.tokens.size(); ++i) {
            const auto& token = example.tokens[i];
            const SceneRole role = example.role_alignment[i];
            const SemanticPointer* concept_pointer = example.scene.get(role);
            const LexicalCategory category = category_for_role(role);
            LexicalEntry& entry = ensure_lexical(token, category);
            if (concept_pointer) {
                LexicalSense& sense = entry.senses[concept_pointer->pointer_id];
                sense.concept_id = concept_pointer->pointer_id; sense.concept_type = concept_pointer->type; ++sense.evidence;
                sense.supporting_episodes.push_back(example.episode_id);
                entry.confidence = confidence(entry);
            }
            ++entry.evidence; ++metrics_.lexical_updates;
        }
        const uint64_t signature = construction_signature(example.tokens);
        if (!example.tokens.empty() && example.tokens.front().context_pointer_id != 0) contextual_scenes_[context_key(example.tokens.front().context_pointer_id, signature)] = example.scene;
        const auto categories = categories_for(example.tokens);
        auto iterator = constructions_.find(signature);
        if (iterator == constructions_.end()) {
            Construction construction;
            construction.construction_id = language_mix(seed_ ^ signature);
            construction.form_signature = signature;
            construction.categories = categories;
            construction.roles = example.role_alignment;
            construction.exemplar_lexical_ids.reserve(example.tokens.size());
            for (const auto& token : example.tokens) construction.exemplar_lexical_ids.push_back(token.lexical_type_id);
            construction.support = 1; construction.fit_gain = 1.0f; construction.complexity_penalty = 0.02f * static_cast<float>(categories.size());
            construction.confidence = construction.fit_gain - construction.complexity_penalty;
            constructions_[signature] = construction;
            iterator = constructions_.find(signature);
        } else {
            Construction& construction = iterator->second;
            ++construction.support;
            if (construction.roles != example.role_alignment) ++construction.contradictions;
            construction.confidence = (static_cast<float>(construction.support) - static_cast<float>(construction.contradictions)) / (1.0f + construction.complexity_penalty * static_cast<float>(construction.support));
        }
        Construction& construction = iterator->second;
        if (construction.support >= construction_support_threshold_ && construction.confidence >= construction_promotion_threshold_ && construction.contradictions * 2 < construction.support) construction.promoted = true;
        ++metrics_.construction_updates;
        examples_.push_back(example);
        if (example.tokens.size() > 1) for (size_t i = 1; i < example.tokens.size(); ++i) next_token_counts_[example.tokens[i - 1].lexical_type_id][example.tokens[i].lexical_type_id]++;
        if (example.optional_action.safe) safe_commands_[example.scene.signature()] = example.optional_action;
        last_example_ = example;
    }

    LanguageHypothesis interpret(const std::vector<TokenEvent>& tokens, const CognitiveContext& context = CognitiveContext{}) const {
        LanguageHypothesis hypothesis;
        hypothesis.hypothesis_id = next_hypothesis_id_++;
        if (tokens.empty()) { hypothesis.abstained = true; hypothesis.provenance = HypothesisProvenance::FALLBACK; return hypothesis; }
        const uint64_t signature = construction_signature(tokens);
        if (context.context_id != 0) {
            const auto contextual = contextual_scenes_.find(context_key(context.context_id, signature));
            if (contextual != contextual_scenes_.end()) { hypothesis.predicted_scene = contextual->second; hypothesis.semantic_pointer = contextual->second.semantic_pointer; hypothesis.confidence = 0.90f; hypothesis.provenance = HypothesisProvenance::CONSTRUCTION; return hypothesis; }
        }
        const auto correction = corrections_.find(signature);
        if (correction != corrections_.end()) {
            hypothesis.predicted_scene = correction->second;
            hypothesis.semantic_pointer = correction->second.semantic_pointer;
            hypothesis.confidence = 0.95f;
            hypothesis.provenance = HypothesisProvenance::CONSTRUCTION;
            return hypothesis;
        }
        const auto construction = best_construction(tokens);
        if (construction && construction->promoted) {
            hypothesis.predicted_scene = scene_from_construction(*construction, tokens);
            hypothesis.semantic_pointer = hypothesis.predicted_scene.semantic_pointer;
            hypothesis.confidence = std::min(0.99f, std::max(0.0f, construction->confidence / 4.0f));
            hypothesis.provenance = HypothesisProvenance::CONSTRUCTION;
            return hypothesis;
        }
        for (const auto& example : examples_) {
            if (token_forms_equal(tokens, example.tokens)) {
                hypothesis.predicted_scene = example.scene;
                hypothesis.semantic_pointer = example.scene.semantic_pointer;
                hypothesis.confidence = 0.80f;
                hypothesis.provenance = HypothesisProvenance::EPISODIC;
                return hypothesis;
            }
        }
        SceneGraph lexical_scene;
        lexical_scene.scene_id = language_mix(signature);
        bool unknown = false;
        for (const auto& token : tokens) {
            const auto entry = lexicon_.find(token.lexical_type_id);
            if (entry == lexicon_.end() || entry->second.senses.empty()) { unknown = true; continue; }
        }
        if (unknown) {
            hypothesis.abstained = true; hypothesis.confidence = 0.0f; hypothesis.provenance = HypothesisProvenance::FALLBACK; ++metrics_.abstentions; return hypothesis;
        }
        hypothesis.predicted_scene = lexical_scene;
        hypothesis.confidence = 0.35f;
        hypothesis.provenance = HypothesisProvenance::LEXICAL;
        return hypothesis;
    }

    GeneratedResponse decode(const SceneGraph& scene, const CognitiveContext& context = CognitiveContext{}) const {
        (void)context;
        GeneratedResponse response;
        const Construction* best = nullptr;
        for (const auto& item : constructions_) {
            if (!item.second.promoted || item.second.roles.size() != item.second.categories.size()) continue;
            bool compatible = true;
            for (SceneRole role : item.second.roles) if (!scene.has(role)) compatible = false;
            if (compatible && (!best || item.second.confidence > best->confidence)) best = &item.second;
        }
        if (best) {
            for (size_t i = 0; i < best->roles.size(); ++i) {
                const SceneRole role = best->roles[i];
                const SemanticPointer* filler = scene.get(role);
                if (!filler) { response.tokens.clear(); return response; }
                const auto lexical_id = lexical_for_concept(*filler);
                if (!lexical_id) { response.tokens.clear(); return response; }
                TokenEvent token; token.occurrence_id = next_hypothesis_id_++; token.lexical_type_id = *lexical_id; token.position = static_cast<uint32_t>(i); token.surface = form_for(*lexical_id); token.form_code = SparseCode(algebra_.config().dimension, 0.0f); response.tokens.push_back(token);
            }
            response.provenance = HypothesisProvenance::CONSTRUCTION; response.fallback = false; return response;
        }
        for (const auto& example : examples_) if (example.scene.signature() == scene.signature()) { response.tokens = example.tokens; response.provenance = HypothesisProvenance::EPISODIC; response.fallback = false; return response; }
        response.provenance = HypothesisProvenance::FALLBACK; response.fallback = true; return response;
    }

    std::vector<TokenEvent> generate(const SceneGraph& scene, const CognitiveContext& context = CognitiveContext{}) const { return decode(scene, context).tokens; }

    void replay(const GroundedExample& example) { observe(example); }
    void clear_episodic_examples() { examples_.clear(); }
    void clear_unconsolidated_memory() { examples_.clear(); constructions_.clear(); contextual_scenes_.clear(); corrections_.clear(); }

    void correct(const Feedback& feedback) {
        ++metrics_.corrections;
        const uint64_t signature = construction_signature(feedback.tokens);
        if (feedback.positive) { corrections_.erase(signature); return; }
        corrections_[signature] = feedback.corrected_scene;
        auto iterator = constructions_.find(signature);
        if (iterator != constructions_.end()) { ++iterator->second.contradictions; iterator->second.confidence *= 0.5f; iterator->second.promoted = false; }
    }

    LanguageHypothesis interpret_command(const std::vector<TokenEvent>& tokens, const CognitiveContext& context = CognitiveContext{}) const {
        LanguageHypothesis result = interpret(tokens, context);
        if (result.abstained || result.predicted_scene.roles.find(SceneRole::Verb) == result.predicted_scene.roles.end()) { result.abstained = true; return result; }
        const auto action = safe_commands_.find(result.predicted_scene.signature());
        if (action == safe_commands_.end() || !action->second.safe || action->second.requires_authorization) { result.abstained = true; result.unsafe = action != safe_commands_.end(); return result; }
        return result;
    }

    SceneGraph compose_scene(uint64_t scene_id, const std::map<SceneRole, SemanticPointer>& roles) const {
        SceneGraph scene; scene.scene_id = scene_id; scene.roles = roles;
        std::vector<SemanticPointer> bindings;
        for (const auto& item : roles) { const auto role_pointer = role_pointers_.find(item.first); if (role_pointer == role_pointers_.end()) throw std::invalid_argument("scene role pointer is unavailable"); bindings.push_back(algebra_.bind(role_pointer->second, item.second)); }
        if (!bindings.empty()) scene.semantic_pointer = algebra_.bundle(bindings);
        for (SceneRole role : {SceneRole::Subject, SceneRole::Verb, SceneRole::Object, SceneRole::Location, SceneRole::Time, SceneRole::Modality}) if (!scene.has(role)) scene.missing_roles.insert(role);
        return scene;
    }

    const LexicalEntry* lexical(uint64_t lexical_id) const { const auto iterator = lexicon_.find(lexical_id); return iterator == lexicon_.end() ? nullptr : &iterator->second; }
    const std::map<SceneRole, SemanticPointer>& role_pointers() const { return role_pointers_; }
    const std::map<uint64_t, Construction>& constructions() const { return constructions_; }
    const TokenEncoder& encoder() const { return encoder_; }
    TokenEncoder& encoder() { return encoder_; }
    const SemanticPointerAlgebra& algebra() const { return algebra_; }
    LanguageMetrics metrics() const { return metrics_; }
    size_t lexical_size() const { return lexicon_.size(); }
    size_t construction_size() const { return constructions_.size(); }
    size_t state_estimate_bytes() const { return lexicon_.size() * 256ULL + constructions_.size() * 192ULL + examples_.size() * 160ULL + encoder_.vocabulary().size() * 64ULL; }
    void enforce_memory_budget(size_t max_lexical_entries) {
        if (lexicon_.size() <= max_lexical_entries) return;
        std::vector<std::pair<uint64_t, uint32_t>> ranked;
        for (const auto& item : lexicon_) ranked.push_back({item.first, item.second.evidence});
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { if (a.second != b.second) return a.second > b.second; return a.first < b.first; });
        std::set<uint64_t> keep; for (size_t i = 0; i < max_lexical_entries && i < ranked.size(); ++i) keep.insert(ranked[i].first);
        for (auto iterator = lexicon_.begin(); iterator != lexicon_.end();) { if (keep.count(iterator->first) == 0) iterator = lexicon_.erase(iterator); else ++iterator; }
    }

    float token_prediction_accuracy(const std::vector<GroundedExample>& held_out) const {
        size_t correct = 0, total = 0;
        for (const auto& example : held_out) for (size_t i = 1; i < example.tokens.size(); ++i) { ++total; const auto previous = next_token_counts_.find(example.tokens[i - 1].lexical_type_id); if (previous != next_token_counts_.end() && !previous->second.empty() && std::max_element(previous->second.begin(), previous->second.end(), [](const auto& a, const auto& b) { return a.second < b.second; })->first == example.tokens[i].lexical_type_id) ++correct; }
        return total == 0 ? 0.0f : static_cast<float>(correct) / static_cast<float>(total);
    }

    uint64_t state_hash() const {
        uint64_t hash = encoder_.vocabulary_hash();
        for (const auto& item : lexicon_) { hash ^= item.first + (static_cast<uint64_t>(item.second.evidence) << 32); for (const auto& sense : item.second.senses) hash ^= sense.first + sense.second.evidence + (hash << 6) + (hash >> 2); }
        for (const auto& item : constructions_) hash ^= item.first + item.second.support + (static_cast<uint64_t>(item.second.promoted) << 48) + (hash << 6) + (hash >> 2);
        return hash;
    }

    void save(const std::filesystem::path& path) const {
        std::filesystem::create_directories(path.parent_path()); std::ofstream output(path, std::ios::binary | std::ios::trunc);
        const uint32_t magic = 0x34474E4CU, version = 1; write(output, magic); write(output, version); write(output, seed_); encoder_.serialize(output);
        write(output, static_cast<uint64_t>(lexicon_.size()));
        for (const auto& item : lexicon_) { const auto& entry = item.second; write(output, entry.lexical_type_id); write_string(output, entry.form); write(output, entry.category); write(output, entry.evidence); write(output, entry.contradictions); write(output, entry.confidence); write_pointer(output, entry.form_pointer); write(output, static_cast<uint64_t>(entry.senses.size())); for (const auto& sense_item : entry.senses) { const auto& sense = sense_item.second; write(output, sense.concept_id); write(output, sense.concept_type); write(output, sense.evidence); write(output, sense.contradictions); write_vector(output, sense.supporting_episodes); } }
        write(output, static_cast<uint64_t>(constructions_.size())); for (const auto& item : constructions_) { const auto& c = item.second; write(output, c.construction_id); write(output, c.form_signature); write_vector(output, c.categories); write_vector(output, c.roles); write_vector(output, c.exemplar_lexical_ids); write(output, c.support); write(output, c.contradictions); write(output, c.fit_gain); write(output, c.complexity_penalty); write(output, c.confidence); write(output, c.promoted); }
        if (!output) throw std::runtime_error("language state write failed");
    }

    void load(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary); if (!input) throw std::runtime_error("language state cannot be opened");
        uint32_t magic = 0, version = 0; uint64_t loaded_seed = 0; read(input, magic); read(input, version); read(input, loaded_seed); if (!input || magic != 0x34474E4CU || version != 1) throw std::runtime_error("language state header invalid");
        TokenEncoder loaded_encoder; loaded_encoder.deserialize(input);
        std::map<uint64_t, LexicalEntry> loaded_lexicon; uint64_t lexical_count = 0; read(input, lexical_count); if (lexical_count > 1000000) throw std::runtime_error("language lexicon too large");
        for (uint64_t i = 0; i < lexical_count; ++i) { LexicalEntry entry; read(input, entry.lexical_type_id); read_string(input, entry.form); read(input, entry.category); read(input, entry.evidence); read(input, entry.contradictions); read(input, entry.confidence); read_pointer(input, entry.form_pointer); uint64_t sense_count = 0; read(input, sense_count); if (sense_count > 100000) throw std::runtime_error("language sense table too large"); for (uint64_t j = 0; j < sense_count; ++j) { LexicalSense sense; read(input, sense.concept_id); read(input, sense.concept_type); read(input, sense.evidence); read(input, sense.contradictions); read_vector(input, sense.supporting_episodes); entry.senses[sense.concept_id] = sense; } loaded_lexicon[entry.lexical_type_id] = entry; }
        std::map<uint64_t, Construction> loaded_constructions; uint64_t construction_count = 0; read(input, construction_count); if (construction_count > 100000) throw std::runtime_error("language construction table too large");
        for (uint64_t i = 0; i < construction_count; ++i) { Construction c; read(input, c.construction_id); read(input, c.form_signature); read_vector(input, c.categories); read_vector(input, c.roles); read_vector(input, c.exemplar_lexical_ids); read(input, c.support); read(input, c.contradictions); read(input, c.fit_gain); read(input, c.complexity_penalty); read(input, c.confidence); read(input, c.promoted); loaded_constructions[c.form_signature] = c; }
        if (!input) throw std::runtime_error("language state truncated");
        seed_ = loaded_seed; encoder_ = std::move(loaded_encoder); lexicon_ = std::move(loaded_lexicon); constructions_ = std::move(loaded_constructions);
    }

private:
    static LexicalCategory category_for_role(SceneRole role) { switch (role) { case SceneRole::Subject: case SceneRole::Object: return LexicalCategory::Entity; case SceneRole::Verb: return LexicalCategory::Action; case SceneRole::Location: return LexicalCategory::Location; case SceneRole::Time: return LexicalCategory::Temporal; case SceneRole::Modifier: return LexicalCategory::Modifier; case SceneRole::Modality: return LexicalCategory::Function; } return LexicalCategory::Unknown; }
    static PointerType pointer_type_for_role(SceneRole role) { switch (role) { case SceneRole::Verb: return PointerType::Action; case SceneRole::Location: return PointerType::Location; case SceneRole::Subject: case SceneRole::Object: return PointerType::Entity; default: return PointerType::Modifier; } }
    LexicalEntry& ensure_lexical(const TokenEvent& token, LexicalCategory category) {
        auto iterator = lexicon_.find(token.lexical_type_id);
        if (iterator != lexicon_.end()) { if (iterator->second.category == LexicalCategory::Unknown) iterator->second.category = category; return iterator->second; }
        LexicalEntry entry; entry.lexical_type_id = token.lexical_type_id; entry.form = token.surface; entry.category = category; entry.form_pointer = algebra_.atomic(token.lexical_type_id, PointerType::Lexical); lexicon_[entry.lexical_type_id] = entry; return lexicon_[entry.lexical_type_id];
    }
    static float confidence(const LexicalEntry& entry) { uint32_t total = 0; uint32_t best = 0; for (const auto& item : entry.senses) { total += item.second.evidence; best = std::max(best, item.second.evidence); } return total == 0 ? 0.0f : static_cast<float>(best) / static_cast<float>(total); }
    std::vector<LexicalCategory> categories_for(const std::vector<TokenEvent>& tokens) const { std::vector<LexicalCategory> categories; for (const auto& token : tokens) { const auto iterator = lexicon_.find(token.lexical_type_id); categories.push_back(iterator == lexicon_.end() ? LexicalCategory::Unknown : iterator->second.category); } return categories; }
    uint64_t construction_signature(const std::vector<TokenEvent>& tokens) const { uint64_t hash = tokens.size(); for (const auto& token : tokens) { const auto iterator = lexicon_.find(token.lexical_type_id); const uint64_t category = iterator == lexicon_.end() ? static_cast<uint64_t>(LexicalCategory::Unknown) : static_cast<uint64_t>(iterator->second.category); hash ^= category + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2); } return hash; }
    const Construction* best_construction(const std::vector<TokenEvent>& tokens) const { const uint64_t signature = construction_signature(tokens); const auto iterator = constructions_.find(signature); return iterator == constructions_.end() ? nullptr : &iterator->second; }
    SceneGraph scene_from_construction(const Construction& construction, const std::vector<TokenEvent>& tokens) const {
        SceneGraph scene; scene.scene_id = construction.construction_id;
        std::vector<SemanticPointer> bindings;
        for (size_t i = 0; i < tokens.size(); ++i) { const auto entry = lexicon_.find(tokens[i].lexical_type_id); if (entry == lexicon_.end() || entry->second.senses.empty()) continue; const auto* sense = &entry->second.senses.begin()->second; for (size_t j = i + 1; j < tokens.size(); ++j) { if (construction.roles[j] == construction.roles[i]) break; } scene.roles[construction.roles[i]] = algebra_.atomic(sense->concept_id, sense->concept_type); bindings.push_back(algebra_.bind(role_pointers_.at(construction.roles[i]), scene.roles[construction.roles[i]])); }
        for (SceneRole role : {SceneRole::Subject, SceneRole::Verb, SceneRole::Object, SceneRole::Location, SceneRole::Time, SceneRole::Modality}) if (!scene.has(role)) scene.missing_roles.insert(role);
        if (!bindings.empty()) scene.semantic_pointer = algebra_.bundle(bindings);
        return scene;
    }
    bool token_forms_equal(const std::vector<TokenEvent>& left, const std::vector<TokenEvent>& right) const { if (left.size() != right.size()) return false; for (size_t i = 0; i < left.size(); ++i) if (left[i].lexical_type_id != right[i].lexical_type_id) return false; return true; }
    uint64_t context_key(uint64_t context_id, uint64_t signature) const { return language_mix(context_id ^ (signature + 0x9e3779b97f4a7c15ULL + (context_id << 6) + (context_id >> 2))); }
    std::optional<uint64_t> lexical_for_concept(const SemanticPointer& concept_pointer) const { for (const auto& item : lexicon_) for (const auto& sense : item.second.senses) if (sense.second.concept_id == concept_pointer.pointer_id) return item.first; return std::nullopt; }
    std::string form_for(uint64_t lexical_id) const { const auto* entry = lexical(lexical_id); return entry ? entry->form : std::string{}; }

    template <typename T> static void write(std::ostream& output, const T& value) { output.write(reinterpret_cast<const char*>(&value), sizeof(value)); if (!output) throw std::runtime_error("language state write failed"); }
    template <typename T> static void read(std::istream& input, T& value) { input.read(reinterpret_cast<char*>(&value), sizeof(value)); if (!input) throw std::runtime_error("language state read failed"); }
    static void write_string(std::ostream& output, const std::string& value) { write(output, static_cast<uint64_t>(value.size())); output.write(value.data(), static_cast<std::streamsize>(value.size())); }
    static void read_string(std::istream& input, std::string& value) { uint64_t size = 0; read(input, size); if (size > 1000000) throw std::runtime_error("language string too large"); value.resize(static_cast<size_t>(size)); input.read(value.data(), static_cast<std::streamsize>(size)); if (!input) throw std::runtime_error("language string read failed"); }
    template <typename T> static void write_vector(std::ostream& output, const std::vector<T>& values) { write(output, static_cast<uint64_t>(values.size())); for (const auto& value : values) write(output, value); }
    template <typename T> static void read_vector(std::istream& input, std::vector<T>& values) { uint64_t size = 0; read(input, size); if (size > 1000000) throw std::runtime_error("language vector too large"); values.resize(static_cast<size_t>(size)); for (auto& value : values) read(input, value); }
    static void write_pointer(std::ostream& output, const SemanticPointer& pointer) { write(output, pointer.pointer_id); write(output, pointer.type); write(output, pointer.generation); write(output, pointer.dimension); write_vector(output, pointer.values); write(output, pointer.checksum); }
    static void read_pointer(std::istream& input, SemanticPointer& pointer) { read(input, pointer.pointer_id); read(input, pointer.type); read(input, pointer.generation); read(input, pointer.dimension); read_vector(input, pointer.values); read(input, pointer.checksum); if (!pointer.valid()) throw std::runtime_error("persisted semantic pointer checksum mismatch"); }

    SemanticPointerAlgebra algebra_;
    TokenEncoder encoder_;
    uint64_t seed_ = 424242;
    mutable uint64_t next_hypothesis_id_ = 1;
    uint32_t construction_support_threshold_ = 2;
    float construction_promotion_threshold_ = 0.20f;
    std::map<SceneRole, SemanticPointer> role_pointers_;
    std::map<uint64_t, LexicalEntry> lexicon_;
    std::map<uint64_t, Construction> constructions_;
    std::vector<GroundedExample> examples_;
    std::map<uint64_t, std::map<uint64_t, uint32_t>> next_token_counts_;
    std::map<uint64_t, SceneGraph> corrections_;
    std::map<uint64_t, SceneGraph> contextual_scenes_;
    std::map<uint64_t, ActionCommand> safe_commands_;
    mutable LanguageMetrics metrics_;
    mutable GroundedExample last_example_;
};

} // namespace genesis
