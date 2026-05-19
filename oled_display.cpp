/*
  SIT210 Final Project — Health monitor

  MAX30102 (I2C):
    VCC -> 3.3 V or 5 V (per module)
    GND -> GND
    SDA -> A4
    SCL -> A5

  SSD1306 OLED 128x64 (I2C — same SDA/SCL as sensor):
    VCC -> 3.3 V (preferred)
    GND -> GND
    SDA -> A4
    SCL -> A5

  ESP8266 (if used): TX->D2, RX->D3, GND, 3.3 V

  Libraries: SparkFun MAX3010x, WiFiEsp, Adafruit SSD1306, Adafruit GFX

  Credentials: wifi_config.h
  Serial Monitor: 115200 baud
*/

#include <Wire.h>
#include <MAX30105.h>
#include "wifi_config.h"
#include "platform_connect.h"
#include "oled_display.h"

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  #define BOARD_LOW_RAM 1
#endif

#ifdef BOARD_LOW_RAM
  #include "heartRate.h"
#else
  #include "spo2_algorithm.h"
#endif

// One vitals read + Serial print + ThingSpeak upload every 5 s (finger must be present)
static const unsigned long READING_INTERVAL_MS = 5000UL;
// IR below this → no finger on the sensor window
static const uint32_t FINGER_IR_THRESHOLD = 50000UL;
static const unsigned long FINGER_IDLE_CHECK_MS = 300UL;

MAX30105 particleSensor;

static bool fingerPresent = false;
static bool nothingDetectedMsgShown = false;
static unsigned long lastReadingMs = 0;
static unsigned long lastOledMs = 0;
static const unsigned long OLED_REFRESH_MS = 500UL;

enum MeasureMode : uint8_t {
  MODE_BPM_ONLY = 1,
  MODE_SPO2_ONLY = 2,
  MODE_BOTH = 3,
};

static MeasureMode measureMode = MODE_BOTH;
static bool modeSelected = false;

static void resetVitalsState();
static void refreshOled();
static OledMeasureMode oledModeFromMeasure();

static void printMeasureMenu() {
  Serial.println(F("\n--- Select measurement ---"));
  Serial.println(F("  1 = Check BPM"));
  Serial.println(F("  2 = Check SpO2"));
  Serial.println(F("  3 = Check BOTH"));
#ifdef BOARD_LOW_RAM
  Serial.println(F("  (Uno: BPM only — options 2/3 use BPM.)"));
#endif
  Serial.print(F("Enter 1, 2, or 3: "));
}

static MeasureMode parseMeasureChoice(char c) {
  if (c == '1') return MODE_BPM_ONLY;
  if (c == '2') return MODE_SPO2_ONLY;
  if (c == '3') return MODE_BOTH;
  return MODE_BOTH;
}

static void printActiveMode() {
  Serial.print(F("Selected: "));
  switch (measureMode) {
    case MODE_BPM_ONLY:
      Serial.println(F("BPM only"));
      break;
    case MODE_SPO2_ONLY:
      Serial.println(F("SpO2 only"));
      break;
    default:
      Serial.println(F("BPM + SpO2"));
      break;
  }
}

static void applyMeasureMode(MeasureMode mode) {
#ifdef BOARD_LOW_RAM
  if (mode != MODE_BPM_ONLY) {
    Serial.println(F("SpO2 needs Mega/R4/ESP — using BPM only."));
  }
  measureMode = MODE_BPM_ONLY;
#else
  measureMode = mode;
#endif
  modeSelected = true;
  printActiveMode();
}

static void pollSerialModeChange() {
  if (!Serial.available()) {
    return;
  }
  const char c = Serial.peek();
  if (c != '1' && c != '2' && c != '3') {
    return;
  }
  (void)Serial.read();
  while (Serial.available()) {
    (void)Serial.read();
  }
  applyMeasureMode(parseMeasureChoice(c));
  resetVitalsState();
  lastReadingMs = 0;
  Serial.println(F("Mode changed."));
}

