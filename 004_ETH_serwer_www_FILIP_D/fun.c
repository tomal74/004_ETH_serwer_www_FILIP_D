/*
 * fun.c
 *
 *  Created on: 20 mar 2020
 *      Author: G505s
 */

#include <avr/io.h>
#include <string.h>
#include <stdio.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "fun.h"


void ctc_tim1_init(void) {
	/* CTC mode */
	TCCR1B |= (1<<WGM12);
	/* Pescaler = 1024 */
	TCCR1B |= (1<<CS10) | (1<<CS12);
	/* Compare Match A Interrupt Enable */
	TIMSK1 |= (1<<OCIE1A);
	/* CTC 'A' register = 12206 - F_IRQ = 1Hz - FCPU From ENC28J60 CLKOUT = 12.5MHz */
	OCR1A = 12206;
}


void io_init(void) {
	/* IO pins as inputs, pull-up with internal resistors to Vcc */
	CAPSENS1_DDR &= ~CAPSENS1_MASK;
	CAPSENS1_PORT |= CAPSENS1_MASK;
	CAPSENS2_DDR &= ~CAPSENS2_MASK;
	CAPSENS2_PORT |= CAPSENS2_MASK;

	MAIN_RELAY1_DDR &= ~MAIN_RELAY1_MASK;
	MAIN_RELAY1_PORT |= MAIN_RELAY1_MASK;
	MAIN_RELAY2_DDR &= ~MAIN_RELAY2_MASK;
	MAIN_RELAY2_PORT |= MAIN_RELAY2_MASK;

	FAILURE1_DDR &= ~FAILURE1_MASK;
	FAILURE1_PORT |= FAILURE1_MASK;
	FAILURE2_DDR &= ~FAILURE2_MASK;
	FAILURE2_PORT |= FAILURE2_MASK;

	/* LED pin as output - kathode to uC */
	LED_DDR |= LED_PIN;
	/* Switch off the LED */
	LED_OFF;
}


void ctc_tim0_init(void) {
	/* CTC mode */
	TCCR0A |= (1<<WGM01);
	/* Pescaler = 1024 */
	TCCR0B |= (1<<CS00) | (1<<CS02);
	/* Compare Match A Interrupt Enable */
	TIMSK0 |= (1<<OCIE0A);
	/* CTC 'A' register = 121 - F_IRQ = 100,058Hz - FCPU From ENC28J60 CLKOUT = 12.5MHz */
	OCR0A = 121;
}


/************** funkcja RelaySuperDebounce do obs³ugi np.pojedynczych wyjœæ przekaŸników ***************
 * 							AUTOR: Miros³aw Kardaœ
 * 							MODYFIKACJA: Tomasz Konieczka 13.03.2020r
 * 						   (KORONAWIRUS U BRAM... ZAMKNIÊTE SZKO£Y ITD.)
 * ZALETY:
 * 		- nie wprowadza najmniejszego spowalnienia
 * 		- mo¿na przydzieliæ ró¿ne akcje dla trybu klikniêcia
 *
 * Wymagania:
 * 	Timer programowy utworzony w oparciu o Timer sprzêtowy (przerwanie 100Hz)
 *
 * 	Parametry wejœciowe:
 *
 * 	*key_state - wskaŸnik na zmienn¹ w pamiêci RAM (1 bajt) - do przechowywania stanu klawisza
 *  *KPIN - nazwa PINx portu na którym umieszczony jest klawisz, np: PINB
 *  key_mask - maska klawisza np: (1<<PB3)
 *  push_proc - wskaŸnik do w³asnej funkcji wywo³ywanej raz po zwolenieniu przycisku
 *
 *  return:
 *
 *  0 - klawisz niewcisniety
 *  1 - klawisz wciœniêty
 *  2 - zbocze narastajace
 *  3 - zbocze opadajace
 **************************************************************************************/
uint8_t RelaySuperDebounce( uint8_t * key_state, volatile uint8_t *KPIN, uint8_t key_mask,
		volatile uint8_t *soft_timer, void (*push_proc)(void) ) {

	enum {idle, debounce, rising_edge, pressed};

	uint8_t key_press = !(*KPIN & key_mask);

	if( key_press && !*key_state ) {
		*key_state = debounce;
		*soft_timer = 30;
	}
	else if( *key_state  ) {
		if( key_press && debounce==*key_state && !*soft_timer ) {
			*key_state = rising_edge;
			*soft_timer = 30;
		}
		else if( key_press && rising_edge==*key_state && !*soft_timer ) {
			*key_state = pressed;
			return 2;
		}
		else if( key_press && pressed==*key_state && !*soft_timer ) {
			return 1;
		}
		else if( !key_press && *key_state>1 ) {
			if(push_proc) push_proc();						/* KEY_UP */
			*key_state=idle;
			return 3;
		}
	}
	return 0;
}


