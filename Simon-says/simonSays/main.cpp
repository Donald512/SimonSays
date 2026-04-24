#include <Arduino.h>
#include <myFunctions.h>
#include <pitches.h>


// No Extra Lives
// 5 digits, 2 for current streak, 2 for high score, 1 for showing patterns

#define memPersist static

#define numDigits 5
#define numColors 4
#define numBtns 4

// Colors
#define red      0
#define green    1
#define blue     2
#define yellow   3      

#define colorsPlace 4
#define numSuccessNotes 4 // ! Risk of being forgotten to be changed if array size changes
#define numGameOverNotes 3 // ! Risk of being forgotten to be changed if array size changes
// Not using serial, also dont have to worry abt upload error since i plug the standalone chip into the bootloader

const uint8_t digitsPattern[10] =  {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
u16 colorSounds[numColors] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_A4}; // r g b y
u16 gameOverSounds[] = {NOTE_C4, NOTE_G3, NOTE_E3};
u16 successSounds[numSuccessNotes] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
uint16_t gamOverDuration[] = {5, 4, 2};
uint16_t successDuration[] = {8, 8, 8, 4};
u8 toneDelay = 300;
u8 blinkDelay = 200;
u8 slightDelay = 200;
u8 debounceDelay = 50;
u8 numGameOverBlink = 6;  // todo check if 6 toggles == 3 blinks

// 0-1 is Highscore, 2-3 is current streak, 4 is led to display
volatile u8 displayBuffer[5] = {0};
u8 brightness[5] = {0};


// :-: Pins
// u8 btns[numBtns] = {0, 1, 2, 3};
#define buzzerPin   4
#define latchPin    10
#define clockPin    11
#define dataPin     12 


btnInfo btnObjs[numBtns] = {0}; // alloc memory for numbtns we got
// btnInfo *btnObjs[numBtns];
u8 numCreatedBtns = 0;

u8 colorArray[100] = {0};
u8 lastUpdatedNum;
u8 currentStreak = 0;

bool RUNNING = false;
enum activity {startup, showingPattern, readingBtns, levelPassed, levelFailed, flashCorrectLed};
activity currentActivity = startup;
my595 sRegs = {dataPin, clockPin, latchPin};


// 62500Hz
ISR(TIMER0_OVF_vect){
    memPersist u8 nTimesVisitedISR = 0;
    memPersist u8 currentDigit = 0;
    if (nTimesVisitedISR == 244){   // This only happens once every overflow, so actual update frequency is ~244.1Hz, each digit has a frequency of ~48.8Hz
        PORTB &= ~(1 << (latchPin - 8));    // set latch low, not using dWrite here to save clock cycles from function overhead
        msbShiftOut(sRegs, ~(1 << currentDigit));   // Bring the digit to GND first, will be pushed to second reg
        msbShiftOut(sRegs, displayBuffer[currentDigit]);    // send the current pattern
        currentDigit++;
    }
    if (currentDigit == numDigits){
        currentDigit = 0;
    }
    nTimesVisitedISR++;
}


void setup(){
    delay(1000);
    randomSeed(analogRead(A0));
    for (u8 i = 0; i < 4; i++){ // Loop through buttons
        setupBtn(i);
    }
    for (u8 i = 10; i < 13; i++){ // Loop through pins required for 595
        pinMode(i, OUTPUT);
    }
    pinMode(buzzerPin, OUTPUT);
    dWrite(buzzerPin, LOW);

    RUNNING = true;
}


// todo check how time stamps and flags get initialized for the first time/ led 
// new todo tried to fix by adding startup enum

// prefix of i means incrementer, TS means timestamp

// showingPattern
bool displayNextLed = false;
u32 toneStart1TS = millis();
u32 slightDelay1TS = millis();
u8 iLed = 0;

