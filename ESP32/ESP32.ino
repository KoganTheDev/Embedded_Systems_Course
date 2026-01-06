// ESP32 UART Interface with WiFi and Web Server
// Communicates with Arduino Nano via UART for humidity data
// Hosts web server for UI and message/reset control

#include <WiFi.h>
#include <WebServer.h>

// CONFIGURATION PARAMETERS

#define SSID "YOUR_SSID"
#define PASSWORD "YOUR_PASSWORD"
#define SERVER_PORT 80
#define UART_BAUD 115200
#define UART_RX_PIN 16
#define UART_TX_PIN 17

// GLOBAL VARIABLES

WebServer server(SERVER_PORT);
HardwareSerial nanoSerial(2);

struct SensorData {
  float humidity;
  float minHumidity;
  float maxHumidity;
} sensorData = {0.0, 0.0, 0.0};

bool wifiConnected = false;
unsigned long lastNanoOkMillis = 0;
int uartFailCount = 0;
bool everGotData = false;
unsigned long lastReconnectAttempt = 0;
int wifiReconnectAttempts = 0;
const unsigned long STALE_DATA_THRESHOLD = 5000;
const unsigned long RECONNECT_INTERVAL = 10000;
const int MAX_UART_FAIL_COUNT = 3;
const int MAX_UART_FAIL_COUNT_CAP = 1000;
const int MAX_RECONNECT_ATTEMPTS = 6;

// FUNCTION DECLARATIONS

void setupWiFi(void);
void setupWebServer(void);
bool uartRequestStatus(float &h, float &mn, float &mx);
void uartSendReset(void);
void uartSendMessage(const String &msg);
bool parseStatusLine(const String &line, float &h, float &mn, float &mx);
void handleRoot(void);
void handleReset(void);
void handleSendMessage(void);
void handleGetStatus(void);


void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP32 UART Interface ===");
  
  nanoSerial.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("[UART] Initialized");
  
  setupWiFi();
  setupWebServer();
  
  server.begin();
  Serial.println("[SETUP] Web server started");
}

void loop() {
  server.handleClient();
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  // Auto-reconnect WiFi with proper debouncing and retry logic
  if (!wifiConnected && (millis() - lastReconnectAttempt >= RECONNECT_INTERVAL)) {
    lastReconnectAttempt = millis();
    wifiReconnectAttempts++;
    
    Serial.printf("[WIFI] Attempting reconnect... (attempt %d)\n", wifiReconnectAttempts);
    
    // If too many reconnect attempts fail, do a full reset
    if (wifiReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
      Serial.println("[WIFI] Too many reconnect attempts, doing full reset...");
      WiFi.disconnect(true); // Turn off WiFi
      delay(100);
      WiFi.mode(WIFI_STA);
      WiFi.begin(SSID, PASSWORD);
      wifiReconnectAttempts = 0;
      lastReconnectAttempt = millis();  // Reset timer to prevent immediate retry
    } else {
      WiFi.reconnect();
    }
  }
  
  // Reset counter on successful connection
  if (wifiConnected && wifiReconnectAttempts > 0) {
    wifiReconnectAttempts = 0;
  }
  
  // Log successful connection once
  static bool connectedLogged = false;
  if (wifiConnected && !connectedLogged) {
    connectedLogged = true;
    Serial.println("[WIFI] Connected!");
    Serial.print("[WIFI] IP: "); Serial.println(WiFi.localIP());
  }
  if (!wifiConnected) {
    connectedLogged = false;
  }
  
  // Periodic UART polling (non-blocking relative to network)
  static unsigned long lastUartPoll = 0;
  if (millis() - lastUartPoll >= 1000) {
    lastUartPoll = millis();
    float h, mn, mx;
    if (uartRequestStatus(h, mn, mx)) {
      sensorData.humidity = h;
      sensorData.minHumidity = mn;
      sensorData.maxHumidity = mx;
      lastNanoOkMillis = millis();
      everGotData = true;
      uartFailCount = 0;
    } else {
      if (uartFailCount < MAX_UART_FAIL_COUNT_CAP) {
        uartFailCount++;
      }
    }
  }
  
  delay(10);
}

// WIFI SETUP

void setupWiFi(void) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  
  Serial.print("[WIFI] Connecting to "); Serial.println(SSID);
  Serial.println("[WIFI] Use loop() for non-blocking connection");
  
  // Allow immediate reconnect attempt if needed
  lastReconnectAttempt = millis() - RECONNECT_INTERVAL;
}

// UART COMMUNICATION

