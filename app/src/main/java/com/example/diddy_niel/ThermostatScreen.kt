package com.example.diddy_niel

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp

@Composable
fun ThermostatScreen(
    modifier: Modifier = Modifier,
    statusText: String,
    onSetHotLimit: (String) -> Unit,
    onSetColdLimit: (String) -> Unit,
    onRefresh: () -> Unit
) {
    var showHotDialog by remember { mutableStateOf(false) }
    var showColdDialog by remember { mutableStateOf(false) }
    var tempLimitInput by remember { mutableStateOf("") }

    val currentTemp = remember(statusText) {
        statusText.filter { it.isDigit() || it == '.' }.toDoubleOrNull()
    }

    if (showHotDialog) {
        AlertDialog(
            onDismissRequest = { showHotDialog = false },
            title = { Text("Set Hot Temp Limit") },
            text = {
                OutlinedTextField(
                    value = tempLimitInput,
                    onValueChange = { tempLimitInput = it },
                    label = { Text("Temperature (°C)") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    singleLine = true
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    onSetHotLimit(tempLimitInput)
                    showHotDialog = false
                    tempLimitInput = ""
                }) { Text("Set") }
            },
            dismissButton = {
                TextButton(onClick = { showHotDialog = false }) { Text("Cancel") }
            }
        )
    }

    if (showColdDialog) {
        AlertDialog(
            onDismissRequest = { showColdDialog = false },
            title = { Text("Set Cold Temp Limit") },
            text = {
                OutlinedTextField(
                    value = tempLimitInput,
                    onValueChange = { tempLimitInput = it },
                    label = { Text("Temperature (°C)") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    singleLine = true
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    onSetColdLimit(tempLimitInput)
                    showColdDialog = false
                    tempLimitInput = ""
                }) { Text("Set") }
            },
            dismissButton = {
                TextButton(onClick = { showColdDialog = false }) { Text("Cancel") }
            }
        )
    }

    Column(
        modifier = modifier.fillMaxSize().padding(16.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        val (icon, iconColor) = when {
            currentTemp == null -> Icons.Default.Thermostat to Color.Gray
            currentTemp <= 25.0 -> Icons.Default.AcUnit to Color(0xFF03A9F4)
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

        Text(
            text = if (currentTemp != null) "Current Temperature" else "System Status", 
            style = MaterialTheme.typography.headlineSmall,
            textAlign = TextAlign.Center
        )
        
        val displayStr = if (currentTemp != null) "$statusText°C" else statusText
        val textStyle = if (currentTemp != null) MaterialTheme.typography.displayLarge else MaterialTheme.typography.headlineMedium
        
        Text(
            text = displayStr, 
            style = textStyle,
            textAlign = TextAlign.Center
        )
        
        Spacer(modifier = Modifier.height(32.dp))
        
        Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Button(onClick = { showColdDialog = true }, colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF03A9F4))) { Text("COLD LIMIT") }
            Button(onClick = { showHotDialog = true }, colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFFF5722))) { Text("HOT LIMIT") }
        }
        
        Spacer(modifier = Modifier.height(32.dp))
        Button(onClick = { onRefresh() }) { Text("Refresh") }
    }
}
