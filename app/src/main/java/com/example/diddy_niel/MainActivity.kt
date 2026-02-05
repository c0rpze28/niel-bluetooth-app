package com.example.diddy_niel

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.annotation.RequiresPermission
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.example.diddy_niel.ui.theme.DiddynielTheme
import kotlin.concurrent.thread

class MainActivity : ComponentActivity() {

    private lateinit var bluetooth: BluetoothNiNiel

    @OptIn(ExperimentalMaterial3Api::class)
    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val adapter = BluetoothAdapter.getDefaultAdapter()
        val device = adapter.bondedDevices.firstOrNull()
            ?: throw IllegalStateException("No paired Bluetooth devices found")

        bluetooth = BluetoothNiNiel(device)
        thread { bluetooth.connect() }

        enableEdgeToEdge()

        setContent {
            DiddynielTheme {

                val snackbarHostState = remember { SnackbarHostState() }

                Scaffold(
                    topBar = {
                        TopAppBar(
                            title = { Text("Thermostat Controller") }
                        )
                    },
                    snackbarHost = {
                        SnackbarHost(snackbarHostState)
                    }
                ) { padding ->

                    ThermostatScreen(
                        modifier = Modifier.padding(padding),
                        onIncrease = {
                            thread { bluetooth.send("UP") }
                        },
                        onDecrease = {
                            thread { bluetooth.send("DOWN") }
                        },
                        onRefresh = { update ->
                            thread {
                                try {
                                    bluetooth.send("GET")
                                    val response = bluetooth.read()
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
}

@Composable
fun ThermostatScreen(
    modifier: Modifier = Modifier,
    onIncrease: () -> Unit,
    onDecrease: () -> Unit,
    onRefresh: ((String) -> Unit) -> Unit
) {
    var temperature by remember { mutableStateOf("Loading...") }

    // Initial refresh
    LaunchedEffect(Unit) {
        onRefresh { newTemp ->
            temperature = newTemp
        }
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
            Button(onClick = onDecrease) {
                Text("-")
            }
            Button(onClick = onIncrease) {
                Text("+")
            }
        }
        Button(onClick = {
            onRefresh { newTemp ->
                temperature = newTemp
            }
        }) {
            Text("Refresh")
        }
    }
}
