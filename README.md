# Km58
Encoder ino code
# KM58 BiSS-C Absolute Encoder Reader for ESP32

A working Arduino sketch for reading the **KM58 single-turn absolute encoder** using the **BiSS-C protocol** on an **ESP32 FireBeetle 2** microcontroller.

This code was developed as part of a marine autopilot project and is shared freely for the community. BiSS-C on ESP32 is poorly documented — hopefully this saves someone a lot of time.

---

## What It Does

- Reads absolute position from a KM58 encoder (0–360 degrees)
- 24-bit resolution (16,777,216 counts per revolution)
- Includes an EMA (Exponential Moving Average) filter to suppress electrical noise
- Prints angle to Serial Monitor every 100ms
- Handles invalid reads gracefully — recovers cleanly from noise

---

## Hardware Required

| Part | Detail |
|------|--------|
| Microcontroller | DFRobot FireBeetle 2 ESP32-E |
| Encoder | KM58 single-turn absolute encoder |
| RS422 converter | MAX490 or MAX491 IC |
| Power | 5V for encoder, 3.3V for ESP32 |

The KM58 outputs **RS422 differential signals**. The ESP32 is 3.3V single-ended. The MAX490/MAX491 converts between them. This converter is **essential** — do not connect the encoder directly to the ESP32.

---

## Wiring

```
ESP32 FireBeetle          MAX490 / MAX491           KM58 Encoder
─────────────────         ───────────────           ────────────

GPIO23 (CLK out) ──────── DE + RE + DI ────── A ── CLK+
                                         └─── B ── CLK-

GPIO15 (DATA in) ──────── RO           ────── A ── DATA+
                                         └─── B ── DATA-

3.3V ─────────────────── VCC
GND  ─────────────────── GND
```

### KM58 Connector Pinout

| Pin | Signal |
|-----|--------|
| 1 | VCC (5V) |
| 2 | GND |
| 3 | CLK+ |
| 4 | CLK- |
| 5 | DATA+ |
| 6 | DATA- |

> **Note:** Cable colours vary by manufacturer. Always verify with a multimeter before connecting power.

---

## Installation

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Install ESP32 board support via Board Manager
   - Add this URL in Preferences: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Board Manager → search ESP32 → Install
3. Select board: **FireBeetle-ESP32** (or ESP32 Dev Module)
4. Download `KM58_BissC_ESP32.ino` from this repo
5. Open in Arduino IDE and upload

---

## Serial Monitor Output

Set baud rate to **115200**

```
KM58 BiSS-C Encoder Reader
ESP32 FireBeetle 2
--------------------------
Encoder OK: 179.75 degrees

Reading every 100ms...
Format: raw_counts | angle_degrees | status

Raw: 8384512  |  Angle: 179.75 deg  |  OK
Raw: 8384514  |  Angle: 179.75 deg  |  OK
Raw: 8384498  |  Angle: 179.75 deg  |  OK
```

---

## EMA Filter Tuning

The EMA (Exponential Moving Average) filter smooths out electrical noise, especially important when a motor is running nearby.

```cpp
float EMA_ALPHA = 0.3f;   // Adjust this value
```

| Value | Effect |
|-------|--------|
| 0.1 | Heavy smoothing — noisy environment |
| 0.3 | Default — good balance |
| 0.5 | Light smoothing — clean environment |
| 1.0 | No filter — raw encoder output |

---

## Important Notes

- **Keep encoder cable away from motor wires** — BiSS-C is a fast serial signal susceptible to motor EMI. Separate by at least 50mm where possible.
- Use **shielded cable** if the motor is nearby. Connect shield to GND at the ESP32 end only.
- The KM58 is an **absolute encoder** — it knows its position immediately on power-up. No homing sequence needed.

---

## Background

This code was developed for a marine autopilot system using a VESC motor controller to drive a rudder servo. The KM58 encoder reads rudder position directly from the shaft. The full autopilot project includes:

- BiSS-C encoder position feedback
- VESC motor control via UART
- BLE phone app control
- GPS and compass heading for auto mode
- No-laptop boot sequence with audio guidance

The encoder portion is shared here as a standalone sketch for anyone needing BiSS-C on ESP32.

---

## Protocol Reference

BiSS-C (Bidirectional Serial Synchronous Communication) is an open encoder protocol. One read cycle:

1. Master holds CLK HIGH (idle)
2. Master sends clock pulses
3. Encoder responds with ACK (LOW) then STR (HIGH)
4. 24 position bits follow, MSB first
5. Data sampled on RISING edge of CLK

---

## Licence

Free to use, modify and share. Credit appreciated but not required.

---

## Author

Developed with the assistance of **Claude AI (Anthropic)**.  
Tested on a working marine autopilot installation, May 2026.
