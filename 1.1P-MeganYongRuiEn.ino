// LED ON and OFF Example

int ledPin = 13; // Built-in LED pin on Arduino Uno

void setup() {
  // Set the LED pin as an output
  pinMode(ledPin, OUTPUT);
  loop();
}

void loop() {
  // M = --
  dash(); dash();
  letterSpace(); // pause before next letter
  // E = .
  dot();
  letterSpace();
  // G = --.
  dash(); dash(); dot();
  letterSpace();
  // A = .-
  dot(); dash();
  letterSpace();
  // N = -.
  dash(); dot();
  wordSpace(); // gap between letters
}

void dot() {
  digitalWrite(ledPin, HIGH); // blink ON
  delay(250);
  digitalWrite(ledPin, LOW); // blink OFF
  delay(250);
}

void dash() {
  digitalWrite(ledPin, HIGH); // blink ON
  delay(1000);
  digitalWrite(ledPin, LOW); // blink OFF
  delay(250);
}

void wordSpace() {
  delay(1500); // gap between words
}

void letterSpace() {
  delay(750); // gap between letters
}