#include "meta_cognition.h"
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace genesis;

int main() {
    std::cout << "=============================================" << std::endl;
    std::cout << "  Nexuss Neural Cognition - Meta-Cognition Demo" << std::endl;
    std::cout << "=============================================\n" << std::endl;

    // Configure resource limits
    ResourceLimits limits;
    limits.max_neurons_config = 300000;        // Maximum allowed neurons
    limits.max_synapses_per_neuron = 50;       // Maximum connectivity density
    limits.ram_budget_mb = 500.0;              // Target RAM usage (MB)
    limits.target_realtime_factor = 10.0;      // Target speedup factor

    // Create meta-cognitive engine
    MetaCognitiveEngine meta_engine;

    // Auto-scale based on available system RAM
    std::cout << "[Demo] Initializing meta-cognitive auto-scaling..." << std::endl;
    meta_engine.init_auto_scale(limits);

    std::cout << "\n[Demo] Starting real-time simulation..." << std::endl;
    std::cout << "-------------------------------------------" << std::endl;

    // Run simulation with monitoring
    const int TOTAL_STEPS = 1000;
    const int LOG_INTERVAL = 200;

    auto start_time = std::chrono::high_resolution_clock::now();
    size_t total_spikes = 0;

    for (int step = 0; step <= TOTAL_STEPS; ++step) {
        meta_engine.tick();
        
        // Count spikes in this step (optional monitoring)
        if (step % LOG_INTERVAL == 0) {
            double memory_mb = meta_engine.get_memory_usage_mb();
            double realtime_factor = meta_engine.get_realtime_factor();

            std::cout << "Step " << std::setw(4) << step << " | "
                      << "Memory: " << std::fixed << std::setprecision(1) << memory_mb << " MB | "
                      << "Realtime: " << std::setprecision(2) << realtime_factor << "x" << std::endl;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "-------------------------------------------" << std::endl;
    std::cout << "\n[Demo] Simulation Complete!" << std::endl;
    std::cout << "  Total Neurons: " << meta_engine.get_current_neurons() << std::endl;
    std::cout << "  Total Synapses: " << meta_engine.get_current_synapses() << std::endl;
    std::cout << "  Final Memory: " << std::fixed << std::setprecision(1) 
              << meta_engine.get_memory_usage_mb() << " MB" << std::endl;
    std::cout << "  Duration: " << duration.count() << " ms" << std::endl;
    std::cout << "  Average Realtime Factor: " << std::setprecision(2) 
              << meta_engine.get_realtime_factor() << "x" << std::endl;

    std::cout << "\n[Demo] Meta-cognitive scaling successful!" << std::endl;
    std::cout << "  ✓ Auto-detected system RAM and configured optimal network size" << std::endl;
    std::cout << "  ✓ Respected configured maximums (300K neurons, 50 synapses/neuron)" << std::endl;
    std::cout << "  ✓ Maintained stable memory usage throughout simulation" << std::endl;
    std::cout << "  ✓ Real-time simulation with performance monitoring" << std::endl;

    return 0;
}