bool parseStatusLine(const String &line, float &h, float &mn, float &mx) {
  int posH = line.indexOf("H=");
  int posMIN = line.indexOf("MIN=");
  int posMAX = line.indexOf("MAX=");
  
  if (posH == -1 || posMIN == -1 || posMAX == -1) return false;
  
  int endH = line.indexOf(';', posH);
  if (endH == -1) return false;
  h = line.substring(posH + 2, endH).toFloat();
  
  int endMIN = line.indexOf(';', posMIN);
  if (endMIN == -1) return false;
  mn = line.substring(posMIN + 4, endMIN).toFloat();
  
  if (posMAX >= (int)line.length()) return false;
  mx = line.substring(posMAX + 4).toFloat();
  return true;
}

bool uartRequestStatus(float &h, float &mn, float &mx) {
  while (nanoSerial.available()) nanoSerial.read();
  
  nanoSerial.print("GET\n");
  
  unsigned long startTime = millis();
  String response = "";
  bool gotNewline = false;
  
  while (millis() - startTime < 200) {  // Reduced from 500ms to 200ms
    if (nanoSerial.available()) {
      char c = nanoSerial.read();
      if (c == '\n') {
        gotNewline = true;
        break;
      }
      response += c;
    }
  }
  
  if (!gotNewline) {
    Serial.println("[UART] Timeout");
    return false;
  }
  
  if (parseStatusLine(response, h, mn, mx)) {
    Serial.printf("[UART] H=%.1f MIN=%.1f MAX=%.1f\n", h, mn, mx);
    return true;
  }
  
  Serial.printf("[UART] Parse failed: %s\n", response.c_str());
  return false;
}

void uartSendReset(void) {
  nanoSerial.print("RESET\n");
  Serial.println("[UART] Reset sent");
}

void uartSendMessage(const String &msg) {
  String truncated = msg.substring(0, 20);
  nanoSerial.print("MSG:");
  nanoSerial.print(truncated);
  nanoSerial.print("\n");
  Serial.printf("[UART] MSG: %s\n", truncated.c_str());
}

// WEB SERVER SETUP

void setupWebServer(void) {
  server.on("/", handleRoot);
  server.on("/status", handleGetStatus);
  server.on("/reset", handleReset);
  server.on("/message", handleSendMessage);
  
  // Handle 404
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
}

// HTTP REQUEST HANDLERS

