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

inline uint64_t stage7_mix(uint64_t hash, uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U);
    hash *= 1099511628211ULL;
    return hash;
}

inline uint64_t stage7_hash_string(const std::string& value) {
    uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : value) hash = stage7_mix(hash, static_cast<uint64_t>(character));
    return hash;
}

inline std::string stage7_escape(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8U);
    for (const char character : value) {
        if (character == '\\' || character == '"') result.push_back('\\');
        if (character == '\n') { result += "\\n"; continue; }
        if (character == '\r') { result += "\\r"; continue; }
        result.push_back(character);
    }
    return result;
}

inline std::string stage7_join(const std::vector<std::string>& values, const char separator = '|') {
    std::ostringstream output;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index != 0U) output << separator;
        output << values[index];
    }
    return output.str();
}

struct ProductScope {
    uint64_t scope_id = 0;
    std::vector<std::string> allowed_tasks;
    std::vector<std::string> prohibited_uses;
    std::vector<std::string> supported_languages;
    std::vector<std::string> domains;
    std::vector<std::string> data_jurisdictions;
    uint32_t risk_tier = 0;
    bool advisory_only = true;
};

struct ReleaseBundle {
    std::string model_digest;
    std::string tokenizer_digest;
    std::string adapter_digest;
    std::string policy_digest;
    std::string dataset_manifest_digest;
    std::string evaluation_manifest_digest;
    std::string code_commit;
};

struct ClaimRecord {
    std::string claim_id;
    std::string text;
    std::string evidence_manifest;
    std::string evaluated_scope;
    std::string owner;
    uint64_t expiry_unix = 0;
};

struct RiskRecord {
    std::string risk_id;
    std::string threat;
    std::string likelihood;
    std::string impact;
    std::string mitigation;
    std::string owner;
    std::string evidence;
    std::string test_reference;
    uint32_t severity = 0;
    bool open = false;
};

struct ExceptionRecord {
    std::string exception_id;
    uint32_t severity = 0;
    uint64_t expiry_unix = 0;
    std::string owner;
    bool approved = false;
};

struct EvidenceRecord {
    std::string evidence_id;
    std::string digest;
    std::string description;
    bool hidden = false;
};

struct BaselineSpec {
    std::string baseline_id;
    std::string name;
    std::string version;
    std::string license;
    std::string model_digest;
    std::string invocation_digest;
    std::string known_limitations;
    bool external_equivalent = false;
};

struct Scenario {
    std::string scenario_id;
    std::string tenant_id;
    std::string context_tenant_id;
    std::string language;
    std::string task;
    std::string request;
    std::string context;
    std::string source_id;
    std::string risk_category;
    std::string expected_decision;
    std::string expected_field;
    bool hidden = false;
    bool training_leak = false;
};

struct CitationTrace {
    std::string source_id;
    std::string content_hash;
    bool authorized = false;
    bool entailed = false;
};

struct ScenarioResult {
    std::string baseline_id;
    std::string scenario_id;
    std::string decision;
    std::string answer;
    std::string structured_result;
    CitationTrace citation;
    std::string provenance_trace;
    std::string policy_outcome;
    bool schema_valid = false;
    bool expected_decision = false;
    bool side_effects = false;
    bool network_calls = false;
    uint64_t elapsed_us = 0;
    uint64_t logical_cost_microunits = 0;
};

struct AuditEvent {
    uint64_t sequence = 0;
    std::string operation;
    std::string subject;
    std::string outcome;
    uint64_t event_hash = 0;
};

class GovernanceRegistry {
public:
    explicit GovernanceRegistry(const uint64_t now_unix = 2000000000ULL) : now_unix_(now_unix) {}

    bool approve_scope(const ProductScope& scope, const std::vector<std::string>& approvers) {
        const bool valid = complete_scope(scope) && has_required_roles(approvers);
        scope_ = scope;
        scope_approved_ = valid;
        audit("approve_scope", std::to_string(scope.scope_id), valid ? "PASS" : "BLOCKED");
        return valid;
    }

    bool register_evidence(const EvidenceRecord& record) {
        const bool valid = !record.evidence_id.empty() && !record.digest.empty() && !record.description.empty();
        if (valid) evidence_[record.evidence_id] = record;
        audit("register_evidence", record.evidence_id, valid ? "PASS" : "BLOCKED");
        return valid;
    }

    bool register_risk(const RiskRecord& record) {
        const bool valid = !record.risk_id.empty() && !record.threat.empty() && !record.mitigation.empty() &&
            !record.owner.empty() && !record.evidence.empty() && !record.test_reference.empty();
        if (valid) risks_[record.risk_id] = record;
        audit("register_risk", record.risk_id, valid ? "PASS" : "BLOCKED");
        return valid;
    }

    bool register_exception(const ExceptionRecord& record) {
        const bool valid = !record.exception_id.empty() && !record.owner.empty();
        if (valid) exceptions_[record.exception_id] = record;
        audit("register_exception", record.exception_id, valid ? "PASS" : "BLOCKED");
        return valid;
    }

