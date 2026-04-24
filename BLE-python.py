import asyncio
import sys
from winsdk.windows.devices.bluetooth.advertisement import (
    BluetoothLEAdvertisementPublisher, 
    BluetoothLEManufacturerData
)
from winsdk.windows.storage.streams import DataWriter

async def broadcast_code(hex_payload):
    publisher = BluetoothLEAdvertisementPublisher()
    
    # Disney ID (83 01)
    manufacturer_data = BluetoothLEManufacturerData()
    manufacturer_data.company_id = 0x0183
    
    try:
        # Convert hex string to bytes
        writer = DataWriter()
        data_bytes = bytes.fromhex(hex_payload)
        writer.write_bytes(data_bytes)
        manufacturer_data.data = writer.detach_buffer()
        
        publisher.advertisement.manufacturer_data.append(manufacturer_data)
        
        # Start the broadcast
        publisher.start()
        print(f"📡 Broadcasting payload for 3 seconds...")
        await asyncio.sleep(3)
        publisher.stop()
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