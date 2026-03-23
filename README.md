# ATOM - RC Controller Monitor

A real-time RC receiver channel monitoring system for ESP32 with web-based dashboard, LED control, and motor management.

## Overview

ATOM is an ESP32-based project that monitors RC receiver channels in real-time through a beautiful web interface. It reads PPM signals from an RC receiver, controls LEDs based on channel inputs, and manages motor control with smooth acceleration.

## Features

- **Real-time Channel Monitoring**: Display all 4 RC channels (CH1, CH2, CH5, CH6) with live updates
- **Web Dashboard**: Beautiful ATOM-themed web interface with Chart.js visualization
- **LED Control**: 
  - Left LED (GPIO 23) - Blinks when CH5 is UP
  - Right LED (GPIO 21) - Blinks when CH5 is DOWN
  - CH6 LED (GPIO 22) - Toggles ON/OFF based on CH6 position
  - Brightness LED (GPIO 5) - PWM controlled based on throttle
- **Motor Control**: Dual motor control with smooth acceleration and steering
- **Min/Max Tracking**: Tracks minimum and maximum values for each channel
- **Responsive Design**: Works on desktop, tablet, and mobile devices

## Hardware Connections

### Receiver Inputs (PPM Signals)

| Channel | Function | GPIO Pin | ESP32 Pin |
|---------|----------|----------|-----------|
| CH1 | Steering | GPIO 34 | D34 |
| CH2 | Throttle | GPIO 35 | D35 |
| CH5 | LED Toggle (3-position) | GPIO 15 | D15 |
| CH6 | On/Off Toggle | GPIO 4 | D4 |

### LED Outputs

| LED | Function | GPIO Pin | ESP32 Pin | Type |
|-----|----------|----------|-----------|------|
| Left LED | Blink when CH5 UP | GPIO 23 | D23 | Digital |
| Right LED | Blink when CH5 DOWN | GPIO 21 | D21 | Digital |
| CH6 LED | Toggle ON/OFF | GPIO 22 | D22 | Digital |
| Brightness LED | Throttle indicator | GPIO 5 | D5 | PWM |

### Motor Control

| Motor | Function | IN Pin | EN Pin | ESP32 Pins |
|-------|----------|--------|--------|-----------|
| Left Motor | Forward/Reverse | IN1, IN2 | ENA | D25, D26, D33 |
| Right Motor | Forward/Reverse | IN3, IN4 | ENB | D27, D14, D32 |

**Detailed Motor Pins:**

| Signal | GPIO | ESP32 Pin | Purpose |
|--------|------|-----------|---------|
| IN1 | GPIO 25 | D25 | Left Motor Direction 1 |
| IN2 | GPIO 26 | D26 | Left Motor Direction 2 |
| IN3 | GPIO 27 | D27 | Right Motor Direction 1 |
| IN4 | GPIO 14 | D14 | Right Motor Direction 2 |
| ENA | GPIO 33 | D33 | Left Motor Speed (PWM Channel 0) |
| ENB | GPIO 32 | D32 | Right Motor Speed (PWM Channel 1) |

## Channel Behavior

### CH1 - Steering (GPIO 34)
- **Center**: ~1500 µs
- **Left**: < 1400 µs
- **Right**: > 1600 µs
- Controls left/right motor differential

### CH2 - Throttle (GPIO 35)
- **Neutral**: ~1500 µs
- **Forward**: > 1700 µs
- **Reverse**: < 1300 µs
- Controls motor speed and brightness LED

### CH5 - LED Toggle (GPIO 15) - 3-Position Switch
- **UP** (> 1750 µs): Left LED blinks
- **MIDDLE** (1250-1750 µs): Both LEDs off
- **DOWN** (< 1250 µs): Right LED blinks

### CH6 - On/Off Toggle (GPIO 4)
- **ON** (> 1750 µs): CH6 LED turns on
- **OFF** (< 1750 µs): CH6 LED turns off

## LED Behavior

### Left LED (GPIO 23)
- Blinks at 500ms interval when CH5 is UP
- Off when CH5 is MIDDLE or DOWN

