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
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.example.diddy_niel.ui.theme.DiddynielTheme
import kotlin.concurrent.thread

class MainActivity : ComponentActivity() {

    private var bluetooth: BluetoothNiNiel? = null

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            val granted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                permissions[Manifest.permission.BLUETOOTH_CONNECT] == true
            } else {
                // On older versions, location is also needed for BLE
                permissions[Manifest.permission.ACCESS_FINE_LOCATION] == true
            }

            if (granted) {
                initializeBluetooth()
            }
        }

    @OptIn(ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
        }
        requestPermissionLauncher.launch(permissions)

        setContent {
            DiddynielTheme {
                val snackbarHostState = remember { SnackbarHostState() }
                Scaffold(
                    topBar = { TopAppBar(title = { Text("Thermostat Controller") }) },
                    snackbarHost = { SnackbarHost(snackbarHostState) }
                ) { padding ->
                    ThermostatScreen(
                        modifier = Modifier.padding(padding),
                        onIncrease = { thread { bluetooth?.send("UP") } },
                        onDecrease = { thread { bluetooth?.send("DOWN") } },
                        onRefresh = { update ->
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
                // Context is now required for BLE
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
    
    // Parse numeric value to enforce limits
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
        Text(text = "Current Temperature", style = MaterialTheme.typography.headlineSmall)
        Text(text = if (temperatureText.isEmpty()) "..." else temperatureText, 
             style = MaterialTheme.typography.displayLarge)
        
        Spacer(modifier = Modifier.height(32.dp))
        
        Row(
            modifier = Modifier.padding(16.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Disable Decrease button if at or below 15C
            Button(
                onClick = onDecrease,
                enabled = currentTemp == null || currentTemp > minLimit
            ) { Text("-") }
            
            // Disable Increase button if at or above 60C
            Button(
                onClick = onIncrease,
                enabled = currentTemp == null || currentTemp < maxLimit
            ) { Text("+") }
        }
        
        Button(onClick = { onRefresh { newTemp -> temperatureText = newTemp } }) {
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
