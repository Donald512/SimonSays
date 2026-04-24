#include "func.h"

void msbShiftOut(my595 &shiftReg, u8 pattern){

	for (uint8_t i = 0; i < 8; i++){		
		if (shiftReg.dataPin < 8){
			if ((pattern & (1 << 7)) != 0) PORTD |= (1 << shiftReg.dataPin);
			else PORTD &= ~(1 << shiftReg.dataPin);	
		}
		else if (shiftReg.dataPin < 14){
			if ((pattern & (1 << 7)) != 0) PORTB |= (1 << (shiftReg.dataPin - 8));
			else PORTB &= ~(1 << (shiftReg.dataPin - 8));
		}
		else if (shiftReg.dataPin < A6){
			if ((pattern & (1 << 7)) != 0) PORTC |= (1 << (shiftReg.dataPin - A0));
			else PORTC &= ~(1 << (shiftReg.dataPin - 8));
		}
		else return;
 
		pattern = pattern << 1;

		if (shiftReg.clockPin < 8) {
			PORTD |= (1 << shiftReg.clockPin);
			PORTD &= ~(1 << shiftReg.clockPin);
		}
		else if (shiftReg.clockPin < 14){
			PORTB |= (1 << (shiftReg.clockPin - 8));
			PORTB &= ~(1 << (shiftReg.clockPin - 8));
		}
		else {
			PORTC |= (1 << (shiftReg.clockPin - A0));
			PORTC &= ~(1 << (shiftReg.clockPin - A0));
		}
	}

}

void dWrite(uint8_t pin, bool value){
	if (pin > A5) return;
	if (pin < 8){
		if (value) 	PORTD |= ( 1 << pin);
		else 		PORTD &= ~(1 << pin);
	}
	else if (pin < 14){
		pin -= 8;
		if (value) 	PORTB |= ( 1 << (pin));
		else 		PORTB &= ~(1 << (pin));
	}
	else{
		pin -= A0;
		if (value) 	PORTC |= ( 1 << (pin));
		else 		PORTC &= ~(1 << (pin));
	} 
}

bool dRead(uint8_t pin){
	if (pin > A5)	return LOW;
	if (pin < 8)	return (PIND & (1 << pin)) != LOW;
	else if (pin < A0) return (PINB & (1 << (pin - 8))) != LOW;
	else return (PINC & (1 << (pin - A0))) != LOW;
}

void dToggle(u8 pin){
	if (pin > A5) return;
	if (pin < 8){
		PORTD ^= (1 << pin);
	}
	else if (pin < 14){
		pin -= 8;
		PORTB ^= ( 1 << (pin));
	}
	else{
		pin -= A0;
		PORTC ^= ( 1 << (pin));
	} 
}


/*
// Prescaler of 1
// 16 ticks per Us, 16 million ticks per s
// I will set OCR2A to 255
// This ISR will run 62500 times a second

// Since the bit switch runs once every 256 (Before n overflows), the real frequncy is ~244.1 Hz/

#define persistentMemory static
volatile uint8_t createdPinsDCycle[20] = {0};

volatile uint8_t PWMEnabledD = 0;	// pins 0 to 7
volatile uint8_t PWMEnabledB = 0;	// pins 8 to 13
volatile uint8_t PWMEnabledC = 0;	// pins A0 to A5

ISR(TIMER2_COMPA_vect){
	persistentMemory uint8_t n = 0;

	uint8_t d = PWMEnabledD;
	uint8_t b = PWMEnabledB;
	uint8_t c = PWMEnabledC;

	// NOTE: overflow takes care of resetting

	if (d | b | c){
		if (n == 0){
			PORTD |= d;
			PORTB |= b;
			PORTC |= c;
		}
		else{
			// NOTE: Heard this was faster than loops
			if (d){
				if (n == createdPinsDCycle[0])	PORTD &= ~_BV(0);	// NOTE: _BV preprocessor macro replaces with the value before runtime 
				if (n == createdPinsDCycle[1])	PORTD &= ~_BV(1);
				if (n == createdPinsDCycle[2])	PORTD &= ~_BV(2);
				if (n == createdPinsDCycle[3])	PORTD &= ~_BV(3);
				if (n == createdPinsDCycle[4])	PORTD &= ~_BV(4);
				if (n == createdPinsDCycle[5])	PORTD &= ~_BV(5);
				if (n == createdPinsDCycle[6])	PORTD &= ~_BV(6);
				if (n == createdPinsDCycle[7])	PORTD &= ~_BV(7);
			}
			if (b){
				if (n == createdPinsDCycle[8])	PORTB &= ~_BV(0);
				if (n == createdPinsDCycle[9])	PORTB &= ~_BV(1);
				if (n == createdPinsDCycle[10])	PORTB &= ~_BV(2);
				if (n == createdPinsDCycle[11])	PORTB &= ~_BV(3);
				if (n == createdPinsDCycle[12])	PORTB &= ~_BV(4);
				if (n == createdPinsDCycle[13])	PORTB &= ~_BV(5);
			}
			if (c){
				if (n == createdPinsDCycle[14])	PORTC &= ~_BV(0);
				if (n == createdPinsDCycle[15])	PORTC &= ~_BV(1);
				if (n == createdPinsDCycle[16])	PORTC &= ~_BV(2);
				if (n == createdPinsDCycle[17])	PORTC &= ~_BV(3);
				if (n == createdPinsDCycle[18])	PORTC &= ~_BV(4);
				if (n == createdPinsDCycle[19])	PORTC &= ~_BV(5);
			}
		}
	}

	n++;
} 

void aWrite(uint8_t pin, uint8_t dutyCycle){
	if( pin < 8){
		if (dutyCycle == 0){
			PWMEnabledD &= ~(1 << pin);
			PORTD &= ~(1 << pin);	// Becomes digital, turn off
		}
		else if (dutyCycle == 255){
			PWMEnabledD &= ~(1 << pin);
			PORTD |= (1 << pin);	// Becomes pure digital, turn on
		}
		else{
			PWMEnabledD |= (1 << pin);
			createdPinsDCycle[pin] = dutyCycle;
		}	return;
	}
	else if (pin < 14){
		pin = pin - 8;
		if (dutyCycle == 0){
			PWMEnabledB &= ~(1 << pin);
			PORTB &= ~(1 << pin);
		}
		else if (dutyCycle == 255){
			PWMEnabledB &= ~(1 << pin);
			PORTB |= (1 << pin);
		}
		else{
			PWMEnabledB |= (1 << pin);
			createdPinsDCycle[pin + 8] = dutyCycle;
		}	return;
	}
	else if (pin < 20){
		pin = pin - 14;
		if (dutyCycle == 0){
			PWMEnabledC &= ~(1 << pin);
			PORTC &= ~(1 << pin);
		}
		else if (dutyCycle == 255){
			PWMEnabledC &= ~(1 << pin);
			PORTC |= (1 << pin);
		}
		else{
			PWMEnabledC |= (1 << pin);
			createdPinsDCycle[pin + 8] = dutyCycle;
		}	return;

	}

}
	*/