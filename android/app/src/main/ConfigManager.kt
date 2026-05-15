package com.monerominer

import android.content.Context
import com.google.gson.Gson

// FIX: MinerConfig must implement Serializable so it can be passed
//      as an Intent extra in MinerViewModel.startMining().
data class SubaddressConfig(
    val enabled: Boolean = false,
    val miningSubaddress: String = "",
    val label: String = "Mining Rig 1",
    val rotateSubaddress: Boolean = false,
    val rotationIntervalDays: Int = 30,
    val additionalRigs: Map<String, String> = emptyMap()
) : java.io.Serializable

data class MinerConfig(
    val poolHost: String = "pool.supportxmr.com",
    val poolPort: Int = 3333,
    val wallet: String = "",
    val worker: String = "android_miner",
    val threads: Int = 2,
    val password: String = "x",
    val subaddress: SubaddressConfig = SubaddressConfig()
) : java.io.Serializable {

    fun getRigNames(): List<String> =
        listOf("default") + subaddress.additionalRigs.keys.toList()

    fun getWorkerName(rigName: String): String =
        if (rigName == "default") worker else "${worker}_$rigName"
}

object ConfigManager {
    private const val PREFS_NAME = "miner_config"
    private const val KEY_CONFIG = "full_config"
    private val gson = Gson()

    fun saveConfig(context: Context, config: MinerConfig) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putString(KEY_CONFIG, gson.toJson(config))
            .apply()
    }

    fun getConfig(context: Context): MinerConfig {
        val json = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .getString(KEY_CONFIG, null) ?: return MinerConfig()
        return try {
            gson.fromJson(json, MinerConfig::class.java)
        } catch (e: Exception) {
            MinerConfig()
        }
    }

    fun importConfigFromJson(context: Context, jsonString: String) {
        try {
            saveConfig(context, gson.fromJson(jsonString, MinerConfig::class.java))
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
