#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <mutex>
#include <random>
#include "randomx.h"
#include "stratum_client.h"

struct MinerConfig {
    std::string pool_host;
    uint16_t pool_port;
    std::string wallet;
    std::string worker;
    std::string password;
    bool use_ssl;
    uint32_t num_threads;
    uint32_t init_threads;
    bool huge_pages;
    bool numa_aware;
    uint32_t scratchpad_size;
};

struct HashResult {
    std::string nonce;
    std::string hash;
    uint64_t attempts;
    bool valid;
};

class RandomXMiner {
public:
    explicit RandomXMiner(const MinerConfig& config);
    ~RandomXMiner();
    
    bool initialize();
    void start_mining();
    void stop_mining();
    double get_hashrate() const;
    uint64_t get_accepted_shares() const { return m_accepted_shares.load(); }
    uint64_t get_rejected_shares() const { return m_rejected_shares.load(); }
    
private:
    void mining_thread(uint32_t thread_id);
    bool initialize_randomx(const std::string& seed_hash);
    void on_new_job(const MiningJob& job);
    void on_share_result(bool accepted, const std::string& response);
    
    std::vector<uint8_t> construct_mining_blob(const std::string& blob_hex, uint32_t nonce);
    uint64_t target_to_uint64(const std::string& target_hex);
    bool hash_meets_target(const uint8_t* hash, uint64_t target);
    
    MinerConfig m_config;
    std::unique_ptr<StratumClient> m_stratum;
    
    std::vector<std::thread> m_mining_threads;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_new_job{false};
    std::atomic<uint64_t> m_total_hashes{0};
    std::chrono::steady_clock::time_point m_start_time;
    
    std::mutex m_job_mutex;
    MiningJob m_current_job;
    std::string m_current_seed_hash;

    // FIX: mutex protecting RandomX VMs during re-initialisation
    std::mutex m_rx_mutex;
    
    randomx_cache* m_rx_cache{nullptr};
    randomx_dataset* m_rx_dataset{nullptr};
    std::vector<randomx_vm*> m_rx_vms;
    
    std::atomic<uint64_t> m_accepted_shares{0};
    std::atomic<uint64_t> m_rejected_shares{0};
};
