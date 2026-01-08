/**
 * @file WROVER_Web_Hub.ino
 * @brief ESP32 WROVER Web Server with Professional Serial Debugging.
 */

#include <WiFi.h>
#include <WebServer.h>

// --- HARDWARE & NETWORK CONSTANTS ---
const uint16_t MONITOR_BAUD  = 9600; // Increased for cleaner debug output
const uint16_t NANO_BAUD     = 9600;
const uint8_t  PIN_NANO_RX   = 27;
const uint8_t  PIN_NANO_TX   = 14;

const char* WIFI_SSID        = "SSID";
const char* WIFI_PASS        = "Password";

// --- GLOBAL STATE ---
WebServer server(80);

float currentHum = 0.0;
float minHum     = 100.0;
float maxHum     = 0.0;

// --- HTML INTERFACE (No changes to your working UI) ---
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>WROVER Smart Monitor</title>
    <style>
        :root { --bg: #f0f2f5; --card: #ffffff; --text: #333; --cyan: #00d2d3; --teal: #0097a7; --blue: #2e86de; }
        body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); text-align: center; padding: 20px; }
        .container { max-width: 400px; margin: auto; }
        .card { background: var(--card); padding: 25px; border-radius: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.05); margin-bottom: 20px; }
        .progress-container { background: #eee; border-radius: 15px; height: 20px; width: 100%; margin: 20px 0; overflow: hidden; }
        #progress-bar { height: 100%; width: 0%; transition: width 0.5s ease, background-color 0.5s ease; border-radius: 15px; }
        .val-big { font-size: 3.5rem; font-weight: bold; margin: 10px 0; color: #444; }
        .stats { display: flex; justify-content: space-around; border-top: 1px solid #eee; padding-top: 15px; }
        input[type=text] { width: 100%; box-sizing: border-box; padding: 12px; border: 1px solid #ddd; border-radius: 12px; margin-bottom: 12px; font-size: 1rem; }
        button { width: 100%; padding: 14px; border: none; border-radius: 12px; font-weight: bold; cursor: pointer; transition: 0.2s; font-size: 1rem; }
        .btn-send { background: #007bff; color: white; margin-bottom: 10px; }
        .btn-reset { background: #6c757d; color: white; }
        #toast { visibility: hidden; background: #333; color: #fff; padding: 16px; position: fixed; left: 50%; bottom: 30px; transform: translateX(-50%); border-radius: 50px; }
        #toast.show { visibility: visible; animation: fade 0.5s; }
    </style>
</head>
<body>
    <div class="container">
        <h1 style="color: #555;">Humidity Hub</h1>
        <div class="card">
            <div id="hum-val" class="val-big">--%</div>
            <div class="progress-container"><div id="progress-bar"></div></div>
            <div class="stats">
                <div>Min: <b id="min-val">--</b>%</div>
                <div>Max: <b id="max-val">--</b>%</div>
            </div>
        </div>
        <div class="card">
            <input type="text" id="msgInput" placeholder="LCD Message...">
            <button class="btn-send" onclick="sendMsg()">Send Message</button>
            <button class="btn-reset" onclick="resetValues()">Reset History</button>
        </div>
    </div>
    <div id="toast">Sent!</div>
    <script>
        setInterval(fetchData, 3000);
        function fetchData() {
            fetch('/api/data').then(res => res.json()).then(data => {
                document.getElementById('hum-val').innerText = data.curr + '%';
                document.getElementById('min-val').innerText = data.min;
                document.getElementById('max-val').innerText = data.max;
                let bar = document.getElementById('progress-bar');
                let val = data.curr;
                bar.style.width = val + '%';
                if(val < 35) bar.style.backgroundColor = 'var(--cyan)';
                else if(val <= 65) bar.style.backgroundColor = 'var(--teal)';
                else bar.style.backgroundColor = 'var(--blue)';
            });
        }
        function showToast(m) {
            var x = document.getElementById("toast"); x.innerText = m; x.className = "show";
            setTimeout(function(){ x.className = ""; }, 3000);
        }
        function sendMsg() {
            let v = document.getElementById('msgInput').value;
            if(!v) return;
            fetch('/api/msg?val=' + encodeURIComponent(v)).then(() => {
                showToast("Sent to LCD!"); document.getElementById('msgInput').value = "";
            });
        }
        function resetValues() { fetch('/api/reset').then(() => showToast("History Reset!")); }
    </script>
</body>
</html>
)=====";

// --- HANDLERS ---
void handleRoot() { 
    Serial.println("[HTTP] Client requested Index Page");
    server.send(200, "text/html", INDEX_HTML); 
}

void handleGetData() {
    String json = "{\"curr\":" + String(currentHum, 1) + ",\"min\":" + String(minHum, 1) + ",\"max\":" + String(maxHum, 1) + "}";
    server.send(200, "application/json", json);
}

void handleMsg() {
    String val = server.arg("val");
    Serial.printf("[HTTP] Sending message to Nano: %s\n", val.c_str());
    Serial2.println("M:" + val); 
    server.send(200, "text/plain", "OK");
}

void handleReset() {
    Serial.println("[HTTP] Resetting Min/Max Command Received");
    Serial2.println("R:1");
    server.send(200, "text/plain", "OK");
}

void setup() {
    Serial.begin(MONITOR_BAUD);
    Serial2.begin(NANO_BAUD, SERIAL_8N1, PIN_NANO_RX, PIN_NANO_TX); 
    delay(2000); // Give power a moment to stabilize

    Serial.println("\n\n[SYSTEM] Resetting WiFi Flash Settings...");
    
    // Explicitly clean up WiFi state
    WiFi.disconnect(true); // Delete saved credentials
    WiFi.mode(WIFI_STA);   // Set station mode explicitly
    delay(100);

    Serial.printf("[WiFi] Connecting to: %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 30) {
        delay(500);
        Serial.print(".");
        attempt++;
        
        // If it hangs, try to re-trigger
        if(attempt == 15) {
            Serial.println("\n[WiFi] Retrying handshake...");
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
    }

    if(WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.printf("[WiFi] IP Address:  %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("\n[ERROR] WiFi Failed. Status: %d\n", WiFi.status());
    }

    // Configure Server Handlers
    server.on("/", handleRoot);
    server.on("/api/data", handleGetData);
    server.on("/api/msg", handleMsg);
    server.on("/api/reset", handleReset);
    
    server.begin();
    Serial.println("[HTTP] Server Ready.");
}

void loop() {
    server.handleClient();

    // Process UART Traffic
    if (Serial2.available()) {
        String incoming = Serial2.readStringUntil('\n');
        incoming.trim();
        
        if (incoming.startsWith("H:")) {
            int c1 = incoming.indexOf(',');
            int c2 = incoming.indexOf(',', c1 + 1);
            if (c1 != -1 && c2 != -1) {
                currentHum = incoming.substring(2, c1).toFloat();
                minHum = incoming.substring(c1 + 1, c2).toFloat();
                maxHum = incoming.substring(c2 + 1).toFloat();
                
                // Keep the serial quiet but confirm data flow
                Serial.printf("[UART] Update: %.1f%% (Min: %.1f Max: %.1f)\n", currentHum, minHum, maxHum);
            }
        }
    }
}