/**
 * @file Nano_Humidity_Controller.ino
 * @brief Nano-side controller compatible with ESP32 WROVER Web Hub.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// --- CONSTANTS ---
const uint8_t PIN_DHT          = 4;      
const uint16_t BAUD_RATE       = 9600;   
const uint32_t SENSOR_INTERVAL = 2000;   

// LCD Configuration
const uint8_t LCD_I2C_ADDR     = 0x27;   
const uint8_t LCD_COLUMNS      = 16;
const uint8_t LCD_ROWS         = 2;

// --- GLOBAL OBJECTS ---
DHT dht(PIN_DHT, DHT11);
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLUMNS, LCD_ROWS);

float currentHum = 0.0;
float minHum     = 100.0;
float maxHum     = 0.0;
unsigned long lastSensorReadTime = 0;

void setup() {
  Serial.begin(BAUD_RATE);
  dht.begin();
  
  lcd.init();
  lcd.backlight();
  
  // Initial Boot Screen
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM STARTING");
  lcd.setCursor(0, 1);
  lcd.print("WAITING FOR DATA");
  delay(1500);
  lcd.clear();
}

void loop() {
  unsigned long currentTime = millis();

  // 1. Task: Read Sensor & Transmit to ESP32
  if (currentTime - lastSensorReadTime >= SENSOR_INTERVAL) {
    float h = dht.readHumidity();

    if (!isnan(h)) {
      currentHum = h;
      
      // Update Min/Max tracking
      if (currentHum < minHum) minHum = currentHum;
      if (currentHum > maxHum) maxHum = currentHum;

      // Update Local LCD Display
      lcd.setCursor(0, 0);
      lcd.print("Humidity: "); 
      lcd.print(currentHum, 1);
      lcd.print("% "); 
      
      lcd.setCursor(0, 1);
      lcd.print("L:"); lcd.print(minHum, 0); 
      lcd.print("%  H:"); lcd.print(maxHum, 0); lcd.print("%  ");

      // TRANSMIT TO ESP32: Format "H:current,min,max"
      // This is what the ESP32 logic is looking for
      Serial.print("H:");
      Serial.print(currentHum, 1);
      Serial.print(",");
      Serial.print(minHum, 1);
      Serial.print(",");
      Serial.println(maxHum, 1);
    }
    lastSensorReadTime = currentTime;
  }

  // 2. Task: Handle Commands from ESP32
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // Command: RESET
    if (cmd == "R:1") {
      minHum = currentHum;
      maxHum = currentHum;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(">> RESETTING <<");
      lcd.setCursor(0,1);
      lcd.print(" MIN/MAX CLEARED");
      delay(2000);
      lcd.clear();
    } 
    // Command: MESSAGE FROM WEB
    else if (cmd.startsWith("M:")) {
      String msg = cmd.substring(2);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WEB MESSAGE:");
      lcd.setCursor(0, 1);
      // Clean up and display the message
      lcd.print(msg.substring(0, 16)); 
      delay(4000); // Show message for 4 seconds
      lcd.clear();
    }
  }
}