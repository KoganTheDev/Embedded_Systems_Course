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
  delay(10);
}

// WIFI SETUP

void setupWiFi(void) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  
  Serial.print("[WIFI] Connecting to "); Serial.println(SSID);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n[WIFI] Connected!");
    Serial.print("[WIFI] IP: "); Serial.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    Serial.println("\n[WIFI] Failed");
  }
}

// UART COMMUNICATION

bool parseStatusLine(const String &line, float &h, float &mn, float &mx) {
  int posH = line.indexOf("H=");
  int posMIN = line.indexOf("MIN=");
  int posMAX = line.indexOf("MAX=");
  
  if (posH == -1 || posMIN == -1 || posMAX == -1) return false;
  
  int endH = line.indexOf(';', posH);
  h = line.substring(posH + 2, endH).toFloat();
  
  int endMIN = line.indexOf(';', posMIN);
  mn = line.substring(posMIN + 4, endMIN).toFloat();
  
  mx = line.substring(posMAX + 4).toFloat();
  return true;
}

bool uartRequestStatus(float &h, float &mn, float &mx) {
  while (nanoSerial.available()) nanoSerial.read();
  
  nanoSerial.print("GET\n");
  
  unsigned long startTime = millis();
  String response = "";
  bool gotNewline = false;
  
  while (millis() - startTime < 500) {
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
        <div class="data-value" id="humidity">-<span class="unit">%</span></div>
      </div>
      <div class="data-card">
        <div class="data-label">Max Humidity</div>
        <div class="data-value" id="maxHumidity">-<span class="unit">%</span></div>
      </div>
      <div class="data-card">
        <div class="data-label">Min Humidity</div>
        <div class="data-value" id="minHumidity">-<span class="unit">%</span></div>
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
          document.getElementById('humidity').textContent = data.humidity.toFixed(1) + '%';
          document.getElementById('maxHumidity').textContent = data.maxHumidity.toFixed(1) + '%';
          document.getElementById('minHumidity').textContent = data.minHumidity.toFixed(1) + '%';
          
          const now = new Date();
          document.getElementById('lastUpdate').textContent = now.toLocaleTimeString();
          
          const wifiStatus = document.getElementById('wifiStatus');
          if (data.wifiConnected) {
            wifiStatus.textContent = '✓ WiFi Connected';
            wifiStatus.className = 'status connected';
          } else {
            wifiStatus.textContent = '✗ WiFi Disconnected';
            wifiStatus.className = 'status disconnected';
          }
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
  float h = sensorData.humidity;
  float mn = sensorData.minHumidity;
  float mx = sensorData.maxHumidity;
  bool ok = uartRequestStatus(h, mn, mx);
  
  if (ok) {
    sensorData.humidity = h;
    sensorData.minHumidity = mn;
    sensorData.maxHumidity = mx;
  }
  
  String json = "{";
  json += "\"humidity\":" + String(sensorData.humidity, 2) + ",";
  json += "\"minHumidity\":" + String(sensorData.minHumidity, 2) + ",";
  json += "\"maxHumidity\":" + String(sensorData.maxHumidity, 2) + ",";
  json += "\"wifiConnected\":" + String(wifiConnected ? "true" : "false") + ",";
  json += "\"nanoOk\":" + String(ok ? "true" : "false");
  json += "}";
  
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", json);
}

void handleReset(void) {
  uartSendReset();
  String json = "{\"success\":true}";
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", json);
}

void handleSendMessage(void) {
  if (!server.hasArg("msg")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }
  
  String message = server.arg("msg");
  uartSendMessage(message);
  
  String json = "{\"success\":true}";
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", json);
}
