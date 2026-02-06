package com.example.diddy_niel

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.*
import kotlin.concurrent.thread

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
                Log.d(TAG, "Connected. Discovering services...")
                thread {
                    Thread.sleep(600)
                    gatt.discoverServices()
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(TAG, "Disconnected. Status: $status")
                _receivedData.value = "Disconnected"

                // NOW close the gatt properly
                gatt.close()  // ADD THIS LINE

                if (status != BluetoothGatt.GATT_SUCCESS) onConnectionFailed?.invoke()
            }
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
                    descriptor?.let {
                        it.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(it)
                    }
                    gatt.requestMtu(517) // Request MTU after service discovery
                }
            } else {
                 _receivedData.value = "Discovery Failed"
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "Handshake Complete. Ready.")
                _receivedData.value = "Ready"
                thread {
                    Thread.sleep(500) // Wait for ESP32 to be ready
                    send("GET")
                }
            }
        }
        
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid == TX_READ_UUID) {
                val bytes = characteristic.value ?: return
                val cleanValue = bytes.takeWhile { it.toInt() != 0 }.map { it.toInt().toChar() }.joinToString("").trim()
                if (cleanValue.isNotEmpty()) {
                    _receivedData.value = cleanValue
                }
            }
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) Log.e(TAG, "Write failed with status: $status")
        }
    }

    @SuppressLint("MissingPermission")
    fun connect() {
        _receivedData.value = "Connecting..."
        thread {
            Thread.sleep(500) // CRITICAL: Wait for old connection to die
            bluetoothGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        }
    }

    @SuppressLint("MissingPermission")
    fun send(command: String) {
        val gatt = bluetoothGatt ?: return
        val char = writeCharacteristic ?: return
        
        // Your ESP32 code expects a raw string, not one with a newline
        char.value = command.toByteArray()
        char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        
        gatt.writeCharacteristic(char)
    }

    @SuppressLint("MissingPermission")
    fun close() {
        bluetoothGatt?.disconnect()
        bluetoothGatt = null
    }
}