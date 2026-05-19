#include "platform_connect.h"
#include "wifi_config.h"

static const unsigned long CONNECT_TIMEOUT_MS = 15000UL;

static char ssid[] = WIFI_SSID;
static char pass[] = WIFI_PASSWORD;
static const char *writeKey = THINGSPEAK_WRITE_API_KEY;

static bool cloudEnabled = false;
static const char *backend = "none";

// --- Pick Wi-Fi implementation for the board selected in Arduino IDE ----------

#if defined(ARDUINO_UNOR4_WIFI)
  #define NET_USE_WIFI_S3 1
#elif defined(ESP8266)
  #define NET_USE_WIFI_NATIVE 1
#elif defined(ESP32)
  #define NET_USE_WIFI_NATIVE 1
#else
  #define NET_USE_WIFIESP 1
#endif

#if defined(NET_USE_WIFI_S3)
  #include <WiFiS3.h>
  static WiFiClient httpClient;

#elif defined(NET_USE_WIFI_NATIVE)
  #include <WiFi.h>
  static WiFiClient httpClient;

#elif defined(NET_USE_WIFIESP)
  #include <SoftwareSerial.h>
  #include <WiFiEsp.h>
  #include <WiFiEspClient.h>
  static SoftwareSerial espSerial(2, 3);
  static WiFiEspClient httpClient;
  #ifndef ESP8266_BAUD
    #define ESP8266_BAUD 9600
  #endif
#endif

static bool waitForWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println(F("Connecting..."));

  WiFi.begin(ssid, pass);
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  return WiFi.status() == WL_CONNECTED;
}

static void printWifiConnectionStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("Wi-Fi: connected"));
  } else {
    Serial.println(F("Wi-Fi: not connected"));
  }
}

static bool uploadHttp(int32_t hr, int32_t sp) {
  if (!httpClient.connect("api.thingspeak.com", 80)) {
    return false;
  }

  httpClient.print(F("GET /update?api_key="));
  httpClient.print(writeKey);
  httpClient.print(F("&field1="));
  httpClient.print(hr);
  httpClient.print(F("&field2="));
  httpClient.print(sp);
  httpClient.println(F(" HTTP/1.1"));
  httpClient.println(F("Host: api.thingspeak.com"));
  httpClient.println(F("Connection: close"));
  httpClient.println();

  unsigned long t = millis();
  while (httpClient.available() == 0 && millis() - t < 8000UL) {
    delay(10);
  }

  const long entryId = httpClient.parseInt();
  httpClient.stop();
  return entryId > 0;
}

bool connectSetup() {
#if defined(NET_USE_WIFI_S3)
  backend = "WiFiS3 (Uno R4 WiFi)";
#elif defined(NET_USE_WIFI_NATIVE)
  #if defined(ESP8266)
    backend = "ESP8266 built-in Wi-Fi";
  #else
    backend = "ESP32 built-in Wi-Fi";
  #endif
#elif defined(NET_USE_WIFIESP)
  backend = "WiFiEsp + ESP8266 module";
  espSerial.begin(ESP8266_BAUD);
  delay(500);
  WiFi.init(&espSerial);
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println(F("ESP8266 module not detected on pins 2 (RX) / 3 (TX)."));
    cloudEnabled = false;
    printWifiConnectionStatus();
    Serial.println(F("Sensor data will still print on Serial only."));
    return false;
  }
#endif

  Serial.print(F("Network backend: "));
  Serial.println(backend);

  cloudEnabled = waitForWifi();
  printWifiConnectionStatus();
  if (!cloudEnabled) {
    Serial.println(F("Sensor data will still print on Serial only."));
  }
  return cloudEnabled;
}

void connectMaintain() {
  if (!cloudEnabled) {
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    cloudEnabled = waitForWifi();
  }
}

bool connectIsReady() {
  return cloudEnabled;
}

bool connectUploadVitals(int32_t heartRateBpm, int32_t spo2Percent) {
  if (!cloudEnabled) {
    return false;
  }
  return uploadHttp(heartRateBpm, spo2Percent);
}

const char *connectBackendName() {
  return backend;
}
