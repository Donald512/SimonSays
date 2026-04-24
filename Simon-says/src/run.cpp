// #include <Arduino.h>
// #include <func.h>
// #include <pitches.h>



// #define memPersist static

// #define numDigits 5
// #define numColors 4
// #define numBtns 4

// // Colors
// #define red      0
// #define green    1
// #define blue     2
// #define yellow   3      

// #define colorsPlace 4
// // Not using serial, also dont have to worry abt upload error since i plug the standalone chip into the bootloader


// u8 blinkDelay = 200;
// u8 toneDelay = 255;
// u8 slightDelay = 200;
// u8 debounceDelay = 50;
// u8 numGameOverBlink = 6;  // todo check if 6 toggles == 3 blinks



// // :-: Pins
// // u8 btns[numBtns] = {0, 1, 2, 3};
// #define buzzerPin   4
// #define latchPin    10
// #define clockPin    11
// #define dataPin     12 

// void updateCurrentScore();
// void setupTimer2();
// void setupBtn(u8 pin);

// btnInfo btnObjs[numBtns] = {0}; // alloc memory for numbtns we got
// u8 numCreatedBtns = 0;

// u8 colorArray[100] = {0};
// u8 lastUpdatedNum;
// u8 currentStreak = 0;

// bool RUNNING = false;
// enum activity {startup, showingPattern, readingBtns, levelPassed, levelFailed, flashCorrectLed};
// activity currentActivity = startup;
// my595 sRegs = {dataPin, clockPin, latchPin};

// volatile u8 currentDigit = 0;
// volatile const uint8_t digitsPattern[10] =  {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
// // 0-1 is Highscore, 2-3 is current streak, 4 is led to display
// volatile u8 displayBuffer[5] = {0};
// ISR(TIMER2_OVF_vect){
//     PORTB &= ~(1 << (latchPin - 8)); 
//     msbShiftOut(sRegs, ~(1 << currentDigit)); 
//     msbShiftOut(sRegs, displayBuffer[currentDigit]); 
//     PORTB |= (1 << (latchPin - 8)); 

//     currentDigit++;
//     if (currentDigit >= numDigits) currentDigit = 0;
// }


// void setup(){
//     DDRD |= (1 << 7);

//     setupTimer2();
//     randomSeed(analogRead(A0));
//     for (u8 i = 0; i < 4; i++){ // Loop through buttons
//         setupBtn(i);
//     }
//     for (u8 i = 10; i < 13; i++){ // Loop through pins required for 595
//         pinMode(i, OUTPUT);
//     }
//     pinMode(buzzerPin, OUTPUT);
//     dWrite(buzzerPin, LOW);

//     RUNNING = true;
    
// }
// u8 led = 0;
// bool ledIsOn = false;
// u32 streakTS = millis();
// u32 ledTS = millis();

// void loop(){
//     updateCurrentScore();
//     if (currentStreak == 100){
//         currentStreak = 0;
//     }
//     if (led == numColors){
//         led = 0;
//     }
//     if (millis() - streakTS > 200){
//         PORTD ^= (1 << 7);
//         currentStreak++;
//         streakTS = millis();
//     }
//     if (/*!ledIsOn && */millis() - ledTS > 200){
//         displayBuffer[colorsPlace] = (1 << led);
//         ledTS = millis();
//         // ledIsOn = true;
//         led++;
//     }

// }


// void setupBtn(u8 pin){
//     // Alr created space for 5 btns in global
//     pinMode(pin, INPUT_PULLUP);
//     btnObjs[numCreatedBtns].btnPin = pin;
//     btnObjs[numCreatedBtns].lastCheckedState = false; // when its first created its off
//     // btnObjs[numCreatedBtns] = &btn; // Not useful
//     numCreatedBtns++;
// }

// void updateBtns(){
//     for (u8 i = 0; i < numCreatedBtns; i++){
//         if (millis() - btnObjs[i].lastCheckedTime > debounceDelay){
//             bool currentState = dRead(btnObjs[i].btnPin);
//             if (currentState != btnObjs[i].lastCheckedState){
//                 btnObjs[i].lastCheckedState = currentState;
//                 if (currentState == LOW){
//                     btnObjs[i].pressed = true;
//                 }
                
//                 btnObjs[i].lastCheckedTime = millis();

//             }
//         }
//     }
// }

// bool wasPressed(btnInfo &btn){
//     if (btn.pressed){
//         btn.pressed = false;
//         return true;
//     }
//     return false;
// }

// void updateCurrentScore(){
//     if (lastUpdatedNum == currentStreak){
//         return;
//     }
//     else{
//         lastUpdatedNum = currentStreak;
//         u8 firstDigit = currentStreak/10;
//         u8 secondDigit = currentStreak - (firstDigit * 10);  // minus faster than modulus

