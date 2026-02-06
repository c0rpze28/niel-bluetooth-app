package com.example.diddy_niel

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.*

class BluetoothNiNiel(private val context: Context, private val device: BluetoothDevice) {

    private var bluetoothGatt: BluetoothGatt? = null
    private var writeCharacteristic: BluetoothGattCharacteristic? = null
    
    private val _receivedData = MutableStateFlow("Loading...")
    val receivedData: StateFlow<String> = _receivedData.asStateFlow()

    companion object {
        private const val TAG = "BluetoothNiNiel"
        // Standard Nordic UART Service (NUS) UUIDs - common for ESP32 BLE
        val SERVICE_UUID: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val RX_WRITE_UUID: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
        val TX_READ_UUID: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
        // CCCD UUID for enabling notifications
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG, "Connected to GATT server.")
                _receivedData.value = "Connected"
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(TAG, "Disconnected from GATT server.")
                _receivedData.value = "Disconnected"
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt.getService(SERVICE_UUID)
                writeCharacteristic = service?.getCharacteristic(RX_WRITE_UUID)
                
                val readChar = service?.getCharacteristic(TX_READ_UUID)
                if (readChar != null) {
                    // 1. Enable notifications locally
                    gatt.setCharacteristicNotification(readChar, true)
                    
                    // 2. Write to the Descriptor (CCCD) to tell the ESP32 to start sending
                    val descriptor = readChar.getDescriptor(CCCD_UUID)
                    if (descriptor != null) {
                        descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(descriptor)
                        Log.d(TAG, "Notification descriptor written.")
                    }
                }
            } else {
                Log.w(TAG, "onServicesDiscovered received: $status")
            }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid == TX_READ_UUID) {
                val value = String(characteristic.value).trim()
                Log.d(TAG, "Received: $value")
                _receivedData.value = value
            }
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "Write successful")
            }
        }
    }

    @SuppressLint("MissingPermission")
    fun connect() {
        // Use TRANSPORT_LE for BLE 5.0 compatibility
        bluetoothGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    fun send(command: String) {
        val char = writeCharacteristic ?: return
        char.value = (command + "\n").toByteArray()
        // Use WRITE_TYPE_DEFAULT for reliable 5.0 communication
        char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        bluetoothGatt?.writeCharacteristic(char)
    }

    @SuppressLint("MissingPermission")
    fun close() {
        bluetoothGatt?.close()
        bluetoothGatt = null
    }
}
