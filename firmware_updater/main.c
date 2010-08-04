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

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <avr/io.h>
#include <string.h>
#include <inttypes.h>
#include "delay.h"
#include "mmc.h"
#include "lcd.h"

void (*app_start)(void) = 0x0000;

typedef union convert {
	uint8_t c[4];
//	uint16_t s[2];
	uint32_t l;
} convert;

static uint8_t buff[512];
static uint8_t cfgblock[248];

#define LED_PORT PORTC
#define LED_DDR DDRC
#define LED_PIN PINC

#define LED_1 5
#define LED_2 6
#define LED_3 7
#define LED_MASK 0xE0

void led_init(void) {
	LED_PORT &= ~LED_MASK;
	LED_DDR |= LED_MASK;
}

#define led_off() LED_PORT |= LED_MASK
#define led_error() LED_PORT &= ~(1<<LED_1)
#define led_read_start() LED_PORT &= ~(1<<LED_2)
#define led_read_end() LED_PORT |= (1<<LED_2)
#define led_write_start() LED_PORT &= ~(1<<LED_3)
#define led_write_end() LED_PORT |= (1<<LED_3)

void boot_program_page (uint32_t page, uint8_t *buf) {
	uint16_t i;
	eeprom_busy_wait();

	boot_page_erase(page);
	boot_spm_busy_wait();

	for (i=0; i<SPM_PAGESIZE; i+=2) { 
		uint16_t w = *buf++;
		w += (*buf++) << 8;
		boot_page_fill (page + i, w);
	}
	boot_page_write (page);
	boot_spm_busy_wait();
	boot_rww_enable (); 
}

void upgrade_firmware(void) {
	uint8_t i,adr;
	uint32_t sector;
	cli(); //Disable interrupts, just to be sure
	led_off();
	eeprom_read_block(cfgblock,0,248);
	if (memcmp(cfgblock,"SIO2SDFW",8)!=0) {
		led_error();
		lcd_send_err();
		return;
	}
	lcd_set_cursor();
	adr=0;
	do {
		led_read_start();
		sector = *(uint32_t*)(cfgblock+8+adr*4);
		if (mmc_read_sector(sector,buff)==0) {
			if (mmc_read_sector(sector,buff)==0) {
				led_error();	
				lcd_send_err();
				return;
			}
		}
		led_read_end();
		led_write_start();
		for (i=0 ; i<512/SPM_PAGESIZE ; i++) {
			boot_program_page(adr*512+i*SPM_PAGESIZE,buff+i*SPM_PAGESIZE);
		}
		led_write_end();
		adr++;
		if ((adr&0x3)==0) {
			lcd_next_hash();
		}
	} while (adr<60);
	eeprom_write_byte((uint8_t*)0,0xFF);
	led_read_start();
	led_write_start();
	lcd_send_ok();
}

int main(void) {
	uint8_t c;
	led_init();
	upgrade_firmware();
	for (c=0 ; c<20 ; c++) {	// wait 1s
		DELAY(US_TO_TICKS(50000))
	}
//	while (1) {}
	app_start();
	return 0;
}
