# ESP32 Smart Garage Door Controller 🚪

Native garage door integration with the Google Home ecosystem using an ESP32 microcontroller and the SinricPro platform. This project supports voice commands, physical open/close state sensing, and eliminates the common logic level mismatch problem for 5V relays driven by 3.3V MCUs.

## Features
- 🗣️ **Voice Control:** Full integration with Google Assistant ("Hey Google, open the garage").
- 🔌 **Open-Drain Relay Control:** A software-based solution to the logic voltage mismatch (3.3V vs 5V). It prevents the 5V relay from remaining permanently triggered without the need for external logic level shifters.
- 🧲 **Magnetic Sensor Support:** Reads physical states (Open/Closed) using hardware `INPUT_PULLUP` to prevent floating pins.
- 💡 **LED Status:** The built-in PCB LED indicates whether the door is currently open.

## Hardware Requirements
1. **ESP32** Development Board (e.g., NodeMCU-32S).
2. **Step-Down Converter** (e.g. LM2596S 3.2V-35V 3A)
3. **1-Channel Relay Module** (5V Coil, Low-Level Trigger).
4. **2x Magnetic Reed Switches** (e.g., KN-OC55) - optional.

## Wiring Diagram (Pinout)

| Component | Component Pin | ESP32 Pin | Notes |
| :--- | :--- | :--- | :--- |
| **5V Relay** | VCC | OUT+ | StepDown Converter |
| | GND | OUT- | StepDown Converter |
| | IN (Signal) | **GPIO 5** | Control (Open-Drain) |
| **Sensor (CLOSED)** | Wire 1 | **GPIO 14** | `INPUT_PULLUP` |
| | Wire 2 | GND | |
| **Sensor (OPEN)** | Wire 1 | **GPIO 12** | `INPUT_PULLUP` |
| | Wire 2 | GND | |

## Dependencies
Install the following libraries via the Arduino IDE Library Manager:
- `SinricPro` (by Sinric)
- `WebSockets` (by Markus Sattler)

## Configuration
1. Clone or download the `SmartGarage.ino` code.
2. Register at [Sinric.pro](https://sinric.pro) and create a new **Garage Door** device.
3. Update the credentials in the code:
   - `WIFI_SSID` and `WIFI_PASS`
   - `APP_KEY`, `APP_SECRET`, and `GARAGE_DOOR_ID`