// readingBtns
u8 iBtnLed = 0;
u8 iBtnLooper = 0;
u32 toneStart2TS = millis();
bool btnPressed = false;
u8 pressedBtn = 255; // anything >= numCreatedBtns
bool validatePress = false;
bool acceptingInput = true;

// levelPassed
u8 iNote3 = 0;
u8 toneStart3TS = millis();
u32 slightDelay3TS = millis();
bool playNote3 = false;

// levelFailed
u8 iNote4 = 0;
u8 toneStart4TS = millis();
u32 slightDelay4TS = millis();
bool playNote4 = false;

// flashCorrectLed
u8 iBlink5 = 0;
u32 toneStart5TS = millis();

void loop(){
    while(RUNNING){
        updateBtns();
        updateCurrentScore();
        switch(currentActivity){
            // todo 60% done with this part
            case startup:{
                displayNextLed = false;
                toneStart1TS = millis();
                slightDelay1TS = millis();
                u8 iLed = 0;
                currentStreak = 0;
            }   break;
            
            case showingPattern:{
                if (iLed == currentStreak){
                    iLed = 0; // reset for next round
                    currentActivity = readingBtns;
                }
                else{
                    if (millis() - slightDelay1TS > slightDelay){
                        displayNextLed = true;
                    }
                    else if (displayNextLed){
                        displayBuffer[colorsPlace] = (1 << iLed);
                        tone(buzzerPin, colorSounds[iLed]);
                        toneStart1TS = millis();
                        displayNextLed = false;
                    }
                    else if (millis() - toneStart1TS > toneDelay && !displayNextLed /* Low-key dont think i need this*/){ // todo check if second clause is required 
                        // displayBuffer[4] = displayBuffer[4] ^ (1 << colorArray[i]); // colorArray is full of numbers from 0 to 4, and each num reperesents the color to flash, toggle
                        displayBuffer[4] = 0;   // turn everything off
                        noTone(buzzerPin);
                        slightDelay1TS = millis();
                        iLed++;
                    }
                }
                // C'est tres difficile
            }   break;

            case readingBtns:{
                if (iBtnLed == currentStreak){
                    iBtnLed = 0;
                    currentStreak++;
                    currentActivity = levelPassed;
                    // toneStart1TS = millis();
                    toneStart3TS = millis();
                    playNote3 = true;
                }
                else{                    
                    if (iBtnLooper == numCreatedBtns){
                        iBtnLooper = 0;
                    }
                    if (acceptingInput && wasPressed(btnObjs[iBtnLooper])){   // todo once this line runs, it becomes false and only runs once unless the user keeps pressing the button or any other buttons
                        acceptingInput = false;     // this makes it only run once per iBtnLed
                        btnPressed = true;
                        displayBuffer[colorsPlace] = (1 << iBtnLooper);
                        tone(buzzerPin, colorSounds[iBtnLooper]);
                        toneStart2TS = millis();    // todo check if this block runs once, or else toneStart2TS might keep resetting to millis and the tone will never stop
                        // btnPressed = iBtnLooper;
                        pressedBtn = iBtnLooper;
                    }
                    if (btnPressed && millis() - toneStart2TS > toneDelay){
                        displayBuffer[colorsPlace] = 0; // turn off leds
                        noTone(buzzerPin);
                        validatePress = true;
                    }
                    if (validatePress ){
                        if (pressedBtn == colorArray[iBtnLed]){
                            iBtnLed++;
                            // reset the flags
                            acceptingInput = true;
                            validatePress = false;
                            btnPressed = false;
                        }
                        else{
                            currentActivity = levelFailed;
                        }
                    }
                    iBtnLooper++;
                }
            }   break;

            case levelPassed:{
                if (iNote3 == numSuccessNotes){
                    iNote3 = 0;
                    playNote3 = false;
                    toneStart1TS = millis();
                    currentActivity = showingPattern;
                }
                else{
                    if (playNote3){
                        tone(buzzerPin, successSounds[iNote3]);
                    }
                    if (millis() - toneStart3TS > toneDelay){
                        playNote3 = false;
                        noTone(buzzerPin);
                    }
                    if (millis() - slightDelay3TS > slightDelay){
                        playNote3 = true;
                        iNote3++;
                    }
                }
            }   break;


            case levelFailed:{
                if (iNote4 == numGameOverNotes){
                    iNote4 = 0;
                    playNote4 = false;
                    toneStart5TS = millis();
                    currentActivity = showingPattern;
                }
                if (playNote4){
                    tone(buzzerPin, successSounds[iNote4]);
                }
                if (millis() - toneStart4TS > toneDelay){
                    playNote4 = false;
                    noTone(buzzerPin);
                }
                if (millis() - slightDelay4TS > slightDelay){
                    playNote4 = true;
                    iNote4++;
                }
            }   break;

            case flashCorrectLed:{
                if (iBlink5 == numGameOverBlink){
                    iBlink5 = 0;
                    displayBuffer[colorsPlace] = 0;
                    currentActivity = startup;
                }
                else{
                    if (millis() - toneStart5TS > slightDelay){
                        displayBuffer[colorsPlace] = displayBuffer[colorsPlace] ^ (1 << colorArray[iBtnLed]); 
                    }
                }
            }   break;

        }
    }
}
void setupBtn(u8 pin){
    // Alr created space for 5 btns in global
    pinMode(pin, INPUT_PULLUP);
    btnObjs[numCreatedBtns].btnPin = pin;
    btnObjs[numCreatedBtns].lastCheckedState = false; // when its first created its off
    // btnObjs[numCreatedBtns] = &btn; // Not useful
    numCreatedBtns++;
}

