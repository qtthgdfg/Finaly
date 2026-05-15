#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct MiningJob {
    std::string job_id;
    std::string blob;
    std::string target;
    std::string seed_hash;
    uint64_t height;
    std::chrono::steady_clock::time_point received_time;
};

class StratumClient {
public:
    using JobCallback = std::function<void(const MiningJob&)>;
    using ShareCallback = std::function<void(bool, const std::string&)>;
    
    StratumClient(const std::string& pool_host, 
                  uint16_t pool_port,
                  const std::string& wallet,
                  const std::string& password = "x",
                  const std::string& worker = "worker1",
                  bool use_ssl = false);
    ~StratumClient();
    
    bool connect();
    bool login();
    void start_receive_loop();
    void stop();
    bool submit_share(const std::string& job_id,
                     const std::string& nonce,
                     const std::string& result);
    
    void set_job_callback(JobCallback callback);
    void set_share_callback(ShareCallback callback);
    MiningJob get_current_job() const;
    
    bool is_connected() const { return m_connected.load(); }
    
private:
    void handle_message(const std::string& message);
    void send_message(const std::string& message);
    bool resolve_host();
    
    std::string m_pool_host;
    uint16_t m_pool_port;
    std::string m_wallet;
    std::string m_password;
    std::string m_worker;
    bool m_use_ssl;
    
    int m_socket_fd{-1};
    struct sockaddr_in m_server_addr{};
    
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_running{false};
    std::thread m_receive_thread;
    
    mutable std::mutex m_job_mutex;
    MiningJob m_current_job;
    
    mutable std::mutex m_send_mutex;
    JobCallback m_job_callback;
    ShareCallback m_share_callback;
    
    std::string m_buffer;
    uint32_t m_message_id{1};
};
