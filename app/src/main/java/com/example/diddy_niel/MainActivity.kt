package com.example.diddy_niel

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AcUnit
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material.icons.filled.Whatshot
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.example.diddy_niel.ui.theme.DiddynielTheme
import kotlin.concurrent.thread

class MainActivity : ComponentActivity() {

    private var bluetooth by mutableStateOf<BluetoothNiNiel?>(null)
    private val DEBUG_MODE = false
    private var mockTemperature = 25.0
    private var mockIsOn = true

    private lateinit var sharedPrefs: SharedPreferences
    private val PREF_MAC_KEY = "last_device_mac"
    private val TARGET_DEVICE_NAME = "ESP32-C3-NUS"
    private var isScanning = false
    private val mainHandler = Handler(Looper.getMainLooper())
    private var connectionTimeoutRunnable: Runnable? = null

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            val granted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                permissions[Manifest.permission.BLUETOOTH_CONNECT] == true &&
                permissions[Manifest.permission.BLUETOOTH_SCAN] == true &&
                permissions[Manifest.permission.ACCESS_FINE_LOCATION] == true
            } else {
                permissions[Manifest.permission.ACCESS_FINE_LOCATION] == true
            }

            if (granted && !DEBUG_MODE) {
                initializeBluetooth()
            }
        }

    @OptIn(ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        sharedPrefs = getSharedPreferences("ble_prefs", Context.MODE_PRIVATE)

        if (!DEBUG_MODE) {
            val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                arrayOf(
                    Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.ACCESS_COARSE_LOCATION
                )
            } else {
                arrayOf(
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.ACCESS_COARSE_LOCATION
                )
            }
            requestPermissionLauncher.launch(permissions)
        }

        setContent {
            DiddynielTheme {
                val bluetoothData by bluetooth?.receivedData?.collectAsState("Searching...") ?: remember { mutableStateOf("Ready to Connect") }
                
                Scaffold(
                    topBar = { TopAppBar(title = { Text("Thermostat Controller") }) }
                ) { padding ->
                    ThermostatScreen(
                        modifier = Modifier.padding(padding),
                        statusText = if (DEBUG_MODE) (if (mockIsOn) "%.1f".format(mockTemperature) else "OFF") else bluetoothData,
                        onSetHot = { if (DEBUG_MODE) mockTemperature = 45.0 else thread { bluetooth?.send("HOT") } },
                        onSetCold = { if (DEBUG_MODE) mockTemperature = 18.0 else thread { bluetooth?.send("COLD") } },
                        onTurnOn = { if (DEBUG_MODE) mockIsOn = true else thread { bluetooth?.send("ON") } },
                        onTurnOff = { if (DEBUG_MODE) mockIsOn = false else thread { bluetooth?.send("OFF") } },
                        onRefresh = {
                            if (DEBUG_MODE) mockTemperature += (Math.random() - 0.5)
                            else {
                                // If stuck in Connecting or Searching, allow a manual retry
                                val isNotConnected = bluetooth == null || 
                                                     bluetoothData == "Disconnected" || 
                                                     bluetoothData == "Ready to Connect" ||
                                                     bluetoothData == "Searching..." ||
                                                     bluetoothData == "Connecting..."
                                
                                if (isNotConnected) {
                                    initializeBluetooth()
                                } else {
                                    thread { bluetooth?.send("GET") }
                                }
                            }
                        }
                    )
                }
            }
        }
    }

    private fun initializeBluetooth() {
        val manager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = manager.adapter ?: return

        stopScan()
        cancelConnectionTimeout()

        val cachedMac = sharedPrefs.getString(PREF_MAC_KEY, null)
        if (cachedMac != null) {
            Log.d("MainActivity", "Found cached MAC: $cachedMac. Attempting direct connection.")
            try {
                connectToDevice(adapter.getRemoteDevice(cachedMac))
            } catch (e: Exception) {
                Log.e("MainActivity", "Cached device error: ${e.message}")
                sharedPrefs.edit().remove(PREF_MAC_KEY).apply()
                startScan(adapter)
            }
        } else {
            startScan(adapter)
        }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            @Suppress("MissingPermission")
            val deviceName = device.name ?: result.scanRecord?.deviceName ?: "Unknown"
            
            val serviceUuids = result.scanRecord?.serviceUuids
            val hasUartService = serviceUuids?.any { it.uuid == BluetoothNiNiel.SERVICE_UUID } ?: false

            Log.d("MainActivity", "Scanned: $deviceName ($hasUartService) - ${device.address}")

            if (deviceName == TARGET_DEVICE_NAME || hasUartService) {
                Log.d("MainActivity", "MATCH FOUND! Connecting to ${device.address}")
                stopScan()
                sharedPrefs.edit().putString(PREF_MAC_KEY, device.address).apply()
                connectToDevice(device)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e("MainActivity", "Scan failed with error: $errorCode")
            isScanning = false
        }
    }

    private fun startScan(adapter: BluetoothAdapter) {
        if (isScanning) return
        val scanner = adapter.bluetoothLeScanner ?: return
        
        Log.d("MainActivity", "Starting smart scan...")
        isScanning = true
        
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(BluetoothNiNiel.SERVICE_UUID))
            .build()
            
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        try {
            scanner.startScan(listOf(filter), settings, scanCallback)
            mainHandler.postDelayed({
                if (isScanning) {
                    Log.d("MainActivity", "Scan timed out.")
                    stopScan()
                }
            }, 10000)
        } catch (e: SecurityException) {
            scanner.startScan(scanCallback)
        }
    }

    private fun stopScan() {
        if (!isScanning) return
        val manager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        val scanner = manager.adapter?.bluetoothLeScanner ?: return
        
        Log.d("MainActivity", "Stopping scan.")
        try {
            scanner.stopScan(scanCallback)
        } catch (e: Exception) {
            e.printStackTrace()
        } finally {
            isScanning = false
            mainHandler.removeCallbacksAndMessages(null)
        }
    }

    private fun connectToDevice(device: android.bluetooth.BluetoothDevice) {
        bluetooth?.close()
        cancelConnectionTimeout()
        Thread.sleep(1000)
        val bt = BluetoothNiNiel(this, device)
        bt.onConnectionFailed = {
            Log.w("MainActivity", "Direct connection failed. Clearing cache.")
            sharedPrefs.edit().remove(PREF_MAC_KEY).apply()
            cancelConnectionTimeout()
        }
        bluetooth = bt

        // Set a timeout watchdog: if not connected in 10s, clear cache and reset
        connectionTimeoutRunnable = Runnable {
            if (bt.receivedData.value == "Connecting...") {
                Log.w("MainActivity", "Connection timed out. Resetting.")
                bt.close()
                sharedPrefs.edit().remove(PREF_MAC_KEY).apply()
                // Let the UI state reflect Disconnected so user can try again
            }
        }
        mainHandler.postDelayed(connectionTimeoutRunnable!!, 10000)

        thread {
            try {
                bt.connect()
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun cancelConnectionTimeout() {
        connectionTimeoutRunnable?.let { mainHandler.removeCallbacks(it) }
        connectionTimeoutRunnable = null
    }

    override fun onDestroy() {
        super.onDestroy()
        stopScan()
        cancelConnectionTimeout()
        bluetooth?.close()
    }
}

@Composable
fun ThermostatScreen(
    modifier: Modifier = Modifier,
    statusText: String,
    onSetHot: () -> Unit,
    onSetCold: () -> Unit,
    onTurnOn: () -> Unit,
    onTurnOff: () -> Unit,
    onRefresh: () -> Unit
) {
    val currentTemp = remember(statusText) {
        statusText.filter { it.isDigit() || it == '.' }.toDoubleOrNull()
    }
    
    val isOn = !statusText.contains("OFF", ignoreCase = true) && 
               !statusText.contains("Disconnected", ignoreCase = true) && 
               !statusText.contains("Searching", ignoreCase = true) && 
               !statusText.contains("No Device", ignoreCase = true) && 
               !statusText.contains("Ready to Connect", ignoreCase = true) && 
               !statusText.contains("Connecting", ignoreCase = true)

    Column(
        modifier = modifier.fillMaxSize(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        val (icon, iconColor) = when {
            !isOn -> Icons.Default.PowerSettingsNew to Color.Gray
            currentTemp == null -> Icons.Default.Thermostat to Color.Gray
            currentTemp <= 20.0 -> Icons.Default.AcUnit to Color(0xFF03A9F4)
            currentTemp >= 40.0 -> Icons.Default.Whatshot to Color(0xFFFF5722)
            else -> Icons.Default.Thermostat to Color(0xFF4CAF50)
        }

        Icon(
            imageVector = icon,
            contentDescription = null,
            modifier = Modifier.size(100.dp),
            tint = iconColor
        )

        Spacer(modifier = Modifier.height(16.dp))

        Text(text = if (isOn) "Current Temperature" else "System Status", style = MaterialTheme.typography.headlineSmall)
        
        val displayText = if (currentTemp != null) "$statusText°C" else statusText
        Text(text = displayText, style = MaterialTheme.typography.displayLarge)
        
        Spacer(modifier = Modifier.height(32.dp))
        
        Row(
            modifier = Modifier.padding(8.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Button(
                onClick = onTurnOff,
                enabled = isOn,
                colors = ButtonDefaults.buttonColors(containerColor = Color.DarkGray)
            ) { Text("OFF") }

            Button(
                onClick = onTurnOn,
                enabled = !isOn,
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF4CAF50))
            ) { Text("ON") }
        }

        Row(
            modifier = Modifier.padding(8.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Button(
                onClick = onSetCold,
                enabled = isOn,
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF03A9F4))
            ) { Text("COLD") }
            
            Button(
                onClick = onSetHot,
                enabled = isOn,
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFFF5722))
            ) { Text("HOT") }
        }
        
        Spacer(modifier = Modifier.height(16.dp))

        Button(onClick = onRefresh) {
            val isActionConnect = statusText == "Ready to Connect" || 
                                 statusText == "Disconnected" || 
                                 statusText == "Searching..." ||
                                 statusText == "Connecting..."
            
            Text(if (isActionConnect) "Connect" else "Refresh")
        }
    }
}
