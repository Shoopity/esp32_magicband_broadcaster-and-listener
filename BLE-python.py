import asyncio
import sys
from winsdk.windows.devices.bluetooth.advertisement import (
    BluetoothLEAdvertisementPublisher, 
    BluetoothLEManufacturerData
)
from winsdk.windows.storage.streams import DataWriter

async def broadcast_code(hex_payload):
    async def _broadcast(payload, times, duration):
        sleep_time = duration / times
        print(f"📡 Broadcasting '{payload}' {times} times over {duration} seconds...")
        
        # Verify hex first
        data_bytes = bytes.fromhex(payload)
            
        for _ in range(times):
            publisher = BluetoothLEAdvertisementPublisher()
            manufacturer_data = BluetoothLEManufacturerData()
            manufacturer_data.company_id = 0x0183
            
            writer = DataWriter()
            writer.write_bytes(data_bytes)
            manufacturer_data.data = writer.detach_buffer()
            
            publisher.advertisement.manufacturer_data.append(manufacturer_data)
            
            publisher.start()
            await asyncio.sleep(sleep_time)
            publisher.stop()

    try:
        await _broadcast("cc03000000", 8, 2.0)
        await _broadcast(hex_payload, 8, 2.0)
        print("✅ Done. Ready for next code.\n")
        
    except ValueError:
        print("❌ Error: That doesn't look like valid hex. Check for non-hex characters.")
    except Exception as e:
        print(f"❌ Hardware Error: {e}")

async def main():
    print("=== MagicBand+ BLE Command Lab ===")
    print("Paste your hex string below (with or without '8301').")
    print("Press Ctrl+C to exit.\n")
    
    while True:
        try:
            # Get input and clean it up
            user_input = input("Enter Code: ").strip().replace(" ", "")
            
            if not user_input:
                continue
            
            # Remove 8301 prefix if present (since the script adds it via company_id)
            if user_input.lower().startswith("8301"):
                user_input = user_input[4:]
            
            await broadcast_code(user_input)
            
        except KeyboardInterrupt:
            print("\nExiting Lab. See ya real soon!")
            sys.exit()

if __name__ == "__main__":
    asyncio.run(main())