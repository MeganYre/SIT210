/*
 * SIT210 Task 8.1HD — Arduino Nano 33 IoT
 *
 * Voice-activated lighting (assessed path): microphone and speech run on the
 * Raspberry Pi; this sketch runs on the Arduino with LEDs, light sensor, and
 * exhaust fan (demo with LEDs). Pi and Arduino communicate over Bluetooth
 * Low Energy (BLE).
 *
 * IDE: board package "Arduino Mbed OS Nano Boards" → Arduino Nano 33 IoT.
 * Library: ArduinoBLE (Library Manager).
 *
 * Pins:
 *   D2 — bathroom light (LED + resistor)
 *   D3 — hallway light (LED + resistor)
 *   D4 — exhaust fan (LED or relay + driver)
 *   A0 — light sensor (analog voltage divider)
 *
 * BLE local name: Task8.1AudioProcessing
 *
 * GATT (must match raspberry_pi/voice_bluetooth_control.py):
 *   Service 12345678-1234-1234-1234-123456789001
 *   Command (write byte): ...89002 — 0x00 all off; 0x01/02 bath on/off;
 *                          0x03/04 hall on/off; 0x05/06 fan on/off; 0xFF poll
 *   Status (read 2 bytes): ...89003 — [mask][light 0..255]
 *                          mask bit0 bath, bit1 hall, bit2 fan
 *
 * Light level: bathroom and hallway ON only when room is dark enough (LIGHT_ON_MAX).
 * Fan is not gated by light level.
 */

#include <ArduinoBLE.h>

const int PIN_BATH = 2;
const int PIN_HALL = 3;
const int PIN_FAN = 4;
const int PIN_LIGHT_SENSOR = A0;
const uint8_t LIGHT_ON_MAX = 255;

// LED polarity:
// If LEDs turn ON when the output pin is HIGH, set OUTPUT_ACTIVE_HIGH=true.
// If LEDs turn ON when the output pin is LOW, set OUTPUT_ACTIVE_HIGH=false.
const bool OUTPUT_ACTIVE_HIGH = false;
const uint8_t OUTPUT_ON_LEVEL = OUTPUT_ACTIVE_HIGH ? HIGH : LOW;
const uint8_t OUTPUT_OFF_LEVEL = OUTPUT_ACTIVE_HIGH ? LOW : HIGH;

#define CMD_ALL_OFF 0x00
#define CMD_BATH_ON 0x01
#define CMD_BATH_OFF 0x02
#define CMD_HALL_ON 0x03
#define CMD_HALL_OFF 0x04
#define CMD_FAN_ON 0x05
#define CMD_FAN_OFF 0x06
#define CMD_POLL 0xFF

#define MASK_BATH 0x01
#define MASK_HALL 0x02
#define MASK_FAN 0x04

BLEService lightsService("12345678-1234-1234-1234-123456789001");
BLEByteCharacteristic commandChar("12345678-1234-1234-1234-123456789002", BLEWrite);
BLECharacteristic statusChar("12345678-1234-1234-1234-123456789003", BLERead, 2);

// Start with all outputs ON (bathroom, hallway, fan).
// Requirement: `{room} on/off` should only change that room LED.
byte outputMask = MASK_BATH | MASK_HALL | MASK_FAN;

void applyOutputs() {
  digitalWrite(PIN_BATH, (outputMask & MASK_BATH) ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL);
  digitalWrite(PIN_HALL, (outputMask & MASK_HALL) ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL);
  digitalWrite(PIN_FAN, (outputMask & MASK_FAN) ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL);
}

byte readLightByte() {
  int raw = analogRead(PIN_LIGHT_SENSOR);
  return (byte)map(constrain(raw, 0, 1023), 0, 1023, 0, 255);
}

void refreshStatusChar() {
  byte buf[2] = {outputMask, readLightByte()};
  statusChar.writeValue(buf, 2);
}

bool darkEnoughForLights() {
  return readLightByte() <= LIGHT_ON_MAX;
}

