#include "randomx_miner.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <random>

RandomXMiner::RandomXMiner(const MinerConfig& config)
    : m_config(config) {

    m_stratum = std::make_unique<StratumClient>(
        config.pool_host,
        config.pool_port,
        config.wallet,
        config.password,
        config.worker,
        config.use_ssl
    );
}

RandomXMiner::~RandomXMiner() {
    stop_mining();

    for (auto* vm : m_rx_vms) {
        if (vm) randomx_destroy_vm(vm);
    }
    if (m_rx_dataset) randomx_release_dataset(m_rx_dataset);
    if (m_rx_cache)   randomx_release_cache(m_rx_cache);
}

bool RandomXMiner::initialize() {
    // Set up callbacks BEFORE starting the receive loop so no job is missed
    m_stratum->set_job_callback([this](const MiningJob& job) {
        this->on_new_job(job);
    });

    m_stratum->set_share_callback([this](bool accepted, const std::string& response) {
        this->on_share_result(accepted, response);
    });

    if (!m_stratum->connect()) {
        std::cerr << "Failed to connect to pool" << std::endl;
        return false;
    }

    if (!m_stratum->login()) {
        std::cerr << "Failed to login to pool" << std::endl;
        return false;
    }

    m_stratum->start_receive_loop();

    std::cout << "Successfully connected and logged in to pool" << std::endl;
    return true;
}

// FIX: guard with m_rx_mutex so only ONE thread rebuilds the dataset;
//      the others wait and then reuse the freshly created VMs.
bool RandomXMiner::initialize_randomx(const std::string& seed_hash) {
    std::lock_guard<std::mutex> lock(m_rx_mutex);

    if (seed_hash == m_current_seed_hash) return true;  // already up to date

    std::cout << "Initializing RandomX for seed: "
              << seed_hash.substr(0, 16) << "..." << std::endl;

    // Destroy old VMs
    for (auto* vm : m_rx_vms) {
        if (vm) randomx_destroy_vm(vm);
    }
    m_rx_vms.clear();

    if (m_rx_dataset) { randomx_release_dataset(m_rx_dataset); m_rx_dataset = nullptr; }
    if (m_rx_cache)   { randomx_release_cache(m_rx_cache);     m_rx_cache   = nullptr; }

    // Parse seed hash
    std::vector<uint8_t> seed_bytes;
    seed_bytes.reserve(seed_hash.length() / 2);
    for (size_t i = 0; i + 1 < seed_hash.length(); i += 2) {
        seed_bytes.push_back(
            static_cast<uint8_t>(strtol(seed_hash.substr(i, 2).c_str(), nullptr, 16)));
    }

    randomx_flags flags = RANDOMX_FLAG_DEFAULT;

#ifdef ENABLE_HUGEPAGES
    if (m_config.huge_pages)
        flags = static_cast<randomx_flags>(flags | RANDOMX_FLAG_HUGE_PAGES);
#endif

    m_rx_cache = randomx_alloc_cache(flags);
    if (!m_rx_cache) {
        std::cerr << "Failed to allocate RandomX cache" << std::endl;
        return false;
    }

    randomx_init_cache(m_rx_cache, seed_bytes.data(), seed_bytes.size());

    m_rx_dataset = randomx_alloc_dataset(flags);
    if (!m_rx_dataset) {
        std::cerr << "Failed to allocate RandomX dataset" << std::endl;
        randomx_release_cache(m_rx_cache); m_rx_cache = nullptr;
        return false;
    }

    uint32_t num_dataset_items = randomx_dataset_item_count();
    randomx_init_dataset(m_rx_dataset, m_rx_cache, 0, num_dataset_items);

    // Create one VM per thread
    for (uint32_t i = 0; i < m_config.num_threads; ++i) {
        randomx_vm* vm = randomx_create_vm(flags, m_rx_cache, m_rx_dataset);
        if (!vm) {
            std::cerr << "Failed to create RandomX VM for thread " << i << std::endl;
            return false;
        }
        m_rx_vms.push_back(vm);
    }

    m_current_seed_hash = seed_hash;
    std::cout << "RandomX initialized successfully (" << m_config.num_threads
              << " VMs)" << std::endl;
    return true;
}

