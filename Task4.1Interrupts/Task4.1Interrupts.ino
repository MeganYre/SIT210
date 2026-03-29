// Task4.1Interrupts.ino

const int led1 = 10;
const int led2 = 9;
const int switch_pin = 2;
const int PIR_pin = 6;
const int lightSensor = A0;

volatile bool switchTriggered = false;

void Switch_ISR() {
  if (digitalRead(switch_pin) == HIGH) {
    switchTriggered = true;
  } else {
    switchTriggered = false;
  }
}

void setup() {
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(switch_pin, INPUT);
  pinMode(PIR_pin, INPUT);

  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(switch_pin), Switch_ISR, CHANGE);
}

void loop() {
  int sensorRead = analogRead(lightSensor);
  int val = digitalRead(PIR_pin);
  
  if (val == HIGH)
  {
   	Serial.print("Motion Detected!");
  }

  if (switchTriggered || (sensorRead <= 100 && val)) 
  {
    digitalWrite(led1, HIGH);
    digitalWrite(led2, HIGH);
    Serial.println("Lights ON");
  } 
  else 
  {
    digitalWrite(led1, LOW);
    digitalWrite(led2, LOW);
    Serial.println("Lights OFF");
  }

  delay(2000);
}
