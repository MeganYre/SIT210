#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>

// WiFi credentials
char ssid[] = "SE_Guest";
char pass[] = "ThankYou#";

// MQTT broker details
const char broker[] = "broker.emqx.io";
int port = 1883; // plain MQTT
const char topicWave[] = "ES/Wave";
const char topicPat[]  = "ES/Pat";

WiFiClient client;
MqttClient mqttClient(client);

// Hardware pins
const int trigPin = 11;
const int echoPin = 12;
int LEDpin1 = 2;
int LEDpin2 = 3;

long duration;
int distance;

// Gesture thresholds
const int patThreshold = 5;  
const int waveThreshold = 20;  

// Measure distance
int readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;
  return distance;
}

// Publish helper
void publishMessage(const char* topic, String message) {
  mqttClient.beginMessage(topic);
  mqttClient.print(message);
  mqttClient.endMessage();
  Serial.print("Published to ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);
}

// Handle incoming MQTT messages
void onMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  String payload;

  while (mqttClient.available()) {
    char c = mqttClient.read();
    payload += c;
  }

  Serial.print("Received on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(payload);

  // Act on topic
  if (topic == topicWave) {
    digitalWrite(LEDpin1, HIGH);
    digitalWrite(LEDpin2, HIGH);
  } else if (topic == topicPat) {
    digitalWrite(LEDpin1, LOW);
    digitalWrite(LEDpin2, LOW);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(LEDpin1, OUTPUT);
  pinMode(LEDpin2, OUTPUT);

  // Connect WiFi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(800);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wifi!");

  // Connect MQTT
  if (!mqttClient.connect(broker, port)) {
    Serial.println("MQTT connection failed!");
    while (1);
  }
  Serial.println("Connected to MQTT broker!");

  mqttClient.onMessage(onMessage);
  mqttClient.subscribe(topicWave);
  mqttClient.subscribe(topicPat);
}

void loop() {
  mqttClient.poll();

  int dist = readDistance();
  Serial.print("Distance: ");
  Serial.print(dist);
  Serial.println(" cm");

  // Gesture detection
  if (dist > 0 && dist <= patThreshold) {
    // Pat → OFF
    publishMessage(topicPat, "PAT! Megan");
    digitalWrite(LEDpin1, LOW);
    digitalWrite(LEDpin2, LOW);
    Serial.println("Pat detected → LEDs OFF");
    delay(500);
  } 
  else if (dist > patThreshold && dist <= waveThreshold) {
    // Wave → ON
    publishMessage(topicWave, "WAVE! Megan");
    digitalWrite(LEDpin1, HIGH);
    digitalWrite(LEDpin2, HIGH);
    Serial.println("Wave detected → LEDs ON");
    delay(500);
  }
}
