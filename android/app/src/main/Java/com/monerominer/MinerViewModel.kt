// FILE: MoneroRandomXMiner/android/app/src/main/java/com/monerominer/MinerViewModel.kt

package com.monerominer

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

data class MinerUIState(
    val isMining: Boolean = false,
    val status: String = "Stopped",
    val hashrate: Double = 0.0,
    val hashrateFormatted: String = "0 H/s",
    val acceptedShares: Int = 0,
    val rejectedShares: Int = 0,
    val uptime: Long = 0L,
    val uptimeFormatted: String = "00:00:00",
    val poolHost: String = "",
    val poolPort: Int = 3333,
    val wallet: String = "",
    val worker: String = "android_miner",
    val threads: Int = 2,
    val errors: List<String> = emptyList(),
    val logs: List<String> = emptyList(),
    // Subaddress fields
    val subaddressEnabled: Boolean = false,
    val miningSubaddress: String = "",
    val subaddressLabel: String = "Mining Rig 1",
    val rotateSubaddress: Boolean = false,
    val rotationIntervalDays: Int = 30,
    val additionalRigs: Map<String, String> = emptyMap(),
    val selectedRig: String = "default",
    val availableRigs: List<String> = listOf("default")
)

class MinerViewModel(application: Application) : AndroidViewModel(application) {
    
    private val _uiState = MutableStateFlow(MinerUIState())
    val uiState: StateFlow<MinerUIState> = _uiState.asStateFlow()
    
    private val _toastMessage = MutableLiveData<String>()
    val toastMessage: LiveData<String> = _toastMessage
    
    init {
        loadSavedConfig()
    }
    
    fun loadSavedConfig() {
        val config = ConfigManager.getConfig(getApplication())
        _uiState.value = _uiState.value.copy(
            poolHost = config.poolHost,
            poolPort = config.poolPort,
            wallet = config.wallet,
            worker = config.worker,
            threads = config.threads,
            subaddressEnabled = config.subaddress.enabled,
            miningSubaddress = config.subaddress.miningSubaddress,
            subaddressLabel = config.subaddress.label,
            rotateSubaddress = config.subaddress.rotateSubaddress,
            rotationIntervalDays = config.subaddress.rotationIntervalDays,
            additionalRigs = config.subaddress.additionalRigs,
            availableRigs = config.getRigNames(),
            selectedRig = "default"
        )
    }
    
    /**
     * Import config from JSON string (like the one provided)
     */
    fun importConfigFromJson(jsonString: String) {
        ConfigManager.importConfigFromJson(getApplication(), jsonString)
        loadSavedConfig()
        addLog("✅ Configuration imported successfully")
    }
    
    /**
     * Get the active mining address based on subaddress settings
     */
    fun getActiveMiningAddress(): String {
        val config = ConfigManager.getConfig(getApplication())
        val selectedRig = _uiState.value.selectedRig
        
        return if (selectedRig == "default") {
            if (config.subaddress.enabled && config.subaddress.miningSubaddress.isNotEmpty()) {
                config.subaddress.miningSubaddress
            } else {
                config.wallet
            }
        } else {
            config.subaddress.additionalRigs[selectedRig] ?: config.wallet
        }
    }
    
    /**
     * Select a rig from available rigs
     */
    fun selectRig(rigName: String) {
        if (rigName in _uiState.value.availableRigs) {
            _uiState.value = _uiState.value.copy(selectedRig = rigName)
            
            val activeAddress = getActiveMiningAddress()
            addLog("Selected rig: $rigName")
            addLog("Active address: ${activeAddress.take(10)}...${activeAddress.takeLast(6)}")
        }
    }
    
