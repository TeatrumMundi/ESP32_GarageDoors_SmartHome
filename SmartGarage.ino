/**
 * @file SmartGarage.ino
 * @brief ESP32 Garage Door Controller integrated with Google Home via SinricPro.
 * * This project utilizes an Open-Drain topology to drive a 5V relay module 
 * using 3.3V logic, effectively eliminating leakage currents across the optocoupler.
 */

#include <Arduino.h>
#include <WiFi.h>
#include "SinricPro.h"
#include "SinricProGarageDoor.h"

// ======================================================================
// USER CONFIGURATION (FILL IN BEFORE UPLOADING)
// ======================================================================
#define WIFI_SSID         "YOUR_WIFI_SSID"
#define WIFI_PASS         "YOUR_WIFI_PASSWORD"

#define APP_KEY           "YOUR_APP_KEY"       // From SinricPro dashboard
#define APP_SECRET        "YOUR_APP_SECRET"    // From SinricPro dashboard
#define GARAGE_DOOR_ID    "YOUR_DEVICE_ID"     // From SinricPro dashboard

// ======================================================================
// HARDWARE PIN MAPPING
// ======================================================================
#define RELAY_PIN         5   // Relay Control (Open-Drain)
#define SENSOR_CLOSED_PIN 14  // CLOSED Reed Switch (Active Low)
#define SENSOR_OPEN_PIN   12  // OPEN Reed Switch (Active Low)
#define LED_BUILTIN       2   // Onboard Status LED

#define BAUD_RATE         115200

// ======================================================================
// GLOBAL STATE VARIABLES
// ======================================================================
bool isDoorClosed = true;           
bool lastSensorClosedState = HIGH;  
bool lastSensorOpenState = HIGH;    

// ======================================================================
// HELPER FUNCTIONS
// ======================================================================
void updateInternalLED() {
  // Built-in LED turns on when the door is OPEN (!isDoorClosed)
  digitalWrite(LED_BUILTIN, isDoorClosed ? LOW : HIGH);
}

// ======================================================================
// CALLBACKS (Google Home -> ESP32)
// ======================================================================
bool onDoorState(const String &deviceId, bool &state) {
  Serial.printf("[SinricPro] Command received: %s\n", state ? "CLOSE" : "OPEN");

  // Generate a 500ms pulse for the garage motor controller's dry contact input
  digitalWrite(RELAY_PIN, LOW);   // Low state: Relay triggered
  delay(500);                     
  digitalWrite(RELAY_PIN, HIGH);  // High-Impedance (Hi-Z) state: Relay released

  isDoorClosed = state;
  updateInternalLED();

  return true; 
}

// ======================================================================
// SENSOR POLLING (ESP32 -> Google Home)
// ======================================================================
void handleSensors() {
  bool currentClosedState = digitalRead(SENSOR_CLOSED_PIN);
  bool currentOpenState = digitalRead(SENSOR_OPEN_PIN);
  SinricProGarageDoor &myGarageDoor = SinricPro[GARAGE_DOOR_ID];

  // Detect falling edge - Door has reached the CLOSED position
  if (currentClosedState == LOW && lastSensorClosedState == HIGH) {
    Serial.println("[Sensors] Door is now CLOSED");
    isDoorClosed = true;
    updateInternalLED();
    myGarageDoor.sendDoorStateEvent(true);
  }

  // Detect falling edge - Door has reached the OPEN position
  if (currentOpenState == LOW && lastSensorOpenState == HIGH) {
    Serial.println("[Sensors] Door is now OPEN");
    isDoorClosed = false;
    updateInternalLED();
    myGarageDoor.sendDoorStateEvent(false);
  }

  // Buffer states for the next cycle
  lastSensorClosedState = currentClosedState;
  lastSensorOpenState = currentOpenState;
}

// ======================================================================
// SYSTEM SETUP
// ======================================================================
void setupWiFi() {
  Serial.printf("[Network] Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    printf(".");
    delay(500);
  }
  Serial.printf("\n[Network] Connected. IP Address: %s\n", WiFi.localIP().toString().c_str());
}

void setupSinricPro() {
  SinricProGarageDoor &myGarageDoor = SinricPro[GARAGE_DOOR_ID];
  myGarageDoor.onDoorState(onDoorState);

  SinricPro.onConnected([](){ Serial.println("[SinricPro] Connected to cloud."); }); 
  SinricPro.onDisconnected([](){ Serial.println("[SinricPro] Disconnected from cloud."); });
  
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup() {
  Serial.begin(BAUD_RATE);
  
  // Configure relay pin in Open-Drain mode to prevent 3.3V/5V logic mismatch
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(RELAY_PIN, HIGH); // Initialize in High-Impedance (Hi-Z) state

  pinMode(LED_BUILTIN, OUTPUT);
  updateInternalLED();

  // Internal pull-up resistors to prevent floating logic states
  pinMode(SENSOR_CLOSED_PIN, INPUT_PULLUP);
  pinMode(SENSOR_OPEN_PIN, INPUT_PULLUP);

  setupWiFi();
  setupSinricPro();
}

// ======================================================================
// MAIN LOOP
// ======================================================================
void loop() {
  SinricPro.handle(); // Maintain WebSocket stack
  handleSensors();    // Poll digital inputs
}
