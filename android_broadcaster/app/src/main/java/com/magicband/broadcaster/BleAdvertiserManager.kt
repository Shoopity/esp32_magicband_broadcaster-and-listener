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

    @SuppressLint("MissingPermission")
    fun startAdvertising(hexPayload: String, onComplete: () -> Unit) {
        if (advertiser == null) {
            Log.e("BleAdvertiser", "Bluetooth LE Advertising not supported on this device")
            return
        }

        val payloadBytes = hexStringToByteArray(hexPayload)
        
        val settings = AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY) // 100ms interval (High activity)
            .setConnectable(false)
            .setTimeout(2000) // 2 seconds
            .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
            .build()

        val data = AdvertiseData.Builder()
            .addManufacturerData(0x0183, payloadBytes)
            .build()

        // We use a new callback for each start to avoid "callback already registered" errors
        val callback = object : AdvertiseCallback() {
            override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) {
                super.onStartSuccess(settingsInEffect)
                Log.d("BleAdvertiser", "Broadcasting: $hexPayload")
            }

            override fun onStartFailure(errorCode: Int) {
                super.onStartFailure(errorCode)
                Log.e("BleAdvertiser", "Start failed: $errorCode")
            }
        }

        advertiser.startAdvertising(settings, data, callback)
        
        // Wait the full 2 seconds plus a tiny breather before moving to next step
        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
            advertiser.stopAdvertising(callback)
            onComplete()
        }, 2050)
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