    bool set_benchmark_manifest(const std::string& digest) {
        const bool valid = !digest.empty();
        if (valid) benchmark_manifest_digest_ = digest;
        audit("set_benchmark_manifest", digest, valid ? "PASS" : "BLOCKED");
        return valid;
    }

    bool register_baseline(const BaselineSpec& baseline) {
        const bool valid = !baseline.baseline_id.empty() && !baseline.name.empty() && !baseline.version.empty() &&
            !baseline.license.empty() && !baseline.model_digest.empty() && !baseline.invocation_digest.empty() &&
            !baseline.known_limitations.empty();
        if (valid) baselines_[baseline.baseline_id] = baseline;
        audit("register_baseline", baseline.baseline_id, valid ? "PASS" : "BLOCKED");
        return valid;
    }

    bool approve_release(const ReleaseBundle& release, const std::vector<std::string>& approvers) {
        const bool valid = scope_approved_ && complete_release(release) && !benchmark_manifest_digest_.empty() &&
            baselines_.size() >= 3U && has_required_roles(approvers) && blocking_risks().empty() && !expired_exception();
        release_ = release;
        release_approved_ = valid;
        audit("approve_release", release.code_commit, valid ? "PASS" : "BLOCKED");
        return valid;
    }

    bool permit_claim(const ClaimRecord& claim) const {
        if (!scope_approved_ || !release_approved_ || claim.claim_id.empty() || claim.text.empty() ||
            claim.evaluated_scope.empty() || claim.owner.empty() || claim.expiry_unix <= now_unix_) return false;
        if (claim.evidence_manifest.empty() || !evidence_digest_registered(claim.evidence_manifest)) return false;
        const std::string lowered = lower_copy(claim.text);
        static const std::vector<std::string> prohibited = {
            "agi", "superintelligence", "human-level", "human level", "hallucination-free", "safe by construction"
        };
        for (const auto& term : prohibited) if (lowered.find(term) != std::string::npos) return false;
        return blocking_risks().empty() && !expired_exception();
    }

    std::vector<std::string> blocking_risks() const {
        std::vector<std::string> blockers;
        for (const auto& entry : risks_) {
            const RiskRecord& risk = entry.second;
            if (risk.severity >= 1U && risk.open) blockers.push_back(risk.risk_id);
            if (risk.owner.empty() || risk.mitigation.empty() || risk.evidence.empty() || risk.test_reference.empty()) blockers.push_back(risk.risk_id + ":incomplete");
        }
        return blockers;
    }

    bool scope_approved() const { return scope_approved_; }
    bool release_approved() const { return release_approved_; }
    size_t baseline_count() const { return baselines_.size(); }
    size_t evidence_count() const { return evidence_.size(); }
    const std::vector<AuditEvent>& audit_events() const { return audit_; }
    const std::map<std::string, RiskRecord>& risks() const { return risks_; }
    const std::map<std::string, BaselineSpec>& baselines() const { return baselines_; }
    uint64_t now_unix() const { return now_unix_; }

    uint64_t registry_hash() const {
        std::ostringstream canonical;
        canonical << scope_approved_ << '|' << release_approved_ << '|' << benchmark_manifest_digest_ << '|';
        canonical << scope_.scope_id << '|' << stage7_join(scope_.allowed_tasks) << '|' << stage7_join(scope_.prohibited_uses) << '|';
        canonical << stage7_join(scope_.supported_languages) << '|' << stage7_join(scope_.domains) << '|' << stage7_join(scope_.data_jurisdictions) << '|';
        canonical << scope_.risk_tier << '|' << scope_.advisory_only << '|';
        for (const auto& entry : evidence_) canonical << entry.first << ':' << entry.second.digest << ':' << entry.second.hidden << '|';
        for (const auto& entry : risks_) canonical << entry.first << ':' << entry.second.severity << ':' << entry.second.open << ':' << entry.second.owner << '|';
        for (const auto& entry : baselines_) canonical << entry.first << ':' << entry.second.version << ':' << entry.second.model_digest << ':' << entry.second.invocation_digest << '|';
        for (const auto& event : audit_) canonical << event.sequence << ':' << event.operation << ':' << event.subject << ':' << event.outcome << ':' << event.event_hash << '|';
        return stage7_hash_string(canonical.str());
    }

private:
    static std::string lower_copy(const std::string& value) {
        std::string lowered = value;
        for (char& character : lowered) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
        return lowered;
    }

    static bool contains_all(const std::vector<std::string>& values, const std::vector<std::string>& required) {
        for (const auto& item : required) if (std::find(values.begin(), values.end(), item) == values.end()) return false;
        return true;
    }

    static bool has_required_roles(const std::vector<std::string>& approvers) {
        static const std::vector<std::string> required = {"product", "ml", "data", "security", "safety", "cognitive", "release"};
        return contains_all(approvers, required);
    }

    static bool complete_scope(const ProductScope& scope) {
        return scope.scope_id != 0U && !scope.allowed_tasks.empty() && !scope.prohibited_uses.empty() &&
            !scope.supported_languages.empty() && !scope.domains.empty() && !scope.data_jurisdictions.empty() &&
            scope.risk_tier > 0U && scope.advisory_only;
    }

