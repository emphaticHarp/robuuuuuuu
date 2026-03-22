# ESP32 RC Car - Complete Connection Guide (L298N Motor Driver)

## All Pin Connections for gkg Project

---

## RECEIVER CHANNEL INPUTS (from RC Receiver)

| Channel | GPIO | D-Pin | Function | Signal Type |
|---------|------|-------|----------|-------------|
| CH1 | 34 | D34 | Steering Control | PWM Input (1000-2000µs) |
| CH2 | 35 | D35 | Throttle Control | PWM Input (1000-2000µs) |
| CH5 | 15 | D15 | LED 3-Position Toggle | PWM Input (1000-2000µs) |
| CH6 | 16 | D4 | LED On/Off Toggle | PWM Input (1000-2000µs) |

**Note:** Receiver GND must be connected to ESP32 GND

---

## MOTOR CONNECTIONS (L298N Motor Driver)

### L298N Module Pin Connections
The L298N module has the following pins:
- **+5V** → 5V Power Supply
- **GND** → Ground (Common with ESP32)
- **+12V** → Motor Power Supply (if separate from logic power)
- **OUT1, OUT2** → Left Motor terminals
- **OUT3, OUT4** → Right Motor terminals

### Direction Control Pins (Digital Output from ESP32)
| Motor | L298N Pin | ESP32 GPIO | D-Pin | Function |
|-------|-----------|-----------|-------|----------|
| Left Motor | IN1 | 25 | D25 | Left Motor Direction Pin 1 |
| Left Motor | IN2 | 26 | D26 | Left Motor Direction Pin 2 |
| Right Motor | IN3 | 27 | D27 | Right Motor Direction Pin 1 |
| Right Motor | IN4 | 14 | D14 | Right Motor Direction Pin 2 |

### Speed Control Pins (PWM Output - LEDC)
| Motor | L298N Pin | ESP32 GPIO | D-Pin | PWM Channel | Frequency | Resolution |
|-------|-----------|-----------|-------|-------------|-----------|------------|
| Left Motor | ENA | 33 | D33 | Channel 0 | 1000 Hz | 8-bit (0-255) |
| Right Motor | ENB | 32 | D32 | Channel 1 | 1000 Hz | 8-bit (0-255) |

**L298N Motor Control Logic:**
- IN1=HIGH, IN2=LOW → Left Motor Forward
- IN1=LOW, IN2=HIGH → Left Motor Backward
- IN1=LOW, IN2=LOW → Left Motor Stop
- ENA (0-255) → Controls speed with PWM (0=stop, 255=full speed)

---

## LED CONNECTIONS (Signal Outputs)

| LED Name | GPIO | D-Pin | Function | Control Type | Behavior |
|----------|------|-------|----------|--------------|----------|
| LEFT_LED | 13 | D13 | Left Blink LED | Digital ON/OFF | Blinks (500ms) when CH5=UP |
| RIGHT_LED | 2 | D2 | Right Blink LED | Digital ON/OFF | Blinks (500ms) when CH5=DOWN |
| CH6_LED | 19 | D19 | Toggle LED | Digital ON/OFF | ON when CH6>1750µs, OFF otherwise |
| BRIGHTNESS_LED | 5 | D18 | Brightness LED | PWM (LEDC Ch2) | 100% = stopped, 60% = moving |

**Motor Wiring:**
- Connect ESP32 GND to L298N GND (common ground)
- Connect ESP32 +5V to L298N +5V (logic power)
- Connect battery +12V to L298N +12V
- Connect battery GND to L298N GND
- Connect motors to OUT1/OUT2 (left) and OUT3/OUT4 (right)

**LED Wiring:**
- Connect LED positive (+) to power through a 1kΩ resistor
- Connect LED negative (-) to the GPIO pin (configured as output)
- Alternative: Connect GPIO pin to transistor base for higher power LEDs

---

## CHANNEL MAPPING & BEHAVIOR

### CH1 - Steering Control (GPIO 34)
```
1000µs → Turn Left (-255)
1500µs → Center/Neutral (0)
2000µs → Turn Right (+255)
```
- Controls both motors in opposite directions
- Left motor: throttle - steering
- Right motor: throttle + steering

### CH2 - Throttle Control (GPIO 35)
```
1000µs → Full Forward (255)
1500µs → Center/Neutral (0)
2000µs → Full Backward (-255)
```
- Controls both motors together
- Also controls brightness LED:
  - If throttle = 0: Brightness = 100% (255)
  - If throttle ≠ 0: Brightness = 60% (153)

