// android_main.cpp
// Secondary JNI entry point for MinerService (config-object based API).
// JNI_OnLoad is defined ONLY in miner_bridge.cpp to avoid duplicate symbols.

#include <jni.h>
#include <android/log.h>
#include <string>
#include <thread>
#include <atomic>
#include "randomx_miner.h"
#include "stratum_client.h"

#define LOG_TAG "MoneroMiner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Shared with miner_bridge.cpp via extern
extern std::unique_ptr<RandomXMiner> g_miner;
extern std::atomic<bool>             g_mining;
extern std::mutex                    g_mutex;

static JavaVM*  g_jvm             = nullptr;
static jclass   g_callback_class  = nullptr;
static jobject  g_callback_object = nullptr;

// ── Helper: build MinerConfig from Java object ────────────────
static MinerConfig createMinerConfig(JNIEnv* env, jobject config_obj) {
    MinerConfig config;
    jclass cls = env->GetObjectClass(config_obj);

    auto getStr = [&](const char* field) -> std::string {
        jfieldID fid = env->GetFieldID(cls, field, "Ljava/lang/String;");
        jstring  js  = (jstring)env->GetObjectField(config_obj, fid);
        if (!js) return "";
        const char* c = env->GetStringUTFChars(js, nullptr);
        std::string s(c);
        env->ReleaseStringUTFChars(js, c);
        return s;
    };
    auto getInt = [&](const char* field) -> int {
        jfieldID fid = env->GetFieldID(cls, field, "I");
        return env->GetIntField(config_obj, fid);
    };

    config.pool_host     = getStr("poolHost");
    config.wallet        = getStr("wallet");
    config.worker        = getStr("worker");
    config.password      = getStr("password");
    config.pool_port     = static_cast<uint16_t>(getInt("poolPort"));
    config.num_threads   = static_cast<uint32_t>(getInt("threads"));
    config.use_ssl       = false;
    config.huge_pages    = false;
    config.numa_aware    = false;
    config.init_threads  = std::min(2u, config.num_threads);
    config.scratchpad_size = 0;
    return config;
}

// ── JNI: save JVM on load (called once by miner_bridge.cpp) ──
// FIX: removed duplicate JNI_OnLoad; miner_bridge.cpp owns it and
//      calls android_main_init() so we can still grab g_jvm.
void android_main_init(JavaVM* vm) {
    g_jvm = vm;
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_monerominer_MinerService_nativeStartMining(
        JNIEnv* env,
        jobject thiz,
        jobject config_obj,
        jobject callback_obj) {

    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_mining.load()) {
        LOGE("Miner already running");
        return JNI_FALSE;
    }

    try {
        MinerConfig config = createMinerConfig(env, config_obj);

        // Save global callback refs
        if (g_callback_object) {
            env->DeleteGlobalRef(g_callback_object);
            g_callback_object = nullptr;
        }
        if (g_callback_class) {
            env->DeleteGlobalRef(g_callback_class);
            g_callback_class = nullptr;
        }
        g_callback_class  = (jclass)env->NewGlobalRef(env->GetObjectClass(callback_obj));
        g_callback_object = env->NewGlobalRef(callback_obj);

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
        LOGE("Exception starting miner: %s", e.what());
        g_miner.reset();
        return JNI_FALSE;
    }
}

JNIEXPORT void JNICALL
Java_com_monerominer_MinerService_nativeStopMining(JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_miner) {
        g_miner->stop_mining();
        g_miner.reset();
    }
    g_mining.store(false);

    if (g_callback_object) { env->DeleteGlobalRef(g_callback_object); g_callback_object = nullptr; }
    if (g_callback_class)  { env->DeleteGlobalRef(g_callback_class);  g_callback_class  = nullptr; }

    LOGI("Miner stopped");
}

JNIEXPORT jstring JNICALL
Java_com_monerominer_MinerService_nativeGetStats(JNIEnv* env, jobject thiz) {
    if (!g_miner || !g_mining.load()) {
        return env->NewStringUTF("{\"status\":\"stopped\",\"hashrate\":0,\"accepted\":0,\"rejected\":0}");
    }
    double hashrate = g_miner->get_hashrate();
    uint64_t acc = g_miner->get_accepted_shares();
    uint64_t rej = g_miner->get_rejected_shares();
    char stats[256];
    snprintf(stats, sizeof(stats),
             "{\"status\":\"running\",\"hashrate\":%.2f,\"accepted\":%llu,\"rejected\":%llu}",
             hashrate, (unsigned long long)acc, (unsigned long long)rej);
    return env->NewStringUTF(stats);
}

} // extern "C"
