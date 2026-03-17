#include "ThingSpeak.h"
#include "WiFiNINA.h"
#include "DHT.h"
#include <Adafruit_Sensor.h>
#include "Secrets.h"

// #define SECRET_SSID "yongfamily"
// #define SECRET_PASS "97081630"
// #define SECRET_CH_ID 3294962
// #define SECRET_WRITE_APIKEY "FSMQV74U7ZPERM5X"

#define DHTPIN 2
#define DHTTYPE DHT11
#define LIGHTPIN A0

unsigned long channelID = 3294962;
const char* writeAPIKey = "FSMQV74U7ZPERM5X";

DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;  // Needed for ThingSpeak

void setup() {
  Serial.begin(9600);
  dht.begin();

  // Initialize ThingSpeak
  ThingSpeak.begin(client);

  // Connect to WiFi
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(1000);
  }
  Serial.println("WiFi connected!");
}

float readTemperature() 
{
  return dht.readTemperature();
}

int readLightLevel() 
{
  return analogRead(LIGHTPIN);
}

void sendToThingSpeak(float temp, int light) 
{
  ThingSpeak.setField(1, temp);
  ThingSpeak.setField(2, light);
  int response = ThingSpeak.writeFields(channelID, writeAPIKey);

  if (response == 200) 
  {
    Serial.println("Data sent successfully!");
  } else {
    Serial.println("Error sending data: " + String(response));
  }
}

void loop() 
{
  float temp = readTemperature();
  int light = readLightLevel();

if (isnan(temp)) 
{
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.println("Temp: " + String(temp) + " | Light: " + String(light));

  sendToThingSpeak(temp, light);
  delay(30000);  // 30 seconds
}
