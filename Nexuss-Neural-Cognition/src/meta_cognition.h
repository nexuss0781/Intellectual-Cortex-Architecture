#pragma once

#include "constants.h"
#include "types.h"
#include "utils.h"
#include "bio_engine.h"

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional>
#include <queue>
#include <algorithm>

namespace genesis {

// -----------------------------------------------------------------------------
// Meta-Cognitive Controller v3
// Dynamic resource allocation based on RAM capacity with adaptive scaling
// Priority-based reallocation between neurons and synapses
// -----------------------------------------------------------------------------

struct ResourceLimits {
    double ram_budget_mb;           // Primary constraint: RAM capacity (default 500MB)
    size_t max_neurons_config;      // Absolute ceiling for neurons
    size_t max_synapses_per_neuron; // Maximum connectivity density
    double target_realtime_factor;  // Performance target
    size_t min_neurons;             // Minimum neurons to maintain
    size_t min_synapses_per_neuron; // Minimum synapses per neuron
    
    ResourceLimits() : 
        ram_budget_mb(500.0),
        max_neurons_config(500000),
        max_synapses_per_neuron(100),
        target_realtime_factor(10.0),
        min_neurons(1000),
        min_synapses_per_neuron(5) {}
};

struct MemoryEstimate {
    size_t neurons;
    size_t synapses;
    double memory_mb;
    bool feasible;
    
    MemoryEstimate() : neurons(0), synapses(0), memory_mb(0.0), feasible(false) {}
};

// Tracks neuron activity for dynamic resource reallocation
struct ActivityMetrics {
    std::vector<bool> neuron_active;        // Is neuron currently participating?
    std::vector<size_t> neuron_spike_count; // Spike count in monitoring window
    std::vector<size_t> synapse_usage;      // How often each synapse transmits
    std::vector<bool> synapse_active;       // Is synapse currently used?
    size_t active_neuron_count;
    size_t active_synapse_count;
    size_t inactive_neuron_count;
    size_t inactive_synapse_count;
    double monitoring_window_ticks;
    size_t ticks_since_last_reset;
    
    ActivityMetrics() : active_neuron_count(0), active_synapse_count(0),
                        inactive_neuron_count(0), inactive_synapse_count(0),
                        monitoring_window_ticks(100), ticks_since_last_reset(0) {}
};

// Demand signals for resource allocation
enum class DemandLevel { NONE, LOW, MODERATE, HIGH, CRITICAL };

enum class AllocationStrategy {
    GROW_SMALL,           // Start small, grow when needed
    FAVOR_SYNAPSES,       // Need more synapses than neurons
    FAVOR_NEURONS,        // Need more neurons than synapses
    BALANCED,             // Both needed equally
    SHRINK_INACTIVE       // Release unused resources
};

struct DemandSignals {
    DemandLevel neuron_demand;
    DemandLevel synapse_demand;
    double neuron_utilization;     // active_neurons / allocated_neurons
    double synapse_utilization;    // active_synapses / allocated_synapses
    double neuron_to_synapse_ratio;// Ratio of needed neurons vs synapses
    double growth_pressure;        // How much we need to grow
    bool needs_rebalance;
    AllocationStrategy strategy;
    size_t neurons_needed;         // Additional neurons requested
    size_t synapses_needed;        // Additional synapses requested
    
    DemandSignals() : 
        neuron_demand(DemandLevel::NONE), 
        synapse_demand(DemandLevel::NONE),
        neuron_utilization(0.0), 
        synapse_utilization(0.0),
        neuron_to_synapse_ratio(1.0), 
        growth_pressure(0.0), 
        needs_rebalance(false),
        strategy(AllocationStrategy::GROW_SMALL),
        neurons_needed(0),
        synapses_needed(0) {}
};

// Resource reallocation result
struct ReallocationResult {
    bool success;
    std::string action_taken;
    size_t neurons_before;
    size_t neurons_after;
    size_t synapses_before;
    size_t synapses_after;
    double memory_before;
    double memory_after;
    
    ReallocationResult() : success(false), neurons_before(0), neurons_after(0),
                           synapses_before(0), synapses_after(0),
                           memory_before(0.0), memory_after(0.0) {}
};

class MetaCognitiveEngine {
public:
    MetaCognitiveEngine() = default;
    ~MetaCognitiveEngine() = default;

    // -------------------------------------------------------------------------
    // Initialization: Start small, grow dynamically based on RAM budget
    // -------------------------------------------------------------------------
    
    void init_auto_scale(const ResourceLimits& limits);
    
    // Manual override for testing
    void init_fixed(size_t neurons, size_t synapses_per_neuron, uint64_t seed);
    
    // -------------------------------------------------------------------------
    // Runtime: Dynamic resource reallocation
    // -------------------------------------------------------------------------
    
    void tick();
    
    // Request more resources (called by external systems or internal analysis)
    void request_more_neurons(size_t needed, const std::string& reason = "");
    void request_more_synapses(size_t needed_per_neuron, const std::string& reason = "");
    void release_unused_resources();
    
    // Force specific allocation strategy (for testing)
    void force_allocation_strategy(AllocationStrategy strategy);
    
