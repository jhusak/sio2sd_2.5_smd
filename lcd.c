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
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include "delay.h"
#include "cbisbi.h"

#define nop()  __asm__ __volatile__ ("nop" ::)

#define LCD_PORT PORTA
#define LCD_PIN PINA
#define LCD_DDR DDRA

#define LCDDATA_PORT PORTB
#define LCDDATA_PIN PINB
#define LCDDATA_DDR DDRB

#define LCD_E 5
#define LCD_RW 6
#define LCD_RS 7

static uint8_t lcdon;

void lcd_delay_loop(uint8_t __count) {
	__asm__ volatile (
		"1: dec %0" "\n\t"
		"brne 1b"
		: "=r" (__count)
		: "0" (__count)
	);
}

void lcd_write(uint8_t data,uint8_t reg) {
	cli();
	cbi(LCD_PORT,LCD_RW);

	if (reg) {
		cbi(LCD_PORT,LCD_RS);
	} else {
		sbi(LCD_PORT,LCD_RS);
	}

	LCDDATA_DDR |= 0x0F;

	LCDDATA_PORT = (LCDDATA_PORT & 0xF0) | ((data>>4) & 0x0F);
	sbi(LCD_PORT,LCD_E);
	lcd_delay_loop(3);
	cbi(LCD_PORT,LCD_E);

	if (reg<2) {
		LCDDATA_PORT = (LCDDATA_PORT & 0xF0) | (data & 0x0F);
		sbi(LCD_PORT,LCD_E);
		lcd_delay_loop(3);
		cbi(LCD_PORT,LCD_E);
	}

	LCDDATA_DDR &= 0xF0;

	sei();
}

uint8_t lcd_read(uint8_t reg) {
	uint8_t data;
	cli();
	sbi(LCD_PORT,LCD_RW);

	if (reg) {
		cbi(LCD_PORT,LCD_RS);
	} else {
		sbi(LCD_PORT,LCD_RS);
	}

	LCDDATA_DDR &= 0xF0;

	sbi(LCD_PORT,LCD_E);
	lcd_delay_loop(3);
	data = LCDDATA_PIN << 4;
	cbi(LCD_PORT,LCD_E);
	lcd_delay_loop(3);
	sbi(LCD_PORT,LCD_E);
	lcd_delay_loop(3);
	data = data | (LCDDATA_PIN & 0x0F);
	cbi(LCD_PORT,LCD_E);

	sei();
	return data;
}

void lcd_waitbusy(void) {
	uint16_t count=10000;
	while (lcd_read(1)&0x80) {
		lcd_delay_loop(50);
		count--;
		if (count==0) {
			lcdon=0;
			return;
		}
	}
}

#ifndef FIRMWARE
void lcd_init(void) {
	LCD_DDR |= 0xE0;
	cbi(LCD_PORT,LCD_E);
	lcdon=1;
	DELAY(US_TO_TICKS(30000))
//	delay_ms(30);
	lcd_write(0x30,2);
	DELAY(US_TO_TICKS(5000))
//	delay_ms(5);
	lcd_write(0x30,2);
	DELAY(US_TO_TICKS(1000))
//	delay_ms(1);
	lcd_write(0x20,2);
	lcd_waitbusy();
	lcd_write(0x08,1);
	lcd_waitbusy();
	lcd_write(0x01,1);
	lcd_waitbusy();
	lcd_write(0x06,1);
	lcd_waitbusy();
	lcd_write(0x0c,1);
	lcd_waitbusy();
//	lcd_write('O',0);
//	lcd_waitbusy();
//	lcd_write('K',0);
//	lcd_waitbusy();
}

/*
void lcd_setpos(const int8_t x,const int8_t y) {
	register int8_t c=128;
	if (x<40) {
		c+=x;
	}
	if (y==1) {
		c+=40;
	}
	lcd_write(c,1);
	lcd_waitbusy();
}

void lcd_puts_p(const int8_t *progmem_s ) {
	register int8_t c;        
	while ( (c = pgm_read_byte(progmem_s++)) ) {
		lcd_write(c,0);
		lcd_waitbusy();
	}
}

void lcd_puts(const int8_t *s) {
	register int8_t c;
	while ((c=*s++)) {
		lcd_write(c,0);
		lcd_waitbusy();
	}
}
*/

void lcd_clear_line(const uint8_t lno) {
	register uint8_t i;
	if (lcdon==0) {
		return;
	}
	if (lno==0) {
		lcd_write(128,1);
	} else {
		lcd_write(168,1);
	}
	lcd_waitbusy();
	i=40;
	do {
		lcd_write(' ',0);
		lcd_waitbusy();
		i--;
	} while(i);
}

void lcd_put_line(const uint8_t lno,const uint8_t *buff) {
	register uint8_t i;
	if (lcdon==0) {
		return;
	}
	if (lno==0) {
		lcd_write(128,1);
	} else {
		lcd_write(168,1);
	}
	lcd_waitbusy();
	i=40;
	do {
		lcd_write(*buff++,0);
		lcd_waitbusy();
		i--;
	} while(i);
}

void lcd_put_line_p(const uint8_t lno,const uint8_t *buff) {
	register uint8_t i;
	if (lcdon==0) {
		return;
	}
	if (lno==0) {
		lcd_write(128,1);
	} else {
		lcd_write(168,1);
	}
	lcd_waitbusy();
	i=16;
	do {
		lcd_write(pgm_read_byte(buff),0);
		buff++;
		lcd_waitbusy();
		i--;
	} while(i);
	i=24;
	do {
		lcd_write(' ',0);
		lcd_waitbusy();
		i--;
	} while (i);
}

void lcd_set_cursor(void) {
	lcd_write(168,1);
	lcd_waitbusy();
}

void lcd_next_hash(void) {
	lcd_write('#',0);
	lcd_waitbusy();
}

#else

void lcd_set_cursor(void) {
	lcd_write(168,1);
	lcdon=1;
	lcd_waitbusy();
	lcd_write('#',0);
	lcd_waitbusy();
	lcd_write('#',0);
	lcd_waitbusy();
}

void lcd_next_hash(void) {
	lcd_write('#',0);
	lcd_waitbusy();
}

void lcd_send_err(void) {
	lcd_write(168,1);
	lcd_waitbusy();
	lcd_write('E',0);
	lcd_waitbusy();
	lcd_write('R',0);
	lcd_waitbusy();
	lcd_write('R',0);
	lcd_waitbusy();
	lcd_write('O',0);
	lcd_waitbusy();
	lcd_write('R',0);
	lcd_waitbusy();
	lcd_write(' ',0);
	lcd_waitbusy();
}

void lcd_send_ok(void) {
	uint8_t i;
	lcd_write(168,1);
	lcd_waitbusy();
	for (i=0 ; i<6 ; i++) {
		lcd_write('#',0);
		lcd_waitbusy();
	}
	lcd_write(' ',0);
	lcd_waitbusy();
	lcd_write('O',0);
	lcd_waitbusy();
	lcd_write('K',0);
	lcd_waitbusy();
	lcd_write(' ',0);
	lcd_waitbusy();
}

#endif
