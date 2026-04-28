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
#define redBtn      0
#define greenBtn    1
#define blueBtn     2
#define yellowBtn   3
#define buzzerPin   10
#define oePin       4   // ! make sure to change PORTs if change
#define latchPin    7   // !
#define clockPin    11
#define dataPin     12
#define potPin      A0
#define brokenPins  6, 8, 9

// tone stuff
#define ONE_SECOND_MS 1000
#define delay_Multiplier_S 1.3
#define delay_Multiplier_F 1.5
#define numSuccessNotes 4   // ! remember to change if change
#define numFailureNotes 3   // !

#define ADMUXSetupSettings (1 << REFS1) | (1 << REFS0)  /*use internal 1.1v as referece */ | (1 << ADLAR) // make it left adjusted, so that we only grab ADCH 

void updateCurrentScore();
void setupTimer2();
void setupTimer1();
void myTone(u16 note); 
void endMyTone();
void clearPresses();
void setupADCandPin();

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


enum activity {addLed, showingPattern, readingBtns, levelPassed, levelFailed};
activity currentActivity = addLed;


// For display
// 0-1 is Highscore, 2-3 is current streak, 4 is led to display
volatile u8 displayBuffer[5] = {0};
volatile u8 currentDigit = 0;
volatile const uint8_t digitsPattern[10] =  {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
volatile u8 brightness = 255;
volatile u8 numTimesVisitedInterrupt = 0;

// 244Hz
ISR(TIMER2_OVF_vect){
    if (numTimesVisitedInterrupt == brightness){
        PORTD |= (1 << oePin);  // active low, so sending HIGH connects to GND
    } else if (numTimesVisitedInterrupt == 0){
        PORTD &= ~(1 << oePin);
    }
    PORTD &= ~(1 << latchPin); 
    msbShiftOut(sRegs, ~(1 << currentDigit)); 
    msbShiftOut(sRegs, displayBuffer[currentDigit]); 
    PORTD |= (1 << latchPin); 
    
    currentDigit++;
    if (currentDigit >= numDigits) currentDigit = 0;
    numTimesVisitedInterrupt++;
}

void setup(){
    setupTimer2();
    setupTimer1();
    setupADCandPin();
    randomSeed(analogRead(A0));

    pinMode(oePin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(dataPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    dWrite(buzzerPin, LOW);
}

#define aReadDelay 100
u32 aReadTS = millis();

u8 onLed = 0;
u32 lastLedTS = millis();
// ! storing as a macro to save space, will move to the top later
#define ledOffDelay 100
u8 ledOnDelay = 255;
bool ledIsOn = false;

bool btnPressedEvent = false;
bool flashedPressedLed = false;
u8 pressedBtn;  
u8 indexToEnter;

u8 toneIndex = 0;
bool notePlaying;
u16 noteOffDelay;
#define noteOnDelay 100
u32 noteTS = millis();
u8 correctLed;      // the led that the user was supposed to press

void loop(){
    u32 now = millis();     // avoid calling millis() 15 times per loop becuase i have no delay() and no blocks, difference in accuracy very small
    for (u8 i = 0; i < numColors; i++){
        colorBtns[i].watch(now);
    }
    if ((now - aReadTS > aReadDelay) && ((ADCSRA & (1 << ADSC)) == 0) /*becomes 0 when conversion complete*/ ){
        brightness = ADCH;  // High part, left adjusted
        aReadTS = now;
        ADCSRA |= (1 << ADSC);  // start conversion 
    }
    switch (currentActivity){
        case addLed:{
            u8 randomLed = random(0, numColors);     // max exclusive
            colorArray[currentStreak] = randomLed;
            currentStreak++;
            updateCurrentScore();
            currentActivity = showingPattern;
            onLed = 0;
            ledIsOn = false;
            btnPressedEvent = false;
            flashedPressedLed = false;
            lastLedTS = now;   // Because the next state needs it to be refreshed
            // Because this case runs every time 
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
// atp, im just resetting every variable at each case
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

    // TCCR1A = 0;     // CTC mode
    TCCR1A = (1 << COM1B0);   // Toggle OC1B on compare match, which is what we want
    TCCR1B = (1 << WGM12);  // CTC with OCR1A as top
    // TCCR1B |= (1 << CS10);    // 1 PRESCALER
    TCCR1B |= (1 << CS11) | (1 << CS10);    // 64 PRESCALER
    // TIMSK1 = (1 << OCIE1A);     // Enable compare match A  . commenting cause the cpu is not involved anymore
    OCR1A = 65535;      // starting value
    OCR1B = 65535;      // has to be less than or equal to OCR1A

    sei();

}

void myTone(u16 note){  
    // Timer1 best bet is 64 PRESCALER
    TCNT1 = 0;   // reset the timer so that i can start counting up to OCR1A immediately
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
    ADCSRA |= (1 << ADSC);  // have to write to one to start conversion, no need to do it here, but just do it
}

// /* Bug Log:
//     Using a ISR frequency of 62500 Hz caused loop() to not run 
//         fix -> Changed to PRESCALER of 256. I used 62500Hz so i could use the same vector for other things but it didnt give brain to other functions

//     Using tone broke my logic completely, caused confusing unrelated bugs like the display blinking currentStreak times, instead of the Leds, this is because tone also uses timer2
//         fix ->  Going to create a myTone() function, that is only "closely" accurate for the notes used in this game, which are smaller notes, so i dont have to have a very high resolution


//     The loop was very responsive, causing it to store button presses and when it was read, i read the previous button presses and it messed up the logic. very sneaky bug
//         fix -> I cleared all the buttons before the next round

//     The tone worked, but it sounded grainy, which was most likely due to the ISR in Timer2, when we tried to generate a tone, the two most likely tried to clash with each other
//         fix -> changed from a software interrupt to a hardware interrupt, instead of the cpu running a task or waiting for timer2's ISR to finish, it immediately toggles the pin
//         Could have used OC1A which is Digital pin 9, but my pin 9 on my atmega328p chip is broken, so i used OC1B (on Digital pin 10), and switched latchPin to DP8(PB0)

//     // ! Always check pinMode(), produces annoying bugs
//     // ! Always check GND connections
// */

// #include <arduino.h>

// u8 aRead(u8 pin);

// volatile u8 result;
// #define  ADMUXSetupSettings  (1 << REFS1) | (1 << REFS0)  /*use internal 1.1v as referece */ | (1 << ADLAR) // make it left adjusted, so that we only grab ADCH 

// void setup(){
//     ADMUX = ADMUXSetupSettings;
//     // ADCSRA = (1 << ADPS2);      // 16 prescaler
//     ADCSRA = (1 << ADPS0);      // 2 prescaler
//     Serial.begin(9600);
    

// }

// void loop(){
//     Serial.println(aRead(A0));
//     delay(100);
// }

// u8 aRead(u8 pin){
//     if (pin > A5) return 0;
//     ADMUX = ADMUXSetupSettings | (pin - A0);
    
//     ADCSRA |= (1 << ADEN);  // turn it on
    
//     ADCSRA |= (1 << ADSC);  // have to write to one to start conversion
    
//     // 3 methods
//     while (ADCSRA & (1 << ADSC)); // loop until ADSC bit becomes 0
//     // ADCSRA &= ~(1 << ADEN);  // turn it off when done no need to turn off 
//     return ADCH;
//     // // or replace with
//     // ADCSRA |= (1 << ADIE);   // ADC enable interrupt

// }
// // ISR(ADC_vect){
// //     result = ADCH;
// // }


/*
    Analog input channel i.e Aref or Avcc is selected by writing to the MUX buts in ADMUX    
    Enable ADC by setting the ADC enable bit, ADEN in ADCSRA
    ADEN MUST be set, 
    ADC doesnt consume power when ADEN is cleared 

    ADC returns the 10 bit result in ADCH and ADCL, with bit 8-9 in ADCH, and 0-7 in ADCL, can be changed bty setting the ADLAR in ADMUX, but we dont care abt that 
    NOTE: thinking of making it left adjusted tho, since i dont need crazy precision (only 8 bits), and just read ADCH like the datasheet says, but i have to make it left adjusted 
    0000 0011 1000 1000 which is the normal 10-bit result with a decimal value of 904 becomes 1110 0010 which is 226 in decimal, 904/1024 = 226/256

    Dont think this matters to me but if right adjusted, we have to read ADCL first 
    
    we start a conversion(reading) by disabling the power reduction ADC bit PRADC, and enabling the ADC start conversion bit ADSC, ADSC lives in ADCSRA but PRADC is somewhere else
    NOTE: check if we need to disable PRADC, or if its already disabled
    ADSC stays HIGH as long as conversion is happening,  and is cleared by hardware when fisnished, it will finish the current conversion before moving to a new channel if one happens to be selected mid conversion

    Auto triggering?
        enabled by enabling the ADATE bit in ADCSRA
        trigger source selected by choosing which ADTS2:0 bits are on in ADCSRB, dont think ill need it but seems cool, can be used to start conversion on certain timer overflows or compares etc
        a conversion can be triggered without an interrupt, so just the flag needed, but u need to clear it to detect another positive edge
        go to read this parrt again to understand fully
    
    
    NOTE: By degault, approximately 50 to 200kHz is needed to get max resolution, but if the lower than 10 bits is needed, the input clock freq can be higher than 200kHz to get a higher sample rate, will most likely do

    Prescaling is set by the ADPS bits in ADCSRA. Able to generate any cpu freq above 100kHz (any?.. what abt 300MHz 🤨)
    prescaler runs as long as ADEN is HIGH, resets when ADEN is LOW

    normal conversion takes 13 ADC clock cycles, wonder how many ADC = CPU. Takes 25 the first time to actually initialize internal stuff

    the internal 1.1v takes sometime to stabilize, so the first read might be wwrong


*/