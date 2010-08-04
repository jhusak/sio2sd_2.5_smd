/*
 * SIO2SD
 *
 * Copyright (C) 2005-2010 Jakub Kruszona-Zawadzki
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <avr/io.h>
#include <inttypes.h>
#include "setup.h"

#define LED_PORT PORTC
#define LED_DDR DDRC
#define LED_PIN PINC

#define LED_1 5
#define LED_2 6
#define LED_3 7
#define LED_MASK 0xE0

// modes:
//  0 - LED_2 = sd_read ; LED_3 = sd_write
//  1 - LED_2 = sio_read ; LED_3 = sio_write
//  2 - LED_2 = sd_op ; LED_3 = sio_op

static uint8_t mode;

void led_init(void) {
	LED_PORT &= ~LED_MASK;	// on startp light them all
	LED_DDR |= LED_MASK;
	mode = setup_get_led_mode();
}

void led_error(uint8_t on) {
	if (on) {
		LED_PORT &= ~(1<<LED_1);
	} else {
		LED_PORT |= (1<<LED_1);
	}
}

void led_sd_read(uint8_t on) {
	if (mode==0 || mode==2) {
		if (on) {
			LED_PORT &= ~(1<<LED_2);
		} else {
			LED_PORT |= (1<<LED_2);
		}
	}
}

void led_sd_write(uint8_t on) {
	if (mode==2) {
		if (on) {
			LED_PORT &= ~(1<<LED_2);
		} else {
			LED_PORT |= (1<<LED_2);
		}
	} else if (mode==0) {
		if (on) {
			LED_PORT &= ~(1<<LED_3);
		} else {
			LED_PORT |= (1<<LED_3);
		}
	}
}

void led_sio_read(uint8_t on) {
	if (mode==1) {
		if (on) {
			LED_PORT &= ~(1<<LED_2);
		} else {
			LED_PORT |= (1<<LED_2);
		}
	} else if (mode==2) {
		if (on) {
			LED_PORT &= ~(1<<LED_3);
		} else {
			LED_PORT |= (1<<LED_3);
		}
	}
}

void led_sio_write(uint8_t on) {
	if (mode==1 || mode==2) {
		if (on) {
			LED_PORT &= ~(1<<LED_3);
		} else {
			LED_PORT |= (1<<LED_3);
		}
	}
}

void led_sio_other(uint8_t on) {
	if (mode==2) {
		if (on) {
			LED_PORT &= ~(1<<LED_3);
		} else {
			LED_PORT |= (1<<LED_3);
		}
	}
}

void led_off(void) {
	LED_PORT |= LED_MASK;
}
