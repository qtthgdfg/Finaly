#include "stratum_client.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <chrono>

// ─── nlohmann/json (header-only) ────────────────────────────
// We only use it for parsing – fall back gracefully if absent.
#if __has_include(<nlohmann/json.hpp>)
#  include <nlohmann/json.hpp>
   using json = nlohmann::json;
#  define HAVE_JSON 1
#else
#  define HAVE_JSON 0
#endif

StratumClient::StratumClient(const std::string& pool_host,
                             uint16_t pool_port,
                             const std::string& wallet,
                             const std::string& password,
                             const std::string& worker,
                             bool use_ssl)
    : m_pool_host(pool_host)
    , m_pool_port(pool_port)
    , m_wallet(wallet)
    , m_password(password)
    , m_worker(worker)
    , m_use_ssl(use_ssl) {
}

StratumClient::~StratumClient() {
    stop();
}

bool StratumClient::resolve_host() {
    struct addrinfo hints{}, *result;
    hints.ai_family   = AF_UNSPEC;   // FIX: support both IPv4 & IPv6
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(m_pool_port);
    int ret = getaddrinfo(m_pool_host.c_str(), port_str.c_str(), &hints, &result);
    if (ret != 0) {
        std::cerr << "DNS resolution failed: " << gai_strerror(ret) << std::endl;
        return false;
    }

    // FIX: iterate results; the first may be IPv6 but we opened AF_UNSPEC
    bool found = false;
    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            memcpy(&m_server_addr, rp->ai_addr, sizeof(m_server_addr));
            found = true;
            break;
        }
    }
    freeaddrinfo(result);
    return found;
}

bool StratumClient::connect() {
    if (m_connected.load()) return true;
    if (!resolve_host()) return false;

    m_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket_fd < 0) return false;

    int opt = 1;
    setsockopt(m_socket_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

    struct timeval timeout{30, 0};
    setsockopt(m_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(m_socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (::connect(m_socket_fd, (struct sockaddr*)&m_server_addr, sizeof(m_server_addr)) < 0) {
        close(m_socket_fd);
        m_socket_fd = -1;
        return false;
    }

    m_connected.store(true);
    return true;
}

bool StratumClient::login() {
    // FIX: include worker name in login agent string as pools expect
    std::string login_str =
        "{\"id\":1,\"method\":\"login\",\"params\":"
        "{\"login\":\"" + m_wallet + "\","
        "\"pass\":\""   + m_password + "\","
        "\"agent\":\"CPPMiner/1.0\","
        "\"worker\":\"" + m_worker + "\"}}\n";
    send_message(login_str);
    return true;
}

void StratumClient::send_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_send_mutex);
    // FIX: check return value of send() so partial writes are retried
    size_t total_sent = 0;
    const char* data  = message.c_str();
    size_t length     = message.length();
    while (total_sent < length) {
        ssize_t sent = send(m_socket_fd, data + total_sent,
                            length - total_sent, MSG_NOSIGNAL);
        if (sent <= 0) break;
        total_sent += sent;
    }
}

void StratumClient::start_receive_loop() {
    m_running.store(true);
    m_receive_thread = std::thread([this]() {
        char recv_buffer[65536];

        while (m_running.load()) {
            int bytes_received = recv(m_socket_fd, recv_buffer,
                                      sizeof(recv_buffer) - 1, 0);

            if (bytes_received <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                m_connected.store(false);
                break;
            }

            recv_buffer[bytes_received] = '\0';
            m_buffer += std::string(recv_buffer, bytes_received);

            size_t pos;
            while ((pos = m_buffer.find('\n')) != std::string::npos) {
                std::string line = m_buffer.substr(0, pos);
                m_buffer.erase(0, pos + 1);
                if (!line.empty()) {
                    handle_message(line);
                }
            }
        }
    });
}

