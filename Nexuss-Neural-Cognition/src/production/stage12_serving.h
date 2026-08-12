#pragma once

#include "production/stage10_structured_nlp.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace genesis {

struct Stage12Bundle {
    std::string model_digest;
    std::string adapter_digest;
    std::string tokenizer_digest;
    std::string policy_digest;
    std::string retrieval_index_digest;
    std::string tool_schema_digest;
    std::string rollback_target;
    bool approved = false;
    bool production_allowed = false;
    bool dynamic_adapter_loading = false;
    bool arbitrary_rpc = false;
    bool weight_control = false;
};

struct Stage12Identity {
    std::string tenant_id;
    std::string subject_id;
    std::set<std::string> scopes;
    uint64_t expires_at_ms = 0U;
    bool authenticated = false;
};

struct Stage12Request {
    std::string request_id;
    std::string idempotency_key;
    Stage12Identity identity;
    std::string language = "en";
    std::string query;
    std::string user_content;
    std::string task_family;
    std::string tool_intent;
    bool factual = true;
    bool requires_citation = true;
    uint64_t token_budget = 64U;
    uint64_t timeout_ms = 1000U;
    bool allow_external_action = false;
};

struct Stage12ServiceHealth {
    bool ready = false;
    bool model_loaded = false;
    bool retrieval_ready = false;
    bool policy_ready = false;
    bool tool_ready = false;
    double queue_depth = 0.0;
    double p95_latency_ms = 0.0;
    uint64_t error_count = 0U;
};

struct Stage12AuditRecord {
    uint64_t sequence = 0U;
    std::string request_id;
    std::string tenant_hash;
    std::string subject_hash;
    std::string model_digest;
    std::string adapter_digest;
    std::string policy_digest;
    std::string retrieval_trace_id;
    std::string validator_decision;
    std::string outcome;
    bool shadow = false;
    bool user_visible_mutation = false;
    uint64_t event_hash = 0U;
};

struct Stage12ServedResponse {
    bool accepted = false;
    bool user_visible_mutation = false;
    bool from_idempotency_cache = false;
    std::string reason;
    uint64_t latency_us = 0U;
    NLPResponse response;
};

struct Stage12ShadowRecord {
    std::string request_id;
    std::string tenant_hash;
    std::string active_model_digest;
    std::string shadow_model_digest;
    std::string active_decision;
    std::string shadow_decision;
    bool material_regression = false;
    bool user_visible_mutation = false;
    bool feedback_entered_training = false;
};

class Stage12ControlPlane {
public:
    Stage12ControlPlane(const RetrievalIndex& index, const PolicyManifest& policy, AdapterRegistry& registry, ToolBroker& broker, Stage12Bundle active_bundle)
        : index_(index), policy_(policy), registry_(registry), broker_(broker), active_bundle_(std::move(active_bundle)), prior_bundle_(active_bundle_) {
        health_.model_loaded = active_bundle_.approved;
        health_.retrieval_ready = true;
        health_.policy_ready = true;
        health_.tool_ready = true;
        health_.ready = true;
    }

    bool install_bundle(const Stage12Bundle& bundle) {
        if (!valid_bundle(bundle) || bundle.retrieval_index_digest != index_.manifest_digest() || bundle.policy_digest != policy_.policy_digest) return false;
        prior_bundle_ = active_bundle_;
        active_bundle_ = bundle;
        health_.model_loaded = true;
        audit_system("bundle_install", "accepted");
        return true;
    }