//         // NOTE: can be removed so that 05 shows, instead of just 5
//         if (firstDigit == 0){
//             displayBuffer[2] = 0;
//         }
//         else{
//             displayBuffer[2] = digitsPattern[firstDigit];
//         }
//         displayBuffer[3] = digitsPattern[secondDigit];
//     }
// }
// void setupTimer2(){
//     cli();
//     TCCR2A = 0; 
//     // TCCR2B = (1 << CS22); // 64 Prescaler (Fires ~976 times/sec)
//     TCCR2B = (1 << CS22) | (1 << CS21); // 256 Prescaler (Fires ~244 times/sec)
//     TIMSK2 = (1 << TOIE2); 
//     sei();
// }



#include <Arduino.h>
#include <func.h>
#include <btn.h>
#include <pitches.h>

#define memPersist static
#define prescaler 64
#define freq F_CPU/prescaler
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
void setupTimer1();
void myTone(u16 note); 
void endMyTone();

u8 colorArray[100] = {0};
u8 currentStreak = 0;
u8 lastUpdatedNum = 0;

u16 colorNotes[numColors] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_A4}; // r g b y

Button testBtn(triggerBtn);
my595 sRegs = {dataPin, clockPin, latchPin};


enum activity {startup, addLed, showingPattern, readingBtns, levelPassed, levelFailed, flashCorrectLed};
activity currentActivity = addLed;


// For display
// 0-1 is Highscore, 2-3 is current streak, 4 is led to display
volatile u8 displayBuffer[5] = {0};
volatile u8 currentDigit = 0;
volatile const uint8_t digitsPattern[10] =  {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// 244Hz
ISR(TIMER2_OVF_vect){
    PORTB &= ~(1 << (latchPin - 8)); 
    msbShiftOut(sRegs, ~(1 << currentDigit)); 
    msbShiftOut(sRegs, displayBuffer[currentDigit]); 
    PORTB |= (1 << (latchPin - 8)); 
    
    currentDigit++;
    if (currentDigit >= numDigits) currentDigit = 0;
}

// for tone
volatile bool toneActive = false;
ISR(TIMER1_COMPA_vect){
    if (toneActive){
        PORTD ^= (1 << buzzerPin);
    }
}

void setup(){
    DDRD |= (1 << 7);

    setupTimer2();
    setupTimer1();
    randomSeed(analogRead(A0));
    for (u8 i = 10; i < 13; i++){ // Loop through pins required for 595
        pinMode(i, OUTPUT);
    }
    pinMode(buzzerPin, OUTPUT);
    dWrite(buzzerPin, LOW);
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
                displayBuffer[colorIndex] = 0;
                currentActivity = readingBtns;
            }
            else{
                if (!ledIsOn && millis() - lastLedTS > ledOffDelay){    // NOTE: This is ledOffDelay, not ledOnDelay, using ledOnDelay inverts it 
                    displayBuffer[colorIndex] = (1 << colorArray[onLed]);   // turn on current led 
                    myTone(colorNotes[colorArray[onLed]]);
                    lastLedTS = millis();
                    ledIsOn = true;
                }
                else if (ledIsOn && millis() - lastLedTS > ledOnDelay){
                    endMyTone();
                    displayBuffer[colorIndex] = 0;   // turn off all current led 
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

void setupTimer1(){
    cli();

    TCCR1A = 0;     // CTC mode
    TCCR1B = (1 << WGM12);  // CTC with OCR1A as top
    // TCCR1B |= (1 << CS10);    // 1 prescaler
    TCCR1B |= (1 << CS11) | (1 << CS10);    // 64 prescaler
    TIMSK1 = (1 << OCIE1A);     // Enable compare match A
    OCR1A = 65535;      // starting value

    sei();

}

void myTone(u16 note){  
    // only for specific notes. the only min and max notes used in this program are NOTE_E3 and NOTE_C5:     165Hz and 523Hz respectively 
    // 165 Hz means it has to visit the ISR 330 times, 165 to turn on, and 165 to turn off
    // Timer1 best bet is 64 prescaler
    toneActive = true;
    OCR1A = (freq / (2 * note)) - 1;    // 0 to OCR1A - 1 = OCR1A steps
}

void endMyTone(){
    OCR1A = 65535;
    toneActive = false;
    PORTD &= ~(1 << buzzerPin);     // turn off in case last toggle was on
}

/* Bug Log:
    Using a ISR frequency of 62500 Hz caused loop() to not run 
        fix -> Changed to prescaler of 256. I used 62500Hz so i could use the same vector for other things but it didnt give brain to other functions

    Using tone broke my logic completely, caused confusing unrelated bugs like the display blinking currentStreak times, instead of the Leds, this is because tone also uses timer2
        fix ->  Going to create a myTone() function, that is only "closely" accurate for the notes used in this game, which are smaller notes, so i dont have to have a very high resolution


*/

