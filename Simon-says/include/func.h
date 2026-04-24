#ifndef MYFUNCTIONS_H
#define MYFUNCTIONS_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

#define u8 uint8_t
#define i8 int8_t
#define u32 uint32_t

typedef struct {
    u8 dataPin, clockPin, latchPin;
} my595;


void msbShiftOut(my595 &shiftReg, u8 pattern);
void dWrite(u8 pin, bool value);
bool dRead(u8 pin);
void aWrite(u8 pin, u8 dutyCycle);
void dToggle(u8 pin);


typedef struct {
    u32 lastCheckedTime;
    u8 btnPin;
    bool lastCheckedState;
    bool pressed;
} btnInfo;

#ifdef __cplusplus
}
#endif

#endif