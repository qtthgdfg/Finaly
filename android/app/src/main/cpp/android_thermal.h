// FILE: MoneroRandomXMiner/android/app/src/main/cpp/android_thermal.h

#pragma once

#ifdef ANDROID

#include <jni.h>
#include <android/log.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#define THERMAL_TAG "ThermalManager"
#define THERMAL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, THERMAL_TAG, __VA_ARGS__)
#define THERMAL_LOGW(...) __android_log_print(ANDROID_LOG_WARN, THERMAL_TAG, __VA_ARGS__)
#define THERMAL_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, THERMAL_TAG, __VA_ARGS__)

/**
 * ThermalManager - Monitors and manages CPU temperature
 * Protects device from overheating during mining
 */
class ThermalManager {
public:
    enum class ThermalState {
        NORMAL,         // < 40°C - Full speed
        WARM,           // 40-45°C - Slight reduction
        HOT,            // 45-55°C - Significant reduction
        CRITICAL,       // 55-60°C - Minimum threads
        EMERGENCY       // > 60°C - Stop mining
    };
    
    struct ThermalConfig {
        float normalThreshold{40.0f};      // Below this: full speed
        float warmThreshold{45.0f};        // Below this: slight throttle
        float hotThreshold{55.0f};         // Below this: moderate throttle
        float criticalThreshold{60.0f};    // Below this: severe throttle
        // Above critical: emergency stop
        
        int normalThreads{8};              // Max threads at normal temp
        int warmThreads{4};                // Threads at warm temperature
        int hotThreads{2};                 // Threads at hot temperature
        int criticalThreads{1};            // Threads at critical temperature
        
        int checkIntervalMs{5000};         // How often to check temperature
        int coolDownThresholdMs{30000};    // Time to wait before increasing threads
        
        float miningIntensityNormal{1.0f};    // 100% intensity
        float miningIntensityWarm{0.75f};     // 75% intensity
        float miningIntensityHot{0.50f};      // 50% intensity
        float miningIntensityCritical{0.25f};  // 25% intensity
    };
    
    static ThermalManager& instance();
    
    /**
     * Initialize the thermal manager
     */
    bool initialize();
    
    /**
     * Start thermal monitoring in background thread
     */
    void startMonitoring();
    
    /**
     * Stop thermal monitoring
     */
    void stopMonitoring();
    
    /**
     * Update current CPU temperature
     */
    void updateTemperature(float celsius);
    
    /**
     * Get current CPU temperature
     */
    float getTemperature() const;
    
    /**
     * Get current thermal state
     */
    ThermalState getThermalState() const;
    
    /**
     * Check if thermal throttling is active
     */
    bool isThrottling() const;
    
    /**
     * Get recommended thread count based on temperature
     */
    int getRecommendedThreads(int requestedThreads) const;
    
    /**
     * Get mining intensity (0.0 to 1.0) based on temperature
     */
    float getMiningIntensity() const;
    
    /**
     * Set custom thermal configuration
     */
    void setConfig(const ThermalConfig& config);
    
    /**
     * Register callback for thermal state changes
     */
    using ThermalCallback = std::function<void(ThermalState state, float temperature, int recommendedThreads)>;
    void setThermalCallback(ThermalCallback callback);
    
    /**
     * Check if mining should continue
     */
    bool canContinueMining() const;
    
    /**
     * Get temperature history
     */
    std::vector<float> getTemperatureHistory() const;
    
    /**
     * Get average temperature over last N samples
     */
    float getAverageTemperature(int samples = 10) const;
    
private:
    ThermalManager() = default;
    ~ThermalManager();
    
    ThermalManager(const ThermalManager&) = delete;
    ThermalManager& operator=(const ThermalManager&) = delete;
    
    /**
     * Determine thermal state from temperature
     */
    ThermalState determineState(float temperature) const;
    
    /**
     * Try to read temperature from Android system files
     */
    float readSystemTemperature();
    
    /**
     * Monitoring thread function
     */
    void monitoringLoop();
    
    // Configuration
    ThermalConfig m_config;
    
    // Current state
    std::atomic<float> m_currentTemperature{35.0f};
    std::atomic<ThermalState> m_thermalState{ThermalState::NORMAL};
    std::atomic<bool> m_isThrottling{false};
    std::atomic<bool> m_monitoring{false};
    
    // Temperature history (last 60 samples, 5 minutes at 5s interval)
    static constexpr size_t MAX_HISTORY = 60;
    std::vector<float> m_temperatureHistory;
    std::mutex m_historyMutex;
    
    // Monitoring thread
    std::thread m_monitorThread;
    
    // Callback
    ThermalCallback m_thermalCallback;
    std::mutex m_callbackMutex;
    
    // Cooldown tracking
    std::chrono::steady_clock::time_point m_lastThrottleTime;
    std::chrono::steady_clock::time_point m_lastTemperatureIncrease;
    
    // Thread safety
    std::mutex m_stateMutex;
};

// JNI functions for thermal management
extern "C" {
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeInitThermalManager(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeUpdateThermalState(
        JNIEnv* env, jobject thiz, jfloat temperature);
    
    JNIEXPORT jint JNICALL
    Java_com_monerominer_MinerService_nativeGetThermalThreadLimit(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jboolean JNICALL
    Java_com_monerominer_MinerService_nativeIsThermalThrottling(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jfloat JNICALL
    Java_com_monerominer_MinerService_nativeGetCPUTemperature(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeSetThermalConfig(
        JNIEnv* env, jobject thiz, jfloat normal, jfloat warm, 
        jfloat hot, jfloat critical);
}

#endif // ANDROID
