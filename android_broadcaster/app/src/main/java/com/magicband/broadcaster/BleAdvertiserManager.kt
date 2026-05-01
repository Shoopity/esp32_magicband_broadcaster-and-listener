package com.magicband.broadcaster

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.AdvertiseCallback
import android.bluetooth.le.AdvertiseData
import android.bluetooth.le.AdvertiseSettings
import android.bluetooth.le.BluetoothLeAdvertiser
import android.content.Context
import android.util.Log
import java.util.Locale

class BleAdvertiserManager(context: Context) {
    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter
    private val advertiser: BluetoothLeAdvertiser? = bluetoothAdapter?.bluetoothLeAdvertiser
    private val callbacks = mutableListOf<AdvertiseCallback>()

    @SuppressLint("MissingPermission")
    fun startAdvertising(hexPayload: String, onComplete: () -> Unit) {
        if (advertiser == null) {
            Log.e("BleAdvertiser", "Bluetooth LE Advertising not supported on this device")
            return
        }

        // Stop all previous advertisers
        stopAll()

        val payloadBytes = hexStringToByteArray(hexPayload)
        val data = AdvertiseData.Builder()
            .addManufacturerData(0x0183, payloadBytes)
            .build()

        val settings = AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setConnectable(false)
            .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
            .build()

        // Start 3 concurrent advertisers to triple the packet density (mimics PC speed)
        repeat(3) { index ->
            val callback = object : AdvertiseCallback() {
                override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) {
                    super.onStartSuccess(settingsInEffect)
                    Log.d("BleAdvertiser", "Slot $index started: $hexPayload")
                }
            }
            callbacks.add(callback)
            advertiser.startAdvertising(settings, data, callback)
        }
        
        // Duration: 2.2 seconds (extra 200ms to ensure 8+ packets are seen)
        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
            stopAll()
            onComplete()
        }, 2200)
    }

    @SuppressLint("MissingPermission")
    private fun stopAll() {
        callbacks.forEach { advertiser?.stopAdvertising(it) }
        callbacks.clear()
    }

    private fun hexStringToByteArray(s: String): ByteArray {
        val len = s.length
        val data = ByteArray(len / 2)
        var i = 0
        while (i < len) {
            data[i / 2] = ((Character.digit(s[i], 16) shl 4) + Character.digit(s[i + 1], 16)).toByte()
            i += 2
        }
        return data
    }
}
