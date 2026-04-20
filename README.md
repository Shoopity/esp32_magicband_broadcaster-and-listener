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
*   When using a WROOM, the LED pin is 15
*   When using a C3, the LED pin is 8

## 🚀 Installation & Flashing

This project is built using **PlatformIO**.  I'm assuming you already have your board connected to your computer and you know the COM port

1. Open the project folder in VS Code with the PlatformIO extension installed.
2. In the PlatformIO Sidebar, select the board you want:
   <img width="224" height="229" alt="image" src="https://github.com/user-attachments/assets/5d5980d5-02e2-4efc-9123-fd93bb77db03" />
   broadcaster_wroom
   brodcaster_c3
   listener_wroom
   listener_c3
   ** After expanding one of the folders, it may take a moment for PlatformIO to pull in all the available tasks, so give it a moment

### Flashing the Listener
Simply click the **Upload** button in PlatformIO to compile and flash the firmware to your board.
<img width="306" height="312" alt="image" src="https://github.com/user-attachments/assets/2776b80d-0995-44b8-a741-36cfd3654bd8" />
Wait until it is finished.
You now have an ESP32 that is listening for MagicBand+ Bluetooth codes and will output various colors and animations to WS2812 LEDs on the proper pin (see above).

### Flashing the Broadcaster
Because the Broadcaster relies on a web interface, you must flash **both** the firmware and the filesystem:
1. Click **Upload** under General to flash the firmware.
   <img width="298" height="342" alt="image" src="https://github.com/user-attachments/assets/f4df8b76-7221-40c4-bb03-58bba5b1b8fc" />
2. Click **Upload Filesystem Image** under Platform to upload "website" (This will automatically run Webpack to compile the web assets before pushing them to the board).
   <img width="298" height="342" alt="image" src="https://github.com/user-attachments/assets/02f9f768-0855-414d-9298-3a3098d8398b" />

### Flashing Tip:
When flashing ESP32, it can be finicky.  Sometimes you have to hold the BOOT button, sometimes you have power it on, hold the BOOT, press the EN button, then release the boot.  Sometimes it just flashes without touching anything.

### USB Power warning
If you're connecting the power of your LED strip directly to your board, you HAVE to limit the max brightness, both for your board's sake, and your USB's sake.  In the listener/src/main.cpp code, search for "#define BRIGHTNESS" and change the number to somethhing low (128 is half brightness)

## 🔋 Power Saving Tips
WS2812B LEDs draw a significant "vampire" current even when displaying black. If deploying this on a battery (e.g., a wearable hat), it is highly recommended to wire an N-Channel MOSFET (like an IRLZ44N) between the LED strip and the power source to digitally cut the power when animations are inactive.

## Connecting Tip
After connecting to the ESP32 Wifi AP, your device may or may not automatically open the necessary page.  Browse to 192.168.4.1 to see the page; you might have to turn off mobile data.