static void promptMeasureMode() {
  printMeasureMenu();
  const unsigned long deadline = millis() + 30000UL;
  while (!modeSelected && millis() < deadline) {
    if (Serial.available()) {
      const char c = Serial.read();
      while (Serial.available()) {
        (void)Serial.read();
      }
      if (c == '1' || c == '2' || c == '3') {
        applyMeasureMode(parseMeasureChoice(c));
        return;
      }
    }
    delay(10);
  }
  Serial.println(F("Timeout — default BOTH"));
  applyMeasureMode(MODE_BOTH);
}

#ifdef BOARD_LOW_RAM
  long lastBeatMs = 0;
  int32_t heartRate = 0;
  int8_t validHeartRate = 0;
  int32_t spo2 = 0;
  int8_t validSPO2 = 0;
#else
  #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
    uint16_t irBuffer[100];
    uint16_t redBuffer[100];
  #else
    uint32_t irBuffer[100];
    uint32_t redBuffer[100];
  #endif
  int32_t spo2 = 0;
  int8_t validSPO2 = 0;
  int32_t heartRate = 0;
  int8_t validHeartRate = 0;
  static const int32_t kBufferLength = 100;
  static uint8_t vitalsState = 0;
  static uint8_t vitalsIndex = 0;
  static uint8_t collectIndex = 0;
#endif

static void resetVitalsState() {
  heartRate = 0;
  spo2 = 0;
  validHeartRate = 0;
  validSPO2 = 0;
#ifdef BOARD_LOW_RAM
  lastBeatMs = 0;
#else
  vitalsState = 0;
  vitalsIndex = 0;
  collectIndex = 0;
#endif
}

static OledMeasureMode oledModeFromMeasure() {
  if (measureMode == MODE_BPM_ONLY) {
    return OLED_MODE_BPM;
  }
  if (measureMode == MODE_SPO2_ONLY) {
    return OLED_MODE_SPO2;
  }
  return OLED_MODE_BOTH;
}

static void refreshOled() {
  if (!oledIsReady()) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastOledMs < OLED_REFRESH_MS) {
    return;
  }
  lastOledMs = now;

  const OledMeasureMode oledMode = oledModeFromMeasure();
  const bool wifiOk = connectIsReady();

  if (!fingerPresent) {
    oledShowNoFinger();
    return;
  }

  const bool wantBpm =
      (measureMode == MODE_BPM_ONLY || measureMode == MODE_BOTH);
  const bool wantSpo2 =
      (measureMode == MODE_SPO2_ONLY || measureMode == MODE_BOTH);

  const bool bpmOk = wantBpm && validHeartRate;
  const bool spo2Ok = wantSpo2 && validSPO2;

  if (bpmOk || spo2Ok) {
    oledShowVitals(oledMode, heartRate, bpmOk, spo2, spo2Ok, wifiOk);
  } else {
    oledShowMeasuring(oledMode);
  }
}

static void onFingerDetected() {
  fingerPresent = true;
  nothingDetectedMsgShown = false;
  resetVitalsState();
  lastOledMs = 0;
  Serial.println(F("Finger detected — resuming readings."));
  refreshOled();
}

static void onFingerLost() {
  if (!fingerPresent) {
    return;
  }
  fingerPresent = false;
  nothingDetectedMsgShown = false;
  lastReadingMs = 0;
  resetVitalsState();
  lastOledMs = 0;
  Serial.println(F("Nothing detected — place finger on sensor."));
  oledShowNoFinger();
}

// Fresh IR check before any 5 s read / ThingSpeak upload
static bool confirmFingerPresent() {
  if (!particleSensor.available()) {
    particleSensor.check();
    return fingerPresent;
  }

  const uint32_t ir = particleSensor.getIR();
  particleSensor.getRed();
  particleSensor.nextSample();

  if (!irIndicatesFinger(ir)) {
    onFingerLost();
    return false;
  }
  return true;
}

static bool irIndicatesFinger(uint32_t ir) {
  return ir >= FINGER_IR_THRESHOLD;
}

