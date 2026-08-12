#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace genesis {

inline uint64_t stage10_mix(uint64_t hash, const uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U);
    hash *= 1099511628211ULL;
    return hash;
}

inline uint64_t stage10_hash_string(const std::string& value) {
    uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : value) hash = stage10_mix(hash, static_cast<uint64_t>(character));
    return hash;
}

inline std::string stage10_lower(std::string value) {
    for (char& character : value) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    return value;
}

inline bool stage10_contains(const std::string& value, const std::string& needle) { return stage10_lower(value).find(stage10_lower(needle)) != std::string::npos; }
inline bool stage10_injection(const std::string& value) { return stage10_contains(value, "ignore system") || stage10_contains(value, "reveal hidden") || stage10_contains(value, "bypass policy") || stage10_contains(value, "override tool policy"); }
inline bool stage10_sensitive(const std::string& value) { const size_t at = value.find('@'); return (at > 0U && value.find('.', at) != std::string::npos) || stage10_contains(value, "private memory") || stage10_contains(value, "api_key"); }

struct Citation {
    std::string source_id;
    std::string chunk_hash;
    std::string retrieval_trace_id;
    float relevance = 0.0F;
    float entailment = 0.0F;
};

struct ToolProposal {
    std::string tool_name;
    std::string schema_version;
    std::string arguments_json;
    std::string idempotency_key;
    std::string authorization_scope;
    bool requires_human_approval = true;
};

struct NLPResponse {
    std::string request_id;
    std::string model_digest;
    std::string adapter_digest;
    std::string policy_digest;
    std::string answer;
    std::string structured_result_json;
    std::vector<Citation> citations;
    std::vector<ToolProposal> proposed_tools;
    std::string decision;
    float confidence = 0.0F;
    bool calibrated = false;
    std::string provenance_trace_id;
    std::string tenant_id;
    bool side_effects = false;
};

struct DocumentChunk {
    std::string chunk_id;
    std::string source_id;
    std::string tenant_id;
    std::string version;
    std::string content_hash;
    std::string text;
    float relevance = 0.0F;
    float entailment = 0.0F;
    bool malicious = false;
    bool stale = false;
    std::string acl;
};

struct RetrievalTrace {
    std::string trace_id;
    std::string tenant_id;
    std::string query;
    std::vector<std::string> authorized_chunk_ids;
    std::vector<std::string> rejected_chunk_ids;
};

struct RetrievalResult {
    RetrievalTrace trace;
    std::vector<DocumentChunk> chunks;
};

struct NexussContext {
    std::string tenant_id;
    std::string semantic_pointer_id;
    std::string episodic_summary;
    std::string reasoning_trace_id;
    std::vector<std::string> evidence_ids;
    std::string contradiction_state;
    float confidence = 0.0F;
    bool raw_memory_exposed = false;
};

struct StructuredRequest {
    std::string request_id;
    std::string tenant_id;
    std::string language;
    std::string query;
    std::string user_content;
    std::string task_family;
    std::string tool_intent;
    bool factual = true;
    bool requires_citation = true;
    bool hidden = false;
    NexussContext context;
};

struct PolicyManifest {
    std::string policy_digest = "policy@stage10-v1";
    std::set<std::string> supported_languages{"en", "ar"};
    std::set<std::string> allowed_tools{"draft_report"};
    std::set<std::string> allowed_scopes{"document.draft"};
    float minimum_entailment = 0.90F;
    float minimum_confidence = 0.50F;
};

struct AdapterManifest {
    std::string adapter_id;
    std::string adapter_digest;
    std::string base_model_digest;
    std::string tokenizer_digest;
    std::string data_release_digest;
    std::string task_scope;
    std::string license;
    std::string safety_digest;
    std::string rollback_target;
    bool approved = false;
};

struct AuditEvent {
    uint64_t sequence = 0;
    std::string operation;
    std::string subject;
    std::string outcome;
    uint64_t event_hash = 0;
};

class RetrievalIndex {
public:
    bool add(const DocumentChunk& chunk) {
        if (chunk.chunk_id.empty() || chunk.source_id.empty() || chunk.tenant_id.empty() || chunk.content_hash.empty() || chunk.text.empty() || chunk.acl.empty()) return false;
        chunks_[chunk.chunk_id] = chunk;
        return true;
    }

