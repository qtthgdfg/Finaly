#include "randomx_miner.h"
#include <iostream>
#include <fstream>
#include <csignal>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::atomic<bool> keep_running{true};

void signal_handler(int signal) {
    std::cout << "\nShutting down miner..." << std::endl;
    keep_running.store(false);
}

MinerConfig load_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_file);
    }
    
    json j;
    file >> j;
    
    MinerConfig config;
    config.pool_host = j.value("pool_host", "pool.supportxmr.com");
    config.pool_port = j.value("pool_port", 3333);
    config.wallet = j.value("wallet", "");
    config.worker = j.value("worker", "worker1");
    config.password = j.value("password", "x");
    config.use_ssl = j.value("use_ssl", false);
    config.num_threads = j.value("threads", std::thread::hardware_concurrency());
    config.init_threads = j.value("init_threads", 2);
    config.huge_pages = j.value("huge_pages", true);
    config.numa_aware = j.value("numa_aware", false);
    config.scratchpad_size = j.value("scratchpad_size", 0);
    
    return config;
}

int main(int argc, char* argv[]) {
    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        std::string config_file = "config.json";
        if (argc > 1) {
            config_file = argv[1];
        }
        
        std::cout << "Loading configuration from: " << config_file << std::endl;
        MinerConfig config = load_config(config_file);
        
        if (config.wallet.empty()) {
            std::cerr << "Error: Wallet address is required!" << std::endl;
            return 1;
        }
        
        std::cout << "Starting Monero RandomX Miner" << std::endl;
        std::cout << "Pool: " << config.pool_host << ":" << config.pool_port << std::endl;
        std::cout << "Threads: " << config.num_threads << std::endl;
        
        RandomXMiner miner(config);
        
        if (!miner.initialize()) {
            std::cerr << "Failed to initialize miner" << std::endl;
            return 1;
        }
        
        miner.start_mining();
        
        // Keep main thread alive
        while (keep_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        miner.stop_mining();
        std::cout << "Miner stopped" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
