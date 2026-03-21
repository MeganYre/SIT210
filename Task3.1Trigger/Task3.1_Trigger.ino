#include "Secrets.h"       // contains WIFI_SSID, WIFI_PASS, IFTTT_KEY
#include <WiFiNINA.h>

int sensorPin = A0;        
int lightValue;

WiFiClient client;

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASS;

// IFTTT Webhooks settings
const char* host = "maker.ifttt.com";
const char* event_start = "sunlight_started";
const char* event_stop  = "sunlight_stopped";
const char* key = IFTTT_KEY;

bool sunlightActive = false;

void setup() 
{
  Serial.begin(9600);

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  while (WiFi.begin(ssid, password) != WL_CONNECTED) 
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected!");
}

void loop() 
{
  lightValue = analogRead(sensorPin);
  Serial.print("Light sensor value: ");
  Serial.println(lightValue);

  if (lightValue > 600 && !sunlightActive) 
  {
    sunlightActive = true;
    Serial.println("Sunlight started");
    triggerIFTTT(event_start);
  } 
  else if (lightValue <= 600 && sunlightActive)
  {
    sunlightActive = false;
    Serial.println("Sunlight stopped");
    triggerIFTTT(event_stop);
  }

  delay(1000);
}

void triggerIFTTT(const char* event) 
{
  if (client.connect(host, 80)) 
  {
    String url = "/trigger/";
    url += event;
    url += "/with/key/";
    url += key;

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");

    delay(500);
    while (client.available()) 
    {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    client.stop();
  } 
  else 
  {
    Serial.println("Connection to IFTTT failed");
  }
}