    // Query current state
    size_t get_current_neurons() const { return engine_ ? neuron_count_ : 0; }
    size_t get_current_synapses() const { return engine_ ? synapse_count_ : 0; }
    size_t get_active_neurons() const { return activity_.active_neuron_count; }
    size_t get_active_synapses() const { return activity_.active_synapse_count; }
    size_t get_inactive_neurons() const { return activity_.inactive_neuron_count; }
    size_t get_inactive_synapses() const { return activity_.inactive_synapse_count; }
    double get_memory_usage_mb() const;
    double get_realtime_factor() const { return realtime_factor_; }
    double get_neuron_utilization() const;
    double get_synapse_utilization() const;
    
    // Access underlying engine for interaction
    BioEngine* get_engine() { return engine_.get(); }
    const BioEngine* get_engine() const { return engine_.get(); }
    
    // Get scaling decision info
    const ResourceLimits& get_limits() const { return limits_; }
    const MemoryEstimate& get_estimate() const { return estimate_; }
    const DemandSignals& get_demand() const { return demand_; }
    const ActivityMetrics& get_activity() const { return activity_; }
    
    // Print diagnostic info
    void print_status() const;
    void print_detailed_metrics() const;

    // -------------------------------------------------------------------------
    // Testing utilities
    // -------------------------------------------------------------------------
    
    // Simulate activity patterns for testing
    void simulate_neuron_activity(const std::vector<size_t>& active_ids);
    void simulate_synapse_usage(const std::vector<size_t>& active_ids);
    void reset_activity_metrics();

private:
    // -------------------------------------------------------------------------
    // Resource Detection & Initial Allocation
    // -------------------------------------------------------------------------
    
    double detect_available_ram_mb() const;
    MemoryEstimate calculate_initial_scale(const ResourceLimits& limits);
    void initialize_engine(size_t neurons, size_t synapses, uint64_t seed);
    
    // -------------------------------------------------------------------------
    // Dynamic Resource Reallocation Core Logic
    // -------------------------------------------------------------------------
    
    void analyze_demand();
    ReallocationResult reallocate_resources();
    
    // Strategy implementations
    ReallocationResult execute_grow_small();
    ReallocationResult execute_favor_synapses();
    ReallocationResult execute_favor_neurons();
    ReallocationResult execute_balanced();
    ReallocationResult execute_shrink_inactive();
    
    // Helper functions for reallocation
    bool can_allocate_more_memory(double additional_mb) const;
    double get_available_memory_mb() const;
    double calculate_memory_for_config(size_t neurons, size_t synapses) const;
    
    // Neuron activation/deactivation
    void activate_neurons(size_t count);
    void deactivate_neurons(size_t count);
    void deactivate_specific_neurons(const std::vector<size_t>& neuron_ids);
    void mark_neuron_active(size_t neuron_id);
    void mark_neuron_inactive(size_t neuron_id);
    void update_activity_metrics();
    std::vector<size_t> find_inactive_neurons(size_t count) const;
    
    // Synapse management
    void initialize_synapses_on_demand(size_t neuron_id, size_t count);
    void prune_unused_synapses();
    void prune_synapses_for_neurons(size_t synapses_to_remove);
    std::vector<size_t> find_unused_synapses(size_t count) const;
    
    // Network reconfiguration (creates new engine with different config)
    ReallocationResult reconfigure_network(size_t new_neurons, size_t new_synapses_per_neuron);
    
    // -------------------------------------------------------------------------
    // Performance Monitoring
    // -------------------------------------------------------------------------
    
    void update_performance_metrics();
    DemandLevel assess_demand_level(double utilization, double threshold_low, 
                                    double threshold_high, double threshold_critical);
    
    // -------------------------------------------------------------------------
    // Data
    // -------------------------------------------------------------------------
    
    std::unique_ptr<BioEngine> engine_;
    ResourceLimits limits_;
    MemoryEstimate estimate_;
    ActivityMetrics activity_;
    DemandSignals demand_;
    
    size_t neuron_count_ = 0;
    size_t synapse_count_ = 0;
    size_t synapses_per_neuron_ = 0;
    uint64_t seed_ = 42;
    
    // Performance tracking
    double realtime_factor_ = 1.0;
    std::vector<double> tick_times_ms_;
    size_t performance_window_ = 100;
    size_t tick_counter_ = 0;
    
    // Growth control
    size_t growth_step_neurons_ = 500;
    size_t growth_step_synapses_ = 5;
    size_t last_reallocation_tick_ = 0;
    size_t reallocation_cooldown_ = 20; // Ticks between reallocations
    
    // External requests queue
    std::queue<std::pair<size_t, std::string>> pending_neuron_requests_;
    std::queue<std::pair<size_t, std::string>> pending_synapse_requests_;
    
    // Forced strategy (for testing)
    bool force_strategy_ = false;
    AllocationStrategy forced_strategy_ = AllocationStrategy::GROW_SMALL;
    
    // Constants for memory estimation (empirically derived)
    // Formula: MB = (neurons * 88 + synapses * 32) / 1048576 * 1.15
    static constexpr double BYTES_PER_NEURON = 88.0;
    static constexpr double BYTES_PER_SYNAPSE = 32.0;
    static constexpr double OVERHEAD_FACTOR = 1.15;
    
    // Activity thresholds
    static constexpr double UTILIZATION_HIGH = 0.80;    // Triggers growth
    static constexpr double UTILIZATION_LOW = 0.30;     // Triggers shrink
    static constexpr double UTILIZATION_CRITICAL = 0.95;// Urgent growth needed
    static constexpr double IMBALANCE_THRESHOLD = 0.20; // 20% imbalance triggers rebalance
};

} // namespace genesis