std::vector<uint8_t> RandomXMiner::construct_mining_blob(
        const std::string& blob_hex, uint32_t nonce) {

    std::vector<uint8_t> blob;
    blob.reserve(blob_hex.length() / 2);

    for (size_t i = 0; i + 1 < blob_hex.length(); i += 2) {
        blob.push_back(
            static_cast<uint8_t>(strtol(blob_hex.substr(i, 2).c_str(), nullptr, 16)));
    }

    if (blob.size() < 76) {
        throw std::runtime_error("Invalid mining blob size: " +
                                 std::to_string(blob.size()));
    }

    // Nonce sits at offset 39 in a Monero block header (little-endian)
    constexpr uint32_t nonce_offset = 39;
    blob[nonce_offset]     =  nonce        & 0xFF;
    blob[nonce_offset + 1] = (nonce >>  8) & 0xFF;
    blob[nonce_offset + 2] = (nonce >> 16) & 0xFF;
    blob[nonce_offset + 3] = (nonce >> 24) & 0xFF;

    return blob;
}

// FIX: Monero target is a 4-byte compact form when < 8 hex chars,
//      or 8-byte little-endian uint64.  Convert correctly.
uint64_t RandomXMiner::target_to_uint64(const std::string& target_hex) {
    // Pool sends either 8-hex-char (32-bit LE target) or 16-hex-char (64-bit)
    if (target_hex.length() <= 8) {
        // 4-byte LE → expand to 64-bit: target = 0xFFFFFFFF00000000 / compact
        uint32_t compact = 0;
        for (size_t i = 0; i < target_hex.length() && i < 8; i += 2) {
            uint8_t b = static_cast<uint8_t>(
                strtol(target_hex.substr(i, 2).c_str(), nullptr, 16));
            compact |= static_cast<uint32_t>(b) << (i * 4);
        }
        if (compact == 0) return UINT64_MAX;
        return (UINT64_MAX / compact);
    }

    // 8-byte LE uint64
    uint8_t bytes[8] = {};
    for (size_t i = 0; i < target_hex.length() && i < 16; i += 2) {
        bytes[i / 2] = static_cast<uint8_t>(
            strtol(target_hex.substr(i, 2).c_str(), nullptr, 16));
    }
    uint64_t t = 0;
    for (int i = 0; i < 8; ++i)
        t |= static_cast<uint64_t>(bytes[i]) << (i * 8);
    return t;
}

bool RandomXMiner::hash_meets_target(const uint8_t* hash, uint64_t target) {
    // Hash is little-endian; read last 8 bytes as a uint64
    uint64_t hash_value = 0;
    for (int i = 7; i >= 0; --i)
        hash_value = (hash_value << 8) | hash[24 + i]; // bytes 24-31
    return hash_value < target;
}

