// miner_bridge.cpp
// JNI bridge for MainActivity (simple host/wallet/threads API).
// Owns JNI_OnLoad and the shared g_miner / g_mining globals.

#include <jni.h>
#include <android/log.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include "randomx_miner.h"
#include "stratum_client.h"

#define TAG  "MinerBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Shared globals (android_main.cpp uses them via extern)
std::unique_ptr<RandomXMiner> g_miner;
std::atomic<bool>             g_mining{false};
std::mutex                    g_mutex;

// Forward declaration – implemented in android_main.cpp
void android_main_init(JavaVM* vm);

extern "C" {

// FIX: single JNI_OnLoad; also initialise android_main's JVM reference
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("=== Monero Miner Native Library v1.0 ===");
    android_main_init(vm);
    return JNI_VERSION_1_6;
}

JNIEXPORT jboolean JNICALL
Java_com_monerominer_MainActivity_nativeStartMining(
        JNIEnv* env, jobject thiz,
        jstring host, jint port,
        jstring wallet, jstring worker, jint threads) {

    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_mining.load()) {
        LOGE("Miner already running");
        return JNI_FALSE;
    }

    auto getString = [&](jstring js) -> std::string {
        if (!js) return "";
        const char* c = env->GetStringUTFChars(js, nullptr);
        std::string s(c);
        env->ReleaseStringUTFChars(js, c);
        return s;
    };

    try {
        std::string s_host   = getString(host);
        std::string s_wallet = getString(wallet);
        std::string s_worker = getString(worker);

        LOGI("Starting miner:");
        LOGI("  Pool:    %s:%d",  s_host.c_str(), (int)port);
        LOGI("  Wallet:  %.8s...", s_wallet.c_str());
        LOGI("  Worker:  %s",     s_worker.c_str());
        LOGI("  Threads: %d",     (int)threads);

        MinerConfig config;
        config.pool_host      = s_host;
        config.pool_port      = static_cast<uint16_t>(port);
        config.wallet         = s_wallet;
        config.worker         = s_worker;
        config.password       = "x";
        config.use_ssl        = false;
        config.num_threads    = static_cast<uint32_t>(threads);
        config.init_threads   = 2;
        config.huge_pages     = false;
        config.numa_aware     = false;
        config.scratchpad_size = 0;

        g_miner = std::make_unique<RandomXMiner>(config);

        if (!g_miner->initialize()) {
            LOGE("Failed to initialize miner");
            g_miner.reset();
            return JNI_FALSE;
        }

        g_miner->start_mining();
        g_mining.store(true);

        LOGI("Miner started successfully");
        return JNI_TRUE;

    } catch (const std::exception& e) {
        LOGE("Exception: %s", e.what());
        g_miner.reset();
        return JNI_FALSE;
    }
}

JNIEXPORT void JNICALL
Java_com_monerominer_MainActivity_nativeStopMining(JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_miner) {
        g_miner->stop_mining();
        g_miner.reset();
    }
    g_mining.store(false);
    LOGI("Miner stopped");
}

JNIEXPORT jstring JNICALL
Java_com_monerominer_MainActivity_nativeGetStats(JNIEnv* env, jobject thiz) {
    char stats[256];
    if (g_mining.load() && g_miner) {
        double hr = g_miner->get_hashrate();
        uint64_t acc = g_miner->get_accepted_shares();
        uint64_t rej = g_miner->get_rejected_shares();
        snprintf(stats, sizeof(stats),
                 "{\"hashrate\":%.2f,\"accepted\":%llu,\"rejected\":%llu}",
                 hr, (unsigned long long)acc, (unsigned long long)rej);
    } else {
        snprintf(stats, sizeof(stats),
                 "{\"hashrate\":0,\"accepted\":0,\"rejected\":0}");
    }
    return env->NewStringUTF(stats);
}

} // extern "C"
