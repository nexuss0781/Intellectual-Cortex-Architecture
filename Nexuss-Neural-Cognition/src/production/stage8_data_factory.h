#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace genesis {

inline uint64_t stage8_mix(uint64_t hash, const uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U);
    hash *= 1099511628211ULL;
    return hash;
}

inline uint64_t stage8_hash_string(const std::string& value) {
    uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : value) hash = stage8_mix(hash, static_cast<uint64_t>(character));
    return hash;
}

inline std::string stage8_lower(const std::string& value) {
    std::string result = value;
    for (char& character : result) {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
        if ((character < 'a' || character > 'z') && (character < '0' || character > '9')) character = ' ';
    }
    return result;
}

inline std::set<std::string> stage8_tokens(const std::string& value) {
    std::set<std::string> result;
    std::istringstream input(stage8_lower(value));
    std::string token;
    while (input >> token) result.insert(token);
    return result;
}

inline double stage8_jaccard(const std::string& left, const std::string& right) {
    const auto a = stage8_tokens(left);
    const auto b = stage8_tokens(right);
    if (a.empty() && b.empty()) return 1.0;
    size_t intersection = 0;
    for (const auto& token : a) if (b.find(token) != b.end()) ++intersection;
    const size_t union_size = a.size() + b.size() - intersection;
    return union_size == 0U ? 0.0 : static_cast<double>(intersection) / static_cast<double>(union_size);
}

inline bool stage8_has_pii(const std::string& value) {
    const size_t at = value.find('@');
    if (at != std::string::npos && at > 0U && value.find('.', at) != std::string::npos) return true;
    return false;
}

inline bool stage8_has_secret(const std::string& value) {
    const std::string lowered = stage8_lower(value);
    return lowered.find("api key") != std::string::npos || lowered.find("api_key") != std::string::npos ||
        lowered.find("secret") != std::string::npos || lowered.find("password") != std::string::npos;
}

inline bool stage8_has_prompt_injection(const std::string& value) {
    const std::string lowered = stage8_lower(value);
    return lowered.find("ignore system") != std::string::npos || lowered.find("reveal the hidden") != std::string::npos ||
        lowered.find("bypass policy") != std::string::npos;
}

inline bool stage8_has_safety_marker(const std::string& value) {
    const std::string lowered = stage8_lower(value);
    return lowered.find("harmful instruction") != std::string::npos || lowered.find("illegal") != std::string::npos ||
        lowered.find("self harm") != std::string::npos;
}

struct SourceRecord {
    std::string source_id;
    std::string owner;
    std::string acquisition_method;
    std::string license_or_permission;
    std::string jurisdiction;
    std::string intended_use;
    std::string retention_policy;
    std::string content_hash;
    uint64_t collected_at = 0;
    bool training_allowed = false;
    bool evaluation_allowed = false;
    bool removal_supported = false;
};

struct RawDataRecord {
    std::string item_id;
    std::string source_id;
    std::string split_hint;
    std::string language;
    std::string modality;
    std::string sensitivity;
    std::string content;
    std::string annotation_schema;
    std::string rater_a;
    std::string rater_b;
    bool hidden = false;
    bool benchmark_marker = false;
};

struct DataItem {
    std::string item_id;
    std::string source_id;
    std::string split;
    std::string language;
    std::string modality;
    std::string sensitivity;
    std::string content_hash;
    std::string provenance_hash;
    std::string status;
    std::string decision_reason;
    std::string redacted_content;
    bool pii_detected = false;
    bool secret_detected = false;
    bool safety_reviewed = false;
    bool retained = false;
    bool training_allowed = false;
    bool hidden = false;
    bool benchmark_marker = false;
};

struct DatasetRelease {
    std::string dataset_id;
    std::string version;
    std::string manifest_digest;
    std::string card_digest;
    std::string split_manifest_digest;
    std::string quality_report_digest;
    std::string approval_digest;
};

struct AnnotationRecord {
    std::string item_id;
    std::string schema;
    std::string rater_a;
    std::string rater_b;
    bool valid = false;
    bool agreement = false;
    bool adjudicated = false;
};

