import asyncio
from bleak import BleakScanner

# Disney/MagicBand Company ID
COMPANY_ID = 0x0183

async def main():
    print("=== MagicBand+ Raw BLE Scanner (v3) ===")
    print("Filtering: No -127 RSSI | No C013 | Counting Bursts")
    print("Press Ctrl+C to stop.\n")

    # To track duplicates and see Windows filtering
    last_payload = ""
    burst_count = 0

    def detection_callback(device, advertisement_data):
        nonlocal last_payload, burst_count

        # 1. Filter RSSI -127 (Aged out/Invalid)
        if advertisement_data.rssi <= -127:
            return

        # 2. Check for Disney Manufacturer Data
        if COMPANY_ID in advertisement_data.manufacturer_data:
            data = advertisement_data.manufacturer_data[COMPANY_ID]
            hex_payload = data.hex().upper()
            
            # 3. Filter out C013 (MagicBand status packets)
            if hex_payload.startswith("C013"):
                return
            
            full_data = f"8301{hex_payload}"
            
            # 4. Count duplicates
            if full_data == last_payload:
                burst_count += 1
            else:
                # If it's a new unique payload, print the summary of the last burst?
                # Actually, let's just print every time as requested, but with a counter
                last_payload = full_data
                burst_count = 1
                
            print(f"[{device.address}] {advertisement_data.rssi:4} dBm | {full_data} (x{burst_count})")

    # Use scanning_mode="active" to be as aggressive as possible
    async with BleakScanner(detection_callback, scanning_mode="active"):
        while True:
            await asyncio.sleep(1.0)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nScanner stopped. Have a magical day!")