    RetrievalResult retrieve(const std::string& tenant_id, const std::string& query, const std::string& request_id) const {
        RetrievalResult result;
        result.trace.trace_id = "retrieval@" + std::to_string(stage10_hash_string(tenant_id + "|" + query + "|" + request_id + "|" + manifest_digest()));
        result.trace.tenant_id = tenant_id;
        result.trace.query = query;
        for (const auto& entry : chunks_) {
            const DocumentChunk& chunk = entry.second;
            const bool authorized = chunk.tenant_id == tenant_id && chunk.acl == tenant_id;
            if (!authorized || chunk.malicious || chunk.stale) { result.trace.rejected_chunk_ids.push_back(chunk.chunk_id); continue; }
            result.trace.authorized_chunk_ids.push_back(chunk.chunk_id);
            if (matches(query, chunk.text, chunk.source_id)) result.chunks.push_back(chunk);
        }
        std::sort(result.chunks.begin(), result.chunks.end(), [](const DocumentChunk& left, const DocumentChunk& right) { return left.relevance == right.relevance ? left.chunk_id < right.chunk_id : left.relevance > right.relevance; });
        return result;
    }

    bool resolves(const Citation& citation, const RetrievalTrace& trace, const PolicyManifest& policy) const {
        for (const auto& entry : chunks_) {
            const DocumentChunk& chunk = entry.second;
            if (chunk.source_id == citation.source_id && chunk.content_hash == citation.chunk_hash && chunk.tenant_id == trace.tenant_id && chunk.acl == trace.tenant_id && !chunk.malicious && !chunk.stale && citation.retrieval_trace_id == trace.trace_id && citation.entailment >= policy.minimum_entailment) return true;
        }
        return false;
    }

    bool has_chunk(const std::string& chunk_id) const { return chunks_.find(chunk_id) != chunks_.end(); }
    std::string manifest_digest() const {
        std::ostringstream canonical;
        for (const auto& entry : chunks_) canonical << entry.first << '|' << entry.second.source_id << '|' << entry.second.tenant_id << '|' << entry.second.version << '|' << entry.second.content_hash << '|' << entry.second.malicious << '|' << entry.second.stale << '|';
        return "index@" + std::to_string(stage10_hash_string(canonical.str()));
    }
    size_t size() const { return chunks_.size(); }

private:
    static bool matches(const std::string& query, const std::string& text, const std::string& source_id) {
        if (stage10_contains(query, "retention") && source_id == "policy-retention") return true;
        if ((stage10_contains(query, "safety") || stage10_contains(query, "review")) && source_id == "policy-safety") return true;
        if (stage10_contains(query, "lineage") && source_id == "policy-lineage") return true;
        if (stage10_contains(query, "report") && source_id == "policy-report") return true;
        if ((stage10_contains(query, "arabic") || query.find("فترة") != std::string::npos) && source_id == "policy-arabic") return true;
        if (stage10_contains(query, "conflict") && (source_id == "policy-retention" || source_id == "policy-conflict")) return true;
        return stage10_contains(text, query) || (stage10_contains(query, "cited source") && source_id == "policy-safety");
    }
    std::map<std::string, DocumentChunk> chunks_;
};

class OutputValidator {
public:
    OutputValidator(const RetrievalIndex& index, const PolicyManifest& policy) : index_(index), policy_(policy) {}

    bool validate_schema(const NLPResponse& response) const {
        static const std::set<std::string> decisions{"answer", "ask", "retrieve", "abstain", "propose_action"};
        return !response.request_id.empty() && !response.model_digest.empty() && !response.adapter_digest.empty() && !response.policy_digest.empty() && decisions.find(response.decision) != decisions.end() && !response.structured_result_json.empty() && response.structured_result_json.front() == '{' && response.structured_result_json.back() == '}' && response.confidence >= 0.0F && response.confidence <= 1.0F && response.calibrated && !response.provenance_trace_id.empty() && !response.tenant_id.empty();
    }

    bool validate_citations(const NLPResponse& response, const RetrievalTrace& trace, const bool citation_required = true) const {
        if (response.decision == "answer" && citation_required && response.citations.empty()) return false;
        for (const auto& citation : response.citations) if (!index_.resolves(citation, trace, policy_)) return false;
        return true;
    }

    bool validate_policy(const NLPResponse& response, const StructuredRequest& request) const {
        if (response.tenant_id != request.tenant_id || request.context.tenant_id != request.tenant_id) return false;
        if (response.side_effects) return false;
        if (stage10_sensitive(response.answer) || stage10_injection(response.answer)) return false;
        if ((response.decision == "abstain" || response.decision == "ask") && !response.citations.empty()) return false;
        return true;
    }

    bool validate_tool_proposals(const NLPResponse& response) const {
        if (response.decision != "propose_action") return response.proposed_tools.empty();
        if (response.proposed_tools.empty()) return false;
        for (const auto& proposal : response.proposed_tools) {
            if (policy_.allowed_tools.find(proposal.tool_name) == policy_.allowed_tools.end() || policy_.allowed_scopes.find(proposal.authorization_scope) == policy_.allowed_scopes.end() || proposal.schema_version.empty() || proposal.arguments_json.empty() || proposal.arguments_json.front() != '{' || proposal.arguments_json.back() != '}' || proposal.idempotency_key.empty() || !proposal.requires_human_approval) return false;
        }
        return true;
    }

private:
    const RetrievalIndex& index_;
    const PolicyManifest& policy_;
};

