// FILE: MoneroRandomXMiner/android/app/src/main/cpp/android_battery.h

#pragma once

#ifdef ANDROID

#include <jni.h>
#include <android/log.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#define BATTERY_TAG "BatteryManager"
#define BATTERY_LOGI(...) __android_log_print(ANDROID_LOG_INFO, BATTERY_TAG, __VA_ARGS__)
#define BATTERY_LOGW(...) __android_log_print(ANDROID_LOG_WARN, BATTERY_TAG, __VA_ARGS__)
#define BATTERY_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, BATTERY_TAG, __VA_ARGS__)

/**
 * BatteryManager - Monitors battery status and optimizes mining
 * Reduces power consumption to preserve battery life
 */
class BatteryManager {
public:
    enum class BatteryState {
        CHARGING,           // Plugged in - full power
        FULL,               // 80-100% - full power
        HIGH,               // 60-80% - slight reduction
        MEDIUM,             // 40-60% - moderate reduction
        LOW,                // 20-40% - significant reduction
        CRITICAL,           // 10-20% - minimal power
        SHUTDOWN            // < 10% - stop mining
    };
    
    enum class PowerMode {
        PERFORMANCE,        // No restrictions
        BALANCED,           // Moderate optimization
        POWER_SAVING,       // Aggressive optimization
        LOW_POWER,          // Minimal power usage
        EMERGENCY           // Stop all non-essential work
    };
    
    struct BatteryConfig {
        // Battery thresholds
        float fullThreshold{80.0f};
        float highThreshold{60.0f};
        float mediumThreshold{40.0f};
        float lowThreshold{20.0f};
        float criticalThreshold{10.0f};
        
        // Mining intensity per state (0.0 - 1.0)
        float intensityFull{1.0f};
        float intensityHigh{0.85f};
        float intensityMedium{0.65f};
        float intensityLow{0.40f};
        float intensityCritical{0.15f};
        
        // Thread limits per state
        int threadsFull{8};
        int threadsHigh{6};
        int threadsMedium{4};
        int threadsLow{2};
        int threadsCritical{1};
        
        // Timing
        int checkIntervalMs{10000};         // Battery check interval
        int chargingCheckIntervalMs{30000}; // Check interval when charging
        
        // Power saving features
        bool enableScreenOffOptimization{true};
        bool enableBackgroundOptimization{true};
        bool enableNetworkOptimization{true};
        int screenOffThreadReduction{2};    // Reduce threads by this much when screen off
    };
    
    static BatteryManager& instance();
    
    /**
     * Initialize battery manager
     */
    bool initialize();
    
    /**
     * Start battery monitoring
     */
    void startMonitoring();
    
    /**
     * Stop battery monitoring
     */
    void stopMonitoring();
    
    /**
     * Update battery status
     */
    void updateBatteryStatus(int level, bool isCharging);
    
    /**
     * Get current battery level (0-100)
     */
    int getBatteryLevel() const;
    
    /**
     * Check if device is charging
     */
    bool isCharging() const;
    
    /**
     * Get current battery state
     */
    BatteryState getBatteryState() const;
    
    /**
     * Get current power mode
     */
    PowerMode getPowerMode() const;
    
    /**
     * Check if in low power mode
     */
    bool isLowPowerMode() const;
    
    /**
     * Get recommended thread count based on battery
     */
    int getRecommendedThreads(int requestedThreads) const;
    
    /**
     * Get mining intensity (0.0 - 1.0) based on battery
     */
    float getMiningIntensity() const;
    
    /**
     * Check if mining should continue
     */
    bool canContinueMining() const;
    
    /**
     * Set screen state (for additional optimization)
     */
    void setScreenOn(bool screenOn);
    
    /**
     * Check if screen is on
     */
    bool isScreenOn() const;
    
    /**
     * Set custom battery configuration
     */
    void setConfig(const BatteryConfig& config);
    
    /**
     * Get battery configuration
     */
    BatteryConfig getConfig() const;
    
    /**
     * Register callback for battery state changes
     */
    using BatteryCallback = std::function<void(BatteryState state, int level, 
                                               bool charging, int recommendedThreads)>;
    void setBatteryCallback(BatteryCallback callback);
    
    /**
     * Get estimated mining time remaining (in minutes)
     * Based on current battery level and consumption rate
     */
    int getEstimatedMiningTimeRemaining() const;
    
    /**
     * Get battery consumption rate per hour (estimated)
     */
    float getConsumptionRate() const;
    
private:
    BatteryManager() = default;
    ~BatteryManager();
    
    BatteryManager(const BatteryManager&) = delete;
    BatteryManager& operator=(const BatteryManager&) = delete;
    
    /**
     * Determine battery state from level and charging status
     */
    BatteryState determineState(int level, bool charging) const;
    
    /**
     * Determine power mode from state and screen state
     */
    PowerMode determinePowerMode(BatteryState state, bool screenOn) const;
    
    /**
     * Monitoring thread function
     */
    void monitoringLoop();
    
    // Configuration
    BatteryConfig m_config;
    
    // Current state
    std::atomic<int> m_batteryLevel{100};
    std::atomic<bool> m_isCharging{true};
    std::atomic<bool> m_screenOn{true};
    std::atomic<BatteryState> m_batteryState{BatteryState::FULL};
    std::atomic<PowerMode> m_powerMode{PowerMode::PERFORMANCE};
    std::atomic<bool> m_lowPowerMode{false};
    std::atomic<bool> m_monitoring{false};
    
    // Battery history for consumption calculation
    struct BatterySample {
        int level;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::vector<BatterySample> m_batteryHistory;
    std::mutex m_historyMutex;
    static constexpr size_t MAX_HISTORY = 100;
    
    // Monitoring thread
    std::thread m_monitorThread;
    
    // Callback
    BatteryCallback m_batteryCallback;
    std::mutex m_callbackMutex;
    
    // Thread safety
    std::mutex m_stateMutex;
    
    // Mining statistics for battery estimation
    std::atomic<float> m_consumptionRate{0.0f}; // % per hour
    std::chrono::steady_clock::time_point m_miningStartTime;
};

// JNI functions for battery management
extern "C" {
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeInitBatteryManager(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeUpdateBatteryStatus(
        JNIEnv* env, jobject thiz, jint level, jboolean isCharging);
    
    JNIEXPORT jint JNICALL
    Java_com_monerominer_MinerService_nativeGetBatteryLevel(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jboolean JNICALL
    Java_com_monerominer_MinerService_nativeIsCharging(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jboolean JNICALL
    Java_com_monerominer_MinerService_nativeIsLowPowerMode(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jint JNICALL
    Java_com_monerominer_MinerService_nativeGetMiningIntensity(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jint JNICALL
    Java_com_monerominer_MinerService_nativeGetEstimatedMiningTime(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeSetScreenState(
        JNIEnv* env, jobject thiz, jboolean screenOn);
    
    JNIEXPORT void JNICALL
    Java_com_monerominer_MinerService_nativeSetBatteryConfig(
        JNIEnv* env, jobject thiz, jfloat fullThreshold, jfloat highThreshold,
        jfloat mediumThreshold, jfloat lowThreshold);
}

#endif // ANDROID
