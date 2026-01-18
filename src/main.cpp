#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "time.h"

// ===== Configuration =====
const char* ssid = "mono@curiosity";
const char* password = "freewifi";

// Time Configuration (UTC+8 for Philippines/Asia - Adjust 28800 to your offset)
const long  gmtOffset_sec = 28800;
const int   daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

const int SCAN_INTERVAL = 30;
const int SCAN_DURATION = 5;
const int MAX_DEVICES = 100;
const int MAX_HISTORY = 100;

// ===== Web Server =====
WebServer server(80);

// ===== Data Structures (Simple Database) =====
struct DeviceData {
  String mac;
  String name;
  int rssi;
  String proximityLabel; // Present, In Proximity, Far
  String colorClass;     // CSS class for color
};

struct HistoryRecord {
  String timestampStr; // Real world time string
  int totalDevices;
  int namedDevices;
};

DeviceData currentDevices[MAX_DEVICES];
int currentDeviceCount = 0;

HistoryRecord historyDB[MAX_HISTORY]; // The "Database"
int historyIndex = 0;
int totalScans = 0;

unsigned long lastScanTime = 0;
bool scanInProgress = false;

// ===== Helper: Get Real Time =====
String getLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Time Error";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// ===== BLE Callback Class =====
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (currentDeviceCount >= MAX_DEVICES) return;
    
    int rssi = advertisedDevice.getRSSI();
    String name = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "Unknown";
    String mac = advertisedDevice.getAddress().toString().c_str();

    // Logic for Proximity Categorization
    String label, color;
    if (rssi > -65) {
      label = "Present";
      color = "status-present"; // Green
    } else if (rssi > -85) {
      label = "In Proximity";
      color = "status-proximity"; // Orange
    } else {
      label = "Far / Weak";
      color = "status-far"; // Red
    }
    
    currentDevices[currentDeviceCount].mac = mac;
    currentDevices[currentDeviceCount].name = name;
    currentDevices[currentDeviceCount].rssi = rssi;
    currentDevices[currentDeviceCount].proximityLabel = label;
    currentDevices[currentDeviceCount].colorClass = color;
    
    currentDeviceCount++;
  }
};

MyAdvertisedDeviceCallbacks* myCallbacks;