void RandomXMiner::mining_thread(uint32_t thread_id) {
    // Wait until RandomX is initialised (job arrives and triggers init)
    while (m_running.load()) {
        {
            std::lock_guard<std::mutex> lock(m_rx_mutex);
            if (thread_id < m_rx_vms.size() && m_rx_vms[thread_id]) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!m_running.load()) return;

    randomx_vm* vm = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_rx_mutex);
        vm = m_rx_vms[thread_id];
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> nonce_dist(0, 0xFFFFFFFF);

    // Spread starting nonces across threads so they don't overlap
    uint32_t nonce = nonce_dist(gen) ^ (thread_id * 0x01000193u);

    uint64_t local_hash_count = 0;
    std::string last_seed;

    std::cout << "Mining thread " << thread_id << " started" << std::endl;

    while (m_running.load()) {
        MiningJob job;
        {
            std::lock_guard<std::mutex> lock(m_job_mutex);
            job = m_current_job;
        }

        if (job.blob.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // FIX: re-initialise RandomX if seed changed; only one thread
        //      does the work, the others wait in initialize_randomx().
        if (job.seed_hash != last_seed) {
            if (!initialize_randomx(job.seed_hash)) {
                std::cerr << "RandomX re-init failed on thread " << thread_id << std::endl;
                break;
            }
            {
                std::lock_guard<std::mutex> lock(m_rx_mutex);
                if (thread_id >= m_rx_vms.size()) break;
                vm = m_rx_vms[thread_id];
            }
            last_seed = job.seed_hash;
        }

        uint64_t target = target_to_uint64(job.target);

        const uint32_t BATCH_SIZE = 5000;
        bool new_job_arrived = false;

        for (uint32_t i = 0; i < BATCH_SIZE; ++i) {
            // FIX: check m_new_job per-hash not just per-batch-start
            if (m_new_job.load()) {
                m_new_job.store(false);
                new_job_arrived = true;
                break;
            }

            std::vector<uint8_t> mining_blob;
            try {
                mining_blob = construct_mining_blob(job.blob, nonce);
            } catch (const std::exception& e) {
                std::cerr << "Blob error: " << e.what() << std::endl;
                break;
            }

            uint8_t hash[RANDOMX_HASH_SIZE];
            randomx_calculate_hash(vm, mining_blob.data(), mining_blob.size(), hash);

            ++local_hash_count;

            if (hash_meets_target(hash, target)) {
                std::stringstream nonce_ss, hash_ss;
                nonce_ss << std::hex << std::setw(8) << std::setfill('0') << nonce;

                for (int j = 0; j < RANDOMX_HASH_SIZE; ++j)
                    hash_ss << std::hex << std::setw(2) << std::setfill('0')
                            << static_cast<int>(hash[j]);

                std::cout << "Thread " << thread_id << " found solution! Nonce: "
                          << nonce_ss.str() << std::endl;

                m_stratum->submit_share(job.job_id, nonce_ss.str(), hash_ss.str());

                // FIX: randomise next nonce after a share to avoid duplicates
                nonce = nonce_dist(gen);
                continue;
            }

            ++nonce;
        }

        m_total_hashes.fetch_add(local_hash_count);
        local_hash_count = 0;

        if (!new_job_arrived) {
            m_new_job.store(false); // clear stale flag
        }
    }

    std::cout << "Mining thread " << thread_id << " stopped" << std::endl;
}

void RandomXMiner::on_new_job(const MiningJob& job) {
    {
        std::lock_guard<std::mutex> lock(m_job_mutex);
        m_current_job = job;
    }
    m_new_job.store(true);
}

void RandomXMiner::on_share_result(bool accepted, const std::string& /*response*/) {
    if (accepted) m_accepted_shares.fetch_add(1);
    else          m_rejected_shares.fetch_add(1);
}

void RandomXMiner::start_mining() {
    m_running.store(true);
    m_start_time = std::chrono::steady_clock::now();

    for (uint32_t i = 0; i < m_config.num_threads; ++i) {
        m_mining_threads.emplace_back(&RandomXMiner::mining_thread, this, i);
    }
}

void RandomXMiner::stop_mining() {
    m_running.store(false);

    for (auto& thread : m_mining_threads) {
        if (thread.joinable()) thread.join();
    }
    m_mining_threads.clear();
}

double RandomXMiner::get_hashrate() const {
    auto elapsed = std::chrono::steady_clock::now() - m_start_time;
    double seconds = std::chrono::duration<double>(elapsed).count();
    // FIX: avoid division by zero; also protect with atomic read
    return seconds > 0.1 ? static_cast<double>(m_total_hashes.load()) / seconds : 0.0;
}
