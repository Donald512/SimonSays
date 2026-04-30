

#include <Arduino.h>
#include <func.h>
#include <btn.h>
#include <pitches.h>

#define memPersist static
#define PRESCALER 64
#define FREQ F_CPU/PRESCALER
// red = 0, yellow = 3, goes r -> g -> b -> y
// red is segA 

#define numDigits 6
#define numColors 4
#define numBtns 4
#define colorIndex 4

// Pins
#define redBtn      0
#define greenBtn    1
#define blueBtn     2
#define yellowBtn   3
#define buzzerPin   10
#define oePin       5   // ! make sure to change PORTs if change
#define latchPin    7   // !
#define clockPin    11
#define dataPin     12
#define randomSeedPin   A1
#define potPin      A0
// #define brokenPins  6, 8, 9     // broken legs of my atmega328p dip chip

// tone stuff
#define ONE_SECOND_MS 1000
#define delay_Multiplier_S 1.3
#define delay_Multiplier_F 1.5
#define numSuccessNotes 4   // ! remember to change if change
#define numFailureNotes 3   // !

#define ADMUXSetupSettings (1 << REFS0)  /*use Avcc as reference */ | (1 << ADLAR) // make it left adjusted, so that we only grab ADCH 

// #define stuff for loop, defined because not changing
#define aReadDelay 100
#define ledOffDelay 100
#define noteOnDelay 100



void updateCurrentScore();
void setupTimer2();
void setupTimer1();
void myTone(u16 note); 
void endMyTone();
void clearPresses();
void setupADCandPin();
void tagAlongTimer0();
void updateHighScore();

u8 colorArray[100] = {0};
u8 currentStreak = 0;
u8 lastUpdatedNum = 0;

u16 colorNotes[numColors] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_A4}; // r g b y
Button colorBtns[numColors] = {Button(0), Button(1), Button(2), Button(3)};    // alloc space for 4 buttons


