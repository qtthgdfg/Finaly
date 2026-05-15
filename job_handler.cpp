#include "job_handler.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <cmath>

namespace monero {

// ==================== TargetHelper ====================
uint64_t TargetHelper::compact_to_uint64(const std::string& target_hex) {
    uint64_t target = 0;
    for (size_t i = 0; i < target_hex.length() && i < 16; i += 2) {
        std::string byte_str = target_hex.substr(i, 2);
        target = (target << 8) | static_cast<uint8_t>(strtol(byte_str.c_str(), nullptr, 16));
    }
    return target;
}
std::string TargetHelper::uint64_to_compact(uint64_t target) {
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << target;
    return ss.str();
}
bool TargetHelper::hash_meets_target(const uint8_t* hash, uint64_t target) {
    uint64_t hv = 0;
    for (int i = 7; i >= 0; --i) hv = (hv << 8) | hash[i];
    return hv < target;
}
double TargetHelper::difficulty_from_target(uint64_t target) {
    constexpr uint64_t MAX = 0xFFFFFFFFFFFFFFFF;
    return target == 0 ? 0.0 : static_cast<double>(MAX) / static_cast<double>(target);
}
uint64_t TargetHelper::from_hex_target(const std::string& s) { return compact_to_uint64(s); }
std::string TargetHelper::to_hex_target(uint64_t t) { return uint64_to_compact(t); }

// ==================== BlobHelper ====================
std::vector<uint8_t> BlobHelper::hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2)
        bytes.push_back(static_cast<uint8_t>(strtol(hex.substr(i, 2).c_str(), nullptr, 16)));
    return bytes;
}
std::string BlobHelper::bytes_to_hex(const std::vector<uint8_t>& bytes) { return bytes_to_hex(bytes.data(), bytes.size()); }
std::string BlobHelper::bytes_to_hex(const uint8_t* bytes, size_t length) {
    std::stringstream ss; ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) ss << std::setw(2) << static_cast<int>(bytes[i]);
    return ss.str();
}
void BlobHelper::insert_nonce(std::vector<uint8_t>& blob, uint32_t nonce, uint32_t offset) {
    blob[offset] = nonce & 0xFF; blob[offset+1] = (nonce>>8)&0xFF;
    blob[offset+2] = (nonce>>16)&0xFF; blob[offset+3] = (nonce>>24)&0xFF;
}
uint32_t BlobHelper::extract_nonce(const std::vector<uint8_t>& blob, uint32_t offset) {
    return blob[offset] | (blob[offset+1]<<8) | (blob[offset+2]<<16) | (blob[offset+3]<<24);
}
void BlobHelper::insert_extra_nonce(std::vector<uint8_t>& blob, const std::vector<uint8_t>& extra) {
    blob.insert(blob.begin()+43, extra.begin(), extra.end());
}
std::vector<uint8_t> BlobHelper::extract_extra_nonce(const std::vector<uint8_t>& blob) {
    return blob.size() > 43 ? std::vector<uint8_t>(blob.begin()+43, blob.end()) : std::vector<uint8_t>();
}
bool BlobHelper::validate_blob(const std::vector<uint8_t>& blob) { return blob.size() >= 76; }
size_t BlobHelper::get_nonce_offset(const std::vector<uint8_t>&) { return 39; }

// ==================== JobManager ====================
JobManager::JobManager() { m_stats.reset(); }
JobManager::~JobManager() {}
void JobManager::set_job_callback(JobCallback cb) { std::lock_guard<std::mutex> l(m_mutex); m_job_callback = std::move(cb); }
void JobManager::set_share_callback(ShareCallback cb) { std::lock_guard<std::mutex> l(m_mutex); m_share_callback = std::move(cb); }
void JobManager::new_job(const MiningJob& j) {
    std::lock_guard<std::mutex> l(m_mutex);
    m_current_job = j;
    m_recent_job_ids.push_back(j.job_id);
    if(m_recent_job_ids.size() > m_max_job_history) m_recent_job_ids.erase(m_recent_job_ids.begin());
    if(m_job_callback) m_job_callback(j);
}
MiningJob JobManager::get_current_job() const { std::lock_guard<std::mutex> l(m_mutex); return m_current_job; }
bool JobManager::has_job() const { std::lock_guard<std::mutex> l(m_mutex); return !m_current_job.blob.empty(); }
void JobManager::record_share(const ShareResult& r) {
    std::lock_guard<std::mutex> l(m_mutex);
    if(r.accepted) m_stats.accepted_shares++; else m_stats.rejected_shares++;
}
MiningStats JobManager::get_stats() const {
    std::lock_guard<std::mutex> l(m_mutex);
    MiningStats s;
    s.total_hashes = m_stats.total_hashes;
    s.accepted_shares = m_stats.accepted_shares;
    s.rejected_shares = m_stats.rejected_shares;
    return s;
}
void JobManager::reset_stats() { std::lock_guard<std::mutex> l(m_mutex); m_stats.reset(); }
void JobManager::set_job_history_size(size_t s) { std::lock_guard<std::mutex> l(m_mutex); m_max_job_history = s; }

// ==================== WorkerPool ====================
WorkerPool::WorkerPool(uint32_t n) : m_num_workers(n) {}
WorkerPool::~WorkerPool() {}
void WorkerPool::configure_workers(bool numa, bool aff) {
    m_worker_configs.clear();
    for(uint32_t i=0; i<m_num_workers; i++)
        m_worker_configs.push_back({i, i%std::thread::hardware_concurrency(), numa, numa?i%2:0u});
}
std::vector<WorkerPool::WorkerConfig> WorkerPool::get_worker_configs() const { return m_worker_configs; }
void WorkerPool::detect_numa_topology() {}
uint32_t WorkerPool::get_optimal_thread_count() const { return std::max(1u, std::thread::hardware_concurrency()); }

// ==================== DifficultyManager ====================
DifficultyManager::DifficultyManager(double d) : m_current_difficulty(d) { m_current_target = difficulty_to_target(d); }
void DifficultyManager::update_target(double d) { std::lock_guard<std::mutex> l(m_mutex); m_current_difficulty = d; m_current_target = difficulty_to_target(d); }
uint64_t DifficultyManager::get_current_target() const { std::lock_guard<std::mutex> l(m_mutex); return m_current_target; }
double DifficultyManager::get_current_difficulty() const { std::lock_guard<std::mutex> l(m_mutex); return m_current_difficulty; }
void DifficultyManager::set_fixed_difficulty(double d) { std::lock_guard<std::mutex> l(m_mutex); m_current_difficulty = d; m_current_target = difficulty_to_target(d); }
void DifficultyManager::enable_auto_difficulty(bool e) { std::lock_guard<std::mutex> l(m_mutex); m_auto_difficulty = e; }
uint64_t DifficultyManager::difficulty_to_target(double d) {
    constexpr uint64_t MAX = 0xFFFFFFFFFFFFFFFF;
    return d <= 0.0 ? MAX : static_cast<uint64_t>(static_cast<double>(MAX) / d);
}

} // namespace monero
