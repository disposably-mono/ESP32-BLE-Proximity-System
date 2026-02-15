


# OLPS Smart Proximity System

An ESP32-based BLE device scanner with web dashboard and real-time proximity monitoring.

## Features

- **BLE Device Scanning** - Detects nearby Bluetooth devices with RSSI-based proximity
- **Real-time Dashboard** - Web interface showing all detected devices
- **Proximity Categorization**:
  - 🟢 **Present** (RSSI > -65) - Very close devices
  - 🟡 **In Proximity** (RSSI -65 to -85) - Nearby devices  
  - 🔴 **Far / Weak** (RSSI < -85) - Distant devices
- **Scan History** - Stores up to 100 scans with timestamps
- **Named Device Filter** - Option to hide "Unknown" devices
- **NTP Time Sync** - Accurate timestamps for Philippines timezone (UTC+8)

## Hardware Requirements

- ESP32 development board
- Power supply (USB or battery)

## Installation

1. Install Arduino IDE with ESP32 board support
2. Install required libraries:
   - WiFi
   - WebServer  
   - BLEDevice
3. Update WiFi credentials in code:
   ```cpp
   const char* ssid = "your-ssid";
   const char* password = "your-password";
   ```
4. Upload to ESP32

## Usage

1. Connect ESP32 to power
2. Check Serial Monitor for IP address
3. Open web browser to ESP32's IP address
4. Dashboard shows real-time device detection
5. Use "Scan Now" for manual scans
6. View scan history in Database page

## API Endpoints

- `/` - Main dashboard
- `/history` - Scan history view
- `/api/devices` - JSON of current devices
- `/api/history` - JSON of scan history
- `/api/scan` (POST) - Trigger manual scan

## Configuration

Adjust these parameters in code:
- `SCAN_INTERVAL` - Seconds between auto-scans
- `SCAN_DURATION` - Seconds per scan
- `MAX_HISTORY` - Number of scans to store
- `gmtOffset_sec` - Timezone offset

## License

MIT License
```
