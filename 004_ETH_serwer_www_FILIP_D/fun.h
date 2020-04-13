/*
 * fun.h
 *
 *  Created on: 20 mar 2020
 *      Author: G505s
 */

#ifndef FUN_H_
#define FUN_H_

#include <avr/io.h>

// dioda LED
#define LED_DDR DDRC
#define LED_PIN (1<<PC4)			// definicja pinu do którego pod³¹czona jest dioda
#define LED_TOG PORTC ^= LED_PIN	// makrodefinicja – zmiana stanu diody
#define LED_OFF PORTC |= LED_PIN
#define LED_ON PORTC &= ~LED_PIN

#define CAPSENS1_PORT PORTD
#define CAPSENS1_PIN PIND
#define CAPSENS1_DDR DDRD
#define CAPSENS1_MASK (1<<PD4)

#define CAPSENS2_PORT PORTD
#define CAPSENS2_PIN PIND
#define CAPSENS2_DDR DDRD
#define CAPSENS2_MASK (1<<PD3)

#define MAIN_RELAY1_PORT PORTD
#define MAIN_RELAY1_PIN PIND
#define MAIN_RELAY1_DDR DDRD
#define MAIN_RELAY1_MASK (1<<PD6)

#define MAIN_RELAY2_PORT PORTD
#define MAIN_RELAY2_PIN PIND
#define MAIN_RELAY2_DDR DDRD
#define MAIN_RELAY2_MASK (1<<PD5)

#define FAILURE1_PORT PORTB
#define FAILURE1_PIN PINB
#define FAILURE1_DDR DDRB
#define FAILURE1_MASK (1<<PB0)

#define FAILURE2_PORT PORTD
#define FAILURE2_PIN PIND
#define FAILURE2_DDR DDRD
#define FAILURE2_MASK (1<<PD7)


void ctc_tim1_init(void);
void io_init(void);
void ctc_tim0_init(void);
uint8_t RelaySuperDebounce( uint8_t * key_state, volatile uint8_t *KPIN, uint8_t key_mask,
		volatile uint8_t *soft_timer, void (*push_proc)(void) );



#endif /* FUN_H_ */
