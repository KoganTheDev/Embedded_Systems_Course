/** 
 * Example of a solution without a timer 

 #include <Arduino.h>

const int buttonPin = 2;
const int ledPin = 10;

const unsigned long SHORT_PRESS_TIME = 1500;
const unsigned long LONG_PRESS_TIME  = 4000;

const unsigned long interval = 500;   

int PressCounter = 0;
bool pressed = false;
unsigned long pressTime = 0;

bool Blink = false;                   
unsigned long blinkInterval = 0;      
unsigned long lastFlashStart = 0;
bool ledOn = false;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
}

void loop() {

  // Detect press start
  if (digitalRead(buttonPin) == LOW && !pressed) {
    pressed = true;
    pressTime = millis();
  }

  // Detect release and classify press
  if (digitalRead(buttonPin) == HIGH && pressed) {
    pressed = false;
    unsigned long duration = millis() - pressTime;

    if (duration <= SHORT_PRESS_TIME) {
      PressCounter++;
      Serial.print("Short press number: ");
      Serial.println(PressCounter);

      digitalWrite(ledPin, HIGH);    
      ledOn = true;
      lastFlashStart = millis();
    }
    else if (duration <= LONG_PRESS_TIME) {
      Serial.println("Medium press");

      if (PressCounter == 1) blinkInterval = 1000;
      else if (PressCounter == 2) blinkInterval = 2000;
      else blinkInterval = 3000;

      Blink = true;
      digitalWrite(ledPin, HIGH);
      ledOn = true;
      lastFlashStart = millis();
    }
    else {
      Serial.println("Long press - LED off");

      Blink = false;
      PressCounter = 0;
      digitalWrite(ledPin, LOW);
      ledOn = false;
    }
  }

  // Turn LED off after 500ms
  if (ledOn && (millis() - lastFlashStart >= interval)) {
    digitalWrite(ledPin, LOW);
    ledOn = false;
  }

  // Periodic blinking
  if (Blink && !ledOn && (millis() - lastFlashStart >= blinkInterval)) {
    digitalWrite(ledPin, HIGH);
    ledOn = true;
    lastFlashStart = millis();
  }
}
/**/

#include <Arduino.h>
#include <avr/interrupt.h>

const int buttonPin = 2;
const int ledPin = 10; 

// Press classification thresholds (ms)
const unsigned long SHORT_PRESS_TIME = 1500;
const unsigned long LONG_PRESS_TIME  = 4000;

// Simple debounce window inside ISR (ms)
const unsigned long DEBOUNCE_TIME = 50;

// Shared variables updated from ISR -> must be volatile
volatile int PressCounter = 0;
volatile unsigned long pressTime = 0;
volatile unsigned long lastISRTime = 0; 

// ISR sets "events"; loop() consumes them
volatile bool actionShort = false;
volatile bool actionStart = false; 
volatile bool actionStop = false;

// Main state (used only in loop, not volatile)
bool Blink = false; 
unsigned long lastFlashStart = 0;
bool feedbackLedOn = false;

// ==========================================
// INTERRUPTS
// ==========================================

// External interrupt on button (CHANGE):
// - Debounce using lastISRTime
// - Measure press duration using pressTime
// - Set event flags (actionShort / actionStart / actionStop)
void buttonISR() {
  unsigned long now = millis();
  if (now - lastISRTime < DEBOUNCE_TIME) return;
  lastISRTime = now;

  if (digitalRead(buttonPin) == HIGH) { 
    // Press start timestamp
    pressTime = now;
  } 
  else { 
    // Release: compute duration and classify press
    unsigned long duration = now - pressTime;

    if (duration < SHORT_PRESS_TIME) {
      // Count short presses only when not already blinking
      if (!Blink) { 
        PressCounter++;
        actionShort = true; 
      }
    } 
    else if (duration < LONG_PRESS_TIME) {
      // Medium press -> start blinking based on PressCounter
      actionStart = true; 
    } 
    else {
      // Long press -> stop everything
      actionStop = true;  
    }
  }
}

// Timer1 Compare Match ISR:
// Toggles LED every time OCR1A match occurs (blinking rate set in startTimer())
ISR(TIMER1_COMPA_vect) {
  digitalWrite(ledPin, !digitalRead(ledPin)); 
}

// Configure Timer1 in CTC mode to generate periodic interrupts.
// Prescaler 1024 -> base tick: 16MHz/1024 = 15625 Hz.
// OCR1A = (7812 * counts) - 1 gives ~0.5s * counts per toggle (approx).
void startTimer(int counts) {
  if (counts == 0) counts = 1;

  noInterrupts();
  TCCR1A = 0; TCCR1B = 0; TCNT1 = 0;

  OCR1A = (7812 * counts) - 1;

  // CTC (WGM12=1), prescaler 1024 (CS12=1, CS10=1)
  TCCR1B |= (1 << WGM12) | (1 << CS12) | (1 << CS10);

  // Enable Timer1 compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  interrupts();
}

// Stop Timer1 blinking and reset system state
void stopTimer() {
  TCCR1B = 0;
  TIMSK1 = 0;

  digitalWrite(ledPin, LOW);
  PressCounter = 0;
  Blink = false;
}

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, CHANGE);
  Serial.println("System Ready.");
}

void loop() {
  // Handle stop event (long press)
  if (actionStop) {
    actionStop = false;

    Serial.println("!!! SYSTEM SHUTDOWN !!!");
    Serial.println("The system is turning OFF now...");

    delay(100); 
    stopTimer();
  }

  // Handle short press event: feedback flash + counter print
  if (actionShort) {
    actionShort = false;
    Serial.print("Short press number: ");
    Serial.println(PressCounter);

    digitalWrite(ledPin, HIGH);
    feedbackLedOn = true;
    lastFlashStart = millis();
  }

  // End the feedback flash (non-blocking)
  if (feedbackLedOn && (millis() - lastFlashStart >= 200)) {
    digitalWrite(ledPin, LOW);
    feedbackLedOn = false;
  }

  // Handle start event (medium press): start Timer1 blinking with PressCounter
  if (actionStart) {
    actionStart = false;

    if (!Blink && PressCounter > 0) {
      Serial.print("Medium press. Starting with: ");
      Serial.println(PressCounter);

      digitalWrite(ledPin, LOW);
      startTimer(PressCounter);
      Blink = true;
    }
  }
}
