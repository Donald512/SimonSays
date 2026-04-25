#include <Arduino.h>
#include <func.h>
#include <btn.h>
#include <pitches.h>

#define memPersist static
#define PRESCALER 64
#define FREQ F_CPU/PRESCALER
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

// tone stuff
#define ONE_SECOND_MS 1000
#define delay_Multiplier 1.3
#define numSuccessNotes 4   // ! remember to change if change
#define numFailureNotes 3   // !

void updateCurrentScore();
void setupTimer2();
void setupTimer1();
void myTone(u16 note); 
void endMyTone();

u8 colorArray[100] = {0};
u8 currentStreak = 0;
u8 lastUpdatedNum = 0;

u16 colorNotes[numColors] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_A4}; // r g b y
Button colorBtns[numColors] = {Button(0), Button(1), Button(2), Button(3)};    // alloc space for 4 buttons


u16 gameOverSounds[] = {NOTE_C4, NOTE_G3, NOTE_E3};
u16 successSounds[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
u8 gamOverDuration[] = {5, 4, 2};
u8 successDuration[] = {8, 8, 8, 4};

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

bool btnPressedEvent = false;
bool flashedPressedLed = false;
u8 pressedBtn;  
u8 indexToEnter;

u8 toneIndex = 0;
bool notePlaying;
u16 noteOffDelay;
u32 noteTS = millis();



void loop(){
    for (u8 i = 0; i < numColors; i++){
        colorBtns[i].watch();
    }
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
            lastLedTS = millis();
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
            if (indexToEnter == currentStreak){
                indexToEnter = 0;
                displayBuffer[colorIndex] = 0;
                btnPressedEvent = false;
                currentActivity = levelPassed;
                lastLedTS = millis();
            }
            else{
                for (u8 i = 0; i < numColors && !btnPressedEvent; i++){
                    if (colorBtns[i].wasPressed()){
                        btnPressedEvent = true;
                        pressedBtn = i;
                        lastLedTS = millis();
                        ledIsOn = false;
                        flashedPressedLed = false;
                    }
                }
                if (btnPressedEvent){
                    // blink and tone the pressed led 
                    if (!flashedPressedLed){
                        if (!ledIsOn && millis() - lastLedTS > ledOffDelay){
                            displayBuffer[colorIndex] = (1 << pressedBtn);   // turn on btn led 
                            myTone(colorNotes[pressedBtn]);
                            lastLedTS = millis();
                            ledIsOn = true;
                        }
                        else if (ledIsOn && millis() - lastLedTS > ledOnDelay){
                            endMyTone();
                            displayBuffer[colorIndex] = 0;   // turn off all current led 
                            ledIsOn = false;
                            lastLedTS = millis();
                            flashedPressedLed = true;
                        }
                    }
                    else{
                        if (pressedBtn == colorArray[indexToEnter]){
                            for (u8 i = 0; i < numColors && !btnPressedEvent; i++){
                                colorBtns[i].wasPressed();      // clear stored presses
                            }                           
                            btnPressedEvent = false;
                            indexToEnter++;
                        }
                        else{
                            currentActivity = levelFailed;
                        }
                    }
                }
            }
        }   break;
        case levelPassed:{
            if (toneIndex == numSuccessNotes){
                currentActivity = addLed;
                toneIndex = 0;
            }
            else{
                if (!notePlaying){
                    myTone(successSounds[toneIndex]);
                    noteOffDelay = delay_Multiplier * ONE_SECOND_MS / successDuration[toneIndex];
                    notePlaying = true;
                    noteTS = millis();
                }
                else if (notePlaying && millis() - noteTS > noteOffDelay){
                    endMyTone();
                    notePlaying = false;
                    toneIndex++;
                }
            }
        }   break;
        case levelFailed:{
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
    // TCCR2B = (1 << CS22); // 64 PRESCALER (Fires ~976 times/sec)
    TCCR2B = (1 << CS22) | (1 << CS21); // 256 PRESCALER (Fires ~244 times/sec)
    TIMSK2 = (1 << TOIE2); 
    sei();
}

void setupTimer1(){
    cli();

    TCCR1A = 0;     // CTC mode
    TCCR1B = (1 << WGM12);  // CTC with OCR1A as top
    // TCCR1B |= (1 << CS10);    // 1 PRESCALER
    TCCR1B |= (1 << CS11) | (1 << CS10);    // 64 PRESCALER
    TIMSK1 = (1 << OCIE1A);     // Enable compare match A
    OCR1A = 65535;      // starting value

    sei();

}

void myTone(u16 note){  
    // only for specific notes. the only min and max notes used in this program are NOTE_E3 and NOTE_C5:     165Hz and 523Hz respectively 
    // 165 Hz means it has to visit the ISR 330 times, 165 to turn on, and 165 to turn off
    // Timer1 best bet is 64 PRESCALER
    toneActive = true;
    OCR1A = (FREQ / (2 * note)) - 1;    // 0 to OCR1A - 1 = OCR1A steps
}

void endMyTone(){
    OCR1A = 65535;
    toneActive = false;
    PORTD &= ~(1 << buzzerPin);     // turn off in case last toggle was on
}

/* Bug Log:
    Using a ISR frequency of 62500 Hz caused loop() to not run 
        fix -> Changed to PRESCALER of 256. I used 62500Hz so i could use the same vector for other things but it didnt give brain to other functions

    Using tone broke my logic completely, caused confusing unrelated bugs like the display blinking currentStreak times, instead of the Leds, this is because tone also uses timer2
        fix ->  Going to create a myTone() function, that is only "closely" accurate for the notes used in this game, which are smaller notes, so i dont have to have a very high resolution


    The loop was very responsive, causing it to store button presses and when it was read, i read the previous button presses and it messed up the logic. very sneaky bug
        fix -> I cleared all the buttons before the next round
*/

