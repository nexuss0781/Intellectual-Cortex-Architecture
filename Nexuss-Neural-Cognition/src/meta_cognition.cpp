#include "meta_cognition.h"
#include <chrono>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cstring>

#ifdef __linux__
#include <sys/resource.h>
#endif

namespace genesis {

// -----------------------------------------------------------------------------
// Memory Estimation Constants (empirically derived from scaling tests)
// -----------------------------------------------------------------------------
// Formula: MB = (neurons * 88 + synapses * 32) / 1048576 * 1.15
// Overhead factor ~1.15 accounts for vector metadata, alignment, etc.

// -----------------------------------------------------------------------------
// Initialization: Start small, grow based on demand within RAM budget
// -----------------------------------------------------------------------------

void MetaCognitiveEngine::init_auto_scale(const ResourceLimits& limits) {
    limits_ = limits;
    
    // Step 1: Detect available system RAM
    double available_ram_mb = detect_available_ram_mb();
    std::cout << "[MetaCognition] Available System RAM: " << std::fixed 
              << std::setprecision(1) << available_ram_mb << " MB" << std::endl;
    
    // Step 2: Apply user budget constraint (default 500MB)
    double ram_budget = std::min(available_ram_mb * 0.8, limits.ram_budget_mb);
    std::cout << "[MetaCognition] RAM Budget: " << ram_budget << " MB" << std::endl;
    
    // Step 3: Start small - will grow dynamically based on demand
    estimate_ = calculate_initial_scale(limits_);
    
    if (!estimate_.feasible) {
        std::cerr << "[MetaCognition] ERROR: Cannot fit minimum network in available RAM!" << std::endl;
        // Fallback to minimal configuration
        estimate_.neurons = limits.min_neurons;
        estimate_.synapses = estimate_.neurons * limits.min_synapses_per_neuron;
        estimate_.memory_mb = calculate_memory_for_config(estimate_.neurons, estimate_.synapses);
        estimate_.feasible = true;
    }
    
    std::cout << "[MetaCognition] Initial Scale (will grow on demand):" << std::endl;
    std::cout << "  Neurons: " << estimate_.neurons << std::endl;
    std::cout << "  Synapses: " << estimate_.synapses 
              << " (" << (estimate_.synapses / (double)estimate_.neurons) << " per neuron)" << std::endl;
    std::cout << "  Estimated Memory: " << std::fixed << std::setprecision(2) 
              << estimate_.memory_mb << " MB" << std::endl;
    std::cout << "  Available headroom: " << (ram_budget - estimate_.memory_mb) << " MB" << std::endl;
    
    // Step 4: Initialize engine with initial scale
    initialize_engine(estimate_.neurons, estimate_.synapses, seed_);
    
    // Step 5: Initialize activity tracking
    activity_.neuron_active.resize(neuron_count_, false);
    activity_.neuron_spike_count.resize(neuron_count_, 0);
    activity_.synapse_usage.resize(synapse_count_, 0);
    activity_.synapse_active.resize(synapse_count_, false);
    activity_.active_neuron_count = 0;
    activity_.active_synapse_count = 0;
    activity_.inactive_neuron_count = neuron_count_;
    activity_.inactive_synapse_count = synapse_count_;
}

void MetaCognitiveEngine::init_fixed(size_t neurons, size_t synapses_per_neuron, uint64_t seed) {
    seed_ = seed;
    size_t synapses = neurons * synapses_per_neuron;
    
    estimate_.neurons = neurons;
    estimate_.synapses = synapses;
    estimate_.memory_mb = calculate_memory_for_config(neurons, synapses);
    estimate_.feasible = true;
    
    initialize_engine(neurons, synapses, seed);
}

void MetaCognitiveEngine::initialize_engine(size_t neurons, size_t synapses, uint64_t seed) {
    neuron_count_ = neurons;
    synapse_count_ = synapses;
    synapses_per_neuron_ = synapses / neurons;
    seed_ = seed;
    
    engine_ = std::make_unique<BioEngine>();
    engine_->init_structured(neurons, synapses, seed);
    
    // Create a default global layer
    engine_->create_layer("Global", neurons, LAYER_ID_CORTEX, PLASTICITY_SCALE_CORTEX);
    
    // Simple random connectivity for baseline
    auto& synapses_ref = const_cast<SynapseBlock&>(engine_->get_synapses());
    utils::FastRNG rng(seed);
    
    size_t synapse_cursor = 0;
    size_t current_synapses_per_neuron = synapses_per_neuron_;
    
    for (size_t pre_id = 0; pre_id < neurons; ++pre_id) {
        for (size_t c = 0; c < current_synapses_per_neuron && synapse_cursor < synapses; ++c) {
            uint32_t post_id = rng.next_int(0, static_cast<int>(neurons - 1));
            if (post_id == pre_id) post_id = (post_id + 1) % neurons;
            
            synapses_ref.pre_indices[synapse_cursor] = static_cast<uint32_t>(pre_id);
            synapses_ref.post_indices[synapse_cursor] = post_id;
            synapses_ref.weights[synapse_cursor] = rng.next_float() * 0.1f + 0.01f;
            synapses_ref.eligibility_traces[synapse_cursor] = 0.0f;
            synapses_ref.is_inhibitory[synapse_cursor] = (rng.next_float() < 0.2f);
            synapses_ref.delays[synapse_cursor] = 1;
            
            synapse_cursor++;
        }
    }
    
    engine_->set_synapse_cursor(synapse_cursor);
    engine_->bake_topology();
    
    std::cout << "[MetaCognition] Engine initialized with " 
              << neuron_count_ << " neurons, " << synapse_cursor << " synapses" << std::endl;
}

// -----------------------------------------------------------------------------
// Runtime: Real-time simulation with dynamic resource management
// -----------------------------------------------------------------------------

void MetaCognitiveEngine::tick() {
    if (!engine_) return;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    engine_->tick();
    
    auto end = std::chrono::high_resolution_clock::now();
    double tick_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Track performance
    tick_times_ms_.push_back(tick_ms);
    if (tick_times_ms_.size() > performance_window_) {
        tick_times_ms_.erase(tick_times_ms_.begin());
    }
    
    tick_counter_++;
    activity_.ticks_since_last_reset++;
    
    // Update performance metrics every N ticks
    if (tick_counter_ % 10 == 0) {
        update_performance_metrics();
    }
    
    // Analyze demand and reallocate resources periodically
    if (tick_counter_ % reallocation_cooldown_ == 0 && tick_counter_ > reallocation_cooldown_) {
        analyze_demand();
        if (demand_.needs_rebalance || force_strategy_) {
            reallocate_resources();
        }
    }
}

void MetaCognitiveEngine::update_performance_metrics() {
    if (tick_times_ms_.empty()) return;
    
    double avg_tick_ms = std::accumulate(tick_times_ms_.begin(), 
                                          tick_times_ms_.end(), 0.0) / tick_times_ms_.size();
    
    double target_tick_ms = 1.0;
    realtime_factor_ = target_tick_ms / avg_tick_ms;
}

// -----------------------------------------------------------------------------
// Resource Detection & Memory Calculation
// -----------------------------------------------------------------------------

double MetaCognitiveEngine::detect_available_ram_mb() const {
#ifdef __linux__
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    long available_kb = 0;
    
    while (std::getline(meminfo, line)) {
        if (line.find("MemAvailable:") == 0) {
            std::istringstream iss(line);
            std::string key;
            iss >> key >> available_kb;
            break;
        }
    }
    
    if (available_kb > 0) {
        return available_kb / 1024.0;
    }
#endif
    
    // Fallback: assume 512 MB if detection fails
    return 512.0;
}

MemoryEstimate MetaCognitiveEngine::calculate_initial_scale(const ResourceLimits& limits) {
    MemoryEstimate result;
    
    // Start small - minimum viable network
    // System will grow based on actual demand
    result.neurons = limits.min_neurons;
    result.synapses = result.neurons * limits.min_synapses_per_neuron;
    result.memory_mb = calculate_memory_for_config(result.neurons, result.synapses);
    result.feasible = (result.memory_mb <= limits.ram_budget_mb);
    
    return result;
}

double MetaCognitiveEngine::calculate_memory_for_config(size_t neurons, size_t synapses) const {
    return (neurons * BYTES_PER_NEURON + synapses * BYTES_PER_SYNAPSE) / 1048576.0 * OVERHEAD_FACTOR;
}

double MetaCognitiveEngine::get_memory_usage_mb() const {
    if (!engine_) return 0.0;
    return calculate_memory_for_config(neuron_count_, synapse_count_);
}

double MetaCognitiveEngine::get_available_memory_mb() const {
    double current_usage = get_memory_usage_mb();
    return std::max(0.0, limits_.ram_budget_mb - current_usage);
}

bool MetaCognitiveEngine::can_allocate_more_memory(double additional_mb) const {
    return get_available_memory_mb() >= additional_mb;
}

// -----------------------------------------------------------------------------
// Demand Analysis
// -----------------------------------------------------------------------------

void MetaCognitiveEngine::analyze_demand() {
    if (!engine_ || neuron_count_ == 0) return;
    
    // Update activity metrics from engine state
    update_activity_metrics();
    
    // Calculate utilization ratios
    demand_.neuron_utilization = (double)activity_.active_neuron_count / neuron_count_;
    demand_.synapse_utilization = (double)activity_.active_synapse_count / synapse_count_;
    
    // Assess demand levels
    demand_.neuron_demand = assess_demand_level(demand_.neuron_utilization, 
                                                 UTILIZATION_LOW, UTILIZATION_HIGH, UTILIZATION_CRITICAL);
    demand_.synapse_demand = assess_demand_level(demand_.synapse_utilization,
                                                  UTILIZATION_LOW, UTILIZATION_HIGH, UTILIZATION_CRITICAL);
    
    // Calculate imbalance ratio
    if (demand_.synapse_utilization > 0.01) {
        demand_.neuron_to_synapse_ratio = demand_.neuron_utilization / demand_.synapse_utilization;
    } else {
        demand_.neuron_to_synapse_ratio = 1.0;
    }
    
    // Determine allocation strategy based on demand patterns
    demand_.needs_rebalance = false;
    demand_.growth_pressure = 0.0;
    
    if (force_strategy_) {
        demand_.strategy = forced_strategy_;
        demand_.needs_rebalance = true;
    } else if (demand_.neuron_demand == DemandLevel::CRITICAL || 
               demand_.synapse_demand == DemandLevel::CRITICAL) {
        // Critical: need both urgently - use balanced growth
        demand_.strategy = AllocationStrategy::BALANCED;
        demand_.needs_rebalance = true;
        demand_.growth_pressure = 1.0;
    } else if (demand_.neuron_demand == DemandLevel::HIGH && demand_.synapse_demand == DemandLevel::LOW) {
        // Need more neurons than synapses
        if (demand_.neuron_to_synapse_ratio > (1.0 + IMBALANCE_THRESHOLD)) {
            demand_.strategy = AllocationStrategy::FAVOR_NEURONS;
            demand_.needs_rebalance = true;
        }
    } else if (demand_.synapse_demand == DemandLevel::HIGH && demand_.neuron_demand == DemandLevel::LOW) {
        // Need more synapses than neurons
        if (demand_.neuron_to_synapse_ratio < (1.0 - IMBALANCE_THRESHOLD)) {
            demand_.strategy = AllocationStrategy::FAVOR_SYNAPSES;
            demand_.needs_rebalance = true;
        }
    } else if (demand_.neuron_demand == DemandLevel::NONE && demand_.synapse_demand == DemandLevel::NONE) {
        // Both low - check if we should shrink
        if (demand_.neuron_utilization < UTILIZATION_LOW || demand_.synapse_utilization < UTILIZATION_LOW) {
            demand_.strategy = AllocationStrategy::SHRINK_INACTIVE;
            demand_.needs_rebalance = true;
        }
    } else if (demand_.neuron_demand >= DemandLevel::MODERATE || demand_.synapse_demand >= DemandLevel::MODERATE) {
        // Moderate demand - grow small
        demand_.strategy = AllocationStrategy::GROW_SMALL;
        demand_.needs_rebalance = true;
        demand_.growth_pressure = std::max(demand_.neuron_utilization, demand_.synapse_utilization) - UTILIZATION_LOW;
    }
    
    // Process pending requests
    if (!pending_neuron_requests_.empty()) {
        demand_.neurons_needed += pending_neuron_requests_.front().first;
        pending_neuron_requests_.pop();
        demand_.needs_rebalance = true;
    }
    if (!pending_synapse_requests_.empty()) {
        demand_.synapses_needed += pending_synapse_requests_.front().first;
        pending_synapse_requests_.pop();
        demand_.needs_rebalance = true;
    }
}

DemandLevel MetaCognitiveEngine::assess_demand_level(double utilization, double threshold_low, 
                                                      double threshold_high, double threshold_critical) {
    if (utilization >= threshold_critical) return DemandLevel::CRITICAL;
    if (utilization >= threshold_high) return DemandLevel::HIGH;
    if (utilization >= threshold_low) return DemandLevel::MODERATE;
    if (utilization > 0.0) return DemandLevel::LOW;
    return DemandLevel::NONE;
}

// -----------------------------------------------------------------------------
// Resource Reallocation Core
// -----------------------------------------------------------------------------

ReallocationResult MetaCognitiveEngine::reallocate_resources() {
    ReallocationResult result;
    result.neurons_before = neuron_count_;
    result.synapses_before = synapse_count_;
    result.memory_before = get_memory_usage_mb();
    
    if (tick_counter_ - last_reallocation_tick_ < reallocation_cooldown_) {
        result.action_taken = "Cooldown active - skipping reallocation";
        return result;
    }
    
    last_reallocation_tick_ = tick_counter_;
    
    switch (demand_.strategy) {
        case AllocationStrategy::GROW_SMALL:
            result = execute_grow_small();
            break;
        case AllocationStrategy::FAVOR_SYNAPSES:
            result = execute_favor_synapses();
            break;
        case AllocationStrategy::FAVOR_NEURONS:
            result = execute_favor_neurons();
            break;
        case AllocationStrategy::BALANCED:
            result = execute_balanced();
            break;
        case AllocationStrategy::SHRINK_INACTIVE:
            result = execute_shrink_inactive();
            break;
    }
    
    result.neurons_after = neuron_count_;
    result.synapses_after = synapse_count_;
    result.memory_after = get_memory_usage_mb();
    
    if (result.success) {
        std::cout << "[MetaCognition] Reallocated: " << result.action_taken << std::endl;
        std::cout << "  Neurons: " << result.neurons_before << " -> " << result.neurons_after << std::endl;
        std::cout << "  Synapses: " << result.synapses_before << " -> " << result.synapses_after << std::endl;
        std::cout << "  Memory: " << std::fixed << std::setprecision(2) 
                  << result.memory_before << " MB -> " << result.memory_after << " MB" << std::endl;
    }
    
    return result;
}

ReallocationResult MetaCognitiveEngine::execute_grow_small() {
    ReallocationResult result;
    
    // Small incremental growth
    size_t new_neurons = neuron_count_ + growth_step_neurons_;
    size_t new_synapses_per_neuron = synapses_per_neuron_;
    
    // Check memory budget
    size_t total_synapses = new_neurons * new_synapses_per_neuron;
    double required_memory = calculate_memory_for_config(new_neurons, total_synapses);
    
    if (required_memory > limits_.ram_budget_mb) {
        // Try growing synapses only first
        new_neurons = neuron_count_;
        new_synapses_per_neuron = synapses_per_neuron_ + growth_step_synapses_;
        total_synapses = new_neurons * new_synapses_per_neuron;
        required_memory = calculate_memory_for_config(new_neurons, total_synapses);
        
        if (required_memory > limits_.ram_budget_mb) {
            result.action_taken = "Cannot grow - at memory limit";
            return result;
        }
    }
    
    // Cap at maximum configured values
    new_neurons = std::min(new_neurons, limits_.max_neurons_config);
    new_synapses_per_neuron = std::min(new_synapses_per_neuron, limits_.max_synapses_per_neuron);
    
    return reconfigure_network(new_neurons, new_synapses_per_neuron);
}

ReallocationResult MetaCognitiveEngine::execute_favor_synapses() {
    ReallocationResult result;
    
    // Deactivate some inactive neurons to free memory for synapses
    size_t neurons_to_deactivate = std::min(activity_.inactive_neuron_count, 
                                            std::max((size_t)1, activity_.inactive_neuron_count / 4));
    
    if (neurons_to_deactivate == 0) {
        // No inactive neurons - try small growth in synapses only
        size_t new_synapses_per_neuron = synapses_per_neuron_ + growth_step_synapses_ * 2;
        new_synapses_per_neuron = std::min(new_synapses_per_neuron, limits_.max_synapses_per_neuron);
        
        size_t total_synapses = neuron_count_ * new_synapses_per_neuron;
        double required_memory = calculate_memory_for_config(neuron_count_, total_synapses);
        
        if (required_memory > limits_.ram_budget_mb) {
            result.action_taken = "Cannot add synapses - at memory limit";
            return result;
        }
        
        return reconfigure_network(neuron_count_, new_synapses_per_neuron);
    }
    
    // Calculate new config: fewer neurons, more synapses per neuron
    size_t new_neurons = neuron_count_ - neurons_to_deactivate;
    size_t freed_memory = neurons_to_deactivate * BYTES_PER_NEURON / 1048576.0 * OVERHEAD_FACTOR;
    size_t additional_synapses = (size_t)(freed_memory * 1048576.0 / OVERHEAD_FACTOR / BYTES_PER_SYNAPSE);
    size_t new_total_synapses = synapse_count_ + additional_synapses;
    size_t new_synapses_per_neuron = new_total_synapses / new_neurons;
    new_synapses_per_neuron = std::min(new_synapses_per_neuron, limits_.max_synapses_per_neuron);
    
    return reconfigure_network(new_neurons, new_synapses_per_neuron);
}

ReallocationResult MetaCognitiveEngine::execute_favor_neurons() {
    ReallocationResult result;
    
    // Prune unused synapses to free memory for neurons
    size_t synapses_to_prune = std::min(activity_.inactive_synapse_count,
                                        std::max((size_t)1, activity_.inactive_synapse_count / 4));
    
    if (synapses_to_prune == 0) {
        // Try reducing synapses per neuron to add neurons
        if (synapses_per_neuron_ > limits_.min_synapses_per_neuron) {
            size_t new_synapses_per_neuron = synapses_per_neuron_ - growth_step_synapses_;
            new_synapses_per_neuron = std::max(new_synapses_per_neuron, limits_.min_synapses_per_neuron);
            
            size_t new_neurons = neuron_count_ + growth_step_neurons_ * 2;
            size_t total_synapses = new_neurons * new_synapses_per_neuron;
            double required_memory = calculate_memory_for_config(new_neurons, total_synapses);
            
            if (required_memory > limits_.ram_budget_mb) {
                result.action_taken = "Cannot add neurons - at memory limit";
                return result;
            }
            
            return reconfigure_network(new_neurons, new_synapses_per_neuron);
        }
        
        result.action_taken = "Cannot prune synapses - all in use";
        return result;
    }
    
    // Calculate new config: more neurons, fewer synapses per neuron
    size_t freed_memory = synapses_to_prune * BYTES_PER_SYNAPSE / 1048576.0 * OVERHEAD_FACTOR;
    size_t additional_neurons = (size_t)(freed_memory * 1048576.0 / OVERHEAD_FACTOR / BYTES_PER_NEURON);
    size_t new_neurons = neuron_count_ + additional_neurons;
    new_neurons = std::min(new_neurons, limits_.max_neurons_config);
    
    size_t new_total_synapses = synapse_count_ - synapses_to_prune;
    size_t new_synapses_per_neuron = new_total_synapses / new_neurons;
    new_synapses_per_neuron = std::max(new_synapses_per_neuron, limits_.min_synapses_per_neuron);
    
    return reconfigure_network(new_neurons, new_synapses_per_neuron);
}

ReallocationResult MetaCognitiveEngine::execute_balanced() {
    ReallocationResult result;
    
    // Balanced growth of both neurons and synapses
    size_t new_neurons = neuron_count_ + growth_step_neurons_;
    size_t new_synapses_per_neuron = synapses_per_neuron_ + growth_step_synapses_;
    
    new_neurons = std::min(new_neurons, limits_.max_neurons_config);
    new_synapses_per_neuron = std::min(new_synapses_per_neuron, limits_.max_synapses_per_neuron);
    
    size_t total_synapses = new_neurons * new_synapses_per_neuron;
    double required_memory = calculate_memory_for_config(new_neurons, total_synapses);
    
    if (required_memory > limits_.ram_budget_mb) {
        // Scale back proportionally to fit budget
        double scale_factor = limits_.ram_budget_mb / required_memory;
        new_neurons = std::max((size_t)(new_neurons * scale_factor), limits_.min_neurons);
        new_synapses_per_neuron = std::max((size_t)(new_synapses_per_neuron * scale_factor), 
                                           limits_.min_synapses_per_neuron);
        total_synapses = new_neurons * new_synapses_per_neuron;
        required_memory = calculate_memory_for_config(new_neurons, total_synapses);
        
        if (required_memory > limits_.ram_budget_mb) {
            result.action_taken = "Cannot grow balanced - at memory limit";
            return result;
        }
    }
    
    return reconfigure_network(new_neurons, new_synapses_per_neuron);
}

ReallocationResult MetaCognitiveEngine::execute_shrink_inactive() {
    ReallocationResult result;
    
    // Shrink to match actual usage
    size_t target_neurons = std::max(activity_.active_neuron_count, limits_.min_neurons);
    size_t target_synapses = std::max(activity_.active_synapse_count, 
                                      target_neurons * limits_.min_synapses_per_neuron);
    
    if (target_neurons >= neuron_count_ && target_synapses >= synapse_count_) {
        result.action_taken = "Nothing to shrink";
        return result;
    }
    
    // Don't shrink too aggressively
    target_neurons = std::max(target_neurons, neuron_count_ * 3 / 4);
    target_synapses = std::max(target_synapses, synapse_count_ * 3 / 4);
    
    size_t new_synapses_per_neuron = target_synapses / target_neurons;
    new_synapses_per_neuron = std::max(new_synapses_per_neuron, limits_.min_synapses_per_neuron);
    
    return reconfigure_network(target_neurons, new_synapses_per_neuron);
}

ReallocationResult MetaCognitiveEngine::reconfigure_network(size_t new_neurons, size_t new_synapses_per_neuron) {
    ReallocationResult result;
    
    if (new_neurons == neuron_count_ && new_synapses_per_neuron == synapses_per_neuron_) {
        result.action_taken = "No change needed";
        result.success = true;
        return result;
    }
    
    size_t new_total_synapses = new_neurons * new_synapses_per_neuron;
    double new_memory = calculate_memory_for_config(new_neurons, new_total_synapses);
    
    if (new_memory > limits_.ram_budget_mb) {
        result.action_taken = "New config exceeds memory budget";
        return result;
    }
    
    // Save important state before reconfiguration
    // (In a full implementation, we'd migrate active neurons/synapses)
    
    // Reinitialize with new configuration
    neuron_count_ = new_neurons;
    synapse_count_ = new_total_synapses;
    synapses_per_neuron_ = new_synapses_per_neuron;
    
    // Recreate engine with new config
    engine_ = std::make_unique<BioEngine>();
    engine_->init_structured(neuron_count_, synapse_count_, seed_);
    engine_->create_layer("Global", neuron_count_, LAYER_ID_CORTEX, PLASTICITY_SCALE_CORTEX);
    
    // Initialize connectivity
    auto& synapses_ref = const_cast<SynapseBlock&>(engine_->get_synapses());
    utils::FastRNG rng(seed_ + tick_counter_); // Different seed for variety
    
    size_t synapse_cursor = 0;
    for (size_t pre_id = 0; pre_id < neuron_count_; ++pre_id) {
        for (size_t c = 0; c < synapses_per_neuron_ && synapse_cursor < synapse_count_; ++c) {
            uint32_t post_id = rng.next_int(0, static_cast<int>(neuron_count_ - 1));
            if (post_id == pre_id) post_id = (post_id + 1) % neuron_count_;
            
            synapses_ref.pre_indices[synapse_cursor] = static_cast<uint32_t>(pre_id);
            synapses_ref.post_indices[synapse_cursor] = post_id;
            synapses_ref.weights[synapse_cursor] = rng.next_float() * 0.1f + 0.01f;
            synapses_ref.eligibility_traces[synapse_cursor] = 0.0f;
            synapses_ref.is_inhibitory[synapse_cursor] = (rng.next_float() < 0.2f);
            synapses_ref.delays[synapse_cursor] = 1;
            
            synapse_cursor++;
        }
    }
    
    engine_->set_synapse_cursor(synapse_cursor);
    engine_->bake_topology();
    
    // Resize activity tracking
    activity_.neuron_active.resize(neuron_count_, false);
    activity_.neuron_spike_count.resize(neuron_count_, 0);
    activity_.synapse_usage.resize(synapse_count_, 0);
    activity_.synapse_active.resize(synapse_count_, false);
    
    result.success = true;
    result.action_taken = "Network reconfigured successfully";
    return result;
}

// -----------------------------------------------------------------------------
// Activity Tracking & Management
// -----------------------------------------------------------------------------

void MetaCognitiveEngine::update_activity_metrics() {
    if (!engine_) return;
    
    const auto& neurons = engine_->get_neurons();
    
    activity_.active_neuron_count = 0;
    activity_.inactive_neuron_count = 0;
    activity_.active_synapse_count = 0;
    activity_.inactive_synapse_count = 0;
    
    // Count active neurons (those that have spiked recently)
    for (size_t i = 0; i < neuron_count_ && i < activity_.neuron_spike_count.size(); ++i) {
        if (activity_.neuron_spike_count[i] > 0) {
            activity_.neuron_active[i] = true;
            activity_.active_neuron_count++;
        } else {
            activity_.neuron_active[i] = false;
            activity_.inactive_neuron_count++;
        }
    }
    
    // Count active synapses (those that have transmitted recently)
    for (size_t i = 0; i < synapse_count_ && i < activity_.synapse_usage.size(); ++i) {
        if (activity_.synapse_usage[i] > 0) {
            activity_.synapse_active[i] = true;
            activity_.active_synapse_count++;
        } else {
            activity_.synapse_active[i] = false;
            activity_.inactive_synapse_count++;
        }
    }
    
    // Decay spike counts periodically
    if (activity_.ticks_since_last_reset >= activity_.monitoring_window_ticks) {
        std::fill(activity_.neuron_spike_count.begin(), activity_.neuron_spike_count.end(), 0);
        std::fill(activity_.synapse_usage.begin(), activity_.synapse_usage.end(), 0);
        activity_.ticks_since_last_reset = 0;
    }
}

void MetaCognitiveEngine::mark_neuron_active(size_t neuron_id) {
    if (neuron_id < activity_.neuron_active.size()) {
        if (!activity_.neuron_active[neuron_id]) {
            activity_.neuron_active[neuron_id] = true;
            activity_.active_neuron_count++;
            if (activity_.inactive_neuron_count > 0) activity_.inactive_neuron_count--;
        }
        activity_.neuron_spike_count[neuron_id]++;
    }
}

void MetaCognitiveEngine::mark_neuron_inactive(size_t neuron_id) {
    if (neuron_id < activity_.neuron_active.size()) {
        if (activity_.neuron_active[neuron_id]) {
            activity_.neuron_active[neuron_id] = false;
            activity_.active_neuron_count--;
            activity_.inactive_neuron_count++;
        }
    }
}

std::vector<size_t> MetaCognitiveEngine::find_inactive_neurons(size_t count) const {
    std::vector<size_t> inactive;
    for (size_t i = 0; i < neuron_count_ && inactive.size() < count; ++i) {
        if (i < activity_.neuron_active.size() && !activity_.neuron_active[i]) {
            inactive.push_back(i);
        }
    }
    return inactive;
}

std::vector<size_t> MetaCognitiveEngine::find_unused_synapses(size_t count) const {
    std::vector<size_t> unused;
    for (size_t i = 0; i < synapse_count_ && unused.size() < count; ++i) {
        if (i < activity_.synapse_active.size() && !activity_.synapse_active[i]) {
            unused.push_back(i);
        }
    }
    return unused;
}

void MetaCognitiveEngine::activate_neurons(size_t count) {
    auto inactive = find_inactive_neurons(count);
    for (size_t id : inactive) {
        mark_neuron_active(id);
    }
}

void MetaCognitiveEngine::deactivate_neurons(size_t count) {
    // Find least active neurons
    std::vector<std::pair<size_t, size_t>> activity_with_id;
    for (size_t i = 0; i < neuron_count_ && i < activity_.neuron_spike_count.size(); ++i) {
        if (activity_.neuron_active[i]) {
            activity_with_id.push_back({activity_.neuron_spike_count[i], i});
        }
    }
    
    std::sort(activity_with_id.begin(), activity_with_id.end());
    
    size_t to_deactivate = std::min(count, activity_with_id.size());
    for (size_t i = 0; i < to_deactivate; ++i) {
        mark_neuron_inactive(activity_with_id[i].second);
    }
}

void MetaCognitiveEngine::deactivate_specific_neurons(const std::vector<size_t>& neuron_ids) {
    for (size_t id : neuron_ids) {
        mark_neuron_inactive(id);
    }
}

void MetaCognitiveEngine::initialize_synapses_on_demand(size_t neuron_id, size_t count) {
    // Mark synapses as active for this neuron
    size_t start_idx = neuron_id * synapses_per_neuron_;
    size_t end_idx = std::min(start_idx + count, synapse_count_);
    
    for (size_t i = start_idx; i < end_idx; ++i) {
        if (i < activity_.synapse_active.size()) {
            activity_.synapse_active[i] = true;
            activity_.active_synapse_count++;
            if (activity_.inactive_synapse_count > 0) activity_.inactive_synapse_count--;
        }
    }
}

void MetaCognitiveEngine::prune_unused_synapses() {
    auto unused = find_unused_synapses(activity_.inactive_synapse_count / 2);
    // In a full implementation, would actually remove these synapses
    // For now, just mark them for potential removal
    (void)unused; // Suppress unused warning
}

void MetaCognitiveEngine::prune_synapses_for_neurons(size_t synapses_to_remove) {
    // Reduce synapses per neuron
    if (synapses_per_neuron_ > limits_.min_synapses_per_neuron) {
        size_t reduction = std::min(synapses_to_remove / neuron_count_, 
                                    synapses_per_neuron_ - limits_.min_synapses_per_neuron);
        synapses_per_neuron_ -= reduction;
        synapse_count_ = neuron_count_ * synapses_per_neuron_;
    }
}

// -----------------------------------------------------------------------------
// External Request Handling
// -----------------------------------------------------------------------------

void MetaCognitiveEngine::request_more_neurons(size_t needed, const std::string& reason) {
    std::cout << "[MetaCognition] Request: " << needed << " neurons (" << reason << ")" << std::endl;
    pending_neuron_requests_.push({needed, reason});
}

void MetaCognitiveEngine::request_more_synapses(size_t needed_per_neuron, const std::string& reason) {
    std::cout << "[MetaCognition] Request: " << needed_per_neuron << " synapses/neuron (" << reason << ")" << std::endl;
    pending_synapse_requests_.push({needed_per_neuron, reason});
}

void MetaCognitiveEngine::release_unused_resources() {
    demand_.strategy = AllocationStrategy::SHRINK_INACTIVE;
    demand_.needs_rebalance = true;
}

void MetaCognitiveEngine::force_allocation_strategy(AllocationStrategy strategy) {
    force_strategy_ = true;
    forced_strategy_ = strategy;
    demand_.needs_rebalance = true;
}

// -----------------------------------------------------------------------------
// Testing Utilities
// -----------------------------------------------------------------------------

void MetaCognitiveEngine::simulate_neuron_activity(const std::vector<size_t>& active_ids) {
    std::fill(activity_.neuron_active.begin(), activity_.neuron_active.end(), false);
    std::fill(activity_.neuron_spike_count.begin(), activity_.neuron_spike_count.end(), 0);
    
    for (size_t id : active_ids) {
        if (id < neuron_count_) {
            activity_.neuron_active[id] = true;
            activity_.neuron_spike_count[id] = 10; // Simulate recent activity
        }
    }
    
    activity_.active_neuron_count = active_ids.size();
    activity_.inactive_neuron_count = neuron_count_ - activity_.active_neuron_count;
}

void MetaCognitiveEngine::simulate_synapse_usage(const std::vector<size_t>& active_ids) {
    std::fill(activity_.synapse_active.begin(), activity_.synapse_active.end(), false);
    std::fill(activity_.synapse_usage.begin(), activity_.synapse_usage.end(), 0);
    
    for (size_t id : active_ids) {
        if (id < synapse_count_) {
            activity_.synapse_active[id] = true;
            activity_.synapse_usage[id] = 10;
        }
    }
    
    activity_.active_synapse_count = active_ids.size();
    activity_.inactive_synapse_count = synapse_count_ - activity_.active_synapse_count;
}

void MetaCognitiveEngine::reset_activity_metrics() {
    std::fill(activity_.neuron_active.begin(), activity_.neuron_active.end(), false);
    std::fill(activity_.neuron_spike_count.begin(), activity_.neuron_spike_count.end(), 0);
    std::fill(activity_.synapse_usage.begin(), activity_.synapse_usage.end(), 0);
    std::fill(activity_.synapse_active.begin(), activity_.synapse_active.end(), false);
    activity_.active_neuron_count = 0;
    activity_.active_synapse_count = 0;
    activity_.inactive_neuron_count = neuron_count_;
    activity_.inactive_synapse_count = synapse_count_;
    activity_.ticks_since_last_reset = 0;
}

// -----------------------------------------------------------------------------
// Utility & Diagnostic Functions
// -----------------------------------------------------------------------------

double MetaCognitiveEngine::get_neuron_utilization() const {
    if (neuron_count_ == 0) return 0.0;
    return (double)activity_.active_neuron_count / neuron_count_;
}

double MetaCognitiveEngine::get_synapse_utilization() const {
    if (synapse_count_ == 0) return 0.0;
    return (double)activity_.active_synapse_count / synapse_count_;
}

void MetaCognitiveEngine::print_status() const {
    std::cout << "\n=== Meta-Cognitive Engine Status ===" << std::endl;
    std::cout << "RAM Budget: " << std::fixed << std::setprecision(1) 
              << limits_.ram_budget_mb << " MB" << std::endl;
    std::cout << "Current Config:" << std::endl;
    std::cout << "  Neurons: " << neuron_count_ 
              << " (max: " << limits_.max_neurons_config << ")" << std::endl;
    std::cout << "  Synapses: " << synapse_count_ 
              << " (" << synapses_per_neuron_ << " per neuron)" << std::endl;
    std::cout << "  Memory: " << get_memory_usage_mb() << " MB" << std::endl;
    std::cout << "  Available: " << get_available_memory_mb() << " MB" << std::endl;
    std::cout << "Performance:" << std::endl;
    std::cout << "  Realtime Factor: " << std::setprecision(2) << realtime_factor_ << "x" << std::endl;
    std::cout << "=====================================\n" << std::endl;
}

void MetaCognitiveEngine::print_detailed_metrics() const {
    std::cout << "\n=== Detailed Meta-Cognitive Metrics ===" << std::endl;
    std::cout << "Activity:" << std::endl;
    std::cout << "  Active Neurons: " << activity_.active_neuron_count 
              << " / " << neuron_count_ 
              << " (" << std::setprecision(1) << get_neuron_utilization() * 100 << "%)" << std::endl;
    std::cout << "  Active Synapses: " << activity_.active_synapse_count 
              << " / " << synapse_count_ 
              << " (" << std::setprecision(1) << get_synapse_utilization() * 100 << "%)" << std::endl;
    std::cout << "Demand:" << std::endl;
    std::cout << "  Neuron Demand: ";
    switch (demand_.neuron_demand) {
        case DemandLevel::NONE: std::cout << "NONE"; break;
        case DemandLevel::LOW: std::cout << "LOW"; break;
        case DemandLevel::MODERATE: std::cout << "MODERATE"; break;
        case DemandLevel::HIGH: std::cout << "HIGH"; break;
        case DemandLevel::CRITICAL: std::cout << "CRITICAL"; break;
    }
    std::cout << std::endl;
    std::cout << "  Synapse Demand: ";
    switch (demand_.synapse_demand) {
        case DemandLevel::NONE: std::cout << "NONE"; break;
        case DemandLevel::LOW: std::cout << "LOW"; break;
        case DemandLevel::MODERATE: std::cout << "MODERATE"; break;
        case DemandLevel::HIGH: std::cout << "HIGH"; break;
        case DemandLevel::CRITICAL: std::cout << "CRITICAL"; break;
    }
    std::cout << std::endl;
    std::cout << "  Strategy: ";
    switch (demand_.strategy) {
        case AllocationStrategy::GROW_SMALL: std::cout << "GROW_SMALL"; break;
        case AllocationStrategy::FAVOR_SYNAPSES: std::cout << "FAVOR_SYNAPSES"; break;
        case AllocationStrategy::FAVOR_NEURONS: std::cout << "FAVOR_NEURONS"; break;
        case AllocationStrategy::BALANCED: std::cout << "BALANCED"; break;
        case AllocationStrategy::SHRINK_INACTIVE: std::cout << "SHRINK_INACTIVE"; break;
    }
    std::cout << std::endl;
    std::cout << "=====================================\n" << std::endl;
}

} // namespace genesis
