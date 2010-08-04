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

#define KEYS_PORT PORTB
#define KEYS_DDR DDRB
#define KEYS_PIN PINB

// set low 5 bits as inputs, and pullups them
void keys_init(void) {
	KEYS_PORT = KEYS_PORT | 0x1F;
	KEYS_DDR = KEYS_DDR & 0xE0;
}

uint8_t keys_shift(void) {
	uint8_t c;
	c = KEYS_PIN & 0x1F;
	return (c==0x10 || c==0x0F)?1:0;
}

uint8_t keys_get(void) {
	uint8_t c;
	c = KEYS_PIN & 0x1F;
	switch (c) {
	case 0x1E:
	case 0x01:
		return 1;
	case 0x1D:
	case 0x02:
		return 2;
	case 0x1B:
	case 0x04:
		return 3;
	case 0x17:
	case 0x08:
		return 4;
	case 0x0E:
	case 0x11:
		return 5;
	case 0x0D:
	case 0x12:
		return 6;
	case 0x0B:
	case 0x14:
		return 7;
	case 0x07:
	case 0x18:
		return 8;
	}
	return 0;
}
