package com.example.diddy_niel

import android.annotation.SuppressLint
import android.app.Application
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.content.edit
import androidx.lifecycle.AndroidViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlin.concurrent.thread

class BleViewModel(application: Application) : AndroidViewModel(application) {

    private val sharedPrefs = application.getSharedPreferences("ble_prefs", Context.MODE_PRIVATE)
    private val mainHandler = Handler(Looper.getMainLooper())
    
    var bleUartClient by mutableStateOf<BleUartClient?>(null)
        private set

    private val _isScanning = MutableStateFlow(false)
    val isScanning: StateFlow<Boolean> = _isScanning.asStateFlow()

    private var mockTemperature by mutableStateOf(25.0)
    var isDebugMode by mutableStateOf(false)
        private set

    private var connectionTimeoutRunnable: Runnable? = null

    companion object {
        private const val PREF_MAC_KEY = "last_device_mac"
        private const val TARGET_DEVICE_NAME = "ESP32-C3-NUS"
    }

    fun setDebug(debug: Boolean) {
        isDebugMode = debug
    }

    fun initializeBluetooth() {
        if (isDebugMode) return
        
        val manager = getApplication<Application>().getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = manager.adapter ?: return

        stopScan()
        cancelConnectionTimeout()

        val cachedMac = sharedPrefs.getString(PREF_MAC_KEY, null)
        if (cachedMac != null) {
            try {
                connectToDevice(adapter.getRemoteDevice(cachedMac))
            } catch (e: Exception) {
                sharedPrefs.edit { remove(PREF_MAC_KEY) }
                startScan(adapter)
            }
        } else {
            startScan(adapter)
        }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            @SuppressLint("MissingPermission")
            val deviceName = device.name ?: result.scanRecord?.deviceName ?: "Unknown"
            
            val hasUartService = result.scanRecord?.serviceUuids?.any { 
                it.uuid == BleUartClient.SERVICE_UUID 
            } ?: false

            if (deviceName == TARGET_DEVICE_NAME || hasUartService) {
                stopScan()
                sharedPrefs.edit { putString(PREF_MAC_KEY, device.address) }
                connectToDevice(device)
            }
        }
        override fun onScanFailed(errorCode: Int) {
            _isScanning.value = false
        }
    }

    @SuppressLint("MissingPermission")
    fun startScan(adapter: BluetoothAdapter) {
        if (_isScanning.value || isDebugMode) return
        val scanner = adapter.bluetoothLeScanner ?: return
        _isScanning.value = true
        
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(BleUartClient.SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        try {
            scanner.startScan(listOf(filter), settings, scanCallback)
            mainHandler.postDelayed({ if (_isScanning.value) stopScan() }, 10000)
        } catch (e: SecurityException) {
            _isScanning.value = false
        }
    }

    @SuppressLint("MissingPermission")
    fun stopScan() {
        if (!_isScanning.value) return
        val manager = getApplication<Application>().getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val scanner = manager.adapter?.bluetoothLeScanner ?: return
        try {
            scanner.stopScan(scanCallback)
        } catch (e: Exception) {
            Log.e("BleViewModel", "Stop scan error: ${e.message}")
        } finally {
            _isScanning.value = false
            mainHandler.removeCallbacksAndMessages(null)
        }
    }

    private fun connectToDevice(device: android.bluetooth.BluetoothDevice) {
        bleUartClient?.close()
        cancelConnectionTimeout()
        
        val client = BleUartClient(getApplication(), device)
        client.onConnectionFailed = {
            sharedPrefs.edit { remove(PREF_MAC_KEY) }
            cancelConnectionTimeout()
        }
        bleUartClient = client

        connectionTimeoutRunnable = Runnable {
            if (client.receivedData.value == "Connecting...") {
                client.close()
                sharedPrefs.edit { remove(PREF_MAC_KEY) }
            }
        }
        mainHandler.postDelayed(connectionTimeoutRunnable!!, 10000)

        thread { client.connect() }
    }

    fun setHotLimit(limit: String) {
        if (isDebugMode) {
            mockTemperature = limit.toDoubleOrNull() ?: 45.0
        } else {
            sendCommand("SETHOT $limit")
        }
    }

    fun setColdLimit(limit: String) {
        if (isDebugMode) {
            mockTemperature = limit.toDoubleOrNull() ?: 18.0
        } else {
            sendCommand("SETCOLD $limit")
        }
    }

    fun refresh(currentStatus: String) {
        if (isDebugMode) {
            mockTemperature += (Math.random() - 0.5)
        } else {
            val isNotConnected = bleUartClient == null || 
                                 currentStatus == "Disconnected" || 
                                 currentStatus == "Ready to Connect" ||
                                 currentStatus == "Searching..." ||
                                 currentStatus == "Connecting..."
            
            if (isNotConnected) {
                initializeBluetooth()
            } else {
                sendCommand("GET")
            }
        }
    }

    fun getDisplayStatus(bluetoothData: String): String {
        return if (isDebugMode) "%.1f".format(mockTemperature) else bluetoothData
    }

    private fun sendCommand(command: String) {
        thread {
            bleUartClient?.send(command)
            if (command.startsWith("SET")) {
                Thread.sleep(200)
                bleUartClient?.send("GET")
            }
        }
    }

    private fun cancelConnectionTimeout() {
        connectionTimeoutRunnable?.let { mainHandler.removeCallbacks(it) }
        connectionTimeoutRunnable = null
    }

    override fun onCleared() {
        super.onCleared()
        stopScan()
        bleUartClient?.close()
    }
}