struct AccessEvent {
    uint64_t sequence = 0;
    std::string actor;
    std::string operation;
    std::string item_id;
    std::string outcome;
    uint64_t event_hash = 0;
};

class DataFactory {
public:
    explicit DataFactory(const uint64_t now_unix = 2000000200ULL) : now_unix_(now_unix) {}

    bool ingest(const SourceRecord& source) {
        if (source.source_id.empty()) return false;
        sources_[source.source_id] = source;
        const bool admitted = rights_valid(source);
        audit("ingest", source.source_id, admitted ? "admitted" : "quarantined_rights");
        return admitted;
    }

    std::vector<DataItem> process(const std::string& source_id, const std::vector<RawDataRecord>& records) {
        erase_items_for_source(source_id);
        const auto found = sources_.find(source_id);
        const bool source_admitted = found != sources_.end() && rights_valid(found->second);
        const bool source_trainable = source_admitted && found->second.training_allowed;
        std::vector<DataItem> processed;
        std::map<std::string, std::string> exact_hash_owner;
        std::vector<std::pair<std::string, std::string>> retained_texts;
        for (const auto& existing : items_) if (existing.second.retained) {
            exact_hash_owner[existing.second.content_hash] = existing.second.item_id;
            retained_texts.emplace_back(existing.second.item_id, existing.second.redacted_content);
        }
        for (const auto& record : records) {
            if (record.source_id != source_id) continue;
            DataItem item;
            item.item_id = record.item_id;
            item.source_id = source_id;
            item.language = record.language;
            item.modality = record.modality;
            item.sensitivity = record.sensitivity;
            item.split = record.split_hint;
            item.hidden = record.hidden;
            item.benchmark_marker = record.benchmark_marker;
            item.content_hash = std::to_string(stage8_hash_string(record.content));
            item.provenance_hash = std::to_string(stage8_hash_string(source_id + record.item_id + item.content_hash));
            item.training_allowed = source_trainable;
            item.redacted_content = record.content;

            if (!source_admitted) reject(item, "rights");
            else if (record.content.empty()) reject(item, "malformed");
            else if (record.hidden || !source_trainable) reject(item, "sealed_or_not_training");
            else if (record.benchmark_marker) reject(item, "contamination");
            else if (stage8_has_pii(record.content)) { item.pii_detected = true; item.redacted_content = "[REDACTED_PII]"; reject(item, "privacy"); }
            else if (stage8_has_secret(record.content)) { item.secret_detected = true; item.redacted_content = "[REDACTED_SECRET]"; reject(item, "secret"); }
            else if (stage8_has_prompt_injection(record.content)) { item.safety_reviewed = true; reject(item, "prompt_injection_quarantine"); }
            else if (stage8_has_safety_marker(record.content) || record.sensitivity == "safety") { item.safety_reviewed = true; reject(item, "safety_quarantine"); }
            else if (!annotation_valid(record)) reject(item, "annotation_schema");
            else if (exact_hash_owner.find(item.content_hash) != exact_hash_owner.end()) reject(item, "exact_duplicate");
            else {
                bool near_duplicate = false;
                for (const auto& prior : retained_texts) if (stage8_jaccard(prior.second, record.content) >= 0.75) { near_duplicate = true; break; }
                if (near_duplicate) reject(item, "near_duplicate");
                else {
                    item.split = record.split_hint.empty() ? deterministic_split(item.content_hash) : record.split_hint;
                    if (item.split != "train" && item.split != "development" && item.split != "release" && item.split != "sealed") item.split = deterministic_split(item.content_hash);
                    item.status = "retained";
                    item.retained = true;
                    item.safety_reviewed = true;
                    exact_hash_owner[item.content_hash] = item.item_id;
                    retained_texts.emplace_back(item.item_id, record.content);
                }
            }
            items_[item.item_id] = item;
            processed.push_back(item);
            audit("process", item.item_id, item.status);
        }
        processed_sources_.insert(source_id);
        return processed;
    }