// When idle, only poll IR occasionally until a finger appears
static bool waitForFinger() {
  if (fingerPresent) {
    return true;
  }

  static unsigned long lastCheckMs = 0;
  const unsigned long now = millis();
  if (now - lastCheckMs < FINGER_IDLE_CHECK_MS) {
    if (!nothingDetectedMsgShown) {
      nothingDetectedMsgShown = true;
      Serial.println(F("Nothing detected — place finger on sensor."));
      oledShowNoFinger();
    }
    return false;
  }
  lastCheckMs = now;

  if (!particleSensor.available()) {
    particleSensor.check();
    if (!nothingDetectedMsgShown) {
      nothingDetectedMsgShown = true;
      Serial.println(F("Nothing detected — place finger on sensor."));
      oledShowNoFinger();
    }
    return false;
  }

  const uint32_t ir = particleSensor.getIR();
  particleSensor.getRed();
  particleSensor.nextSample();

  if (irIndicatesFinger(ir)) {
    onFingerDetected();
    return true;
  }

  if (!nothingDetectedMsgShown) {
    nothingDetectedMsgShown = true;
    Serial.println(F("Nothing detected — place finger on sensor."));
    oledShowNoFinger();
  }
  return false;
}

#ifdef BOARD_LOW_RAM
static void processSensorLowRam() {
  if (!fingerPresent) {
    return;
  }

  if (!particleSensor.available()) {
    particleSensor.check();
    return;
  }

  const long ir = particleSensor.getIR();
  if (!irIndicatesFinger((uint32_t)ir)) {
    onFingerLost();
    particleSensor.getRed();
    particleSensor.nextSample();
    return;
  }

  // BPM Calculations
  if (checkForBeat(ir)) {
    const long delta = millis() - lastBeatMs;
    lastBeatMs = millis();
    if (delta > 0) {
      const float bpm = 60.0f / (delta / 1000.0f);
      if (bpm > 20.0f && bpm < 255.0f) {
        heartRate = (int32_t)bpm;
        validHeartRate = 1;
      }
    }
  }

  particleSensor.getRed();
  particleSensor.nextSample();
}
#else
static void processSensorFull() {
  if (!fingerPresent) {
    return;
  }

  if (vitalsState != 1) {
    if (!particleSensor.available()) {
      particleSensor.check();
      return;
    }
  }

  // SpO2 Calculations
  switch (vitalsState) {
    case 0:
      redBuffer[vitalsIndex] = particleSensor.getRed();
      irBuffer[vitalsIndex] = particleSensor.getIR();
      if (!irIndicatesFinger(irBuffer[vitalsIndex])) {
        onFingerLost();
        particleSensor.nextSample();
        return;
      }
      particleSensor.nextSample();
      vitalsIndex++;
      if (vitalsIndex >= 100) {
        maxim_heart_rate_and_oxygen_saturation(
            irBuffer, kBufferLength, redBuffer,
            &spo2, &validSPO2, &heartRate, &validHeartRate);
        vitalsState = 1;
      }
      break;
    case 1:
      for (uint8_t i = 25; i < 100; i++) {
        redBuffer[i - 25] = redBuffer[i];
        irBuffer[i - 25] = irBuffer[i];
      }
      collectIndex = 75;
      vitalsState = 2;
      break;
    case 2:
      redBuffer[collectIndex] = particleSensor.getRed();
      irBuffer[collectIndex] = particleSensor.getIR();
      if (!irIndicatesFinger(irBuffer[collectIndex])) {
        onFingerLost();
        particleSensor.nextSample();
        return;
      }
      particleSensor.nextSample();
      collectIndex++;
      if (collectIndex >= 100) {
        maxim_heart_rate_and_oxygen_saturation(
            irBuffer, kBufferLength, redBuffer,
            &spo2, &validSPO2, &heartRate, &validHeartRate);
        vitalsState = 1;
      }
      break;
    default:
      vitalsState = 0;
      vitalsIndex = 0;
      break;
  }
}
#endif