u16 gameOverSounds[] = {NOTE_C4, NOTE_G3, NOTE_E3};
u16 successSounds[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
u8 gameOverDuration[] = {5, 4, 2};
u8 successDuration[] = {8, 8, 8, 4};

my595 sRegs = {dataPin, clockPin, latchPin};


enum activity {addLed, showingPattern, readingBtns, levelPassed, levelFailed, celebration};
activity currentActivity = addLed;


// For display
// 0-1 is Highscore, 2-3 is current streak, 4 is led to display, 5 is onProgressLeds
volatile u8 displayBuffer[numDigits] = {0};
volatile u8 currentDigit = 0;
volatile const uint8_t digitsPattern[10] =  {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
volatile u8 highScore;

// 244Hz
ISR(TIMER2_OVF_vect){
    PORTD &= ~(1 << latchPin); 
    msbShiftOut(sRegs, ~(1 << currentDigit)); 
    msbShiftOut(sRegs, displayBuffer[currentDigit]); 
    PORTD |= (1 << latchPin); 
    
    currentDigit++;
    if (currentDigit >= numDigits) currentDigit = 0;
}

void setup(){
    randomSeed(analogRead(A1));
    setupTimer2();
    setupTimer1();
    setupADCandPin(); 
    tagAlongTimer0();

    pinMode(oePin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(dataPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    dWrite(buzzerPin, LOW);
}

u32 aReadTS = millis();

u8 onProgressLeds = 0;

u8 onLed = 0;
u32 lastLedTS = millis();
u8 ledOnDelay = 255;
bool ledIsOn = false;

bool btnPressedEvent = false;
bool flashedPressedLed = false;
u8 pressedBtn;  
u8 indexToEnter;

u8 toneIndex = 0;
bool notePlaying;
u16 noteOffDelay;
u32 noteTS = millis();
u8 correctLed;      // the led that the user was supposed to press



void loop(){
    u32 now = millis();     // avoid calling millis() 15 times per loop becuase i have no delay() and no blocks, difference in accuracy very small
    for (u8 i = 0; i < numColors; i++){
        colorBtns[i].watch(now);
    }
    if ((now - aReadTS > aReadDelay) && ((ADCSRA & (1 << ADSC)) == 0) /*becomes 0 when conversion complete*/ ){
        OCR0B = ADCH;  // High part, left adjusted
        aReadTS = now;
        ADCSRA |= (1 << ADSC);  // start conversion 
    }
    switch (currentActivity){
        case addLed:{
            u8 randomLed = random(0, numColors);     // max exclusive
            colorArray[currentStreak] = randomLed;
            currentStreak++;
            updateCurrentScore();
            if (currentStreak % 5 == 0){    // no risk of currentStreak == 0 at this point
                onProgressLeds = (onProgressLeds << 1 /*double*/) + 1; // every 5 currentStreaks, turn on one more LED, 0 -> 1 -> 11 -> 111, remember LSB is SegA, first physical LED from left to right 
                displayBuffer[5] = onProgressLeds;
                ledOnDelay -= 50;
                // current logic assumes max streak < 20, so no risk of u8 underflow
            }
            if (currentStreak > highScore){
                highScore = currentStreak;
                updateHighScore();
            }
            currentActivity = showingPattern;
            onLed = 0;
            ledIsOn = false;
            btnPressedEvent = false;
            flashedPressedLed = false;
            lastLedTS = now;   // Because the next state needs it to be refreshed
            // this case runs once every currentStreak
        }   break;
        case showingPattern:{
            if (onLed == currentStreak){
                onLed = 0;
                // no need to reset ledIsOn because the else block is usually the last to run, and does it already 
                displayBuffer[colorIndex] = 0;
                currentActivity = readingBtns;
                lastLedTS = now;
                clearPresses(); // made me fail if i pressed a btn mid display, or could be use to cheat, to remember one less color, the first one
            }
            else{
                if (!ledIsOn && now - lastLedTS > ledOffDelay){    // NOTE: This is ledOffDelay, not ledOnDelay, using ledOnDelay inverts it 
                    displayBuffer[colorIndex] = (1 << colorArray[onLed]);   // turn on current led 
                    myTone(colorNotes[colorArray[onLed]]);
                    lastLedTS = now;
                    ledIsOn = true;
                }
                else if (ledIsOn && now - lastLedTS > ledOnDelay){
                    endMyTone();
                    displayBuffer[colorIndex] = 0;   // turn off all current led 
                    ledIsOn = false;
                    lastLedTS = now;
                    onLed++;
                }
            }   
        }   break;
        case readingBtns:{
            if (indexToEnter == currentStreak){
                indexToEnter = 0;
                toneIndex = 0;
                displayBuffer[colorIndex] = 0;
                btnPressedEvent = false;
                currentActivity = levelPassed;
                lastLedTS = now;
            }
            else{
                for (u8 i = 0; i < numColors && !btnPressedEvent; i++){
                    // colorBtns[i].watch(); this made it feel unresponsive, because of this loop only runs after flashled, the delay used to create a blinking effect finishes, coupled with the debounce delays
                    if (colorBtns[i].wasPressed()){
                        btnPressedEvent = true;
                        pressedBtn = i;
                        lastLedTS = now;
                        ledIsOn = false;
                        flashedPressedLed = false;
                    }
                }
                if (btnPressedEvent){
                    // blink and tone the pressed led 
                    if (!flashedPressedLed){
                        if (!ledIsOn && now - lastLedTS > ledOffDelay){
                            displayBuffer[colorIndex] = (1 << pressedBtn);   // turn on btn led 
                            myTone(colorNotes[pressedBtn]);
                            lastLedTS = now;
                            ledIsOn = true;
                        }
                        else if (ledIsOn && now - lastLedTS > ledOnDelay){
                            endMyTone();
                            displayBuffer[colorIndex] = 0;   // turn off all current led 
                            ledIsOn = false;
                            lastLedTS = now;
                            flashedPressedLed = true;
                        }
                    }
                    else{
                        if (pressedBtn == colorArray[indexToEnter]){
                            btnPressedEvent = false;
                            flashedPressedLed = false;
                            indexToEnter++;
                        }
                        else{
                            currentActivity = levelFailed;
                            correctLed = colorArray[indexToEnter];
                            notePlaying = false;
                            // Reseting the below here, because this doesnt go through the proper exit right below each case start
                            toneIndex = 0;
                            indexToEnter = 0;
                            lastLedTS = now;
                            ledIsOn = false;
                            noteTS = now;
                            // 
                            btnPressedEvent = false;
                        }
                    }
                }
            }
        }   break;
        case levelPassed:{
            if (toneIndex == numSuccessNotes){
                currentActivity = addLed;
                toneIndex = 0;
                lastLedTS = now;
            }
            else{
                if (!notePlaying){
                    myTone(successSounds[toneIndex]);
                    noteOffDelay = delay_Multiplier_S * ONE_SECOND_MS / successDuration[toneIndex];
                    notePlaying = true;
                    noteTS = now;
                }
                else if (notePlaying && now - noteTS > noteOffDelay){
                    endMyTone();
                    notePlaying = false;
                    toneIndex++;
                }
            }
        }   break;
        case levelFailed:{
            if (toneIndex == numFailureNotes){
                currentActivity = addLed;
                toneIndex = 0;
                currentStreak = 0;
                flashedPressedLed = false;
                onProgressLeds = 0;
                displayBuffer[5] = onProgressLeds;
                ledOnDelay = 255;
            }
            else{
                if (!notePlaying && now - noteTS > noteOnDelay){
                    displayBuffer[colorIndex] = (1 << correctLed);
                    myTone(gameOverSounds[toneIndex]);
                    noteOffDelay = delay_Multiplier_F * ONE_SECOND_MS / gameOverDuration[toneIndex]; 
                    notePlaying = true;
                    noteTS = now;
                }
                else if (notePlaying && now - noteTS > noteOffDelay){
                    endMyTone();
                    displayBuffer[colorIndex] = 0;
                    notePlaying = false;
                    toneIndex++;
                    noteTS = now;
                }
            }
        }   break;

    }
// at this point, im just resetting every variable at each case
}

void updateCurrentScore(){
    if (lastUpdatedNum == currentStreak){
        return;
    }
    else{
        lastUpdatedNum = currentStreak;
        u8 firstDigit = currentStreak/10;
        u8 secondDigit = currentStreak % 10;

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



void updateHighScore(){
    u8 firstDigit = highScore/10;
    u8 secondDigit = highScore % 10;

    // NOTE: can be removed so that 05 shows, instead of just 5
    if (firstDigit == 0){
        displayBuffer[2] = 0;
    }
    else{
        displayBuffer[0] = digitsPattern[firstDigit];
    }
    displayBuffer[1] = digitsPattern[secondDigit] | (1 << 7) /*Enable the colon seperator*/;
}

void setupTimer2(){
    cli();
    TCCR2A = 0; 
    TCCR2B = (1 << CS22) | (1 << CS21); // 256 PRESCALER (Fires ~244 times/sec)
    TIMSK2 = (1 << TOIE2); 
    sei();
}

void setupTimer1(){
    cli();

    TCCR1A = (1 << COM1B0);   // Toggle OC1B on compare match, which is what we want
    TCCR1B = (1 << WGM12);  // CTC with OCR1A as top
    TCCR1B |= (1 << CS11) | (1 << CS10);    // 64 PRESCALER
    // TIMSK1 = (1 << OCIE1A);     // Enable compare match A  . commenting cause the cpu is not involved anymore
    OCR1A = 65535;      // starting value
    OCR1B = 65535;      // has to be less than or equal to OCR1A
    sei();
}

void myTone(u16 note){  
    // Timer1 best bet is 64 PRESCALER
    TCNT1 = 0;   // reset the timer so that i can start counting up to OCR1A immediately, very important
    OCR1A = (FREQ / (2 * note)) - 1;    // 0 to OCR1A - 1 = OCR1A steps
    OCR1B = OCR1A;
    TCCR1A |= (1 << COM1B0);    //  Enable the toggle OC1B on compare match
}

void endMyTone(){
    TCCR1A &= ~(1 << COM1B0);   // changes it back to a normal GPIO pin
    dWrite(buzzerPin, LOW);
}

void clearPresses(){
    for (u8 i = 0; i < numColors && !btnPressedEvent; i++){
        colorBtns[i].wasPressed();      // clear stored presses
    } 
}

void setupADCandPin(){
    ADMUX = ADMUXSetupSettings | (potPin - A0); // which pin to watch
    ADCSRA = (1 << ADPS2);  // 16 prescaler no need for high accuracy
    ADCSRA |= (1 << ADEN);  // enable conversion
    ADCSRA |= (1 << ADSC);  // have to write to one to start conversion, no need to do it here, but just do it
}

void tagAlongTimer0(){
    DDRD |= ( 1 << oePin);      // set pin 5 as output
    TCCR0A |= (1 << COM0B1) | (1 << COM0B0);    // fast pwm mode, inverting mode, enable OC0B inverting mode because oe is active low
    // OCR0B doesnt have to be > OCR0A in fast PWM, they are independednt of each other
    // Only reason i used OCR0A instead of OCR0B is because pin 6 is broken
}
