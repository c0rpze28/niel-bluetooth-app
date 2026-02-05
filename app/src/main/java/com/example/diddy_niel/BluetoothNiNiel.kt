package com.example.diddy_niel

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID

class BluetoothNiNiel(private val device: BluetoothDevice) {

    private val uuid: UUID =
        UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    private var socket: BluetoothSocket? = null
    private var input: InputStream? = null
    private var output: OutputStream? = null

    fun connect() {
        socket = device.createRfcommSocketToServiceRecord(uuid)
        socket?.connect()

        input = socket?.inputStream
        output = socket?.outputStream
    }

    fun send(command: String) {
        output?.write((command + "\n").toByteArray())
    }

    fun read(): String {
        val buffer = ByteArray(64)
        val bytes = input?.read(buffer) ?: 0
        return String(buffer, 0, bytes)
    }

    fun close() {
        input?.close()
        output?.close()
        socket?.close()
    }
}