    bool approve_release(const DatasetRelease& release) {
        bool valid = !release.dataset_id.empty() && !release.version.empty() && !release.manifest_digest.empty() &&
            !release.card_digest.empty() && !release.split_manifest_digest.empty() && !release.quality_report_digest.empty() &&
            !release.approval_digest.empty() && retained_contamination_count() == 0U && missing_lineage_count() == 0U &&
            cross_split_exact_duplicates() == 0U && cross_split_near_duplicates() == 0U;
        for (const auto& entry : items_) {
            const DataItem& item = entry.second;
            if (item.retained && (!item.training_allowed || item.pii_detected || item.secret_detected || item.hidden || item.benchmark_marker)) valid = false;
        }
        if (valid) release_ = release;
        release_approved_ = valid;
        audit("approve_release", release.dataset_id, valid ? "approved" : "blocked");
        return valid;
    }

    bool delete_by_source(const std::string& source_id) {
        const auto source = sources_.find(source_id);
        if (source == sources_.end() || !source->second.removal_supported) { audit("delete_by_source", source_id, "blocked_no_removal_support"); return false; }
        std::vector<std::string> deleted;
        for (auto iterator = items_.begin(); iterator != items_.end();) {
            if (iterator->second.source_id == source_id) { deleted.push_back(iterator->first); iterator = items_.erase(iterator); }
            else ++iterator;
        }
        deleted_sources_.insert(source_id);
        audit("delete_by_source", source_id, "deleted_" + std::to_string(deleted.size()));
        return true;
    }

    bool allow_for_training(const std::string& item_id) const {
        const auto found = items_.find(item_id);
        return found != items_.end() && found->second.retained && found->second.training_allowed && found->second.split == "train";
    }

    bool try_read_sealed_for_training(const std::string& item_id, const std::string& actor) {
        const auto found = items_.find(item_id);
        const bool sealed = found != items_.end() && found->second.hidden && found->second.split == "sealed";
        audit("sealed_read_training", actor + ":" + item_id, sealed ? "blocked" : "not_sealed");
        return false;
    }

    bool allow_independent_sealed_read(const std::string& item_id, const std::string& actor) {
        const auto found = items_.find(item_id);
        const bool allowed = actor == "independent_evaluator" && found != items_.end() && found->second.hidden;
        audit("sealed_read_evaluation", actor + ":" + item_id, allowed ? "allowed" : "blocked");
        return allowed;
    }

    uint64_t manifest_hash() const {
        std::ostringstream canonical;
        for (const auto& entry : sources_) canonical << entry.first << ':' << entry.second.content_hash << ':' << entry.second.license_or_permission << ':' << entry.second.training_allowed << ':' << entry.second.evaluation_allowed << ':' << entry.second.removal_supported << '|';
        for (const auto& entry : items_) canonical << entry.first << ':' << entry.second.source_id << ':' << entry.second.content_hash << ':' << entry.second.provenance_hash << ':' << entry.second.status << ':' << entry.second.split << ':' << entry.second.retained << '|';
        return stage8_hash_string(canonical.str());
    }

