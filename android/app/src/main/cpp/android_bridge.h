// FILE: MoneroRandomXMiner/android/app/src/main/cpp/android_bridge.h

#pragma once

#ifdef ANDROID

#include <jni.h>
#include <android/log.h>
#include <string>
#include <atomic>
#include <functional>
#include <chrono>

#define LOG_TAG "MoneroMinerNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

/**
 * Android Bridge - Main interface between Java/Kotlin and native C++ code
 * Provides JNI bindings and Android-specific functionality
 */
class AndroidBridge {
public:
    static AndroidBridge& instance();
    
    // Initialize the bridge with JVM reference
    bool initialize(JavaVM* jvm, JNIEnv* env, jobject callbackObj);
    
    // Set mining status callback
    using StatusCallback = std::function<void(const std::string& status, const std::string& data)>;
    void setStatusCallback(StatusCallback callback);
    
    // Notify Java layer of events
    void notifyStatus(const std::string& status, const std::string& data);
    void notifyShareFound(const std::string& jobId, const std::string& nonce, const std::string& hash);
    void notifyError(const std::string& error);
    void notifyConnectionChange(bool connected, int port);
    
    // Battery and thermal states
    void updateBatteryLevel(int level, bool charging);
    void updateTemperature(float celsius);
    
    // Thread management
    int getOptimalThreadCount();
    bool shouldThrottle();
    float getThrottleLevel(); // Returns 0.0 (no throttle) to 1.0 (full throttle)
    
    // Configuration
    void setMiningConfig(const std::string& poolHost, int port, 
                        const std::string& wallet, const std::string& worker,
                        int threads, bool useSSL);
    
private:
    AndroidBridge() = default;
    ~AndroidBridge();
    
    // Prevent copying
    AndroidBridge(const AndroidBridge&) = delete;
    AndroidBridge& operator=(const AndroidBridge&) = delete;
    
    // JVM references
    JavaVM* m_jvm{nullptr};
    jclass m_callbackClass{nullptr};
    jobject m_callbackObject{nullptr};
    
    // Thread safety
    std::mutex m_mutex;
    
    // Status callback
    StatusCallback m_statusCallback;
    
    // Device state
    std::atomic<int> m_batteryLevel{100};
    std::atomic<bool> m_isCharging{true};
    std::atomic<float> m_cpuTemperature{35.0f};
    std:: atomic<bool> m_thermalThrottled{false};
    
    // Mining config
    std::string m_poolHost;
    int m_poolPort{443};
    std::string m_wallet;
    std::string m_worker{"android_miner"};
    int m_threads{2};
    bool m_useSSL{true};
    
    // Helper to get JNI environment
    JNIEnv* getJNIEnv();
    void detachJNIThread();
};

// JNI function declarations
extern "C" {
    JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
    
    // Mining control
    JNIEXPORT jboolean JNICALL
    Java_com_monerominer_MinerService_nativeStartMining(
        JNIEnv* env, jobject thiz, jobject configObj, jobject callbackObj);
    
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeStopMining(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jstring JNICALL
    Java_com_monerominer_MinerService_nativeGetStats(
        JNIEnv* env, jobject thiz);
    
    // Connection
    JNIEXPORT jboolean JNICALL
    Java_com_monerominer_MinerService_nativeConnectSSL(
        JNIEnv* env, jobject thiz, jstring host, jint port,
        jstring wallet, jstring password, jstring worker, jboolean useSSL);
    
    // Battery monitoring
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeUpdateBatteryStatus(
        JNIEnv* env, jobject thiz, jint level, jboolean charging);
    
    JNIEXPORT jboolean JNICALL
    Java_com_monerominer_MinerService_nativeIsLowPowerMode(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jint JNICALL
    Java_com_monerominer_MinerService_nativeGetMiningIntensity(
        JNIEnv* env, jobject thiz);
    
    // Thermal monitoring
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeUpdateTemperature(
        JNIEnv* env, jobject thiz, jfloat temperature);
    
    JNIEXPORT jfloat JNICALL
    Java_com_monerominer_MinerService_nativeGetTemperature(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jboolean JNICALL
    Java_com_monerominer_MinerService_nativeIsThermalThrottled(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jint JNICALL
    Java_com_monerominer_MinerService_nativeGetThermalThreadLimit(
        JNIEnv* env, jobject thiz);
    
    // System info
    JNIEXPORT jlong JNICALL
    Java_com_monerominer_MinerService_nativeGetAvailableMemory(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jint JNICALL
    Java_com_monerominer_MinerService_nativeGetOptimalThreadCount(
        JNIEnv* env, jobject thiz);
}

#endif // ANDROID
