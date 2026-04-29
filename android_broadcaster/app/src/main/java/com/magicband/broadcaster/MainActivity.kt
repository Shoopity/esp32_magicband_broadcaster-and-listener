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
        "Boot Animation" to "e100e90e00830fb5b9b2adb659190b48aeb0",
        "Rainbow Pulse" to "e100e90f00110f4f425807488dd2462a0717b8",
        "Red Flash" to "031b012102ff000000000000000000000000000000000000",
        "Blue Steady" to "031b0121020000ff00000000000000000000000000000000"
    )

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            "MagicBand+ Lab",
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
                    // Sequence: Wake up then Payload
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
        
        Text("Quick Actions", color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Medium)
        Spacer(modifier = Modifier.height(16.dp))

        LazyVerticalGrid(
            columns = GridCells.Fixed(2),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
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
                    Text(name)
                }
            }
        }
    }
}