    size_t source_count() const { return sources_.size(); }
    size_t item_count() const { return items_.size(); }
    size_t retained_count() const { return count_status("retained"); }
    size_t quarantine_count() const { return items_.size() - retained_count(); }
    size_t contamination_count() const { return count_reason("contamination"); }
    size_t retained_contamination_count() const {
        size_t count = 0;
        for (const auto& entry : items_) if (entry.second.retained && entry.second.benchmark_marker) ++count;
        return count;
    }
    size_t pii_count() const { return count_flag_pii(); }
    size_t secret_count() const { return count_flag_secret(); }
    size_t missing_lineage_count() const {
        size_t count = 0;
        for (const auto& entry : items_) if (entry.second.retained && (entry.second.provenance_hash.empty() || sources_.find(entry.second.source_id) == sources_.end())) ++count;
        return count;
    }
    size_t cross_split_exact_duplicates() const {
        std::map<std::string, std::set<std::string>> splits;
        for (const auto& entry : items_) if (entry.second.retained) splits[entry.second.content_hash].insert(entry.second.split);
        size_t count = 0;
        for (const auto& entry : splits) if (entry.second.size() > 1U) count += entry.second.size() - 1U;
        return count;
    }
    size_t cross_split_near_duplicates() const {
        std::vector<const DataItem*> retained;
        for (const auto& entry : items_) if (entry.second.retained) retained.push_back(&entry.second);
        size_t count = 0;
        for (size_t left = 0; left < retained.size(); ++left) for (size_t right = left + 1U; right < retained.size(); ++right) if (retained[left]->split != retained[right]->split && stage8_jaccard(retained[left]->redacted_content, retained[right]->redacted_content) >= 0.75) ++count;
        return count;
    }
    size_t deleted_reference_count(const std::string& source_id) const {
        size_t count = 0;
        for (const auto& entry : items_) if (entry.second.source_id == source_id) ++count;
        return count;
    }
    bool release_approved() const { return release_approved_; }
    const DatasetRelease& release() const { return release_; }
    const std::map<std::string, SourceRecord>& sources() const { return sources_; }
    const std::map<std::string, DataItem>& items() const { return items_; }
    const std::vector<AccessEvent>& audit_events() const { return audit_; }
    uint64_t now_unix() const { return now_unix_; }

private:
    static bool rights_valid(const SourceRecord& source) {
        const std::string license = stage8_lower(source.license_or_permission);
        return !source.owner.empty() && !source.acquisition_method.empty() && !source.license_or_permission.empty() &&
            license.find("unknown") == std::string::npos && license.find("unclear") == std::string::npos &&
            source.jurisdiction != "" && source.jurisdiction != "unknown" && !source.intended_use.empty() &&
            !source.retention_policy.empty() && !source.content_hash.empty() && source.collected_at > 0U && source.removal_supported;
    }

    static bool annotation_valid(const RawDataRecord& record) {
        const bool known_schema = record.annotation_schema == "qa" || record.annotation_schema == "safety" ||
            record.annotation_schema == "preference" || record.annotation_schema == "tool";
        const bool known_labels = record.rater_a == "answer" || record.rater_a == "refuse" || record.rater_a == "chosen" || record.rater_a == "tool_proposal";
        return known_schema && known_labels && !record.rater_b.empty() && record.rater_a == record.rater_b;
    }

    static std::string deterministic_split(const std::string& content_hash) {
        const uint64_t value = stage8_hash_string(content_hash) % 10U;
        if (value < 6U) return "train";
        if (value < 8U) return "development";
        return "release";
    }

    static void reject(DataItem& item, const std::string& reason) {
        item.status = "quarantined";
        item.decision_reason = reason;
        item.retained = false;
        item.training_allowed = false;
    }

    void erase_items_for_source(const std::string& source_id) {
        for (auto iterator = items_.begin(); iterator != items_.end();) {
            if (iterator->second.source_id == source_id) iterator = items_.erase(iterator);
            else ++iterator;
        }
    }

    size_t count_status(const std::string& status) const {
        size_t count = 0;
        for (const auto& entry : items_) if (entry.second.status == status) ++count;
        return count;
    }
    size_t count_reason(const std::string& reason) const {
        size_t count = 0;
        for (const auto& entry : items_) if (entry.second.decision_reason == reason) ++count;
        return count;
    }
    size_t count_flag_pii() const {
        size_t count = 0;
        for (const auto& entry : items_) if (entry.second.pii_detected) ++count;
        return count;
    }
    size_t count_flag_secret() const {
        size_t count = 0;
        for (const auto& entry : items_) if (entry.second.secret_detected) ++count;
        return count;
    }

    void audit(const std::string& operation, const std::string& subject, const std::string& outcome) {
        const uint64_t sequence = static_cast<uint64_t>(audit_.size() + 1U);
        audit_.push_back({sequence, "stage8", operation, subject, outcome, stage8_mix(stage8_hash_string(operation + subject + outcome), sequence)});
    }

    uint64_t now_unix_ = 0;
    std::map<std::string, SourceRecord> sources_;
    std::map<std::string, DataItem> items_;
    std::set<std::string> processed_sources_;
    std::set<std::string> deleted_sources_;
    std::vector<AccessEvent> audit_;
    DatasetRelease release_;
    bool release_approved_ = false;
};

} // namespace genesis
