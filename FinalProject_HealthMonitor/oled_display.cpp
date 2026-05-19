/*
  SSD1306 128x64 OLED — I2C (Adafruit library)

  Install: "Adafruit SSD1306" and "Adafruit GFX Library"

  Wiring (Maker UNO / Uno — same I2C bus as MAX30102):
    OLED VCC -> 3.3 V (use 5 V only if your module label allows it)
    OLED GND -> GND
    OLED SDA -> A4
    OLED SCL -> A5
    MAX30102 stays on the same SDA/SCL lines (address 0x57, OLED usually 0x3C)
*/

#include "oled_display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_I2C_ADDR 0x3C

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static bool oledReady = false;

bool oledIsReady() {
  return oledReady;
}

bool oledInit() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    oledReady = false;
    return false;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  oledReady = true;
  oledShowBoot();
  return true;
}

void oledShowBoot() {
  if (!oledReady) {
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Health monitor"));
  display.println(F("SSD1306 OLED"));
  display.display();
}

void oledShowNoFinger() {
  if (!oledReady) {
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("SIT210 Vitals"));
  display.println();
  display.setTextSize(2);
  display.println(F("No finger"));
  display.setTextSize(1);
  display.println();
  display.println(F("Place finger on"));
  display.println(F("MAX30102 sensor"));
  display.display();
}

void oledShowMeasuring(OledMeasureMode mode) {
  if (!oledReady) {
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Mode: "));
  if (mode == OLED_MODE_BPM) {
    display.println(F("BPM"));
  } else if (mode == OLED_MODE_SPO2) {
    display.println(F("SpO2"));
  } else {
    display.println(F("BPM+SpO2"));
  }
  display.println();
  display.setTextSize(2);
  display.println(F("Measuring"));
  display.setTextSize(1);
  display.println(F("Hold still..."));
  display.display();
}

void oledShowVitals(OledMeasureMode mode, int32_t bpm, bool bpmOk,
                    int32_t spo2, bool spo2Ok, bool wifiOk) {
  if (!oledReady) {
    return;
  }

  const bool showBpm = (mode == OLED_MODE_BPM || mode == OLED_MODE_BOTH);
  const bool showSpo2 = (mode == OLED_MODE_SPO2 || mode == OLED_MODE_BOTH);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("WiFi: "));
  display.println(wifiOk ? F("OK") : F("--"));

  if (showBpm) {
    display.print(F("BPM: "));
    if (bpmOk) {
      display.println(bpm);
    } else {
      display.println(F("--"));
    }
  }

  if (showSpo2) {
    display.print(F("SpO2: "));
    if (spo2Ok) {
      display.print(spo2);
      display.println(F(" %"));
    } else {
      display.println(F("--"));
    }
  }

  display.display();
}
