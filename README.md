# ☔ IoT Humidity Gateway & Hub
The **IoT Humidity Gateway & Hub** is a dual-processor embedded system that bridges local environmental sensing with a modern web interface. It utilizes an Arduino Nano for high-precision sensor timing and an ESP32 WROVER to host an asynchronous web dashboard, providing real-time humidity tracking and remote hardware control.

---

## 🧠 Overview

This project implements a robust **UART-to-Web Gateway**. By splitting the system into two specialized modules, it ensures high reliability and non-blocking performance:

* Local Node (Arduino Nano): Dedicated to high-frequency monitoring of the DHT11 sensor and driving a 16x2 I2C LCD for local status updates.
* Network Gateway (ESP32): Manages WiFi connectivity, serves a responsive HTML5/CSS3 dashboard, and handles remote API commands via a RESTful approach.

---

## ✨ Features

* 📶 **Robust WiFi Management:** Forced "clean-start" sequence with `esp_log` silencing to eliminate association errors.
* 📊 **Real-time Dashboard:** Responsive CSS3 interface with dynamic progress bars and automatic JSON polling every 3s.
* 💬 **Remote LCD Messaging:** Send text from any browser directly to the local 16x2 LCD display.
* 📉 **Historical Tracking:** Automatic Min/Max humidity recording with remote reset capability.
* 🛠️ **Full Serial Logging:** Comprehensive debug prints for both Web-to-Nano and Nano-to-ESP32 communication.

---

## 🏛️ High Level Architecture

![high_level_architecture](/docs/images/high_level_architecture.png)

---

## 📚 Table of Contents

* [🚀 How to Deploy](https://www.google.com/search?q=%23-how-to-deploy)
* [🛠️ Tech Stack](https://www.google.com/search?q=%23%EF%B8%8F-tech-stack)
* [📡 Communication Protocol](https://www.google.com/search?q=%23-communication-protocol)

---

## 🚀 How to Deploy

1. **Wiring:**
* Connect Nano **TX (D1)** to ESP32 **IO27 (RX2)**.
* Connect Nano **RX (D0)** to ESP32 **IO14 (TX2)**.
* Ensure a **Common Ground (GND)** between both boards.


2. **Configuration:** Update `WIFI_SSID` and `WIFI_PASS` in the ESP32 code.
3. **Library Dependencies:**
* `DHT sensor library` by Adafruit.
* `LiquidCrystal I2C` by Frank de Brabander.


4. **Access:** Open the ESP32 Serial Monitor to find the local IP, then navigate to it in your browser.

---

## 🛠️ Tech Stack

| Category | Technologies |
| --- | --- |
| **Languages** | C++, HTML5, CSS3, JavaScript (ES6) |
| **Hardware** | ESP32-WROVER, Arduino Nano, DHT11, I2C LCD |
| **Communication** | UART (9600 Baud), HTTP REST |
| **Protocol** | JSON, Custom String Parsing |

---

## 📡 Communication Protocol

| Direction | Format | Purpose |
| --- | --- | --- |
| **Nano → ESP32** | `[DHT11] Current = X, Min = Y, Max = Z,` | Telemetry Update |
| **ESP32 → Nano** | `R:1` | Reset Min/Max History |
| **ESP32 → Nano** | `M:<message>` | Display Web Message on LCD |
