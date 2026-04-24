#include "btn.h"


Button::Button(u8 p){
    pin = p;
    pinMode(pin, INPUT_PULLUP);
}

void Button::watch(){ // put at top of loop
    if (millis() - lastCheckedTime > debounceDelay){
        bool currentState = dRead(pin);
        if (currentState != lastCheckedState){
            lastCheckedState = currentState;
            if (currentState == LOW){ // high to low, held down
                pressEvent = true;
            }
        }

        lastCheckedTime = millis();
    }
}

bool Button::wasPressed(){
    if (pressEvent){
        pressEvent = false;
        return true;
    }
    return false;
}
