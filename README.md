# ESP32 Smart Garage Door Controller 🚪

Native garage door integration with the Google Home ecosystem using an ESP32 microcontroller and the SinricPro platform. This project supports voice commands, closed-position sensing, and eliminates the common logic level mismatch problem for 5V relays driven by 3.3V MCUs.

## Features

- 🗣️ **Voice Control:** Full integration with Google Assistant ("Hey Google, open the garage").
- 🔌 **Open-Drain Relay Control:** A software-based solution to the logic voltage mismatch (3.3V vs 5V). It prevents the 5V relay from remaining permanently triggered without the need for external logic level shifters.
- 🧲 **Magnetic Sensor Support:** Reads the closed end-stop state using hardware `INPUT_PULLUP` to prevent floating pins.
- 🔄 **Sensor-Driven Cloud Sync:** Google Home state follows the real sensor state, not just the requested command.
- 💡 **LED Status:** The built-in PCB LED indicates whether the door is not confirmed closed.

## Hardware Requirements

1. **ESP32** Development Board (e.g., NodeMCU-32S).
2. **Step-Down Converter** (e.g. LM2596S 3.2V-35V 3A)
3. **1-Channel Relay Module** (5V Coil, Low-Level Trigger).
4. **1x Magnetic Reed Switch** (e.g., KN-OC55) - optional.

## Wiring Diagram (Pinout)

| Component           | Component Pin | ESP32 Pin   | Notes                |
| :------------------ | :------------ | :---------- | :------------------- |
| **5V Relay**        | VCC           | OUT+        | StepDown Converter   |
|                     | GND           | OUT-        | StepDown Converter   |
|                     | IN (Signal)   | **GPIO 5**  | Control (Open-Drain) |
| **Sensor (CLOSED)** | Wire 1        | **GPIO 14** | `INPUT_PULLUP`       |
|                     | Wire 2        | GND         |                      |

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

## State Handling

- When sensors are enabled, the relay pulse only triggers the garage motor; the reported door state is still derived from the sensors.
- `CLOSED` is reported only when the closed reed switch is active.
- `OPEN` is reported when the closed reed switch is not active.
- With a single sensor, the sketch does not try to distinguish open and moving in the app; both are treated as `OPEN`.
- If a command does not match the actual sensor state, the sketch keeps the physical state as the source of truth.

## Serial Monitor Commands

After uploading, open the Serial Monitor at `115200` baud and send one of the commands below:

- `W` Show connected Wi-Fi network and IP address
- `S` Show SinricPro connection status
- `SE` Show reed sensor state (CLOSED only) and detection flags since boot
- `H` Show help
