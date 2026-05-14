// FILE: MoneroRandomXMiner/android_bridge.cpp

#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>

#define LOG_TAG "MoneroMinerJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Battery optimization variables
static int g_battery_level = 100;
static bool g_is_charging = true;
static bool g_low_power_mode = false;
static float g_cpu_temperature = 35.0f;
static bool g_thermal_throttled = false;

extern "C" {

// ============================================================
// Battery Monitoring
// ============================================================
JNIEXPORT void JNICALL
Java_com_monerominer_MinerService_nativeUpdateBatteryStatus(
    JNIEnv* env, jobject thiz, jint level, jboolean charging) {
    
    g_battery_level = level;
    g_is_charging = charging;
    
    // Enable low power mode if battery < 20% and not charging
    if (level < 20 && !charging) {
        g_low_power_mode = true;
        LOGW("Low battery (%d%%), enabling power saving mode", level);
    } else if (charging) {
        g_low_power_mode = false;
    }
    
    LOGI("Battery: %d%%, Charging: %s, Low Power: %s", 
         level, charging ? "Yes" : "No", 
         g_low_power_mode ? "Enabled" : "Disabled");
}

JNIEXPORT jboolean JNICALL
Java_com_monerominer_MinerService_nativeIsLowPowerMode(JNIEnv* env, jobject thiz) {
    return g_low_power_mode ? JNI_TRUE : JNI_FALSE;
}

// ============================================================
// Thermal Monitoring
// ============================================================
JNIEXPORT void JNICALL
Java_com_monerominer_MinerService_nativeUpdateTemperature(
    JNIEnv* env, jobject thiz, jfloat temperature) {
    
    g_cpu_temperature = temperature;
    
    // Thermal throttling thresholds (in Celsius)
    const float THROTTLE_WARNING = 45.0f;  // Start reducing threads
    const float THROTTLE_CRITICAL = 55.0f; // Significant throttle
    const float SHUTDOWN_TEMP = 65.0f;     // Stop mining
    
    if (temperature >= SHUTDOWN_TEMP) {
        LOGW("CRITICAL TEMPERATURE: %.1f°C - Stopping mining!", temperature);
        // Signal Java to stop mining
    } else if (temperature >= THROTTLE_CRITICAL) {
        g_thermal_throttled = true;
        LOGW("High temperature: %.1f°C - Reducing to 1 thread", temperature);
        // Reduce threads to minimum
    } else if (temperature >= THROTTLE_WARNING) {
        LOGW("Elevated temperature: %.1f°C - Reducing threads", temperature);
        // Reduce threads by half
    } else {
        g_thermal_throttled = false;
    }
}

JNIEXPORT jfloat JNICALL
Java_com_monerominer_MinerService_nativeGetTemperature(JNIEnv* env, jobject thiz) {
    return g_cpu_temperature;
}

JNIEXPORT jboolean JNICALL
Java_com_monerominer_MinerService_nativeIsThermalThrottled(JNIEnv* env, jobject thiz) {
    return g_thermal_throttled ? JNI_TRUE : JNI_FALSE;
}

// ============================================================
// System Memory Info
// ============================================================
JNIEXPORT jlong JNICALL
Java_com_monerominer_MinerService_nativeGetAvailableMemory(JNIEnv* env, jobject thiz) {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<jlong>(info.freeram) * info.mem_unit;
    }
    return -1;
}

JNIEXPORT jint JNICALL
Java_com_monerominer_MinerService_nativeGetOptimalThreadCount(JNIEnv* env, jobject thiz) {
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    long available_memory = 0;
    
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        available_memory = info.freeram * info.mem_unit;
    }
    
    // RandomX needs ~2GB per thread
    long memory_per_thread = 2LL * 1024 * 1024 * 1024;
    int memory_threads = static_cast<int>(available_memory / memory_per_thread);
    
    // Consider thermal state
    if (g_thermal_throttled) {
        cpu_count = std::min(cpu_count, 2); // Max 2 threads when hot
    }
    
    // Consider battery
    if (g_low_power_mode) {
        cpu_count = std::min(cpu_count, 1); // Max 1 thread in low power
    }
    
    // Leave at least 1 core for system on devices with > 4 cores
    if (cpu_count > 4) {
        cpu_count -= 1;
    }
    
    // Don't exceed memory constraints
    int result = std::min({cpu_count, memory_threads, 8});
    result = std::max(result, 1); // At least 1 thread
    
    LOGI("Optimal threads: %d (CPU: %d, Memory: %d threads worth)", 
         result, cpu_count, memory_threads);
    
    return result;
}

} // extern "C"
#endif
