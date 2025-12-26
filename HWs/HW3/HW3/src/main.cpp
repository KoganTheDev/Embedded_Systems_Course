#include <Arduino.h>

const int buttonPin = 2;
const int ledPin = 10; 

const unsigned long SHORT_PRESS_TIME = 1500; // Max duration for a "Short" press
const unsigned long LONG_PRESS_TIME = 4000; // Threshold between "Long" and "Very Long"
const unsigned long DEBOUNCE_TIME = 50;   // Noise suppression for button ISR

volatile int PressCounter = 0; // Stores number of short presses
volatile unsigned long pressTime = 0; // Timestamp of button press start
volatile unsigned long lastISRTime = 0; // For software debouncing
volatile unsigned long lastDuration = 0; // Stores duration of the last released press
volatile bool actionShort = false; // Flag: Short press detected
volatile bool actionStart = false; // Flag: Long press detected (Confirm)
volatile bool actionStop = false; // Flag: Very long press detected (Off)

bool Blink            = false; // System status: currently blinking or not
bool feedbackLedOn    = false; // Status of the 200ms visual feedback flash
unsigned long lastFlashStart = 0; // Timestamp to track feedback flash duration

/**
 * buttonISR: External Interrupt Service Routine
 * Triggered on CHANGE (Pin 2). Measures high-pulse duration to classify press types.
 */
void buttonISR()
{
    unsigned long now = millis();
    if (now - lastISRTime < DEBOUNCE_TIME) { return; }
    lastISRTime = now;

    if (digitalRead(buttonPin) == HIGH)
    { 
        // Button Pressed: record start time
        pressTime = now;
    } 
    else
    { 
        // Button Released: calculate duration
        unsigned long duration = now - pressTime;
        lastDuration = duration;

        if (duration < SHORT_PRESS_TIME)
        {
            // Classify as Short Press
            if (!Blink)
            { 
                PressCounter++;
                actionShort = true; 
            }
        } 
        else if (duration < LONG_PRESS_TIME)
        {
            // Classify as Long Press (1.5s - 4.0s)
            actionStart = true; 
        } 
        else
        {
            // Classify as Very Long Press (> 4.0s)
            actionStop = true;  
        }
    }
}

/**
 * Timer1 Compare Match ISR
 * Toggles the LED state based on hardware timer frequency.
 */
ISR(TIMER1_COMPA_vect)
{
    if (Blink) 
    {
        digitalWrite(ledPin, !digitalRead(ledPin)); 
    }
}

/**
 * startTimer: Configures Timer1 in CTC Mode
 * @param counts: Number of 500ms increments.
 * Sets the OCR1A register based on 16MHz clock and 1024 prescaler.
 */
void startTimer(int counts)
{
    if (counts <= 0) counts = 1;

    noInterrupts();
    TCCR1A = 0; 
    TCCR1B = 0; 
    TCNT1 = 0;

    // Formula: OCR1A = (16MHz / (prescaler * toggle_frequency)) - 1
    // 7812 ticks = 0.5 seconds at 1024 prescaler.
    OCR1A = (7812 * counts) - 1;

    // WGM12 = CTC Mode, CS12/CS10 = 1024 Prescaler
    TCCR1B |= (1 << WGM12) | (1 << CS12) | (1 << CS10);
    
    // Enable Timer Compare Interrupt
    TIMSK1 |= (1 << OCIE1A);
    interrupts();
}

/**
 * stopTimer: Completely disables Timer1 and resets system state
 * Ensures the LED is physically LOW and registers are cleared.
 */
void stopTimer()
{
    noInterrupts();
    TIMSK1 &= ~(1 << OCIE1A); // Disable timer interrupt
    TCCR1B = 0; // Stop clock
    TCCR1A = 0; // Reset control
    TCNT1  = 0; // Reset counter
    
    digitalWrite(ledPin, LOW); 
    PressCounter  = 0;
    Blink         = false;
    feedbackLedOn = false; 
    interrupts();
}

/**
 * setup: Standard Arduino initialization
 */
void setup()
{
    Serial.begin(9600);
    Serial.println("System Ready. Wiring: Pin 2 (Button), Pin 10 (LED).");
    
    pinMode(buttonPin, INPUT);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);

    // Attach External Interrupt to Pin 2
    attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, CHANGE);
}

/**
 * loop: Main execution thread
 * Processes flags raised by the ISR and manages Serial communication.
 */
void loop()
{
    // --- Process OFF Event ---
    if (actionStop)
    {
        actionStop = false; 
        actionStart = false; // Prevent accidental start after stop
        Serial.print("OFF: Press duration ");
        Serial.print(lastDuration);
        Serial.println("ms. System Shutdown.");
        stopTimer();
    }

    // --- Process SHORT PRESS Event ---
    if (actionShort)
    {
        actionShort = false;
        Serial.print("SHORT PRESS: Counter = ");
        Serial.println(PressCounter);
        
        digitalWrite(ledPin, HIGH); // Start 200ms feedback
        feedbackLedOn = true;
        lastFlashStart = millis();
    }

    // --- Manage Feedback Flash Timeout ---
    if (feedbackLedOn && (millis() - lastFlashStart >= 200))
    {
        digitalWrite(ledPin, LOW);
        feedbackLedOn = false;
    }

    // --- Process START/CONFIRM Event ---
    if (actionStart)
    {
        actionStart = false;

        if (!Blink && PressCounter > 0)
        {      
            Serial.print("CONFIRMED: Blinking every ");
            Serial.print(PressCounter);
            Serial.println(" seconds.");
            Blink = true;
            startTimer(PressCounter);
        }
        else if (!Blink && PressCounter == 0)
        {
            Serial.println("ERROR: No counts recorded.");
        }
    }
}
