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
            // On Android 12+, check if BLUETOOTH_CONNECT was granted
            val granted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                permissions[Manifest.permission.BLUETOOTH_CONNECT] == true
            } else {
                true // Permissions are handled by manifest on older versions
            }

            if (granted) {
                initializeBluetooth()
            }
        }

    @OptIn(ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        // Version check for runtime permissions
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            requestPermissionLauncher.launch(
                arrayOf(
                    Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.BLUETOOTH_SCAN
                )
            )
        } else {
            // Android 11 and below don't need these runtime permissions
            initializeBluetooth()
        }

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
                bluetooth = BluetoothNiNiel(device)
                thread {
                    try {
                        bluetooth?.connect()
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }
            }
        } catch (e: SecurityException) {
            // This can still happen if permissions aren't declared in Manifest
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
    var temperature by remember { mutableStateOf("Loading...") }

    LaunchedEffect(Unit) {
        onRefresh { newTemp -> temperature = newTemp }
    }

    Column(
        modifier = modifier.fillMaxSize(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(text = "Current Temperature", style = MaterialTheme.typography.headlineSmall)
        Text(text = temperature, style = MaterialTheme.typography.displayLarge)
        Spacer(modifier = Modifier.height(32.dp))
        Row(
            modifier = Modifier.padding(16.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Button(onClick = onDecrease) { Text("-") }
            Button(onClick = onIncrease) { Text("+") }
        }
        Button(onClick = { onRefresh { newTemp -> temperature = newTemp } }) {
            Text("Refresh")
        }
    }
}
