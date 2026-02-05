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

    private var bluetooth: BluetoothNiNiel? = null
    private val DEBUG_MODE = true // Toggle this for real Bluetooth or Mock data
    private var mockTemperature = 25.0

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            val granted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                permissions[Manifest.permission.BLUETOOTH_CONNECT] == true
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
                Scaffold(
                    topBar = { TopAppBar(title = { Text("Thermostat Controller ${if (DEBUG_MODE) "(MOCK)" else ""}") }) },
                    snackbarHost = { SnackbarHost(snackbarHostState) }
                ) { padding ->
                    ThermostatScreen(
                        modifier = Modifier.padding(padding),
                        onIncrease = { 
                            if (DEBUG_MODE) {
                                mockTemperature += 0.5
                            } else {
                                thread { bluetooth?.send("UP") }
                            }
                        },
                        onDecrease = { 
                            if (DEBUG_MODE) {
                                mockTemperature -= 0.5
                            } else {
                                thread { bluetooth?.send("DOWN") } 
                            }
                        },
                        onRefresh = { update ->
                            if (DEBUG_MODE) {
                                // Simulate network/bluetooth delay
                                thread {
                                    Thread.sleep(500)
                                    update("%.1f".format(mockTemperature))
                                }
                            } else {
                                thread {
                                    try {
                                        bluetooth?.send("GET")
                                        val response = bluetooth?.read() ?: "No connection"
                                        update(response)
                                    } catch (e: Exception) {
                                        e.printStackTrace()
                                    }
                                }
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
            val device = adapter?.bondedDevices?.firstOrNull()

            if (device != null) {
                bluetooth = BluetoothNiNiel(this, device)
                thread {
                    try {
                        bluetooth?.connect()
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }
            }
        } catch (e: SecurityException) {
            e.printStackTrace()
        }
    }
}

@Composable
fun ThermostatScreen(
    modifier: Modifier = Modifier,
    onIncrease: () -> Unit,
    onDecrease: () -> Unit,
    onRefresh: ((String) -> Unit) -> Unit
) {
    var temperatureText by remember { mutableStateOf("Loading...") }
    
    val currentTemp = remember(temperatureText) {
        temperatureText.filter { it.isDigit() || it == '.' }.toDoubleOrNull()
    }

    val minLimit = 15.0
    val maxLimit = 60.0

    LaunchedEffect(Unit) {
        onRefresh { newTemp -> temperatureText = newTemp }
    }

    Column(
        modifier = modifier.fillMaxSize(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Dynamic Icon based on temperature
        val (icon, iconColor) = when {
            currentTemp == null -> Icons.Default.Thermostat to Color.Gray
            currentTemp <= 20.0 -> Icons.Default.AcUnit to Color(0xFF03A9F4) // Blue/Ice
            currentTemp >= 40.0 -> Icons.Default.Whatshot to Color(0xFFFF5722) // Orange/Hot
            else -> Icons.Default.Thermostat to Color(0xFF4CAF50) // Green/Normal
        }

        Icon(
            imageVector = icon,
            contentDescription = null,
            modifier = Modifier.size(100.dp),
            tint = iconColor
        )

        Spacer(modifier = Modifier.height(16.dp))

        Text(text = "Current Temperature", style = MaterialTheme.typography.headlineSmall)
        
        val displayTemperature = if (currentTemp != null) "$temperatureText°C" else temperatureText
        Text(text = displayTemperature, style = MaterialTheme.typography.displayLarge)
        
        Spacer(modifier = Modifier.height(32.dp))
        
        Row(
            modifier = Modifier.padding(16.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Button(
                onClick = {
                    onDecrease()
                    // Immediately refresh view in mock mode for better UX
                    onRefresh { newTemp -> temperatureText = newTemp }
                },
                enabled = currentTemp != null && currentTemp > minLimit
            ) { Text("-") }
            
            Button(
                onClick = {
                    onIncrease()
                    // Immediately refresh view in mock mode for better UX
                    onRefresh { newTemp -> temperatureText = newTemp }
                },
                enabled = currentTemp != null && currentTemp < maxLimit
            ) { Text("+") }
        }
        
        Button(onClick = { 
            temperatureText = "Loading..."
            onRefresh { newTemp -> temperatureText = newTemp } 
        }) {
            Text("Refresh")
        }

        if (currentTemp != null) {
            if (currentTemp <= minLimit) {
                Text("Minimum limit reached (15°C)", color = MaterialTheme.colorScheme.error)
            } else if (currentTemp >= maxLimit) {
                Text("Maximum limit reached (60°C)", color = MaterialTheme.colorScheme.error)
            }
        }
    }
}
