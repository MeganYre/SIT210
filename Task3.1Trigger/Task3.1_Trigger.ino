#include "Secrets.h"       // contains WIFI_SSID, WIFI_PASS, IFTTT_KEY
#include <WiFiNINA.h>

int sensorPin = A0;
int lightValue;

WiFiClient client;

const char* ssid     = "SE_Guest";
const char* password = "ThankYou#";

// IFTTT Webhooks settings
const char* host = "maker.ifttt.com";
const char* event = "LightSensor_notification";   
const char* key   = "byuM7_ZrXo5abdoV_8p0YV";

bool sunlightActive = false;

void setup() {
  Serial.begin(9600);

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected!");
}

void loop() {
  lightValue = analogRead(sensorPin);
  Serial.print("Light sensor value: ");
  Serial.println(lightValue);

  if (lightValue > 600 && !sunlightActive) {
    sunlightActive = true;
    Serial.println("Sunlight started");
    triggerIFTTT("started");
  } else if (lightValue <= 600 && sunlightActive) {
    sunlightActive = false;
    Serial.println("Sunlight stopped");
    triggerIFTTT("stopped");
  }

  delay(30000);
}

void triggerIFTTT(const char* state) {
  if (client.connect(host, 80)) {
    String url = "/trigger/";
    url += event;
    url += "/with/key/";
    url += key;

    // Build JSON body with value1
    String jsonBody = String("{\"value1\":\"") + state + "\"}";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + jsonBody.length() + "\r\n\r\n" +
                 jsonBody);

    delay(500);
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    client.stop();
  } else {
    Serial.println("Connection to IFTTT failed");
  }
}