    /**
     * Toggle subaddress usage
     */
    fun toggleSubaddress(enabled: Boolean) {
        _uiState.value = _uiState.value.copy(subaddressEnabled = enabled)
        
        val config = ConfigManager.getConfig(getApplication())
        val updatedConfig = config.copy(
            subaddress = config.subaddress.copy(enabled = enabled)
        )
        ConfigManager.saveConfig(getApplication(), updatedConfig)
        
        if (enabled) {
            addLog("✅ Subaddress mining enabled")
        } else {
            addLog("ℹ️ Subaddress mining disabled, using main wallet")
        }
    }
    
    /**
     * Update mining subaddress
     */
    fun updateMiningSubaddress(address: String) {
        _uiState.value = _uiState.value.copy(miningSubaddress = address)
        
        val config = ConfigManager.getConfig(getApplication())
        val updatedConfig = config.copy(
            subaddress = config.subaddress.copy(miningSubaddress = address)
        )
        ConfigManager.saveConfig(getApplication(), updatedConfig)
    }
    
    /**
     * Add a new rig with its own subaddress
     */
    fun addRig(rigName: String, subaddress: String) {
        val currentRigs = _uiState.value.additionalRigs.toMutableMap()
        currentRigs[rigName] = subaddress
        
        _uiState.value = _uiState.value.copy(
            additionalRigs = currentRigs,
            availableRigs = _uiState.value.availableRigs + rigName
        )
        
        val config = ConfigManager.getConfig(getApplication())
        val updatedConfig = config.copy(
            subaddress = config.subaddress.copy(additionalRigs = currentRigs)
        )
        ConfigManager.saveConfig(getApplication(), updatedConfig)
        
        addLog("✅ Added rig: $rigName")
    }
    
    /**
     * Remove a rig
     */
    fun removeRig(rigName: String) {
        val currentRigs = _uiState.value.additionalRigs.toMutableMap()
        currentRigs.remove(rigName)
        
        _uiState.value = _uiState.value.copy(
            additionalRigs = currentRigs,
            availableRigs = _uiState.value.availableRigs - rigName,
            selectedRig = if (_uiState.value.selectedRig == rigName) "default" 
                         else _uiState.value.selectedRig
        )
        
        val config = ConfigManager.getConfig(getApplication())
        val updatedConfig = config.copy(
            subaddress = config.subaddress.copy(additionalRigs = currentRigs)
        )
        ConfigManager.saveConfig(getApplication(), updatedConfig)
        
        addLog("🗑️ Removed rig: $rigName")
    }
    
    fun updatePoolHost(host: String) {
        _uiState.value = _uiState.value.copy(poolHost = host)
    }
    
    fun updatePoolPort(port: Int) {
        _uiState.value = _uiState.value.copy(poolPort = port)
    }
    
    fun updateWallet(wallet: String) {
        _uiState.value = _uiState.value.copy(wallet = wallet)
    }
    
    fun updateWorker(worker: String) {
        _uiState.value = _uiState.value.copy(worker = worker)
    }
    
    fun updateThreads(threads: Int) {
        val cpuCount = Runtime.getRuntime().availableProcessors()
        if (threads > cpuCount) {
            _toastMessage.value = "Threads exceed CPU cores ($cpuCount)"
            return
        }
        if (threads < 1) {
            _toastMessage.value = "Minimum 1 thread required"
            return
        }
        _uiState.value = _uiState.value.copy(threads = threads)
    }
    
    fun validateConfig(): Boolean {
        val state = _uiState.value
        val activeAddress = getActiveMiningAddress()
        
        if (activeAddress.isEmpty()) {
            _toastMessage.value = "Wallet address is required"
            return false
        }
        
        if (activeAddress.length < 95) {
            _toastMessage.value = "Invalid wallet address length"
            return false
        }
        
        if (state.poolHost.isEmpty()) {
            _toastMessage.value = "Pool host is required"
            return false
        }
        
        if (state.poolPort !in 1..65535) {
            _toastMessage.value = "Invalid pool port"
            return false
        }
        
        return true
    }
    