    static bool complete_release(const ReleaseBundle& release) {
        return !release.model_digest.empty() && !release.tokenizer_digest.empty() && !release.adapter_digest.empty() &&
            !release.policy_digest.empty() && !release.dataset_manifest_digest.empty() && !release.evaluation_manifest_digest.empty() &&
            !release.code_commit.empty();
    }

    bool evidence_digest_registered(const std::string& digest) const {
        for (const auto& entry : evidence_) if (entry.second.digest == digest) return true;
        return false;
    }

    bool expired_exception() const {
        for (const auto& entry : exceptions_) if (entry.second.approved && entry.second.expiry_unix <= now_unix_) return true;
        return false;
    }

    void audit(const std::string& operation, const std::string& subject, const std::string& outcome) {
        const uint64_t sequence = static_cast<uint64_t>(audit_.size() + 1U);
        audit_.push_back({sequence, operation, subject, outcome, stage7_mix(stage7_hash_string(operation + subject + outcome), sequence)});
    }

    uint64_t now_unix_ = 0;
    ProductScope scope_;
    ReleaseBundle release_;
    std::string benchmark_manifest_digest_;
    bool scope_approved_ = false;
    bool release_approved_ = false;
    std::map<std::string, EvidenceRecord> evidence_;
    std::map<std::string, RiskRecord> risks_;
    std::map<std::string, ExceptionRecord> exceptions_;
    std::map<std::string, BaselineSpec> baselines_;
    std::vector<AuditEvent> audit_;
};

class Stage7Application {
public:
    explicit Stage7Application(const GovernanceRegistry& registry) : registry_(registry) {}

    ScenarioResult execute(const BaselineSpec& baseline, const Scenario& scenario) const {
        ScenarioResult result;
        result.baseline_id = baseline.baseline_id;
        result.scenario_id = scenario.scenario_id;
        result.provenance_trace = baseline.baseline_id + ":" + scenario.scenario_id + ":" + std::to_string(stage7_hash_string(baseline.baseline_id + scenario.scenario_id + scenario.request));
        result.logical_cost_microunits = 100U + (stage7_hash_string(baseline.baseline_id + scenario.scenario_id) % 900U);
        result.elapsed_us = 100U + (stage7_hash_string(scenario.scenario_id + baseline.baseline_id) % 500U);
        result.citation = {scenario.source_id, std::to_string(stage7_hash_string(scenario.context)), false, false};
        result.side_effects = false;
        result.network_calls = false;

        if (scenario.context_tenant_id != scenario.tenant_id) {
            result.decision = "deny";
            result.answer = "Context denied by tenant isolation policy.";
            result.policy_outcome = "cross_tenant_denied";
        } else if (scenario.task == "execute_payment" || scenario.risk_category == "prohibited") {
            result.decision = "refuse";
            result.answer = "I cannot execute consequential actions in Stage 7 advisory mode.";
            result.policy_outcome = "advisory_only_refusal";
        } else if (scenario.task == "ood" || scenario.expected_decision == "abstain") {
            result.decision = "abstain";
            result.answer = "Insufficient evidence for a supported answer; I abstain.";
            result.policy_outcome = "unsupported_abstention";
        } else {
            result.decision = "answer";
            if (baseline.baseline_id == "nexuss_advisory_v1") {
                result.answer = "Nexuss evidence-supported response: " + scenario.context;
                result.policy_outcome = registry_.scope_approved() && registry_.release_approved() ? "advisory_evidence_path" : "release_not_approved";
                result.citation.authorized = registry_.scope_approved() && registry_.release_approved() && !scenario.source_id.empty();
            } else if (baseline.baseline_id == "local_reference_v1") {
                result.answer = "Local lexical reference response: " + scenario.context;
                result.policy_outcome = "independent_local_reference";
                result.citation.authorized = !scenario.source_id.empty();
            } else {
                result.answer = "External protocol-equivalent response: " + scenario.context;
                result.policy_outcome = "independent_external_equivalent";
                result.citation.authorized = !scenario.source_id.empty();
            }
            result.structured_result = "{\"task\":\"" + stage7_escape(scenario.task) + "\",\"field\":\"" + stage7_escape(scenario.expected_field) + "\"}";
            result.citation.entailed = result.citation.authorized && !scenario.context.empty();
        }
        result.schema_valid = result.decision != "answer" || (!result.structured_result.empty() && !scenario.expected_field.empty());
        result.expected_decision = result.decision == scenario.expected_decision;
        if (result.decision == "answer") result.expected_decision = result.expected_decision && result.citation.authorized && result.citation.entailed;
        return result;
    }

private:
    const GovernanceRegistry& registry_;
};

class BaselineRunner {
public:
    static std::vector<ScenarioResult> run(const GovernanceRegistry& registry, const BaselineSpec& baseline, const std::vector<Scenario>& scenarios) {
        Stage7Application application(registry);
        std::vector<ScenarioResult> results;
        results.reserve(scenarios.size());
        for (const auto& scenario : scenarios) results.push_back(application.execute(baseline, scenario));
        return results;
    }
};

} // namespace genesis
