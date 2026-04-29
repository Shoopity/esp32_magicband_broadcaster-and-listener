package com.magicband.broadcaster

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

    private var isAdvertising = false

    fun startAdvertising(hexPayload: String, onComplete: () -> Unit) {
        if (advertiser == null) {
            Log.e("BleAdvertiser", "Bluetooth LE Advertising not supported on this device")
            return
        }

        val payloadBytes = hexStringToByteArray(hexPayload)
        
        val settings = AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setConnectable(false)
            .setTimeout(2000) // 2 seconds, similar to the Python script duration
            .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
            .build()

        val data = AdvertiseData.Builder()
            .addManufacturerData(0x0183, payloadBytes)
            .build()

        val callback = object : AdvertiseCallback() {
            override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) {
                super.onStartSuccess(settingsInEffect)
                isAdvertising = true
                Log.d("BleAdvertiser", "Advertising started successfully")
            }

            override fun onStartFailure(errorCode: Int) {
                super.onStartFailure(errorCode)
                isAdvertising = false
                Log.e("BleAdvertiser", "Advertising failed with error code: $errorCode")
            }
        }

        advertiser.startAdvertising(settings, data, callback)
        
        // Android's setTimeout handles the stop, but we can notify completion
        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
            onComplete()
        }, 2000)
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