// ===== HTML Pages =====
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OLPS Smart Proximity System</title>
  <style>
    :root { --primary: #667eea; --bg: #f4f7f6; }
    body { font-family: 'Segoe UI', sans-serif; background: var(--bg); margin: 0; padding: 20px; color: #333; }
    .container { max-width: 1000px; margin: 0 auto; }
    
    /* Header */
    .header { background: white; padding: 20px; border-radius: 12px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
    .header h1 { margin: 0; font-size: 1.5rem; color: var(--primary); }
    .clock { font-size: 1.2rem; font-weight: bold; color: #555; }

    /* Controls */
    .controls { display: flex; gap: 10px; margin-bottom: 20px; align-items: center; background: white; padding: 15px; border-radius: 12px; }
    .toggle-container { display: flex; align-items: center; cursor: pointer; }
    .switch { position: relative; display: inline-block; width: 50px; height: 24px; margin-right: 10px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }
    .slider:before { position: absolute; content: ""; height: 16px; width: 16px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--primary); }
    input:checked + .slider:before { transform: translateX(26px); }
    
    .btn { background: var(--primary); color: white; border: none; padding: 10px 20px; border-radius: 8px; cursor: pointer; font-weight: 600; }
    .btn:hover { opacity: 0.9; }

    /* Device List */
    .device-list { display: grid; gap: 10px; }
    .device-card { background: white; padding: 15px; border-radius: 10px; display: flex; justify-content: space-between; align-items: center; border-left: 5px solid #ccc; box-shadow: 0 2px 4px rgba(0,0,0,0.05); }
    
    /* Proximity Colors */
    .status-present { border-left-color: #2ecc71 !important; }
    .status-proximity { border-left-color: #f1c40f !important; }
    .status-far { border-left-color: #e74c3c !important; }
    
    .badge { padding: 5px 10px; border-radius: 15px; font-size: 0.8rem; font-weight: bold; color: white; min-width: 80px; text-align: center; }
    .bg-present { background: #2ecc71; }
    .bg-proximity { background: #f1c40f; color: #333; }
    .bg-far { background: #e74c3c; }

    .meta { font-size: 0.85rem; color: #777; font-family: monospace; }
    .name { font-weight: bold; font-size: 1.1rem; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div>
        <h1>OLPS Proximity</h1>
        <small id="statusText">System Ready</small>
      </div>
      <div class="clock" id="clock">--:--:--</div>
    </div>

    <div class="controls">
      <label class="toggle-container">
        <label class="switch">
          <input type="checkbox" id="namedOnly" onchange="renderDevices()">
          <span class="slider"></span>
        </label>
        <span>Hide "Unknown" Devices</span>
      </label>
      <div style="flex-grow:1"></div>
      <button class="btn" onclick="manualScan()">Scan Now</button>
      <button class="btn" onclick="location.href='/history'">View Database</button>
    </div>

    <div id="deviceList" class="device-list">
      <div style="text-align:center; padding:20px;">Loading data...</div>
    </div>
  </div>

  <script>
    let allDevices = [];

    function updateClock() {
      const now = new Date();
      document.getElementById('clock').innerText = now.toLocaleTimeString();
    }
    setInterval(updateClock, 1000);

    async function fetchData() {
      try {
        const res = await fetch('/api/devices');
        const data = await res.json();
        allDevices = data.devices;
        document.getElementById('statusText').innerText = data.scanning ? "Scanning..." : "Idle - Last Scan: " + data.lastScanTime;
        renderDevices();
      } catch (e) { console.error(e); }
    }

    function renderDevices() {
      const list = document.getElementById('deviceList');
      const hideUnknown = document.getElementById('namedOnly').checked;
      
      // Filter logic
      const filtered = allDevices.filter(d => {
        if (hideUnknown && d.name === "Unknown") return false;
        return true;
      });

      if (filtered.length === 0) {
        list.innerHTML = '<div style="text-align:center; color:#777; padding:20px;">No devices matching criteria.</div>';
        return;
      }

      list.innerHTML = filtered.map(d => `
        <div class="device-card ${d.colorClass}">
          <div>
            <div class="name">${d.name}</div>
            <div class="meta">${d.mac}</div>
          </div>
          <div style="text-align:right">
            <div class="badge ${d.colorClass.replace('status-', 'bg-')}">${d.proximityLabel}</div>
            <div class="meta" style="margin-top:5px;">${d.rssi} dBm</div>
          </div>
        </div>
      `).join('');
    }

    async function manualScan() {
        document.getElementById('statusText').innerText = "Requesting Scan...";
        await fetch('/api/scan', { method: 'POST' });
        setTimeout(fetchData, 6000);
    }

    setInterval(fetchData, 3000);
    fetchData();
  </script>
</body>
</html>
)rawliteral";

const char HISTORY_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Scan Database</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; padding: 20px; background: #f4f7f6; }
    table { width: 100%; border-collapse: collapse; background: white; border-radius: 8px; overflow: hidden; box-shadow: 0 4px 6px rgba(0,0,0,0.05); }
    th, td { padding: 12px 15px; text-align: left; border-bottom: 1px solid #ddd; }
    th { background: #667eea; color: white; }
    tr:hover { background-color: #f5f5f5; }
    .btn { text-decoration: none; background: #667eea; color: white; padding: 8px 16px; border-radius: 4px; display: inline-block; margin-bottom: 20px; }
  </style>
</head>
<body>
  <div style="max-width: 800px; margin: 0 auto;">
    <h1>📊 Scan History Database</h1>
    <a href="/" class="btn">← Back to Dashboard</a>
    <div id="tableContainer">Loading records...</div>
  </div>
  <script>
    fetch('/api/history')
      .then(r => r.json())
      .then(data => {
        if(data.history.length === 0) {
          document.getElementById('tableContainer').innerHTML = "No records yet.";
          return;
        }
        let html = '<table><thead><tr><th>Timestamp</th><th>Total Devices</th><th>Named Devices</th></tr></thead><tbody>';
        // Reverse to show newest first
        data.history.reverse().forEach(row => {
          html += `<tr>
            <td>${row.timestamp}</td>
            <td>${row.total}</td>
            <td>${row.named}</td>
          </tr>`;
        });
        html += '</tbody></table>';
        document.getElementById('tableContainer').innerHTML = html;
      });
  </script>
</body>
</html>
)rawliteral";

// ===== API Handlers =====
void handleRoot() { server.send(200, "text/html", HTML_PAGE); }
void handleHistoryPage() { server.send(200, "text/html", HISTORY_PAGE); }

void handleGetDevices() {
  String json = "{";
  json += "\"scanning\":" + String(scanInProgress ? "true" : "false") + ",";
  json += "\"lastScanTime\":\"" + (lastScanTime == 0 ? "Never" : getLocalTime()) + "\",";
  json += "\"devices\":[";
  for (int i = 0; i < currentDeviceCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"mac\":\"" + currentDevices[i].mac + "\",";
    json += "\"name\":\"" + currentDevices[i].name + "\",";
    json += "\"rssi\":" + String(currentDevices[i].rssi) + ",";
    json += "\"proximityLabel\":\"" + currentDevices[i].proximityLabel + "\",";
    json += "\"colorClass\":\"" + currentDevices[i].colorClass + "\"";
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleGetHistory() {
  String json = "{\"history\":[";
  int count = min(totalScans, MAX_HISTORY);
  for (int i = 0; i < count; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"timestamp\":\"" + historyDB[i].timestampStr + "\",";
    json += "\"total\":" + String(historyDB[i].totalDevices) + ",";
    json += "\"named\":" + String(historyDB[i].namedDevices);
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleManualScan() {
  lastScanTime = 0; // Force logic in loop
  server.send(200, "application/json", "{\"status\":\"started\"}");
}

// ===== Perform BLE Scan =====
void performScan() {
  scanInProgress = true;
  currentDeviceCount = 0;
  int namedCount = 0;
  
  BLEScan* scan = BLEDevice::getScan();
  scan->start(SCAN_DURATION, false);
  scan->clearResults();
  
  // Count named devices for history
  for(int i=0; i<currentDeviceCount; i++) {
    if(currentDevices[i].name != "Unknown") namedCount++;
  }

  // Save to "Database"
  int idx = historyIndex % MAX_HISTORY;
  historyDB[idx].timestampStr = getLocalTime();
  historyDB[idx].totalDevices = currentDeviceCount;
  historyDB[idx].namedDevices = namedCount;
  
  historyIndex++;
  totalScans++;
  lastScanTime = millis();
  scanInProgress = false;
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  
  // 1. WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
  
  // 2. Time Sync (NTP)
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for time sync...");
  struct tm timeinfo;
  while(!getLocalTime(&timeinfo)){ Serial.print("."); delay(500); }
  Serial.println("\nTime Synced: " + getLocalTime());

  // 3. BLE Init
  BLEDevice::init("");
  BLEScan* scan = BLEDevice::getScan();
  myCallbacks = new MyAdvertisedDeviceCallbacks();
  scan->setAdvertisedDeviceCallbacks(myCallbacks);
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  
  // 4. Server Init
  server.on("/", handleRoot);
  server.on("/history", handleHistoryPage);
  server.on("/api/devices", handleGetDevices);
  server.on("/api/history", handleGetHistory);
  server.on("/api/scan", HTTP_POST, handleManualScan);
  server.begin();
}

void loop() {
  server.handleClient();
  if (millis() - lastScanTime >= SCAN_INTERVAL * 1000) {
    performScan();
  }
}