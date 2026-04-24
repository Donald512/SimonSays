

#include <Arduino.h>
#include <func.h>
#include <btn.h>
#include <pitches.h>

#define memPersist static
// red = 0, yellow = 3, goes r -> g -> b -> y

#define numDigits 5
#define numColors 4
#define numBtns 4
#define colorIndex 4

// Pins
#define buzzerPin   4
#define latchPin    10
#define clockPin    11
#define dataPin     12 
#define triggerBtn  0

void updateCurrentScore();
void setupTimer2();

u8 colorArray[100] = {0};
u8 currentStreak = 0;
u8 lastUpdatedNum = 0;

u16 colorNotes[numColors] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_A4}; // r g b y

Button testBtn(triggerBtn);
my595 sRegs = {dataPin, clockPin, latchPin};


enum activity {startup, addLed, showingPattern, readingBtns, levelPassed, levelFailed, flashCorrectLed};
activity currentActivity = addLed;


volatile u8 currentDigit = 0;
volatile const uint8_t digitsPattern[10] =  {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
// 0-1 is Highscore, 2-3 is current streak, 4 is led to display
volatile u8 displayBuffer[5] = {0};
// 244Hz
ISR(TIMER2_OVF_vect){
    PORTB &= ~(1 << (latchPin - 8)); 
    msbShiftOut(sRegs, ~(1 << currentDigit)); 
    msbShiftOut(sRegs, displayBuffer[currentDigit]); 
    PORTB |= (1 << (latchPin - 8)); 

    currentDigit++;
    if (currentDigit >= numDigits) currentDigit = 0;
}

void setup(){
    DDRD |= (1 << 7);

    setupTimer2();
    randomSeed(analogRead(A0));
    for (u8 i = 10; i < 13; i++){ // Loop through pins required for 595
        pinMode(i, OUTPUT);
    }
}

u8 onLed = 0;
u32 lastLedTS = millis();
u8 ledOnDelay = 255;
u8 ledOffDelay = 100;
bool ledIsOn = false;

// ! Continue tomorrow, remember to actually show the color in the colorArray, not the onLed
void loop(){
    testBtn.watch();
    /*Made it Buggy but i couldnt figure out why, still confused on why this block stopped readingBtns from functioning at all, even when didnt press it all*/
    // if (testBtn.wasPressed()){
    //     PORTD ^= (1 << 7);
    // }
    switch (currentActivity){
        case addLed:{
            u8 randomLed = random(0, numColors);     // max exclusive
            colorArray[currentStreak] = randomLed;
            currentStreak++;
            updateCurrentScore();
            currentActivity = showingPattern;
            onLed = 0;
            ledIsOn = false;
        }   break;
        case showingPattern:{
            if (onLed == currentStreak){
                onLed = 0;
                // displayBuffer[colorIndex] = 0;
                currentActivity = readingBtns;
            }
            else{
                if (!ledIsOn && millis() - lastLedTS > ledOffDelay){    // NOTE: This is ledOffDelay, not ledOnDelay, using ledOnDelay inverts it 
                    tone(buzzerPin, colorNotes[colorArray[onLed]]);
                    displayBuffer[colorIndex] = (1 << colorArray[onLed]);   // turn on current led 
                    lastLedTS = millis();
                    ledIsOn = true;
                }
                else if (ledIsOn && millis() - lastLedTS > ledOnDelay){
                    displayBuffer[colorIndex] = 0;   // turn off all current led 
                    noTone(buzzerPin);
                    ledIsOn = false;
                    lastLedTS = millis();
                    onLed++;
                }

                
            }   
        }   break;
        case readingBtns:{
            if (testBtn.wasPressed()){
                currentActivity = addLed;
            }
        }   break;
    }

}

void updateCurrentScore(){
    if (lastUpdatedNum == currentStreak){
        return;
    }
    else{
        lastUpdatedNum = currentStreak;
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

void setupTimer2(){
    cli();
    TCCR2A = 0; 
    // TCCR2B = (1 << CS22); // 64 Prescaler (Fires ~976 times/sec)
    TCCR2B = (1 << CS22) | (1 << CS21); // 256 Prescaler (Fires ~244 times/sec)
    TIMSK2 = (1 << TOIE2); 
    sei();
}
