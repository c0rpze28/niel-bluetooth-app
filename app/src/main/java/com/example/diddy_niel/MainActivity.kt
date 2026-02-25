package com.example.diddy_niel

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import com.example.diddy_niel.ui.theme.DiddynielTheme

class MainActivity : ComponentActivity() {

    private val viewModel: BleViewModel by viewModels()

    companion object {
        private const val DEBUG_MODE = false
    }

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            val granted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                permissions[Manifest.permission.BLUETOOTH_CONNECT] == true &&
                permissions[Manifest.permission.BLUETOOTH_SCAN] == true &&
                permissions[Manifest.permission.ACCESS_FINE_LOCATION] == true
            } else {
                permissions[Manifest.permission.ACCESS_FINE_LOCATION] == true
            }

            if (granted) {
                viewModel.initializeBluetooth()
            }
        }

    @OptIn(ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        
        viewModel.setDebug(DEBUG_MODE)

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
                val bluetoothData by viewModel.bleUartClient?.receivedData?.collectAsState("Searching...")
                    ?: androidx.compose.runtime.remember { mutableStateOf("Ready to Connect") }
                
                Scaffold(
                    topBar = { TopAppBar(title = { Text("Thermostat Controller") }) }
                ) { padding ->
                    ThermostatScreen(
                        modifier = Modifier.padding(padding),
                        statusText = viewModel.getDisplayStatus(bluetoothData),
                        onSetHotLimit = { viewModel.setHotLimit(it) },
                        onSetColdLimit = { viewModel.setColdLimit(it) },
                        onRefresh = { viewModel.refresh(bluetoothData) }
                    )
                }
            }
        }
    }
}
