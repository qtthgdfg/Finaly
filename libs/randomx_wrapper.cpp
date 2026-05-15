// FILE: MoneroRandomXMiner/libs/randomx_wrapper.cpp

#include "randomx_wrapper.h"
#include <string>
#include <cstring>
#include <stdexcept>
#include <algorithm>

#ifdef ANDROID
#include <android/log.h>
#define LOG_TAG "RandomXWrapper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) 
#define LOGE(...) 
#endif

RandomXWrapper::RandomXWrapper() 
    : m_cache(nullptr)
    , m_dataset(nullptr) {
    LOGI("RandomXWrapper created");
}

RandomXWrapper::~RandomXWrapper() {
    destroy();
    LOGI("RandomXWrapper destroyed");
}

RandomXWrapper::RandomXWrapper(RandomXWrapper&& other) noexcept
    : m_cache(other.m_cache)
    , m_dataset(other.m_dataset)
    , m_vms(std::move(other.m_vms)) {
    other.m_cache = nullptr;
    other.m_dataset = nullptr;
    LOGI("RandomXWrapper moved (constructor)");
}

RandomXWrapper& RandomXWrapper::operator=(RandomXWrapper&& other) noexcept {
    if (this != &other) {
        destroy();
        m_cache = other.m_cache;
        m_dataset = other.m_dataset;
        m_vms = std::move(other.m_vms);
        other.m_cache = nullptr;
        other.m_dataset = nullptr;
    }
    LOGI("RandomXWrapper moved (assignment)");
    return *this;
}

bool RandomXWrapper::initialize(const std::vector<uint8_t>& seed, 
                               uint32_t num_threads,
                               bool huge_pages,
                               bool numa_aware) {
    destroy();
    
    LOGI("Initializing RandomX with %u threads", num_threads);
    
    randomx_flags flags = RANDOMX_FLAG_DEFAULT;
    
#ifdef ANDROID
    huge_pages = false;
    numa_aware = false;
    LOGI("Android detected - huge pages and NUMA disabled");
#else
    if (huge_pages) {
        flags = static_cast<randomx_flags>(flags | RANDOMX_FLAG_HUGE_PAGES);
    }
    if (numa_aware) {
        flags = static_cast<randomx_flags>(flags | RANDOMX_FLAG_NUMA);
    }
#endif
    
#ifdef __aarch64__
    flags = static_cast<randomx_flags>(flags | RANDOMX_FLAG_HARD_AES);
    LOGI("ARM64 hardware AES enabled");
#endif
    
    m_cache = randomx_alloc_cache(flags);
    if (!m_cache) {
        LOGE("Failed to allocate RandomX cache");
        throw std::runtime_error("Failed to allocate RandomX cache");
    }
    LOGI("RandomX cache allocated");
    
    randomx_init_cache(m_cache, seed.data(), seed.size());
    LOGI("RandomX cache initialized with seed");
    
    m_dataset = randomx_alloc_dataset(flags);
    if (!m_dataset) {
        LOGE("Failed to allocate RandomX dataset");
        randomx_release_cache(m_cache);
        m_cache = nullptr;
        throw std::runtime_error("Failed to allocate RandomX dataset");
    }
    LOGI("RandomX dataset allocated");
    
    uint32_t num_items = randomx_dataset_item_count();
    LOGI("Initializing dataset with %u items", num_items);
    
    // FIXED: only 4 arguments
    randomx_init_dataset(m_dataset, m_cache, 0, num_items);
    LOGI("RandomX dataset initialized");
    
    m_vms.reserve(num_threads);
    
    randomx_flags vm_flags = static_cast<randomx_flags>(flags | RANDOMX_FLAG_JIT);
    
    for (uint32_t i = 0; i < num_threads; ++i) {
        randomx_vm* vm = randomx_create_vm(vm_flags, m_cache, m_dataset);
        
        if (!vm) {
            if (vm_flags & RANDOMX_FLAG_JIT) {
                LOGI("JIT failed for thread %u, trying interpreted", i);
                vm_flags = static_cast<randomx_flags>(vm_flags & ~RANDOMX_FLAG_JIT);
                vm = randomx_create_vm(vm_flags, m_cache, m_dataset);
            }
            
            if (!vm) {
                LOGE("Failed to create VM for thread %u", i);
                throw std::runtime_error("Failed to create RandomX VM for thread " + std::to_string(i));
            }
        }
        
        m_vms.push_back(vm);
        LOGI("RandomX VM created for thread %u", i);
    }
    
    LOGI("RandomX initialization complete with %zu VMs", m_vms.size());
    return true;
}

void RandomXWrapper::hash(uint32_t thread_id, 
                         const void* input, 
                         size_t input_size, 
                         void* output) {
    if (thread_id >= m_vms.size()) {
        throw std::out_of_range("Invalid thread ID: " + std::to_string(thread_id) + 
            " (max: " + std::to_string(m_vms.size() - 1) + ")");
    }
    
    if (!input || !output) {
        throw std::invalid_argument("Input or output buffer is null");
    }
    
    randomx_vm* vm = m_vms[thread_id];
    if (!vm) {
        throw std::runtime_error("VM for thread " + std::to_string(thread_id) + " is null");
    }
    
    randomx_calculate_hash(vm, input, input_size, output);
}

void RandomXWrapper::hash_first(const void* input, size_t input_size, void* output) {
    if (m_vms.empty()) throw std::runtime_error("No VMs available");
    hash(0, input, input_size, output);
}

size_t RandomXWrapper::num_vms() const { return m_vms.size(); }

bool RandomXWrapper::is_initialized() const {
    return m_cache != nullptr && m_dataset != nullptr && !m_vms.empty();
}

void RandomXWrapper::destroy() {
    LOGI("Destroying RandomX resources...");
    for (size_t i = 0; i < m_vms.size(); ++i) {
        if (m_vms[i]) randomx_destroy_vm(m_vms[i]);
    }
    m_vms.clear();
    if (m_dataset) { randomx_release_dataset(m_dataset); m_dataset = nullptr; }
    if (m_cache) { randomx_release_cache(m_cache); m_cache = nullptr; }
    LOGI("RandomX resources destroyed");
}

#ifdef ANDROID
bool RandomXWrapper::initialize_android(const std::vector<uint8_t>& seed,
                                        uint32_t num_threads,
                                        int battery_level,
                                        float temperature) {
    uint32_t t = num_threads;
    if (battery_level < 20) t = std::min(t, 1u);
    else if (battery_level < 40) t = std::min(t, 2u);
    if (temperature > 55.0f) return false;
    else if (temperature > 45.0f) t = std::min(t, 1u);
    else if (temperature > 40.0f) t = std::min(t, 2u);
    return initialize(seed, t, false, false);
}
#endif