void handleRoot(void) {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Humidity Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { 
      font-family: Arial, sans-serif; 
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container { 
      max-width: 600px; 
      margin: 0 auto; 
      background: white;
      border-radius: 15px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.3);
      padding: 30px;
    }
    h1 { 
      color: #333; 
      margin-bottom: 30px;
      text-align: center;
      border-bottom: 3px solid #667eea;
      padding-bottom: 15px;
    }
    .sensor-data { 
      display: grid; 
      grid-template-columns: 1fr 1fr; 
      gap: 20px;
      margin-bottom: 30px;
    }
    .data-card { 
      background: #f8f9fa;
      padding: 20px;
      border-radius: 10px;
      border-left: 4px solid #667eea;
    }
    .data-label { 
      color: #666; 
      font-size: 14px;
      margin-bottom: 8px;
    }
    .data-value { 
      font-size: 32px; 
      font-weight: bold;
      color: #667eea;
    }
    .unit {
      font-size: 16px;
      color: #999;
    }
    .controls {
      display: grid;
      gap: 15px;
      margin-bottom: 30px;
    }
    input[type="text"] {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 16px;
      transition: border-color 0.3s;
    }
    input[type="text"]:focus {
      outline: none;
      border-color: #667eea;
    }
    .button-group {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    button {
      padding: 12px 24px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: bold;
      cursor: pointer;
      transition: all 0.3s;
    }
    .btn-send {
      background: #667eea;
      color: white;
    }
    .btn-send:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    .btn-reset {
      background: #ff6b6b;
      color: white;
    }
    .btn-reset:hover {
      background: #ee5a52;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(255, 107, 107, 0.4);
    }
    .status {
      padding: 15px;
      border-radius: 8px;
      text-align: center;
      font-weight: bold;
      margin-top: 20px;
    }
    .status.connected {
      background: #d4edda;
      color: #155724;
    }
    .status.disconnected {
      background: #f8d7da;
      color: #721c24;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🌡️ Humidity Monitor</h1>
    
    <div class="sensor-data">
      <div class="data-card">
        <div class="data-label">Current Humidity</div>
        <div class="data-value"><span id="humidity">-</span><span class="unit">%</span></div>
      </div>
      <div class="data-card">
        <div class="data-label">Max Humidity</div>
        <div class="data-value"><span id="maxHumidity">-</span><span class="unit">%</span></div>
      </div>
      <div class="data-card">
        <div class="data-label">Min Humidity</div>
        <div class="data-value"><span id="minHumidity">-</span><span class="unit">%</span></div>
      </div>
      <div class="data-card">
        <div class="data-label">Last Update</div>
        <div id="lastUpdate" style="font-size: 14px; color: #666; margin-top: 5px;">Connecting...</div>
      </div>
    </div>

    <div class="controls">
      <label for="message" style="font-weight: bold; color: #333;">Send Message to LCD:</label>
      <input type="text" id="message" placeholder="Enter message (max 20 chars)" maxlength="20">
      <div class="button-group">
        <button class="btn-send" onclick="sendMessage()">Send Message</button>
        <button class="btn-reset" onclick="resetMinMax()">Reset Min/Max</button>
      </div>
    </div>

    <div class="status" id="wifiStatus">Checking connection...</div>
  </div>

  <script>
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('humidity').textContent = data.humidity.toFixed(1);
          document.getElementById('maxHumidity').textContent = data.maxHumidity.toFixed(1);
          document.getElementById('minHumidity').textContent = data.minHumidity.toFixed(1);
          
          const now = new Date();
          document.getElementById('lastUpdate').textContent = now.toLocaleTimeString();
          
          const wifiStatus = document.getElementById('wifiStatus');
          let statusMsg = '';
          if (data.wifiConnected) {
            statusMsg = '✓ WiFi Connected';
            wifiStatus.className = 'status connected';
          } else {
            statusMsg = '✗ WiFi Disconnected';
            wifiStatus.className = 'status disconnected';
          }
          if (data.dataStale) {
            statusMsg += ' | ⚠️ Data Stale (UART failures: ' + data.uartFailCount + ')';
          }
          wifiStatus.textContent = statusMsg;
        })
        .catch(error => console.log('Error:', error));
    }

    function sendMessage() {
      const message = document.getElementById('message').value;
      if (message.trim() === '') {
        alert('Enter message');
        return;
      }

      fetch('/message', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'msg=' + encodeURIComponent(message)
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          alert('Sent!');
          document.getElementById('message').value = '';
        }
      })
      .catch(error => console.log('Error:', error));
    }

    function resetMinMax() {
      if (confirm('Reset min/max values?')) {
        fetch('/reset', { method: 'POST' })
          .then(response => response.json())
          .then(data => {
            if (data.success) {
              alert('Reset!');
              updateStatus();
            }
          })
          .catch(error => console.log('Error:', error));
      }
    }

    updateStatus();
    setInterval(updateStatus, 2000);
  </script>
</body>
</html>
  )=====" ;
  
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/html", html);
}

void handleGetStatus(void) {
  // Snapshot sensorData to avoid race condition
  float h = sensorData.humidity;
  float mn = sensorData.minHumidity;
  float mx = sensorData.maxHumidity;
  
  // Determine stale state
  bool dataStale = (everGotData && (millis() - lastNanoOkMillis > STALE_DATA_THRESHOLD));
  bool uartHealthy = (uartFailCount < MAX_UART_FAIL_COUNT);
  
  String json = "{";
  json += "\"humidity\":" + String(h, 2) + ",";
  json += "\"minHumidity\":" + String(mn, 2) + ",";
  json += "\"maxHumidity\":" + String(mx, 2) + ",";
  json += "\"wifiConnected\":" + String(wifiConnected ? "true" : "false") + ",";
  json += "\"uartHealthy\":" + String(uartHealthy ? "true" : "false") + ",";
  json += "\"dataStale\":" + String(dataStale ? "true" : "false") + ",";
  json += "\"uartFailCount\":" + String(uartFailCount);
  json += "}";
  
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", json);
}

void handleReset(void) {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"success\":false,\"error\":\"Method Not Allowed\"}");
    return;
  }
  
  uartSendReset();
  bool uartHealthy = (uartFailCount < MAX_UART_FAIL_COUNT);
  String json = "{\"success\":true,\"queued\":true,\"uartHealthy\":" + String(uartHealthy ? "true" : "false") + "}";
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", json);
}

void handleSendMessage(void) {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"success\":false,\"error\":\"Method Not Allowed\"}");
    return;
  }
  
  if (!server.hasArg("msg")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing msg parameter\"}");
    return;
  }
  
  String message = server.arg("msg");
  uartSendMessage(message);
  
  bool uartHealthy = (uartFailCount < MAX_UART_FAIL_COUNT);
  String json = "{\"success\":true,\"queued\":true,\"uartHealthy\":" + String(uartHealthy ? "true" : "false") + "}";
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", json);
}
