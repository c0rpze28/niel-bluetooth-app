package com.example.diddy_niel

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import java.util.*

class BluetoothNiNiel(private val context: Context, private val device: BluetoothDevice) {

    private var bluetoothGatt: BluetoothGatt? = null
    private var writeCharacteristic: BluetoothGattCharacteristic? = null
    private var lastReceivedValue: String = "No data"

    companion object {
        // Standard Nordic UART Service (NUS) UUIDs
        val SERVICE_UUID: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val RX_WRITE_UUID: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
        val TX_READ_UUID: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                gatt.discoverServices()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt.getService(SERVICE_UUID)
                writeCharacteristic = service?.getCharacteristic(RX_WRITE_UUID)
                
                // Enable notifications for reading data
                val readChar = service?.getCharacteristic(TX_READ_UUID)
                if (readChar != null) {
                    gatt.setCharacteristicNotification(readChar, true)
                }
            }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid == TX_READ_UUID) {
                lastReceivedValue = String(characteristic.value)
            }
        }
    }

    @SuppressLint("MissingPermission")
    fun connect() {
        bluetoothGatt = device.connectGatt(context, false, gattCallback)
    }

    @SuppressLint("MissingPermission")
    fun send(command: String) {
        val char = writeCharacteristic ?: return
        char.value = (command + "\n").toByteArray()
        bluetoothGatt?.writeCharacteristic(char)
    }

    fun read(): String {
        return lastReceivedValue
    }

    @SuppressLint("MissingPermission")
    fun close() {
        bluetoothGatt?.close()
        bluetoothGatt = null
    }
}
