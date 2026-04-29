package com.magicband.broadcaster

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.rememberMultiplePermissionsState

class MainActivity : ComponentActivity() {
    private lateinit var advertiserManager: BleAdvertiserManager

    @OptIn(ExperimentalPermissionsApi::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        advertiserManager = BleAdvertiserManager(this)

        setContent {
            MaterialTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = Color(0xFF121212) // Dark background
                ) {
                    val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        listOf(
                            Manifest.permission.BLUETOOTH_ADVERTISE,
                            Manifest.permission.BLUETOOTH_CONNECT
                        )
                    } else {
                        listOf(
                            Manifest.permission.BLUETOOTH,
                            Manifest.permission.BLUETOOTH_ADMIN,
                            Manifest.permission.ACCESS_FINE_LOCATION
                        )
                    }

                    val permissionState = rememberMultiplePermissionsState(permissions)

                    if (permissionState.allPermissionsGranted) {
                        BroadcasterScreen(advertiserManager)
                    } else {
                        PermissionScreen { permissionState.launchMultiplePermissionRequest() }
                    }
                }
            }
        }
    }
}

@Composable
fun PermissionScreen(onRequestPermissions: () -> Unit) {
    Column(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text("Bluetooth permissions required", color = Color.White)
        Spacer(modifier = Modifier.height(16.dp))
        Button(onClick = onRequestPermissions) {
            Text("Grant Permissions")
        }
    }
}

@Composable
fun BroadcasterScreen(manager: BleAdvertiserManager) {
    var hexInput by remember { mutableStateOf("") }
    var statusText by remember { mutableStateOf("Ready") }
    var isBroadcasting by remember { mutableStateOf(false) }

    val quickActions = listOf(
        "Green Bootup" to "e90b0b0f0f5c5d48a5d1453205",
        "Slow Blink" to "8301e100e90c000f0f5d465bf00532374895",
        "Orange Blink" to "8301e100e90c00ef0f4f4f5bf0fb14374895",
        "5 Color Cycle" to "8301e100e90c000f0fb1b9b5b1a2307b7db0",
        "Taste the Rainbow" to "8301e100e90c000f0f5d465bf005323748b0",
        "Slow Lightning" to "e100e90e00010fbda0a0bda059070048aeb5",
        "Fast Lightning" to "e100e90e00020fbca0bca0bc5917fb48aebb",
        "Green/Purple Lightning" to "e100e90e00110fbca7b9a7b959190248aeb0",
        "Intense Lightning" to "e100e90e00150fbbbbbbbbbb59190248aeb0",
        "Zone Color Fade" to "e100e90e00830fb5b9b2adb659190b48aeb0",
        "Blue/Orange Flicker" to "e100e90f00110f4f425807488dd2462a0717b8",
        "Pink Center Pulse" to "e100e90f002a0f46435812488dd246021200b0",
        "Blood Orange Pulse" to "e100e910000f0f545d58f44882d146090ad065282102",
        "Cyan Pulse Comet" to "e100e91000134897d00ea0d146060f30d04e07b0",
        "Green/Cyan Fade" to "e100e911006f0f565258f44882d1460208d06500b0",
        "Red/Yellow Fade" to "e200e911004f0f444f58f44882d1460607d06543b0",
        "Blue/Teal Fade" to "e100e911000f0f485958f44882d146020dd06505b0",
        "Indigo/Violet Fade" to "e200e911004f0f4f5558f44882d146022ad06501b0",
        "Green/Yellow Fade" to "e100e91100010f5a475bf03134374894d13d0507b0",
        "Aqua/Green Fade" to "e100e91100070f555d58f44882d1460508d06500b0",
        "Blue/Navy Fade" to "e100e91100440f514258f44882d146050fd06500b0",
        "Blue/Yellow Blink" to "e100e91200012904020211114896d00effd1460707b0",
        "Zone Sequential Cycle" to "e200e91200030fa2a2a4a4a230d037f4d2460064fcb0",
        "Mystery Pulse 1" to "e100e91200010fbcbdbdbdbd30d037f4d2460000fcbb",
        "Mystery Pulse 2" to "e100e91300b60f404458f44882d06519d146060a307bff",
        "Mystery Pulse 3" to "e100e9130002d037f0d23d0505000efa8983510ee7a0b0",
        "Mystery Pulse 4" to "e200e91300650fbdb5bcb5bc7aec5c0a2915291548abb0",
        "Mystery Pulse 5" to "e100e914000cd037f0d23d050c0c0eec8983510eee0c3db0",
        "Mystery Pulse 6" to "e200e914002cd037f0d23d0212000eea8983510ee30c1eb0",
        "Mystery Pulse 7" to "e200e91400420f555b58f44882d0651bd1462a02307b5db0",
        "Red Flash" to "031b012102ff000000000000000000000000000000000000",
        "Blue Steady" to "031b0121020000ff00000000000000000000000000000000"
    )

    LazyVerticalGrid(
        columns = GridCells.Fixed(2),
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        item(span = { GridItemSpan(2) }) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text(
                    "MagicBand+ Broadcaster",
                    fontSize = 28.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color.White
                )

                Spacer(modifier = Modifier.height(32.dp))

                OutlinedTextField(
                    value = hexInput,
                    onValueChange = { hexInput = it.replace(" ", "") },
                    label = { Text("Enter Hex Code", color = Color.Gray) },
                    modifier = Modifier.fillMaxWidth(),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = Color.White,
                        unfocusedTextColor = Color.White,
                        focusedBorderColor = Color.Magenta,
                        unfocusedBorderColor = Color.Gray
                    )
                )

                Spacer(modifier = Modifier.height(16.dp))

                Button(
                    onClick = {
                        if (hexInput.isNotEmpty()) {
                            isBroadcasting = true
                            statusText = "Broadcasting..."
                            manager.startAdvertising("cc03000000") {
                                manager.startAdvertising(hexInput) {
                                    isBroadcasting = false
                                    statusText = "Done"
                                }
                            }
                        }
                    },
                    enabled = !isBroadcasting && hexInput.length % 2 == 0,
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.buttonColors(containerColor = Color.Magenta)
                ) {
                    Text("Broadcast Custom Code", color = Color.White)
                }

                Spacer(modifier = Modifier.height(8.dp))
                Text(statusText, color = if (isBroadcasting) Color.Cyan else Color.Green)

                Spacer(modifier = Modifier.height(48.dp))

                Text(
                    "Quick Actions",
                    color = Color.White,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Medium
                )
                Spacer(modifier = Modifier.height(16.dp))
            }
        }

        items(quickActions) { (name, hex) ->
            ElevatedButton(
                onClick = {
                    isBroadcasting = true
                    statusText = "Broadcasting $name..."
                    manager.startAdvertising("cc03000000") {
                        manager.startAdvertising(hex) {
                            isBroadcasting = false
                            statusText = "Done"
                        }
                    }
                },
                enabled = !isBroadcasting,
                modifier = Modifier.height(60.dp),
                colors = ButtonDefaults.elevatedButtonColors(
                    containerColor = Color(0xFF333333),
                    contentColor = Color.White
                )
            ) {
                Text(name, style = LocalTextStyle.current.copy(fontSize = 12.sp))
            }
        }
    }
}