    fun startMining() {
        if (!validateConfig()) return
        
        val config = ConfigManager.getConfig(getApplication())
        val activeAddress = getActiveMiningAddress()
        val workerName = config.getWorkerName(_uiState.value.selectedRig)
        
        val miningConfig = config.copy(
            wallet = activeAddress,
            worker = workerName
        )
        
        ConfigManager.saveConfig(getApplication(), miningConfig)
        
        _uiState.value = _uiState.value.copy(
            isMining = true,
            status = "Starting...",
            errors = emptyList(),
            wallet = activeAddress,
            worker = workerName
        )
        
        addLog("🚀 Starting miner...")
        addLog("Pool: ${miningConfig.poolHost}:${miningConfig.poolPort}")
        addLog("Active Rig: ${_uiState.value.selectedRig}")
        addLog("Wallet: ${activeAddress.take(10)}...${activeAddress.takeLast(6)}")
        addLog("Worker: $workerName")
        addLog("Threads: ${miningConfig.threads}")
        
        if (config.subaddress.enabled) {
            addLog("📋 Using subaddress: ${config.subaddress.label}")
        }
        
        val context = getApplication<Application>()
        val intent = android.content.Intent(context, MinerService::class.java).apply {
            action = MinerService.ACTION_START
            putExtra(MinerService.EXTRA_CONFIG, miningConfig)
        }
        
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            context.startForegroundService(intent)
        } else {
            context.startService(intent)
        }
        
        startMonitoring()
    }
    
    fun stopMining() {
        val intent = android.content.Intent(
            getApplication(), 
            MinerService::class.java
        ).apply {
            action = MinerService.ACTION_STOP
        }
        getApplication<Application>().startService(intent)
        
        _uiState.value = _uiState.value.copy(
            isMining = false,
            status = "Stopped",
            hashrate = 0.0,
            hashrateFormatted = "0 H/s",
            acceptedShares = 0,
            rejectedShares = 0,
            uptime = 0L,
            uptimeFormatted = "00:00:00"
        )
        
        addLog("⏹️ Miner stopped")
    }
    
    private fun startMonitoring() {
        viewModelScope.launch {
            val startTime = System.currentTimeMillis()
            
            while (_uiState.value.isMining) {
                delay(1000)
                
                val uptimeSeconds = (System.currentTimeMillis() - startTime) / 1000
                
                _uiState.value = _uiState.value.copy(
                    status = "Mining",
                    uptime = uptimeSeconds,
                    uptimeFormatted = formatUptime(uptimeSeconds),
                    hashrateFormatted = formatHashrate(_uiState.value.hashrate)
                )
            }
        }
    }
    
    fun addLog(message: String) {
        val timestamp = java.text.SimpleDateFormat(
            "HH:mm:ss", 
            java.util.Locale.getDefault()
        ).format(java.util.Date())
        
        val logEntry = "[$timestamp] $message"
        val currentLogs = _uiState.value.logs.toMutableList()
        currentLogs.add(logEntry)
        
        if (currentLogs.size > 500) {
            currentLogs.removeAt(0)
        }
        
        _uiState.value = _uiState.value.copy(logs = currentLogs)
    }
    
    private fun formatHashrate(hashesPerSecond: Double): String {
        return when {
            hashesPerSecond >= 1_000_000_000 -> 
                String.format("%.2f GH/s", hashesPerSecond / 1_000_000_000)
            hashesPerSecond >= 1_000_000 -> 
                String.format("%.2f MH/s", hashesPerSecond / 1_000_000)
            hashesPerSecond >= 1_000 -> 
                String.format("%.2f KH/s", hashesPerSecond / 1_000)
            else -> 
                String.format("%.0f H/s", hashesPerSecond)
        }
    }
    
    private fun formatUptime(totalSeconds: Long): String {
        val hours = totalSeconds / 3600
        val minutes = (totalSeconds % 3600) / 60
        val seconds = totalSeconds % 60
        return String.format("%02d:%02d:%02d", hours, minutes, seconds)
    }
}
