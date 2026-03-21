const int LEDred_pin = 8;
const int LEDgreen_pin = 9;
const int BUTTON_pin = 7;

void setup() 
{
  pinMode(LEDred_pin, OUTPUT);
  pinMode(LEDgreen_pin, OUTPUT);
  pinMode(BUTTON_pin, INPUT);
}

void loop() {
  if (isButtonPressed())
  {
    runLightingSequence();
  } 
  else 
  {
    turnOffLights();
  }
}

// Check button state
bool isButtonPressed() 
{
  return digitalRead(BUTTON_pin) == HIGH;
}

// Run the lighting sequence
void runLightingSequence() 
{
  turnOnLights();            // Both LEDs ON
  delay(30000);              // Wait 30 seconds

  digitalWrite(LEDred_pin, LOW);   // Red LED OFF
  digitalWrite(LEDgreen_pin, HIGH); // Green stays ON
  delay(30000);              // Wait another 30 seconds

  digitalWrite(LEDgreen_pin, LOW);
}

// Turn LEDs ON
void turnOnLights() 
{ 
  digitalWrite(LEDred_pin, HIGH); 
  digitalWrite(LEDgreen_pin, HIGH); 
}

// Turn off LEDs
void turnOffLights() 
{
  digitalWrite(LEDred_pin, LOW); 
  digitalWrite(LEDgreen_pin, LOW);
}