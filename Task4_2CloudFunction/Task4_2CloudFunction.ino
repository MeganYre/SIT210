#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

char ssid[] = "dont mesh with me";
char pass[] = "97081630";

WiFiSSLClient wifi;
HttpClient client(wifi, "sit210-a7994-default-rtdb.asia-southeast1.firebasedatabase.app", 443);

int livingRoomPin = 2;
int bathroomPin = 3;
int closetPin = 4;

void setup() {
  pinMode(livingRoomPin, OUTPUT);
  pinMode(bathroomPin, OUTPUT);
  pinMode(closetPin, OUTPUT);

  Serial.begin(9600);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected!");
}

bool readFirebaseBool(const char* path) {
  int err = client.get(path);
  if (err != 0) {
    Serial.print(path);
    Serial.print(" connect error ");
    Serial.println(err);
    client.stop();
    return false;
  }

  int status = client.responseStatusCode();
  String body = client.responseBody();
  client.stop();

  if (status != 200) {
    Serial.print(path);
    Serial.print(" HTTP ");
    Serial.print(status);
    Serial.print(" body=");
    Serial.println(body);
    return false;
  }
  body.trim();
  return body == "true";
}

void loop() {
  digitalWrite(livingRoomPin, readFirebaseBool("/livingRoom.json") ? HIGH : LOW);
  delay(50);
  digitalWrite(bathroomPin, readFirebaseBool("/bathroom.json") ? HIGH : LOW);
  delay(50);
  digitalWrite(closetPin, readFirebaseBool("/closet.json") ? HIGH : LOW);

  delay(500);
}