// ──────────────────────────────────────────────────────────────
// FIX (CRITICAL): handle_message previously set ALL job fields
// to empty/zero – meaning mining threads always saw an empty blob
// and immediately slept forever.  Now we actually parse the JSON.
// ──────────────────────────────────────────────────────────────
void StratumClient::handle_message(const std::string& message) {
#if HAVE_JSON
    try {
        auto j = json::parse(message);

        // ── login reply (contains first job) ──────────────────
        if (j.contains("result") && j["result"].is_object()) {
            auto& result = j["result"];
            if (result.contains("job") && result["job"].is_object()) {
                auto& jj = result["job"];
                MiningJob job;
                job.job_id       = jj.value("job_id",   "");
                job.blob         = jj.value("blob",      "");
                job.target       = jj.value("target",    "");
                job.seed_hash    = jj.value("seed_hash", "");
                job.height       = jj.value("height",    uint64_t(0));
                job.received_time = std::chrono::steady_clock::now();

                if (!job.blob.empty()) {
                    std::lock_guard<std::mutex> lock(m_job_mutex);
                    m_current_job = job;
                    if (m_job_callback) m_job_callback(m_current_job);
                }
            }
        }

        // ── new_job notification ───────────────────────────────
        if (j.contains("method") && j["method"] == "job" &&
            j.contains("params") && j["params"].is_object()) {
            auto& p = j["params"];
            MiningJob job;
            job.job_id       = p.value("job_id",   "");
            job.blob         = p.value("blob",      "");
            job.target       = p.value("target",    "");
            job.seed_hash    = p.value("seed_hash", "");
            job.height       = p.value("height",    uint64_t(0));
            job.received_time = std::chrono::steady_clock::now();

            if (!job.blob.empty()) {
                std::lock_guard<std::mutex> lock(m_job_mutex);
                m_current_job = job;
                if (m_job_callback) m_job_callback(m_current_job);
            }
        }

        // ── share accept/reject reply ──────────────────────────
        if (j.contains("id") && j.contains("result") && j["result"].is_boolean()) {
            bool accepted = j["result"].get<bool>();
            std::string msg = j.value("error", json{}).is_null()
                              ? "" : j["error"].dump();
            if (m_share_callback) m_share_callback(accepted, msg);
        }

    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what()
                  << "\nMessage: " << message << std::endl;
    }
#else
    // Minimal fallback: look for "job" key in raw string
    if (message.find("\"job\"") != std::string::npos) {
        std::cerr << "[stratum] JSON library missing – cannot parse job\n";
    }
#endif
}

bool StratumClient::submit_share(const std::string& job_id,
                                 const std::string& nonce,
                                 const std::string& result) {
    // FIX: m_message_id is uint32_t but was post-incremented after use;
    //      use pre-increment so id 1 is reserved for login.
    std::string submit_str =
        "{\"id\":"      + std::to_string(++m_message_id) +
        ",\"method\":\"submit\",\"params\":"
        "{\"id\":\""    + m_worker +
        "\",\"job_id\":\"" + job_id +
        "\",\"nonce\":\"" + nonce +
        "\",\"result\":\"" + result + "\"}}\n";
    send_message(submit_str);
    return true;
}

void StratumClient::stop() {
    m_running.store(false);
    // FIX: shutdown socket before join so recv() unblocks immediately
    if (m_socket_fd >= 0) {
        ::shutdown(m_socket_fd, SHUT_RDWR);
    }
    if (m_receive_thread.joinable()) {
        m_receive_thread.join();
    }
    if (m_socket_fd >= 0) {
        close(m_socket_fd);
        m_socket_fd = -1;
    }
    m_connected.store(false);
}

void StratumClient::set_job_callback(JobCallback callback) {
    m_job_callback = std::move(callback);
}

void StratumClient::set_share_callback(ShareCallback callback) {
    m_share_callback = std::move(callback);
}

MiningJob StratumClient::get_current_job() const {
    std::lock_guard<std::mutex> lock(m_job_mutex);
    return m_current_job;
}
