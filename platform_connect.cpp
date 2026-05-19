#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Arduino.h>

// Same values as MeasureMode in main sketch (1=BPM, 2=SpO2, 3=both)
enum OledMeasureMode : uint8_t {
  OLED_MODE_BPM = 1,
  OLED_MODE_SPO2 = 2,
  OLED_MODE_BOTH = 3,
};

bool oledInit();
bool oledIsReady();

void oledShowBoot();
void oledShowNoFinger();
void oledShowMeasuring(OledMeasureMode mode);
void oledShowVitals(OledMeasureMode mode, int32_t bpm, bool bpmOk,
                    int32_t spo2, bool spo2Ok, bool wifiOk);

#endif
