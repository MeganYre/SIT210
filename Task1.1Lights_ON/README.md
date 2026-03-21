Overview
This Arduino project demonstrates how to control two LEDs (red and green) using a push button. When the button is pressed, a timed lighting sequence runs. If the button is not pressed, both LEDs remain off.

Hardware Setup
-Red LED connected to pin 8
-Green LED connected to pin 9
-Push button connected to pin 7

How It Works
1. Setup Phase
  - Configures LED pins as outputs.
  - Configures the button pin as input.

2. Loop Phase
  - Continuously checks if the button is pressed.
  - If pressed, runs the lighting sequence.
  - If not pressed, turns off both LEDs.

3. Lighting Sequence
  - Both LEDs turn ON for 30 seconds.
  - Red LED turns OFF, green LED stays ON for another 30 seconds.
  - Finally, the green LED turns OFF.

4. Helper Functions
  - isButtonPressed() → returns true if the button is pressed.
  - runLightingSequence() → executes the timed LED sequence.
  - turnOnLights() → turns both LEDs ON.
  - turnOffLights() → turns both LEDs OFF.
