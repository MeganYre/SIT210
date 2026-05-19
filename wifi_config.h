#ifndef PLATFORM_CONNECT_H
#define PLATFORM_CONNECT_H

#include <Arduino.h>

// Call once from setup(); returns true if cloud upload is available.
bool connectSetup();

// Keep Wi-Fi alive (call from loop).
void connectMaintain();

bool connectIsReady();

// Upload Field1 = HR (BPM), Field2 = SpO2 (%). Returns true on success.
bool connectUploadVitals(int32_t heartRateBpm, int32_t spo2Percent);

// Human-readable label printed at startup (e.g. "WiFiS3", "WiFiEsp").
const char *connectBackendName();

#endif
