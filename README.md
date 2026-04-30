<img width="1273" height="1324" alt="SimonSaysPhysical" src="https://github.com/user-attachments/assets/1dcb4018-874d-43fe-8e3a-1e583b48d973" />

# Simon-says
Hardware Simon Says game for memory test and entertainment built with Cpp on ATmega328p, using non blocking ADC, state machine, and direct register control for speed

Hardware Configuation
- Two 74HC595 shift registers manage a 6-digit display i.e: 4 digit 7 segments, and 2 groups of Led displays
- A 10K potentiometer used to control brightness by connecting the output enable pin to PD5 instead of GND and uses Timer0's Fast PWM hardware for global brightness control
- Used PD2 (OC1B) to take advantage of Timer1 hardware toggling

  The state machine avoids delay() and uses a state machine to make sure display refresh and button debouncing remain responsive
  
  Timer2 multiplexing was configured with a 256 prescaler (~244 Hz) to refresh the display buffer, with minimal flicker
  Timer1 audio uses CTC mode, and toggles OC1B directly with hardware to generate the square wave, and avoid software cpu handling, to reduce overhead from entering and exiting the ISR at high frequencies
  Timer0 pwm uses OC0B to generate a FAST PWM of ~976.5Hz
  To prevent the game from being blocked by analogRead(), it starts the conversion and continues to the game loop while checking if ADC has returned the result
    It also uses a 16 prescaler and 8 bit result, since precision is not very very important



Game Logic
  Dynamic Difficulty: ledOnDelay starts at 255ms and reduces by 50ms every 5 successful level increase, which increases audio and visual urgency
  The 6th digit serves as a progress bar/difficulty level, every 5 levels the onProgressLeds variable is bit-shifted and incremented ((onProgressLeds << 1) + 1), lighting up an additional segment in a clockwise fill pattern.

Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [=         ]  11.0% (used 225 bytes from 2048 bytes)
Flash: [=         ]  14.8% (used 4768 bytes from 32256 bytes)
Which allows for more features like EEPROM high score saving, using PROGMEM to store digit patterns, Showing Words like "Press any button to start" on the 4 digit display
<img width="2806" height="1984" alt="SimonSaysSchematic" src="https://github.com/user-attachments/assets/dae0296c-2060-4da7-8e96-b281524f399d" />
This game also uses only 8 resistors for 39 Leds, which reduces breadboard space cramping by a lot

Bug Log:
  The display multiplexing was first done with an ISR frequency of 62.5KHz, i.e: (10.42KHz per digit) and while this produced a very crsip display, it took all the CPU processing power and didnt allow loop() to run
    This was fixed by changing to the rescaler to 256.

  Using tone() resulted in weird bugs, like my 4 digit 7 segment display blinking currentStreak times during showingPattern instead of the LEDS, and this was becasue tone() also uses timer2
    This was fixed by creating a myTone() function that uses Timer1 OC1B

  Because of the responsiveness of the loop, button presses were stored at unnatural times, which messed with the game, it was fixed by clearing the presses in showingPattern before moving on to readingBtns

  While myTone() worked, it sounded grainy and this was because it was originally done with software interrupt, and when the cpu had two ISRs running triggered in the same window, one had to wait for the other
    Changed to hardware interrupt that just toggled Pin 5

  myTone() worked and was crisp, but sometimes the notes were shorter or just clicks or won't sound at all
    Added a TCNT1 = 0 at every myTone() call to make sure the counter started at 0 before TOP
    