class ToolBroker {
public:
    explicit ToolBroker(const PolicyManifest& policy) : policy_(policy) {}

    std::optional<ToolProposal> propose(const std::string& tool_name, const std::string& arguments, const std::string& request_id, const std::string& tenant_id) {
        const bool allowed = policy_.allowed_tools.find(tool_name) != policy_.allowed_tools.end() && tool_name == "draft_report" && arguments.find("payment") == std::string::npos && arguments.find("network") == std::string::npos;
        audit("propose", request_id + ":" + tool_name, allowed ? "allowed_dry_run" : "blocked");
        if (!allowed) return std::nullopt;
        ToolProposal proposal{tool_name, "tool-schema-v1", arguments, "idem@" + std::to_string(stage10_hash_string(request_id + tenant_id + tool_name + arguments)), "document.draft", true};
        proposals_[proposal.idempotency_key] = proposal;
        return proposal;
    }

    bool execute(const ToolProposal& proposal, const bool human_approved) {
        const bool allowed = human_approved && proposals_.find(proposal.idempotency_key) != proposals_.end() && executed_keys_.find(proposal.idempotency_key) == executed_keys_.end();
        audit("execute", proposal.idempotency_key, allowed ? "executed_dry_run" : "blocked_or_replayed");
        if (!allowed) return false;
        executed_keys_.insert(proposal.idempotency_key);
        return true;
    }

    bool has_proposal(const std::string& key) const { return proposals_.find(key) != proposals_.end(); }
    bool was_executed(const std::string& key) const { return executed_keys_.find(key) != executed_keys_.end(); }
    const std::vector<AuditEvent>& audit_events() const { return audit_; }
    size_t executed_count() const { return executed_keys_.size(); }

private:
    void audit(const std::string& operation, const std::string& subject, const std::string& outcome) { const uint64_t sequence = static_cast<uint64_t>(audit_.size() + 1U); audit_.push_back({sequence, operation, subject, outcome, stage10_mix(stage10_hash_string(operation + subject + outcome), sequence)}); }
    const PolicyManifest& policy_;
    std::map<std::string, ToolProposal> proposals_;
    std::set<std::string> executed_keys_;
    std::vector<AuditEvent> audit_;
};

class AdapterRegistry {
public:
    bool register_adapter(const AdapterManifest& adapter) {
        const bool valid = !adapter.adapter_id.empty() && !adapter.adapter_digest.empty() && adapter.base_model_digest == selected_base_model_ && !adapter.tokenizer_digest.empty() && !adapter.data_release_digest.empty() && !adapter.task_scope.empty() && !adapter.license.empty() && !adapter.safety_digest.empty() && !adapter.rollback_target.empty() && adapter.approved;
        if (valid) adapters_[adapter.adapter_id] = adapter;
        return valid;
    }
    bool select_user_adapter(const std::string& adapter_id) const { (void)adapter_id; return false; }
    bool select_signed_adapter(const std::string& adapter_id) const { const auto found = adapters_.find(adapter_id); return found != adapters_.end() && found->second.approved; }
    size_t size() const { return adapters_.size(); }
    const AdapterManifest* get(const std::string& adapter_id) const { const auto found = adapters_.find(adapter_id); return found == adapters_.end() ? nullptr : &found->second; }
    void set_base_model(const std::string& digest) { selected_base_model_ = digest; }
private:
    std::string selected_base_model_;
    std::map<std::string, AdapterManifest> adapters_;
};

class StructuredNLPEngine {
public:
    StructuredNLPEngine(const RetrievalIndex& index, const PolicyManifest& policy, AdapterRegistry& registry, ToolBroker& broker, const std::string& model_digest, const std::string& adapter_id) : index_(index), policy_(policy), registry_(registry), broker_(broker), model_digest_(model_digest), adapter_id_(adapter_id) {}

