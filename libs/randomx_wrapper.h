// FILE: MoneroRandomXMiner/libs/randomx_wrapper.h

#pragma once

#include "randomx.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <stdexcept>

class RandomXWrapper {
public:
    /**
     * Constructor - creates an empty wrapper
     */
    RandomXWrapper();
    
    /**
     * Destructor - cleans up all RandomX resources
     */
    ~RandomXWrapper();
    
    // Non-copyable
    RandomXWrapper(const RandomXWrapper&) = delete;
    RandomXWrapper& operator=(const RandomXWrapper&) = delete;
    
    // Movable
    RandomXWrapper(RandomXWrapper&& other) noexcept;
    RandomXWrapper& operator=(RandomXWrapper&& other) noexcept;
    
    /**
     * Initialize RandomX with the given seed
     * @param seed - The seed hash from the mining pool
     * @param num_threads - Number of mining threads
     * @param huge_pages - Enable huge pages (desktop only)
     * @param numa_aware - Enable NUMA awareness
     * @return true if initialization successful
     * @throws std::runtime_error on failure
     */
    bool initialize(const std::vector<uint8_t>& seed, 
                   uint32_t num_threads = 4,
                   bool huge_pages = true,
                   bool numa_aware = false);
    
#ifdef ANDROID
    /**
     * Android-specific initialization with battery and thermal awareness
     */
    bool initialize_android(const std::vector<uint8_t>& seed,
                           uint32_t num_threads,
                           int battery_level = 100,
                           float temperature = 35.0f);
#endif
    
    /**
     * Calculate a RandomX hash using a specific VM thread
     * @param thread_id - The VM thread to use
     * @param input - Input data (mining blob)
     * @param input_size - Size of input data
     * @param output - Output buffer (must be RANDOMX_HASH_SIZE bytes)
     * @throws std::out_of_range if thread_id is invalid
     * @throws std::invalid_argument if input/output is null
     */
    void hash(uint32_t thread_id, 
             const void* input, 
             size_t input_size, 
             void* output);
    
    /**
     * Calculate hash using the first VM (convenience method)
     */
    void hash_first(const void* input, 
                   size_t input_size, 
                   void* output);
    
    /**
     * Get the number of available VMs
     */
    size_t num_vms() const;
    
    /**
     * Check if the wrapper is initialized
     */
    bool is_initialized() const;
    
private:
    /**
     * Destroy all RandomX resources
     */
    void destroy();
    
    randomx_cache* m_cache{nullptr};
    randomx_dataset* m_dataset{nullptr};
    std::vector<randomx_vm*> m_vms;
};