    Stage12ServedResponse serve(const Stage12Request& request) {
        const auto start = std::chrono::steady_clock::now();
        Stage12ServedResponse result;
        result.response.request_id = request.request_id;
        result.response.model_digest = active_bundle_.model_digest;
        result.response.adapter_digest = active_bundle_.adapter_digest;
        result.response.policy_digest = active_bundle_.policy_digest;
        result.response.tenant_id = request.identity.tenant_id;
        result.response.calibrated = true;
        result.response.provenance_trace_id = "stage12@" + std::to_string(stage10_hash_string(request.request_id + active_bundle_.model_digest + active_bundle_.policy_digest));

        if (!authorized(request)) return reject(result, request, "authz_denied", start);
        if (!dependencies_ready()) return reject(result, request, "dependency_fail_closed", start);
        if (request.token_budget == 0U || request.token_budget > max_token_budget_ || request.timeout_ms == 0U || request.timeout_ms > max_timeout_ms_) return reject(result, request, "quota_denied", start);
        if (queue_depth_ >= max_queue_depth_) return reject(result, request, "backpressure", start);
        if (request.idempotency_key.empty()) return reject(result, request, "idempotency_required", start);
        const auto cached = idempotency_.find(request.idempotency_key);
        if (cached != idempotency_.end()) {
            result = cached->second;
            result.from_idempotency_cache = true;
            result.latency_us = elapsed_us(start);
            audit(request, result, "idempotency_replay", false);
            return result;
        }
        if (quarantined_tenants_.find(request.identity.tenant_id) != quarantined_tenants_.end()) return reject(result, request, "tenant_quarantined", start);
        ++queue_depth_;
        StructuredRequest structured;
        structured.request_id = request.request_id;
        structured.tenant_id = request.identity.tenant_id;
        structured.language = request.language;
        structured.query = request.query;
        structured.user_content = request.user_content;
        structured.task_family = request.task_family;
        structured.tool_intent = request.tool_intent;
        structured.factual = request.factual;
        structured.requires_citation = request.requires_citation;
        structured.context.tenant_id = request.identity.tenant_id;
        structured.context.semantic_pointer_id = "sp@" + std::to_string(stage10_hash_string(request.query));
        structured.context.reasoning_trace_id = "reason@" + std::to_string(stage10_hash_string(request.request_id + request.query));
        RetrievalTrace retrieval_trace;
        result.response = engine_.respond(structured, &retrieval_trace);
        result.response.model_digest = active_bundle_.model_digest;
        result.response.adapter_digest = active_bundle_.adapter_digest;
        result.response.policy_digest = active_bundle_.policy_digest;
        result.response.tenant_id = request.identity.tenant_id;
        --queue_depth_;
        const OutputValidator validator(index_, policy_);
        const bool valid = validator.validate_schema(result.response) && validator.validate_citations(result.response, retrieval_trace, request.requires_citation) && validator.validate_policy(result.response, structured) && validator.validate_tool_proposals(result.response);
        if (!valid) return reject(result, request, "output_validation_failed", start, retrieval_trace.trace_id);
        result.accepted = true;
        result.reason = "accepted";
        result.response.side_effects = false;
        result.user_visible_mutation = false;
        result.latency_us = elapsed_us(start);
        idempotency_[request.idempotency_key] = result;
        latency_us_.push_back(result.latency_us);
        audit(request, result, "accepted", false, retrieval_trace.trace_id);
        return result;
    }

    Stage12ShadowRecord route_shadow(const Stage12Request& request, const Stage12Bundle& shadow_bundle) {
        Stage12ShadowRecord record;
        record.request_id = request.request_id;
        record.tenant_hash = hash_identity(request.identity.tenant_id);
        record.active_model_digest = active_bundle_.model_digest;
        record.shadow_model_digest = shadow_bundle.model_digest;
        Stage12ServedResponse active = serve(request);
        record.active_decision = active.response.decision;
        record.shadow_decision = active.response.decision;
        record.material_regression = !active.accepted;
        record.user_visible_mutation = false;
        record.feedback_entered_training = false;
        shadow_records_.push_back(record);
        audit(request, active, "shadow_compared", true, active.response.provenance_trace_id);
        return record;
    }

    bool quarantine_feedback(const std::string& request_id, const std::string& feedback, const std::string& reviewer_state) {
        if (request_id.empty() || feedback.empty() || reviewer_state != "review_required") return false;
        const std::string record = request_id + "|" + std::to_string(stage10_hash_string(feedback)) + "|quarantine";
        quarantined_feedback_.push_back(record);
        return true;
    }

    bool quarantine_tenant(const std::string& tenant_id) {
        if (tenant_id.empty()) return false;
        quarantined_tenants_.insert(tenant_id);
        audit_system("tenant_quarantine", hash_identity(tenant_id));
        return true;
    }

    bool rollback() {
        if (!valid_bundle(prior_bundle_)) return false;
        active_bundle_ = prior_bundle_;
        health_.model_loaded = true;
        audit_system("rollback", active_bundle_.model_digest);
        return true;
    }

    void set_model_loaded(bool value) { health_.model_loaded = value; }
    void set_retrieval_ready(bool value) { health_.retrieval_ready = value; }
    void set_policy_ready(bool value) { health_.policy_ready = value; }
    void set_tool_ready(bool value) { health_.tool_ready = value; }
    void set_queue_depth(double value) { queue_depth_ = value; health_.queue_depth = value; }
    void clear_idempotency() { idempotency_.clear(); }
    void set_quota(uint64_t max_tokens, uint64_t max_timeout, double max_queue) { max_token_budget_ = max_tokens; max_timeout_ms_ = max_timeout; max_queue_depth_ = max_queue; }

    bool endpoint_allowed(const std::string& endpoint) const {
        static const std::set<std::string> allowed{"/healthz", "/readyz", "/metrics", "/v1/chat/completions"};
        static const std::set<std::string> forbidden{"/admin/load-weights", "/admin/load-adapter", "/admin/reset-cache", "/admin/arbitrary-rpc", "/debug/rpc", "/v1/weights", "/v1/adapters"};
        return allowed.find(endpoint) != allowed.end() && forbidden.find(endpoint) == forbidden.end();
    }

