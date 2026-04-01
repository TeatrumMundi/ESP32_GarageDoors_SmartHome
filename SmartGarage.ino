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
#define LED_BUILTIN       2   // Onboard Status LED

// Sensor usage configuration
#define USE_SENSOR_CLOSED true  // true = use CLOSED sensor to determine state

#define BAUD_RATE         115200

// ======================================================================
// GLOBAL STATE VARIABLES
// ======================================================================
bool isDoorClosed = true;           
bool lastSensorClosedState = HIGH;  
bool isSinricConnected = false;
bool closedSensorTriggered = false;

enum DoorPosition {
  DOOR_CLOSED,
  DOOR_OPEN,
  DOOR_MOVING,
  DOOR_UNKNOWN
};

DoorPosition currentDoorPosition = DOOR_CLOSED;

String serialCommandBuffer = "";

const bool SENSOR_MODE_ENABLED = USE_SENSOR_CLOSED;

// ======================================================================
// HELPER FUNCTIONS
// ======================================================================
void updateInternalLED() {
  // Built-in LED turns on when the door is OPEN (!isDoorClosed)
  digitalWrite(LED_BUILTIN, isDoorClosed ? LOW : HIGH);
}

void printHelp() {
  Serial.println("[Console] Available commands:");
  Serial.println("  W    Show connected Wi-Fi network");
  Serial.println("  S    Show SinricPro connection status");
  Serial.println("  SE   Show sensors status");
  Serial.println("  H    Show this help");
}

void printWiFiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[Console] Wi-Fi connected to: %s\n", WiFi.SSID().c_str());
    Serial.printf("[Console] IP Address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[Console] Wi-Fi is not connected.");
  }
}

void printSinricStatus() {
  Serial.printf("[Console] SinricPro status: %s\n", isSinricConnected ? "CONNECTED" : "DISCONNECTED");
}

const char* doorPositionToString(DoorPosition position) {
  switch (position) {
    case DOOR_CLOSED: return "CLOSED";
    case DOOR_OPEN: return "OPEN";
    case DOOR_MOVING: return "MOVING";
    default: return "UNKNOWN";
  }
}

void printSensorsStatus() {
  bool currentClosedActive = USE_SENSOR_CLOSED && (digitalRead(SENSOR_CLOSED_PIN) == LOW);

  Serial.println("[Console] Sensors status:");
  Serial.printf("  CLOSED sensor (GPIO %d): %s (%s)\n", SENSOR_CLOSED_PIN, currentClosedActive ? "ACTIVE" : "INACTIVE", USE_SENSOR_CLOSED ? "ENABLED" : "DISABLED");
  Serial.printf("  CLOSED sensor detected since boot: %s\n", closedSensorTriggered ? "YES" : "NO");
  Serial.printf("  Derived door position: %s\n", doorPositionToString(currentDoorPosition));
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();

    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      serialCommandBuffer.trim();
      serialCommandBuffer.toUpperCase();

      if (serialCommandBuffer.length() > 0) {
        if (serialCommandBuffer == "W") {
          printWiFiStatus();
        } else if (serialCommandBuffer == "S") {
          printSinricStatus();
        } else if (serialCommandBuffer == "SE") {
          printSensorsStatus();
        } else if (serialCommandBuffer == "H") {
          printHelp();
        } else {
          Serial.printf("[Console] Unknown command: %s\n", serialCommandBuffer.c_str());
          Serial.println("[Console] Type H for help.");
        }
      }

      serialCommandBuffer = "";
    } else {
      serialCommandBuffer += ch;
    }
  }
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

  // When sensors are enabled, cloud state must follow the physical position, not the requested one.
  if (!SENSOR_MODE_ENABLED) {
    isDoorClosed = state;
    updateInternalLED();
    return true;
  }

  bool closedActive = USE_SENSOR_CLOSED && (digitalRead(SENSOR_CLOSED_PIN) == LOW);
  SinricProGarageDoor &myGarageDoor = SinricPro[GARAGE_DOOR_ID];

  if (closedActive) {
    isDoorClosed = true;
    currentDoorPosition = DOOR_CLOSED;
    closedSensorTriggered = true;
    updateInternalLED();
    myGarageDoor.sendDoorStateEvent(true);
    Serial.println("[Sensors] Resync after command: CLOSED");
  } else {
    isDoorClosed = false;
    currentDoorPosition = DOOR_OPEN;
    Serial.println("[Sensors] Resync after command: OPEN");
    updateInternalLED();
    myGarageDoor.sendDoorStateEvent(false);
  }

  return true;
}

// ======================================================================
// SENSOR POLLING (ESP32 -> Google Home)
// ======================================================================
void handleSensors() {
  bool currentClosedState = USE_SENSOR_CLOSED ? digitalRead(SENSOR_CLOSED_PIN) : HIGH;
  bool closedActive = USE_SENSOR_CLOSED && (currentClosedState == LOW);
  SinricProGarageDoor &myGarageDoor = SinricPro[GARAGE_DOOR_ID];

  // Detect falling edge - Door has reached the CLOSED position
  if (USE_SENSOR_CLOSED && currentClosedState == LOW && lastSensorClosedState == HIGH) {
    Serial.println("[Sensors] Door is now CLOSED");
    isDoorClosed = true;
    currentDoorPosition = DOOR_CLOSED;
    closedSensorTriggered = true;
    updateInternalLED();
    myGarageDoor.sendDoorStateEvent(true);
  }

  // Detect release edge - Door is no longer in the CLOSED position
  if (USE_SENSOR_CLOSED && currentClosedState == HIGH && lastSensorClosedState == LOW) {
    Serial.println("[Sensors] Door is no longer CLOSED (reported as OPEN)");
    isDoorClosed = false;
    currentDoorPosition = DOOR_OPEN;
    updateInternalLED();
    myGarageDoor.sendDoorStateEvent(false);
  }

  isDoorClosed = closedActive;
  if (closedActive) {
    currentDoorPosition = DOOR_CLOSED;
  } else if (currentDoorPosition != DOOR_OPEN) {
    currentDoorPosition = DOOR_OPEN;
  }

  // Buffer states for the next cycle
  lastSensorClosedState = currentClosedState;
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

  SinricPro.onConnected([](){
    isSinricConnected = true;
    Serial.println("[SinricPro] Connected to cloud.");
  });
  SinricPro.onDisconnected([](){
    isSinricConnected = false;
    Serial.println("[SinricPro] Disconnected from cloud.");
  });
  
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
  if (USE_SENSOR_CLOSED) {
    pinMode(SENSOR_CLOSED_PIN, INPUT_PULLUP);
  }

  // Initialize current state from sensors if sensor mode is enabled.
  if (SENSOR_MODE_ENABLED) {
    if (USE_SENSOR_CLOSED && digitalRead(SENSOR_CLOSED_PIN) == LOW) {
      isDoorClosed = true;
      currentDoorPosition = DOOR_CLOSED;
      closedSensorTriggered = true;
    } else {
      isDoorClosed = false;
      currentDoorPosition = DOOR_OPEN;
    }
    updateInternalLED();
  }

  setupWiFi();
  setupSinricPro();
  printHelp();
}

// ======================================================================
// MAIN LOOP
// ======================================================================
void loop() {
  SinricPro.handle(); // Maintain WebSocket stack
  handleSensors();    // Poll digital inputs
  handleSerialCommands(); // Handle Serial Monitor commands
}