### Right LED (GPIO 21)
- Blinks at 500ms interval when CH5 is DOWN
- Off when CH5 is MIDDLE or UP

### CH6 LED (GPIO 22)
- Solid ON when CH6 > 1750 µs
- Solid OFF when CH6 ≤ 1750 µs

### Brightness LED (GPIO 5) - PWM
- **100% brightness** (255 PWM): When car is stopped (throttle < 20)
- **60% brightness** (153 PWM): When car is moving

## Motor Control Logic

### Speed Calculation
```
throttle = map(CH2, 1000, 2000, -255, 255)
steering = map(CH1, 1000, 2000, -255, 255)

left_motor = throttle - steering
right_motor = throttle + steering
```

### Minimum Threshold
- Forward: PWM < 80 → set to 80 (prevents sluggish response)
- Backward: PWM > -80 → set to -80

### Smooth Acceleration
- Uses exponential smoothing with alpha = 0.1
- Prevents jerky motor movements

### Dead Zone
- Throttle: ±50 PWM (±100 µs)
- Steering: ±20 PWM

## Web Interface

### Dashboard Features
- **Header**: Shows ATOM branding and LED status badges
- **Chart Section**: Real-time bar chart of all 4 channels
- **Channel Cards**: Individual display for each channel with status
- **LED Status**: Summary of all LED states
- **Graph Stats**: Min/Max values for each channel

### Data Update Rate
- Updates every 500ms (2 Hz)
- Fetches from `/data` endpoint
- Real-time min/max tracking

## WiFi Configuration

Edit the following in `src/main.cpp`:
```cpp
const char* ssid = "Airtel_Soumyajyoti wifi";
const char* password = "12345678";
```

## API Endpoints

### GET /
Returns the ATOM web dashboard HTML page

### GET /data
Returns JSON with current channel values and LED status

**Response Format:**
```json
{
  "ch1": 1500,
  "ch2": 1500,
  "ch5": 1500,
  "ch6": 1500,
  "ledStatus": "OFF",
  "ch6Led": "OFF",
  "brightness": "100% (Stopped)",
  "motorStatus": "STOPPED"
}
```

## Building and Uploading

### Prerequisites
- PlatformIO CLI
- Python 3.x
- ESP32 board support

### Build
```bash
pio run
```

### Upload
```bash
pio run --target upload
```

### Monitor Serial Output
```bash
pio device monitor
```

## Serial Debug Output

The ESP32 outputs detailed debug information every 500ms:
- CH1/CH2 raw pulse values
- Throttle and steering calculations
- Smoothed motor values
- Motor PWM commands

## Memory Usage

- **RAM**: ~13.8% (45,256 / 327,680 bytes)
- **Flash**: ~61.2% (801,781 / 1,310,720 bytes)

## Troubleshooting

### No Channel Values Showing
- Check receiver connections to GPIO pins
- Verify WiFi connection
- Check serial output for timeout messages

### LEDs Not Responding
- Verify GPIO pin connections
- Check LED polarity (anode to GPIO, cathode to GND)
- Test with digitalWrite in serial monitor

### Motors Not Moving
- Check motor driver connections
- Verify motor power supply
- Test motor pins individually

### Web Dashboard Not Loading
- Ensure ESP32 is connected to WiFi
- Check IP address in serial output
- Verify firewall allows port 80

## Project Structure

```
robuuuuuuu/
├── src/
│   └── main.cpp           # Main firmware code
├── include/               # Header files
├── lib/                   # Libraries
├── platformio.ini         # PlatformIO configuration
├── README.md              # This file
├── web.html               # Original web design reference
└── CONNECTIONS.md         # Detailed connection guide
```

## License

This project is open source and available on GitHub: https://github.com/emphaticHarp/robuuuuuuu.git

## Author

Created for RC vehicle monitoring and control system.

---

**Last Updated**: March 2026
**Firmware Version**: 1.0
**ESP32 Board**: ESP32 Dev Module