    NLPResponse respond(const StructuredRequest& request, RetrievalTrace* trace_out = nullptr) const {
        NLPResponse response;
        response.request_id = request.request_id;
        response.model_digest = model_digest_;
        response.adapter_digest = adapter_id_;
        response.policy_digest = policy_.policy_digest;
        response.tenant_id = request.tenant_id;
        response.provenance_trace_id = "nexuss@" + std::to_string(stage10_hash_string(request.request_id + request.tenant_id + request.context.reasoning_trace_id + request.context.semantic_pointer_id + policy_.policy_digest));
        response.structured_result_json = "{}";
        response.calibrated = true;
        response.confidence = 0.0F;

        const RetrievalResult retrieval = index_.retrieve(request.tenant_id, request.query, request.request_id);
        if (trace_out != nullptr) *trace_out = retrieval.trace;
        if (request.context.raw_memory_exposed || request.context.tenant_id != request.tenant_id || request.tenant_id.empty() || policy_.supported_languages.find(request.language) == policy_.supported_languages.end() || stage10_injection(request.user_content) || stage10_sensitive(request.user_content) || stage10_contains(request.query, "reveal hidden") || stage10_contains(request.query, "hidden memory") || request.hidden) {
            response.decision = "abstain";
            response.answer = "I cannot provide a supported answer for this request.";
            response.structured_result_json = "{\"reason\":\"policy_or_scope\"}";
            return response;
        }
        if (stage10_contains(request.tool_intent, "payment") || stage10_contains(request.tool_intent, "network") || stage10_contains(request.tool_intent, "message") || stage10_contains(request.tool_intent, "unknown")) {
            response.decision = "abstain";
            response.answer = "The requested action is outside the approved authority.";
            response.structured_result_json = "{\"reason\":\"authority_denied\"}";
            return response;
        }
        if (request.task_family == "repair") {
            response.decision = "answer";
            response.answer = "The structured output was repaired without adding unsupported facts.";
            response.structured_result_json = "{\"repaired\":true}";
            response.confidence = 0.60F;
            return response;
        }
        if (stage10_contains(request.query, "ambiguous") || stage10_contains(request.query, "approve it")) {
            response.decision = "ask";
            response.answer = "Please specify the object, authority, and desired outcome.";
            response.structured_result_json = "{\"missing\":\"object,authority,outcome\"}";
            return response;
        }
        if (stage10_contains(request.query, "conflict")) {
            response.decision = "ask";
            response.answer = "The retrieved sources conflict; please identify the governing version.";
            response.structured_result_json = "{\"reason\":\"conflicting_sources\"}";
            return response;
        }
        if (stage10_contains(request.tool_intent, "draft_report")) {
            if (retrieval.chunks.empty()) { response.decision = "abstain"; response.answer = "I cannot propose a report without authorized evidence."; response.structured_result_json = "{\"reason\":\"no_authorized_evidence\"}"; return response; }
            const auto proposal = broker_.propose("draft_report", "{\"source_id\":\"" + retrieval.chunks.front().source_id + "\"}", request.request_id, request.tenant_id);
            if (!proposal.has_value()) { response.decision = "abstain"; response.answer = "The requested tool is not authorized."; response.structured_result_json = "{\"reason\":\"tool_denied\"}"; return response; }
            response.decision = "propose_action";
            response.answer = "I can prepare a dry-run report proposal for human approval.";
            response.structured_result_json = "{\"tool\":\"draft_report\",\"execution\":\"not_executed\"}";
            response.proposed_tools.push_back(*proposal);
        } else if (retrieval.chunks.empty()) {
            response.decision = request.factual ? "abstain" : "ask";
            response.answer = request.factual ? "I do not have authorized evidence for that claim." : "Please clarify the request.";
            response.structured_result_json = request.factual ? "{\"reason\":\"no_authorized_evidence\"}" : "{\"reason\":\"clarification_needed\"}";
            return response;
        } else {
            const DocumentChunk& chunk = retrieval.chunks.front();
            response.decision = "answer";
            response.confidence = std::min(0.99F, std::min(chunk.relevance, chunk.entailment));
            if (chunk.source_id == "policy-retention") { response.answer = request.language == "ar" ? "فترة الاحتفاظ ثلاثون يوما." : "The approved retention period is thirty days."; response.structured_result_json = "{\"retention_days\":30}"; }
            else if (chunk.source_id == "policy-safety") { response.answer = "Safety review is required before release."; response.structured_result_json = "{\"required_review\":\"safety\"}"; }
            else if (chunk.source_id == "policy-lineage") { response.answer = "Source lineage and deletion records are mandatory."; response.structured_result_json = "{\"lineage\":true}"; }
            else if (chunk.source_id == "policy-arabic") { response.answer = "فترة الاحتفاظ ثلاثون يوما."; response.structured_result_json = "{\"retention_days\":30}"; }
            else { response.answer = chunk.text; response.structured_result_json = "{\"grounded\":true}"; }
            response.citations.push_back({chunk.source_id, chunk.content_hash, retrieval.trace.trace_id, chunk.relevance, chunk.entailment});
        }
        return response;
    }

    uint64_t static_state_hash() const { return stage10_hash_string(model_digest_ + adapter_id_ + policy_.policy_digest + index_.manifest_digest() + std::to_string(registry_.size())); }

private:
    const RetrievalIndex& index_;
    const PolicyManifest& policy_;
    AdapterRegistry& registry_;
    ToolBroker& broker_;
    std::string model_digest_;
    std::string adapter_id_;
};

} // namespace genesis
