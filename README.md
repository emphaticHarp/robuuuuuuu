# ATOM-1 ESP32 RC Car Project

This project runs on ESP32 (esp32dev board) in PlatformIO. It reads RC receiver PWM inputs, controls H-bridge motor drivers, and hosts a web interface for live monitoring and emergency stop.

## WiFi Connection
- Mode: Station mode (connects to external router/hotspot)
- SSID: `Airtel_Soumyajyoti wifi`
- Password: `12345678`

## Serial Monitor
- Baud rate: `115200`
- Shows only:
  - WiFi connection status
  - IP address

## Pins and Connections
| Function | ESP32 D Pin | GPIO | Notes |
|---|---|---|---|
| Receiver CH1 input | D34 | GPIO 34 | input-only pin |
| Receiver CH2 input | D35 | GPIO 35 | input-only pin |
| Motor left direction IN1 | D25 | GPIO 25 | H-bridge input |
| Motor left direction IN2 | D26 | GPIO 26 | H-bridge input |
| Motor right direction IN3 | D27 | GPIO 27 | H-bridge input |
| Motor right direction IN4 | D14 | GPIO 14 | H-bridge input (HSPI_CS conflict key note) |
| Motor left PWM ENA | D33 | GPIO 33 | LEDC channel 0 |
| Motor right PWM ENB | D32 | GPIO 32 | LEDC channel 1 |

## PWM Settings
- Frequency: `1000` Hz
- Resolution: `8` bits

## Software Behavior
- Read receiver pulses with `pulseIn(pin, HIGH, 25000)`
- Map receiver range `1000-2000` to motor range `-255..+255`
- Apply dead zone at `±20`
- Apply exponential smoothing (`alpha = 0.1`)
- Normalize motor outputs to avoid overdriving BO motors
- Emergency stop via web endpoint `/stop` and resume via `/start`

## Web UI
- Main point: `/`
- JSON data: `/data` returns `{"ch1":...,"ch2":...}`
- Stylish light theme with Chart.js

## Notes
- GPIO 34/35 are input-only; do not connect outputs to these pins.
- GPIO 14 can be used for HSPI; currently assigned to motor direction IN4, so avoid SPI usage.
- Ensure common ground between ESP32 and motor driver supply.
