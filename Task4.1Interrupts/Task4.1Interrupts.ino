// Task4.1Interrupts.ino

const int led1 = 10;
const int led2 = 9;
const int switch_pin = 2;
const int PIR_pin = 6;
const int lightSensor = A0;

// Tune this value for your MH analog light module.
const int darkThreshold = 100;

volatile bool switchEvent = false;
volatile bool pirEvent = false;

bool switchOn = false;
bool motionDetected = false;
bool lightsOn = false;

void Switch_ISR() {
  switchEvent = true;
}

void PIR_ISR() {
  pirEvent = true;
}

void applyLights(bool on) {
  digitalWrite(led1, on ? HIGH : LOW);
  digitalWrite(led2, on ? HIGH : LOW);
}

void setup() {
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(switch_pin, INPUT);
  pinMode(PIR_pin, INPUT);
  pinMode(lightSensor, INPUT);

  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(switch_pin), Switch_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIR_pin), PIR_ISR, CHANGE);
}

void loop() {
  int sensorRead = analogRead(lightSensor);
  bool handleSwitch;
  bool handlePir;

  noInterrupts();
  handleSwitch = switchEvent;
  handlePir = pirEvent;
  switchEvent = false;
  pirEvent = false;
  interrupts();

  // Slider switch state is updated only when its interrupt fires.
  if (handleSwitch) {
    switchOn = (digitalRead(switch_pin) == HIGH);
    Serial.println(switchOn ? "Switch interrupt: ON" : "Switch interrupt: OFF");
  }

  // PIR motion state is updated only when its interrupt fires.
  if (handlePir) {
    motionDetected = (digitalRead(PIR_pin) == HIGH);
    Serial.println(motionDetected ? "PIR interrupt: Motion detected" : "PIR interrupt: Motion cleared");
  }

  bool isDark = (sensorRead <= darkThreshold);
  bool shouldLightsBeOn = switchOn || (isDark && motionDetected);

  if (shouldLightsBeOn != lightsOn) {
    lightsOn = shouldLightsBeOn;
    applyLights(lightsOn);
    Serial.println(lightsOn ? "Lights ON" : "Lights OFF");
  }
  delay(50);
}