### CH5 - LED 3-Position Toggle (GPIO 12)
```
< 1250µs → DOWN Position: Right LED blinks (500ms interval)
1250-1750µs → MIDDLE Position: Both LEDs OFF
> 1750µs → UP Position: Left LED blinks (500ms interval)
```

### CH6 - LED On/Off Toggle (GPIO 16)
```
> 1750µs → ON: CH6_LED turns ON
≤ 1750µs → OFF: CH6_LED turns OFF
```

---

## SUMMARY OF CONNECTIONS

### TOTAL PINS USED: 16

**Input Pins (4):**
- D34 (CH1 Receiver)
- D35 (CH2 Receiver)
- D8 (CH5 Receiver)
- D4 (CH6 Receiver)

**Motor Control Output Pins (6):**
- D25, D26 (Left Motor Direction)
- D27, D14 (Right Motor Direction)
- D33, D32 (Motor PWM)

**LED Output Pins (4):**
- D13 (Left LED)
- D2 (Right LED)
- D19 (CH6 Toggle LED)
- D18 (Brightness LED - PWM)

**Power & Signal:**
- 5V → H-Bridge, LEDs (through resistors)
- GND → All devices (common ground)
- 3.3V from ESP32 → Receiver (optional if receiver powered separately)

---

## QUICK REFERENCE - GPIO to D-PIN MAPPING

| GPIO | D-Pin | Function |
|------|-------|----------|
| 2 | D2 | RIGHT_LED output |
| 4 | D4 | CH6 receiver input |
| 5 | D18 | BRIGHTNESS_LED PWM output |
| 15 | D15 | CH5 receiver input |
| 13 | D13 | LEFT_LED output |
| 14 | D14 | Motor IN4 output |
| 16 | D4 | CH6 receiver input |
| 19 | D19 | CH6_LED output |
| 25 | D25 | Motor IN1 output |
| 26 | D26 | Motor IN2 output |
| 27 | D27 | Motor IN3 output |
| 32 | D32 | Motor ENB PWM output |
| 33 | D33 | Motor ENA PWM output |
| 34 | D34 | CH1 receiver input |
| 35 | D35 | CH2 receiver input |

---

## IMPORTANT NOTES

1. **GPIO 34 & 35 are input-only** - Cannot be used for output
2. **L298N Power Requirements:**
   - Logic Power: +5V (can be from ESP32 or separate source)
   - Motor Power: +12V (separate battery recommended for stable motor operation)
   - **CRITICAL: Connect all GNDs together** (ESP32 GND = L298N GND = Battery GND)
3. **PWM Frequency** - All outputs use 1000 Hz for smooth control
4. **Dead Zone** - ±20 units on steering and throttle to prevent drift
5. **Signal Lines** - Keep receiver signal wires short to avoid interference
6. **Smoothing** - Motor values smoothed with alpha=0.1 for gradual acceleration
7. **L298N Heat** - Motor driver may get warm; ensure adequate ventilation
8. **Motor Current** - L298N can handle up to 2A per motor channel (36W max)

---

## WIRING DIAGRAM TEXT REPRESENTATION

```
RC RECEIVER
    ↓
  CH1 → D34 (GPIO34) → Steering Input
  CH2 → D35 (GPIO35) → Throttle Input
  CH5 → D15 (GPIO15) → LED Toggle Input
  CH6 → D4  (GPIO16) → LED On/Off Input
  GND → GND (Common Ground)

L298N MOTOR DRIVER
    ↓
  L298N Power Connections:
    +5V → L298N +5V
    GND → L298N GND (Common with ESP32 and Battery)
    +12V Battery → L298N +12V
  
  Left Motor Control:
    D25 (GPIO25) → IN1
    D26 (GPIO26) → IN2
    D33 (GPIO33) → ENA (PWM Speed)
    Left Motor → OUT1 & OUT2
  
  Right Motor Control:
    D27 (GPIO27) → IN3
    D14 (GPIO14) → IN4
    D32 (GPIO32) → ENB (PWM Speed)
    Right Motor → OUT3 & OUT4

LEDs
    ↓
  D13 (GPIO13) → LEFT_LED (100mA max)
  D2  (GPIO2)  → RIGHT_LED (100mA max)
  D19 (GPIO19) → CH6_LED (100mA max)
  D18 (GPIO5)  → BRIGHTNESS_LED (PWM, 100mA max)
```

---

**Last Updated:** March 22, 2026
**Project:** gkg (RC Car Multi-LED Controller)
