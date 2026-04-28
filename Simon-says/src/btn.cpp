#include "btn.h"


Button::Button(u8 p){
    pin = p;
    pinMode(pin, INPUT_PULLUP);
}

void Button::watch(u32 now){ // put at top of loop
    if (now - lastCheckedTime > debounceDelay){
        bool currentState = dRead(pin);
        if (currentState != lastCheckedState){
            lastCheckedState = currentState;
            if (currentState == LOW){ // high to low, held down
                pressEvent = true;
            }
        }

        lastCheckedTime = now;  // lean function, difference in old now (millis()) - new now is very tiny
    }
}

bool Button::wasPressed(){
    if (pressEvent){
        pressEvent = false;
        return true;
    }
    return false;
}
