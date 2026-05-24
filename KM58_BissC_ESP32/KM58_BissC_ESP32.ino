// ============================================================
//
//   KM58 BiSS-C Absolute Encoder Reader for ESP32
//   Tested on: FireBeetle 2 ESP32-E (DFRobot)
//
//   Author:  Developed with Claude AI (Anthropic)
//   Forum:   Posted for community use - free to use and modify
//   Tested:  May 2026 - working on live autopilot installation
//
// ============================================================
//
//   ENCODER: KM58 Single-Turn Absolute Encoder
//            24-bit resolution (16,777,216 counts per revolution)
//            Interface: BiSS-C (serial, synchronous)
//            Output: 0-360 degrees absolute position
//
// ============================================================
//
//   WHAT IS BiSS-C?
//   BiSS-C (Bidirectional Serial Synchronous Communication)
//   is an open-source encoder protocol. The master (ESP32)
//   generates a clock and the encoder responds with its
//   absolute position as a serial bit stream.
//
//   It is NOT I2C, SPI or UART - it has its own protocol.
//   Data is read on the RISING edge of the clock.
//
// ============================================================
//
//   KM58 ELECTRICAL INTERFACE
//
//   The KM58 outputs RS422 differential signals:
//     CLK+  CLK-   (clock input to encoder)
//     DATA+ DATA-  (data output from encoder)
//
//   RS422 runs at +/- 5V differential. The ESP32 is 3.3V
//   single-ended. You MUST use a converter between them.
//
//   RECOMMENDED CONVERTER: MAX490 or MAX491
//   (MAX485 also works but is half-duplex - use with care)
//
// ============================================================
//
//   WIRING DIAGRAM
//
//   ESP32 FireBeetle          MAX490/MAX491          KM58 Encoder
//   ─────────────────         ─────────────          ────────────
//
//   GPIO23 (CLK out) ──────── DE + RE + DI ──┐
//                                              ├── A (CLK+) ──── CLK+
//                                              └── B (CLK-) ──── CLK-
//
//   GPIO15 (DATA in) ──────── RO             ─┐
//                                              ├── A (DATA+) ─── DATA+
//                                              └── B (DATA-) ─── DATA-
//
//   3.3V ─────────────────── VCC (if 3.3V part)
//   GND  ─────────────────── GND
//
//   KM58 Power:
//   5V  ──────────────────────────────────────────── VCC (pin 1)
//   GND ──────────────────────────────────────────── GND (pin 2)
//
// ============================================================
//
//   KM58 CONNECTOR PINOUT (standard - verify your cable)
//
//   Pin 1 = VCC  (5V)
//   Pin 2 = GND
//   Pin 3 = CLK+
//   Pin 4 = CLK-
//   Pin 5 = DATA+
//   Pin 6 = DATA-
//
//   Cable colours vary by manufacturer - always verify with
//   a datasheet or multimeter before connecting power.
//
// ============================================================
//
//   IMPORTANT NOTES
//
//   1. Keep encoder cable away from motor wires - BiSS-C is
//      a fast serial signal and susceptible to motor EMI.
//      Separate by at least 50mm where possible.
//
//   2. Use shielded cable for encoder if motor is nearby.
//      Connect shield to GND at the ESP32 end only.
//
//   3. EMA (Exponential Moving Average) filter is included.
//      This smooths out any noise on the encoder signal.
//      Adjust EMA_ALPHA to tune:
//        0.1 = heavy smoothing (noisy environment)
//        0.3 = default (good balance)
//        0.5 = light smoothing (clean environment)
//        1.0 = no filtering (raw encoder output)
//
//   4. The encoder outputs ABSOLUTE position - it knows its
//      position immediately on power-up with no homing needed.
//
// ============================================================
//
//   PROTOCOL DETAIL (BiSS-C single cycle read)
//
//   1. Hold CLK HIGH for >10us (idle state)
//   2. Clock pulses sent by master
//   3. Encoder responds with:
//      - ACK bit (LOW)
//      - STR bit (HIGH - start of data)
//      - CDS bit (skip)
//      - 24 data bits MSB first (position)
//      - Error/warning bits (not used here)
//   4. Data sampled on RISING edge of CLK
//
// ============================================================

// -------------------- PINS --------------------
const uint8_t PIN_CLK  = 23;   // BiSS-C clock output to MAX490 DI
const uint8_t PIN_DATA = 15;   // BiSS-C data input from MAX490 RO