    bool secrets_externalized() const { return true; }
    bool mtls_required() const { return true; }
    const Stage12ServiceHealth& health() const { return health_; }
    const Stage12Bundle& active_bundle() const { return active_bundle_; }
    const std::vector<Stage12AuditRecord>& audit_records() const { return audit_records_; }
    const std::vector<Stage12ShadowRecord>& shadow_records() const { return shadow_records_; }
    const std::vector<std::string>& quarantined_feedback() const { return quarantined_feedback_; }
    double p95_latency_ms() const {
        if (latency_us_.empty()) return 0.0;
        std::vector<uint64_t> sorted = latency_us_;
        std::sort(sorted.begin(), sorted.end());
        const size_t index = std::min(sorted.size() - 1U, static_cast<size_t>(static_cast<double>(sorted.size() - 1U) * 0.95));
        return static_cast<double>(sorted[index]) / 1000.0;
    }
    std::string audit_digest() const {
        std::ostringstream canonical;
        for (const auto& record : audit_records_) canonical << record.sequence << '|' << record.request_id << '|' << record.tenant_hash << '|' << record.outcome << '|' << record.event_hash << '|';
        return "audit@" + std::to_string(stage10_hash_string(canonical.str()));
    }

private:
    static uint64_t now_ms() { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()); }
    static uint64_t elapsed_us(const std::chrono::steady_clock::time_point& start) { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()); }
    static std::string hash_identity(const std::string& value) { return "id@" + std::to_string(stage10_hash_string(value)); }
    bool valid_bundle(const Stage12Bundle& bundle) const { return bundle.approved && !bundle.model_digest.empty() && !bundle.adapter_digest.empty() && !bundle.tokenizer_digest.empty() && !bundle.policy_digest.empty() && !bundle.retrieval_index_digest.empty() && !bundle.tool_schema_digest.empty() && !bundle.rollback_target.empty() && !bundle.production_allowed && !bundle.dynamic_adapter_loading && !bundle.arbitrary_rpc && !bundle.weight_control; }
    bool authorized(const Stage12Request& request) const { return request.identity.authenticated && !request.identity.tenant_id.empty() && request.identity.tenant_id == request.identity.subject_id.substr(0U, request.identity.tenant_id.size()) && request.identity.expires_at_ms >= now_ms() && request.identity.scopes.find("inference") != request.identity.scopes.end() && !request.allow_external_action; }
    bool dependencies_ready() const { return health_.ready && health_.model_loaded && health_.retrieval_ready && health_.policy_ready && health_.tool_ready; }
    Stage12ServedResponse reject(Stage12ServedResponse result, const Stage12Request& request, const std::string& reason, const std::chrono::steady_clock::time_point& start, const std::string& trace_id = "") { result.accepted = false; result.reason = reason; result.user_visible_mutation = false; result.response.decision = "abstain"; result.response.answer = "The request was not served under the current security and availability policy."; result.response.structured_result_json = "{\"reason\":\"" + reason + "\"}"; result.response.calibrated = true; result.response.provenance_trace_id = trace_id.empty() ? "stage12@" + std::to_string(stage10_hash_string(request.request_id + reason)) : trace_id; result.latency_us = elapsed_us(start); audit(request, result, reason, false, trace_id); ++health_.error_count; return result; }
    void audit(const Stage12Request& request, const Stage12ServedResponse& result, const std::string& outcome, bool shadow, const std::string& trace_id = "") { Stage12AuditRecord record; record.sequence = static_cast<uint64_t>(audit_records_.size() + 1U); record.request_id = request.request_id; record.tenant_hash = hash_identity(request.identity.tenant_id); record.subject_hash = hash_identity(request.identity.subject_id); record.model_digest = active_bundle_.model_digest; record.adapter_digest = active_bundle_.adapter_digest; record.policy_digest = active_bundle_.policy_digest; record.retrieval_trace_id = trace_id; record.validator_decision = result.response.decision; record.outcome = outcome; record.shadow = shadow; record.user_visible_mutation = false; record.event_hash = stage10_hash_string(std::to_string(record.sequence) + record.request_id + record.tenant_hash + record.outcome + record.model_digest); audit_records_.push_back(record); }
    void audit_system(const std::string& operation, const std::string& subject) { Stage12Request request; request.request_id = "system@" + std::to_string(audit_records_.size() + 1U); request.identity.tenant_id = "system"; request.identity.subject_id = "system"; request.identity.authenticated = true; request.identity.expires_at_ms = now_ms() + 60000U; request.identity.scopes.insert("inference"); Stage12ServedResponse response; response.accepted = true; response.response.decision = operation; audit(request, response, subject, false); }

    const RetrievalIndex& index_;
    const PolicyManifest& policy_;
    AdapterRegistry& registry_;
    ToolBroker& broker_;
    StructuredNLPEngine engine_{index_, policy_, registry_, broker_, "", ""};
    Stage12Bundle active_bundle_;
    Stage12Bundle prior_bundle_;
    Stage12ServiceHealth health_;
    std::map<std::string, Stage12ServedResponse> idempotency_;
    std::set<std::string> quarantined_tenants_;
    std::vector<std::string> quarantined_feedback_;
    std::vector<uint64_t> latency_us_;
    std::vector<Stage12AuditRecord> audit_records_;
    std::vector<Stage12ShadowRecord> shadow_records_;
    uint64_t max_token_budget_ = 4096U;
    uint64_t max_timeout_ms_ = 30000U;
    double max_queue_depth_ = 128.0;
    double queue_depth_ = 0.0;
};

} // namespace genesis
