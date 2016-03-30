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

#define KEYS_PORT PORTD
#define KEYS_DDR DDRD
#define KEYS_PIN PIND
#define KEYS_MASK 0xf8
#define KEYS_SHIFT 0x80

// set low 5 bits as inputs, and pullups them
void keys_init(void) {
	KEYS_PORT = KEYS_PORT | KEYS_MASK;
	KEYS_DDR = KEYS_DDR & (~KEYS_MASK);
}

uint8_t keys_shift(void) {
	uint8_t c;
	c = KEYS_PIN & KEYS_MASK;
	return (c==KEYS_SHIFT || c==(~KEYS_SHIFT)&KEYS_MASK)?1:0;
}

uint8_t keys_get(void) {
	uint8_t c;
	c = KEYS_PIN;
	switch (c>>3) {
	case 0b11110:
	case 0b00001:
		return 1;
	case 0b11101:
	case 0b00010:
		return 2;
	case 0b11011:
	case 0b00100:
		return 3;
	case 0b10111:
	case 0b01000:
		return 4;
	case 0b01110:
	case 0b10001:
		return 5;
	case 0b01101:
	case 0b10010:
		return 6;
	case 0b01011:
	case 0b10100:
		return 7;
	case 0b00111:
	case 0b11000:
		return 8;
	}
	return 0;
}