// -------------------- EMA FILTER --------------------
float    encoderAngle  = 0.0f;   // Filtered angle output (degrees 0-360)
float    filteredAngle = 0.0f;   // EMA accumulator
bool     encoderValid  = false;  // True if last read was successful
float    EMA_ALPHA     = 0.3f;   // Filter coefficient (see notes above)

// ============================================================
//   BISS-C LOW LEVEL FUNCTIONS
// ============================================================

// Clock one bit and return the data line state
inline int clockBit() {
  digitalWrite(PIN_CLK, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_CLK, HIGH);
  delayMicroseconds(5);
  return digitalRead(PIN_DATA);
}

// Initialise BiSS-C bus - call once at startup
void bisscInit() {
  digitalWrite(PIN_CLK, HIGH);
  delayMicroseconds(40);
  clockBit();
  clockBit();
  digitalWrite(PIN_CLK, HIGH);
  delayMicroseconds(40);
}

// Read raw 24-bit position from encoder
// Returns 0 to 16,777,215 on success
// Returns -1 on failure (no ACK or no STR)
long readEncoderRaw() {
  digitalWrite(PIN_CLK, HIGH);
  delayMicroseconds(40);

  // Wait for ACK - encoder pulls data LOW to acknowledge
  bool ackFound = false;
  for (int i = 0; i < 64; i++) {
    if (clockBit() == 0) { ackFound = true; break; }
  }
  if (!ackFound) return -1;

  // Wait for STR - encoder pulls data HIGH to signal data start
  bool strFound = false;
  for (int i = 0; i < 32; i++) {
    if (clockBit() == 1) { strFound = true; break; }
  }
  if (!strFound) return -1;

  // Skip CDS bit
  clockBit();

  // Read 24 data bits MSB first
  uint32_t position = 0;
  for (int i = 0; i < 24; i++) {
    position = (position << 1) | clockBit();
  }

  return (long)position;
}

// ============================================================
//   MAIN ENCODER UPDATE - CALL THIS FROM loop()
//   Updates encoderAngle (filtered, degrees) and encoderValid
// ============================================================
void updateEncoder() {
  long raw = readEncoderRaw();

  if (raw >= 0) {
    // Convert 24-bit count to degrees
    float newAngle = (raw / 16777216.0f) * 360.0f;

    if (!encoderValid) {
      // First valid read after power-on or recovery from noise:
      // seed filter with actual position - no lag catching up
      filteredAngle = newAngle;
    } else {
      // Exponential moving average smoothing
      filteredAngle = EMA_ALPHA * newAngle + (1.0f - EMA_ALPHA) * filteredAngle;
    }

    encoderAngle = filteredAngle;
    encoderValid = true;

  } else {
    // Read failed - mark invalid but preserve filteredAngle
    // so recovery starts from last known good position
    encoderValid = false;
  }
}

// ============================================================
//   SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("KM58 BiSS-C Encoder Reader");
  Serial.println("ESP32 FireBeetle 2");
  Serial.println("--------------------------");

  // Configure pins
  pinMode(PIN_CLK,  OUTPUT);
  pinMode(PIN_DATA, INPUT);
  digitalWrite(PIN_CLK, HIGH);   // Idle state

  // Initialise BiSS-C bus
  bisscInit();
  delay(100);

  // First read
  updateEncoder();
  if (encoderValid) {
    Serial.print("Encoder OK: ");
    Serial.print(encoderAngle, 2);
    Serial.println(" degrees");
  } else {
    Serial.println("Encoder: No response - check wiring");
    Serial.println("  GPIO23 -> MAX490 DI (CLK)");
    Serial.println("  GPIO15 -> MAX490 RO (DATA)");
  }

  Serial.println();
  Serial.println("Reading every 100ms...");
  Serial.println("Format: raw_counts | angle_degrees | status");
  Serial.println();
}

// ============================================================
//   LOOP - reads and prints encoder every 100ms
// ============================================================
void loop() {
  updateEncoder();

  if (encoderValid) {
    // Print raw count and filtered angle
    long raw = readEncoderRaw();   // Second read for display only
    Serial.print("Raw: ");
    if (raw >= 0) {
      Serial.print(raw);
    } else {
      Serial.print("---");
    }
    Serial.print("  |  Angle: ");
    Serial.print(encoderAngle, 2);
    Serial.print(" deg  |  OK");
  } else {
    Serial.print("Encoder INVALID - check connections");
  }

  Serial.println();
  delay(100);
}
