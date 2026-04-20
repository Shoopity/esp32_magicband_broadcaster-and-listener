# ESP32 MagicBand+ Broadcaster & Listener

A high-fidelity, protocol-aware MagicBand+ ecosystem. This project is a specialized fork of [xxfunprojectsxx/esp32_magicband](https://github.com/xxfunprojectsxx/esp32_magicband), significantly expanded to support advanced show-controller logic. 

Protocol research and decoding guidance provided by the excellent [EMCOT MagicBand+ research page](https://em-cot.github.io/posts/disney-magic-band-plus/).

## 🌟 Features

*   **Advanced Show Controller**: The Listener firmware implements a robust sub-timer state machine capable of executing complex directional animations.
    *   *Green Comet Chase*: Dual comets with fading tails rotating at precise intervals.
    *   *Rainbow Cycle*: 5-zone hue shifting synchronized across the entire LED strand.
    *   *Zone Steppers & Strobes*: Independent zone-based coloring and timed flashes.
*   **Packet Fingerprinting**: Distinguishes between visually identical payload signatures to route multi-byte hexadecimal `E9 0C` and `E9 0E` payloads to the correct animation libraries.
*   **32-Color Custom Palette**: Maps Disney's raw 5-bit color indices to calibrated, human-readable RGB values for perfect color accuracy on WS2812B strips.
*   **Web Interpreter UI**: The Broadcaster features a live, responsive web interface that decodes raw hexadecimal payloads into human-readable labels (Vibration, Color, Zone Masking, and Animation Mode).

## 🗂️ Repository Structure

This project uses a PlatformIO mono-repo structure:

*   `/broadcaster/`: Contains the Web UI (HTML/JS/CSS), Webpack compilation scripts, and the BLE transmission firmware.
*   `/listener/`: Contains the FastLED firmware to drive physical LED strips based on intercepted signals.

## ⚙️ Hardware Requirements

*   **ESP32**: Fully compatible with both `ESP32-WROOM` and `ESP32-C3` architectures.
*   **LEDs**: WS2812B addressable LED strip (Default configured to 148 pixels across 5 distinct zones).

## 🚀 Installation & Flashing

This project is built using **PlatformIO**.

1. Open the project folder in VS Code with the PlatformIO extension installed.
2. At the bottom toolbar, select your target environment:
    *   `env:broadcaster_wroom`
    *   `env:broadcaster_c3`
    *   `env:listener_wroom`
    *   `env:listener_c3`

### Flashing the Listener
Simply click the **Upload** button in PlatformIO to compile and flash the firmware to your board.

### Flashing the Broadcaster
Because the Broadcaster relies on a web interface, you must flash **both** the firmware and the filesystem:
1. Click **Upload** to flash the firmware.
2. Open the PlatformIO sidebar menu, navigate to your environment, and click **Upload Filesystem Image** (This will automatically run Webpack to compile the web assets before pushing them to the board).

## 🔋 Power Saving Tips
WS2812B LEDs draw a significant "vampire" current even when displaying black. If deploying this on a battery (e.g., a wearable hat), it is highly recommended to wire an N-Channel MOSFET (like an IRLZ44N) between the LED strip and the power source to digitally cut the power when animations are inactive.
