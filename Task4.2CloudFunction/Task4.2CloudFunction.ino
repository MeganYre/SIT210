#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

char ssid[] = "dont mesh with me";
char pass[] = "97081630";

WiFiSSLClient wifi;
HttpClient client(wifi, "sit210-a7994-default-rtdb.asia-southeast1.firebasedatabase.app", 443);

int livingRoomPin = 2;
int bathroomPin = 3;
int closetPin = 4;

// If `true` in Firebase lights the LED when you expected it dark (and vice versa), flip these.
// Typical: LED+resistor from pin to GND → use HIGH when on. LED tied active-low → use LOW when on.
const uint8_t kPinWhenFirebaseTrue = LOW;
const uint8_t kPinWhenFirebaseFalse = HIGH;

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
  Serial.println("Serial toggle: type living room | bathroom | closet then Enter (updates Firebase).");
}

/** Maps task wording to the same REST paths as Task4.2D.html */
const char* pathForRoomName(const String& name) {
  String n = name;
  n.trim();
  n.toLowerCase();
  if (n == "living room" || n == "livingroom") return "/livingRoom.json";
  if (n == "bathroom") return "/bathroom.json";
  if (n == "closet") return "/closet.json";
  return nullptr;
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

bool putFirebaseBool(const char* path, bool value) {
  const char* body = value ? "true" : "false";
  int err = client.put(path, "application/json", body);
  if (err != 0) {
    Serial.print("PUT ");
    Serial.print(path);
    Serial.print(" connect error ");
    Serial.println(err);
    client.stop();
    return false;
  }

  int status = client.responseStatusCode();
  client.responseBody();
  client.stop();

  if (status != 200) {
    Serial.print("PUT ");
    Serial.print(path);
    Serial.print(" HTTP ");
    Serial.println(status);
    return false;
  }
  return true;
}

/**
 * Takes a room name string and toggles that LED via the cloud (read boolean, flip, PUT back).
 * Accepts: "living room", "livingroom", "bathroom", "closet" (case-insensitive).
 */
void toggleRoom(String roomName) {
  const char* path = pathForRoomName(roomName);
  if (!path) {
    Serial.print("Unknown room: ");
    Serial.println(roomName);
    return;
  }

  bool on = readFirebaseBool(path);
  if (putFirebaseBool(path, !on)) {
    Serial.print("Toggled ");
    Serial.print(roomName);
    Serial.println(on ? " -> OFF" : " -> ON");
  }
}

void syncLedsFromFirebase() {
  digitalWrite(livingRoomPin, readFirebaseBool("/livingRoom.json") ? kPinWhenFirebaseTrue : kPinWhenFirebaseFalse);
  delay(50);
  digitalWrite(bathroomPin, readFirebaseBool("/bathroom.json") ? kPinWhenFirebaseTrue : kPinWhenFirebaseFalse);
  delay(50);
  digitalWrite(closetPin, readFirebaseBool("/closet.json") ? kPinWhenFirebaseTrue : kPinWhenFirebaseFalse);
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      toggleRoom(line);
    }
  }

  syncLedsFromFirebase();

  delay(500);
}