void onCommand(byte cmd) {
  Serial.print("CMD received: 0x");
  Serial.println(cmd, HEX);
  switch (cmd) {
    case CMD_ALL_OFF:
      outputMask = 0;
      break;
    case CMD_BATH_ON:
      if (darkEnoughForLights()) {
        outputMask |= MASK_BATH;
      } else {
        Serial.println("Bath light ON ignored (too bright).");
      }
      break;
    case CMD_BATH_OFF:
      outputMask &= (byte)~MASK_BATH;
      break;
    case CMD_HALL_ON:
      if (darkEnoughForLights()) {
        outputMask |= MASK_HALL;
      } else {
        Serial.println("Hall light ON ignored (too bright).");
      }
      break;
    case CMD_HALL_OFF:
      outputMask &= (byte)~MASK_HALL;
      break;
    case CMD_FAN_ON:
      outputMask |= MASK_FAN;
      break;
    case CMD_FAN_OFF:
      outputMask &= (byte)~MASK_FAN;
      break;
    case CMD_POLL:
      break;
    default:
      break;
  }

  // Show what we think we are driving.
  Serial.print("New outputMask=");
  Serial.println(outputMask, BIN);

  uint8_t bathLevel = (outputMask & MASK_BATH) ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL;
  uint8_t hallLevel = (outputMask & MASK_HALL) ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL;
  uint8_t fanLevel = (outputMask & MASK_FAN) ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL;
  Serial.print("Pin levels => bath(D2)=");
  Serial.print(bathLevel);
  Serial.print(" hall(D3)=");
  Serial.print(hallLevel);
  Serial.print(" fan(D4)=");
  Serial.println(fanLevel);

  applyOutputs();
  refreshStatusChar();
}

void setup() {
  pinMode(PIN_BATH, OUTPUT);
  pinMode(PIN_HALL, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  applyOutputs();

  // Visual proof the sketch runs (works even if USB Serial is not opened yet)
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(120);
    digitalWrite(LED_BUILTIN, LOW);
    delay(120);
  }

  Serial.begin(115200);
  // On Nano 33 IoT, Serial output can be delayed if the monitor is not open.
  // Do not block here; just give the USB-CDC stack a moment.
  delay(500);

  Serial.println();
  Serial.println("=== Task8.1AudioProcessing | Nano 33 IoT | 115200 baud ===");
  Serial.println("BLE: starting radio…");
  Serial.flush();

  if (!BLE.begin()) {
    Serial.println("ERROR: BLE.begin() failed (check board + ArduinoBLE + NINA firmware).");
    Serial.flush();
    for (;;) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(200);
      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
    }
  }

  BLE.setLocalName("Task8.1AudioProcessing");
  BLE.setAdvertisedService(lightsService);
  commandChar.writeValue(0);
  lightsService.addCharacteristic(commandChar);
  byte initStatus[2] = {outputMask, readLightByte()};
  statusChar.writeValue(initStatus, 2);
  lightsService.addCharacteristic(statusChar);
  BLE.addService(lightsService);
  BLE.advertise();

  Serial.println("BLE advertising as: Task8.1AudioProcessing");
  Serial.print("BLE address: ");
  Serial.println(BLE.address());
  Serial.flush();
  Serial.println("If this window was empty: Tools > Port > pick Nano USB, 115200 baud, press RESET.");
  Serial.flush();
}

void loop() {
  BLE.poll();
  if (commandChar.written()) {
    onCommand(commandChar.value());
  }
  static unsigned long last = 0;
  static unsigned long lastHb = 0;
  unsigned long now = millis();

  if (BLE.connected()) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (now - last > 500) {
    last = now;
    refreshStatusChar();
  }

  // Always print something every 2 seconds so we can confirm Serial output works.
  if (now - lastHb > 2000) {
    lastHb = now;
    Serial.print("Heartbeat: BLE.connected=");
    Serial.println(BLE.connected() ? "true" : "false");
    Serial.flush();
  }
}
