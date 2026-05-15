package com.monerominer

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.util.Log
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

class MinerService : Service() {

    private var wakeLock: PowerManager.WakeLock? = null
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var isMining = false

    companion object {
        const val CHANNEL_ID       = "mining_channel"
        const val NOTIFICATION_ID  = 1001
        const val ACTION_START     = "com.monerominer.START"
        const val ACTION_STOP      = "com.monerominer.STOP"
        const val ACTION_PAUSE     = "com.monerominer.PAUSE"
        const val EXTRA_CONFIG     = "extra_config"

        // Load native library once
        init {
            try {
                System.loadLibrary("monerominer")
                Log.d("MinerService", "Native library loaded")
            } catch (e: UnsatisfiedLinkError) {
                Log.e("MinerService", "Failed to load native library: ${e.message}")
            }
        }
    }

    // ── JNI declarations ──────────────────────────────────────
    private external fun nativeStartMining(config: MinerConfig, callback: Any): Boolean
    private external fun nativeStopMining()
    private external fun nativeGetStats(): String

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        Log.d("MinerService", "Service created")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> {
                // FIX: extract the config and actually start the native miner
                @Suppress("DEPRECATION")
                val config = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    intent.getSerializableExtra(EXTRA_CONFIG, MinerConfig::class.java)
                } else {
                    intent.getSerializableExtra(EXTRA_CONFIG) as? MinerConfig
                }
                startMining(config)
            }
            ACTION_STOP  -> stopMining()
            ACTION_PAUSE -> pauseMining()
        }
        return START_STICKY
    }

    // ── Mining lifecycle ──────────────────────────────────────

    private fun startMining(config: MinerConfig?) {
        if (isMining) return
        if (config == null) {
            Log.e("MinerService", "startMining called with null config")
            return
        }

        acquireWakeLock()
        startForeground(NOTIFICATION_ID, buildNotification("Starting miner…"))

        scope.launch {
            try {
                // FIX: call native start on IO thread (blocks until pool responds)
                val started = nativeStartMining(config, this@MinerService)
                if (started) {
                    isMining = true
                    Log.d("MinerService", "Native miner started")
                    updateNotificationLoop()
                } else {
                    Log.e("MinerService", "Native miner failed to start")
                    stopSelf()
                }
            } catch (e: Exception) {
                Log.e("MinerService", "Error starting native miner: ${e.message}")
                stopSelf()
            }
        }
    }

    private suspend fun updateNotificationLoop() {
        while (isMining && scope.isActive) {
            try {
                val stats = nativeGetStats()
                // Parse hashrate from JSON for notification
                val hr = Regex("\"hashrate\":(\\d+\\.?\\d*)").find(stats)
                    ?.groupValues?.get(1)?.toDoubleOrNull() ?: 0.0
                val statusText = "Mining – ${formatHashrate(hr)}"
                val nm = getSystemService(NotificationManager::class.java)
                nm.notify(NOTIFICATION_ID, buildNotification(statusText))
            } catch (_: Exception) {}
            delay(5_000)
        }
    }

    private fun stopMining() {
        isMining = false
        try { nativeStopMining() } catch (e: Exception) {
            Log.e("MinerService", "Error stopping native miner: ${e.message}")
        }
        releaseWakeLock()
        scope.cancel()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
        Log.d("MinerService", "Mining stopped")
    }

    private fun pauseMining() {
        isMining = false
        try { nativeStopMining() } catch (_: Exception) {}
        releaseWakeLock()
        Log.d("MinerService", "Mining paused")
    }

    // ── Helpers ───────────────────────────────────────────────

    private fun acquireWakeLock() {
        try {
            val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
            wakeLock = pm.newWakeLock(
                PowerManager.PARTIAL_WAKE_LOCK,
                "MoneroMiner::MiningWakeLock"
            ).also { it.acquire(10 * 60 * 1000L) }
        } catch (e: Exception) {
            Log.e("MinerService", "Failed to acquire wake lock: ${e.message}")
        }
    }

    private fun releaseWakeLock() {
        try { wakeLock?.release() } catch (e: Exception) {
            Log.e("MinerService", "Failed to release wake lock: ${e.message}")
        }
        wakeLock = null
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID, "Mining Status", NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Shows mining status and controls"
                setShowBadge(false)
                enableLights(false)
                enableVibration(false)
            }
            getSystemService(NotificationManager::class.java)
                .createNotificationChannel(channel)
        }
    }

    private fun buildNotification(status: String): Notification {
        val openIntent   = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)

        val stopPending  = PendingIntent.getService(
            this, 1,
            Intent(this, MinerService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Monero Miner")
            .setContentText(status)
            .setSmallIcon(android.R.drawable.ic_menu_compass)
            .setContentIntent(openIntent)
            .addAction(android.R.drawable.ic_media_pause, "Stop", stopPending)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    private fun formatHashrate(hps: Double) = when {
        hps >= 1_000_000.0 -> "%.2f MH/s".format(hps / 1_000_000)
        hps >= 1_000.0     -> "%.2f KH/s".format(hps / 1_000)
        else               -> "%.0f H/s".format(hps)
    }

    override fun onDestroy() {
        stopMining()
        super.onDestroy()
    }
}
