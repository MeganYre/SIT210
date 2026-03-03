This system uses an Arduino to output Morse code signals through an LED. 
The LED blinks in short and long intervals to represent dots and dashes, allowing text messages (like “Megan”) to be transmitted visually.

Explanation of Each Part
- void dot()  
  Turns the LED on for a short time (250 ms) to represent a Morse “dot,” then off for 250 ms to seperate signals.

- void dash()  
  Turns the LED on for a longer time (750 ms) to represent a Morse “dash,” then off for 250 ms to separate signals.

- void wordSpace()  
  Creates a longer pause (1500 ms) to separate words in Morse code.

- void letterSpace()  
  Creates a medium pause (750 ms) to separate letters in Morse code.