static void performReadingAndUpload() {
  if (!fingerPresent || !confirmFingerPresent()) {
    return;
  }

  Serial.println(F("--- Vitals reading (every 5 s) ---"));

  const bool wantBpm =
      (measureMode == MODE_BPM_ONLY || measureMode == MODE_BOTH);
  const bool wantSpo2 =
      (measureMode == MODE_SPO2_ONLY || measureMode == MODE_BOTH);

  bool ready = false;

#ifdef BOARD_LOW_RAM
  if (wantBpm) {
    if (!validHeartRate) {
      Serial.println(F("HR not valid yet — keep finger steady."));
      return;
    }
    Serial.print(F("HR="));
    Serial.print(heartRate);
    Serial.println(F(" BPM"));
    ready = true;
  }
#else
  if (wantBpm && wantSpo2) {
    if (!validHeartRate || !validSPO2) {
      Serial.println(F("Waiting for valid HR and SpO2 — keep finger steady."));
      return;
    }
    Serial.print(F("HR="));
    Serial.print(heartRate);
    Serial.print(F(" BPM  SpO2="));
    Serial.print(spo2);
    Serial.println(F(" %"));
    ready = true;
  } else if (wantBpm) {
    if (!validHeartRate) {
      Serial.println(F("HR not valid yet — keep finger steady."));
      return;
    }
    Serial.print(F("HR="));
    Serial.print(heartRate);
    Serial.println(F(" BPM"));
    ready = true;
  } else if (wantSpo2) {
    if (!validSPO2) {
      Serial.println(F("SpO2 not valid yet — keep finger steady."));
      return;
    }
    Serial.print(F("SpO2="));
    Serial.print(spo2);
    Serial.println(F(" %"));
    ready = true;
  }
#endif

  if (!ready) {
    return;
  }

  if (!connectIsReady()) {
    Serial.println(F("ThingSpeak skipped — Wi-Fi not connected."));
    return;
  }

  const int32_t hrUp = (wantBpm && validHeartRate) ? heartRate : 0;
  const int32_t spUp = (wantSpo2 && validSPO2) ? spo2 : 0;

  if (connectUploadVitals(hrUp, spUp)) {
    Serial.println(F("ThingSpeak write OK."));
  } else {
    Serial.println(F("ThingSpeak write failed."));
  }

  lastOledMs = 0;
  refreshOled();
}

static bool initMax30102() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("MAX30102 not found — check I2C wiring and power."));
    return false;
  }
  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeGreen(0);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n--- SIT210 health monitor ---"));
#ifdef BOARD_LOW_RAM
  Serial.println(F("Mode: heart rate on Serial (+ cloud if Wi-Fi works); SpO2 needs Mega/R4/ESP."));
#else
  Serial.println(F("Mode: heart rate + SpO2"));
#endif

  Wire.begin();
  if (!initMax30102()) {
    while (true) {
      delay(1000);
    }
  }

  if (!oledInit()) {
    Serial.println(F("OLED not found at 0x3C — check wiring; continuing without display."));
  }

  connectSetup();
  promptMeasureMode();
  Serial.println(F("Place finger on MAX30102. Send 1/2/3 anytime to change mode.\n"));
}

void loop() {
  pollSerialModeChange();
  connectMaintain();

  // Idle: only light presence polls — no vitals math, no Serial readout, no cloud
  if (!fingerPresent) {
    waitForFinger();
    lastReadingMs = 0;
    refreshOled();
    return;
  }

  if (!confirmFingerPresent()) {
    lastReadingMs = 0;
    return;
  }

#ifdef BOARD_LOW_RAM
  processSensorLowRam();
#else
  processSensorFull();
#endif

  refreshOled();

  if (!fingerPresent) {
    lastReadingMs = 0;
    return;
  }

  const unsigned long now = millis();
  if (now - lastReadingMs < READING_INTERVAL_MS) {
    return;
  }

  if (!confirmFingerPresent()) {
    lastReadingMs = 0;
    return;
  }

  lastReadingMs = now;
  performReadingAndUpload();
}