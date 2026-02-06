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
    
    private val _receivedData = MutableStateFlow("Connecting...")
    val receivedData: StateFlow<String> = _receivedData.asStateFlow()

    var onConnectionFailed: (() -> Unit)? = null

    companion object {
        private const val TAG = "BluetoothNiNiel"
        val SERVICE_UUID: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val RX_WRITE_UUID: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
        val TX_READ_UUID: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG, "Connected. Requesting MTU...")
                gatt.requestMtu(517)
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                _receivedData.value = "Disconnected"
                if (status != BluetoothGatt.GATT_SUCCESS) onConnectionFailed?.invoke()
            }
        }

        @SuppressLint("MissingPermission")
        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            gatt.discoverServices()
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt.getService(SERVICE_UUID)
                writeCharacteristic = service?.getCharacteristic(RX_WRITE_UUID)
                
                val readChar = service?.getCharacteristic(TX_READ_UUID)
                if (readChar != null) {
                    gatt.setCharacteristicNotification(readChar, true)
                    val descriptor = readChar.getDescriptor(CCCD_UUID)
                    if (descriptor != null) {
                        descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(descriptor)
                    }
                }
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                _receivedData.value = "Ready"
            }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid == TX_READ_UUID) {
                val bytes = characteristic.value ?: return
                
                // --- IMPROVED CLEANING LOGIC ---
                // We process the raw bytes to avoid UTF-8 conversion errors
                val cleanValue = bytes
                    .takeWhile { it.toInt() != 0 } // Stop at null terminator
                    .map { it.toInt().toChar() } // Convert to Char
                    .filter { it in ' '..'~' } // Keep only printable ASCII
                    .joinToString("")
                    .trim()

                if (cleanValue.isNotEmpty()) {
                    Log.d(TAG, "Cleaned: $cleanValue")
                    _receivedData.value = cleanValue
                }
            }
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(TAG, "Write failed: $status")
            }
        }
    }

    @SuppressLint("MissingPermission")
    fun connect() {
        bluetoothGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    fun send(command: String) {
        val char = writeCharacteristic ?: return
        char.value = (command + "\n").toByteArray()
        char.writeType = if ((char.properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) != 0) {
            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        } else {
            BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        }
        bluetoothGatt?.writeCharacteristic(char)
    }

    @SuppressLint("MissingPermission")
    fun close() {
        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
    }
}
