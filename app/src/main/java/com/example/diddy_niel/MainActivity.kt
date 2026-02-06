package com.example.diddy_niel

import android.Manifest
import android.bluetooth.BluetoothManager
import android.os.Build
import android.os.Bundle
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
    private val DEBUG_MODE = false // Toggle this for real Bluetooth or Mock data
    private var mockTemperature = 25.0
    private var mockIsOn = true

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            val granted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                permissions[Manifest.permission.BLUETOOTH_CONNECT] == true &&
                permissions[Manifest.permission.BLUETOOTH_SCAN] == true
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

        if (!DEBUG_MODE) {
            val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                arrayOf(Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN)
            } else {
                arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
            }
            requestPermissionLauncher.launch(permissions)
        }

        setContent {
            DiddynielTheme {
                val snackbarHostState = remember { SnackbarHostState() }
                
                // Collect state from Bluetooth flow if available
                val bluetoothData by bluetooth?.receivedData?.collectAsState("Searching...") ?: remember { mutableStateOf("No Device Found") }
                
                Scaffold(
                    topBar = { TopAppBar(title = { Text("Thermostat Controller ${if (DEBUG_MODE) "(MOCK)" else ""}") }) },
                    snackbarHost = { SnackbarHost(snackbarHostState) }
                ) { padding ->
                    ThermostatScreen(
                        modifier = Modifier.padding(padding),
                        statusText = if (DEBUG_MODE) (if (mockIsOn) "%.1f".format(mockTemperature) else "OFF") else bluetoothData,
                        onSetHot = { 
                            if (DEBUG_MODE) {
                                mockTemperature = 45.0
                            } else {
                                thread { bluetooth?.send("HOT") }
                            }
                        },
                        onSetCold = { 
                            if (DEBUG_MODE) {
                                mockTemperature = 18.0
                            } else {
                                thread { bluetooth?.send("COLD") } 
                            }
                        },
                        onTurnOn = {
                            if (DEBUG_MODE) {
                                mockIsOn = true
                            } else {
                                thread { bluetooth?.send("ON") }
                            }
                        },
                        onTurnOff = {
                            if (DEBUG_MODE) {
                                mockIsOn = false
                            } else {
                                thread { bluetooth?.send("OFF") }
                            }
                        },
                        onRefresh = {
                            if (DEBUG_MODE) {
                                // In mock mode, we just trigger a UI update indirectly
                                mockTemperature += (Math.random() - 0.5)
                            } else {
                                thread { bluetooth?.send("GET") }
                            }
                        }
                    )
                }
            }
        }
    }

    private fun initializeBluetooth() {
        try {
            val manager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
            val adapter = manager.adapter
            // Note: ESP32 C3 should be paired in Android Settings first
            val device = adapter?.bondedDevices?.firstOrNull()

            if (device != null) {
                val bt = BluetoothNiNiel(this, device)
                bluetooth = bt
                thread {
                    try {
                        bt.connect()
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }
            }
        } catch (e: SecurityException) {
            e.printStackTrace()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
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
    
    val isOn = statusText != "OFF" && statusText != "Disconnected" && statusText != "Searching..." && statusText != "No Device Found"

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
            Text("Refresh")
        }
    }
}
