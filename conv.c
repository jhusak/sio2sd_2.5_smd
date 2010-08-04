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

#include <avr/pgmspace.h>
#include <inttypes.h>

static const uint8_t hexdigit[16] __attribute__ ((progmem)) = "0123456789ABCDEF";

void conv_byte_to_hex(uint8_t *b,uint8_t n) {
	*b++ = pgm_read_byte(hexdigit+(n>>4));
	*b++ = pgm_read_byte(hexdigit+(n&0xF));
}

void conv_word_to_hex(uint8_t *b,uint16_t n) {
	union t {
		uint16_t s;
		uint8_t c[2];
	} aux;
	aux.s = n;
	*b++ = pgm_read_byte(hexdigit+(aux.c[1]>>4));
	*b++ = pgm_read_byte(hexdigit+(aux.c[1]&0xF));
	*b++ = pgm_read_byte(hexdigit+(aux.c[0]>>4));
	*b++ = pgm_read_byte(hexdigit+(aux.c[0]&0xF));
}

void conv_long_to_hex(uint8_t *b,uint32_t n) {
	union t {
		uint32_t s;
		uint8_t c[4];
	} aux;
	aux.s = n;
	*b++ = pgm_read_byte(hexdigit+(aux.c[3]>>4));
	*b++ = pgm_read_byte(hexdigit+(aux.c[3]&0xF));
	*b++ = pgm_read_byte(hexdigit+(aux.c[2]>>4));
	*b++ = pgm_read_byte(hexdigit+(aux.c[2]&0xF));
	*b++ = pgm_read_byte(hexdigit+(aux.c[1]>>4));
	*b++ = pgm_read_byte(hexdigit+(aux.c[1]&0xF));
	*b++ = pgm_read_byte(hexdigit+(aux.c[0]>>4));
	*b++ = pgm_read_byte(hexdigit+(aux.c[0]&0xF));
}

uint8_t conv_speed(uint8_t *b,uint32_t s) {
	uint8_t c[8],i,j;
	for (j=0 ; j<8 ; j++) {
		c[j]=0;
	}
	i=32;
	do {
		for (j=0 ; j<8 ; j++) {
			c[j]<<=1;
		}
		if (s&0x80000000ULL) {
			c[0]++;
		}
		for (j=0 ; j<7 ; j++) {
			if (c[j]>=10) {
				c[j]-=10;
				c[j+1]++;
			}
		}
		s<<=1;
		i--;
	} while(i);
	j=7;
	i=0;
	do {
		if (i>0 || j<=2 || c[j]) {
			b[i++] = c[j]+'0';
		}
		j--;
	} while (j>1);
	b[i++]='.';
	b[i++]=c[1]+'0';
	b[i++]=c[0]+'0';
	return i;
}

/* convert to decimal is unused, and avr-gcc do not put away unused code */
#if 0
void conv_word_to_dec(uint8_t *b,uint16_t n) {
	uint8_t c1,c2,c3,c4,c5,i;
	c1=0;
	c2=0;
	c3=0;
	c4=0;
	c5=0;
	i=16;
	do {
		c1<<=1;
		c2<<=1;
		c3<<=1;
		c4<<=1;
		c5<<=1;
		if (n&0x8000) {
			c1++;
		}
		if (c1>=10) {
			c1-=10;
			c2++;
		}
		if (c2>=10) {
			c2-=10;
			c3++;
		}
		if (c3>=10) {
			c3-=10;
			c4++;
		}
		if (c4>=10) {
			c4-=10;
			c5++;
		}
		n<<=1;
		i--;
	} while(i);
	*b++=c5+'0';
	*b++=c4+'0';
	*b++=c3+'0';
	*b++=c2+'0';
	*b++=c1+'0';
}
#endif