void updateBtns(){
    for (u8 i = 0; i < numCreatedBtns; i++){
        if (millis() - btnObjs[i].lastCheckedTime > debounceDelay){
            bool currentState = dRead(btnObjs[i].btnPin);
            if (currentState != btnObjs[i].lastCheckedState){
                btnObjs[i].lastCheckedState = currentState;
                if (currentState == LOW){
                    btnObjs[i].pressed = true;
                }
                
                btnObjs[i].lastCheckedTime = millis();

            }
        }
    }
}

bool wasPressed(btnInfo &btn){
    if (btn.pressed){
        btn.pressed = false;
        return true;
    }
    return false;
}

void updateCurrentScore(){
    if (lastUpdatedNum == currentStreak){
        return;
    }
    else{
        lastUpdatedNum == currentStreak;
        u8 firstDigit = currentStreak/10;
        u8 secondDigit = currentStreak - (firstDigit * 10);  // minus faster than modulus

        // NOTE: can be removed so that 05 shows, instead of just 5
        if (firstDigit == 0){
            displayBuffer[2] = 0;
        }
        else{
            displayBuffer[2] = digitsPattern[firstDigit];
        }
        displayBuffer[3] = digitsPattern[secondDigit];
    }
}

/* Pseudocode
:-: 
Light corresponding LED turn by turn in array
After certain delay, turn off LED and stop tone, for blink effect, then start the other, maybe add a delay between leds 
Then when the last led for current turn is reached, enter waiting for user input mode


:-: reading input logic:
loop through all buttons
if anyone is pressed
light the corresponding led and sound tone
wait some time (blink effect)
turn off
if button is the right one
    increment iBtnLed and repeat till currentstreak
else
    enter failed enum, to make it cleaner, dont wanna put everything in else

:-: failed:
    toggle the corresponding led 3 times (or blink to be simpler) and sound note everytime, 
*/

/* Atmega connections
Reset to 10K to +ve rail
10nF accross pin 7 (vcc to +ve rail) and pin 8 (gnd to -ve rail)
crystal accross pin 9 and pin 10
22pF from pin9 to -ve rail 
22pF from pin10 to -ve rail
pin 20 AVCC to +ve rail
pin 22 GND to -ve rail 
*/