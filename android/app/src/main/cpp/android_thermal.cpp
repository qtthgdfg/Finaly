// FILE: MoneroRandomXMiner/android_thermal.cpp

#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#include <thread>
#include <chrono>
#include <atomic>

#define LOG_TAG "ThermalManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// Thermal management state
static std::atomic<float> g_cpu_temperature{35.0f};
static std::atomic<bool> g_thermal_throttling{false};
static std::atomic<int> g_thermal_thread_limit{8};

// Temperature thresholds (Celsius)
constexpr float TEMP_NORMAL = 35.0f;
constexpr float TEMP_WARM = 45.0f;
constexpr float TEMP_HOT = 55.0f;
constexpr float TEMP_CRITICAL = 65.0f;

extern "C" {

JNIEXPORT void JNICALL
Java_com_monerominer_MinerService_nativeUpdateThermalState(
    JNIEnv* env, jobject thiz, jfloat temperature) {
    
    g_cpu_temperature = temperature;
    
    if (temperature >= TEMP_CRITICAL) {
        g_thermal_throttling = true;
        g_thermal_thread_limit = 0; // Stop mining
        LOGW("CRITICAL: %.1f°C - Stopping all mining threads", temperature);
        
    } else if (temperature >= TEMP_HOT) {
        g_thermal_throttling = true;
        g_thermal_thread_limit = 1; // Single thread only
        LOGW("HOT: %.1f°C - Limiting to 1 thread", temperature);
        
    } else if (temperature >= TEMP_WARM) {
        g_thermal_throttling = true;
        g_thermal_thread_limit = 2; // Reduced threads
        LOGW("WARM: %.1f°C - Limiting to 2 threads", temperature);
        
    } else {
        g_thermal_throttling = false;
        g_thermal_thread_limit = 8; // Normal operation
    }
}

JNIEXPORT jint JNICALL
Java_com_monerominer_MinerService_nativeGetThermalThreadLimit(
    JNIEnv* env, jobject thiz) {
    return g_thermal_thread_limit;
}

JNIEXPORT jboolean JNICALL
Java_com_monerominer_MinerService_nativeIsThermalThrottling(
    JNIEnv* env, jobject thiz) {
    return g_thermal_throttling ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloat JNICALL
Java_com_monerominer_MinerService_nativeGetCPUTemperature(
    JNIEnv* env, jobject thiz) {
    return g_cpu_temperature;
}

} // extern "C"
#endif
