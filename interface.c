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

#include <string.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include "lcd.h"
#include "keys.h"
#include "conv.h"
#include "fat.h"
#include "setup.h"

#define STR_AUX(x) #x
#define STR(x) STR_AUX(x)

static uint8_t linebuff[40];
static const uint8_t sestr[16] __attribute__ ((progmem)) = "  SIO2SD setup  ";
static const uint8_t fvstr[16] __attribute__ ((progmem)) = "  version: " STR(VERSMAJ) "." STR(VERSMIN) "  ";
static const uint8_t ncstr[16] __attribute__ ((progmem)) = " no SD/MMC card ";
static const uint8_t iestr[16] __attribute__ ((progmem)) = " card init error";
static const uint8_t ufstr[16] __attribute__ ((progmem)) = " unknown format ";
static const uint8_t npstr[16] __attribute__ ((progmem)) = "  no partition  ";
static const uint8_t nastr[16] __attribute__ ((progmem)) = " no ATARI folder";
static const uint8_t iostr[16] __attribute__ ((progmem)) = " card I/O error ";
static const uint8_t fustr[16] __attribute__ ((progmem)) = " firmware update";
static const uint8_t pwstr[16] __attribute__ ((progmem)) = "   please wait  ";
static const uint8_t tbstr[16] __attribute__ ((progmem)) = "  card too big  ";
static const uint8_t ftstr[16] __attribute__ ((progmem)) = "  format FATxx  ";
//static const uint8_t dbstr[16] __attribute__ ((progmem)) = "Dn: $xx(c) $xxxx";
static uint8_t setup_mode;
static uint8_t lastkey;
static uint16_t repcnt;
static uint16_t actiondelay;

#define SIO_DEBUG_DELAY 20000
#define KEY_FIRST_REPEAT 46000
#define KEY_NEXT_REPEAT 25000

//#define interface_clean_linebuff() memset(linebuff,32,40);
/*
void interface_clean_linebuff(void) {
	uint8_t *lb = linebuff;
	uint8_t c=16;
	do {
		*lb++=' ';
		c--;
	} while (c);
}
*/
void interface_init(uint8_t smode) {
	setup_mode = smode;
}

typedef void(*fn)(void);

void interface_key_action(uint8_t k) {
	fn reboot = (fn)0x0000;
	if (setup_mode) {
		switch (k) {
			case 1:
				setup_next_option();
				break;
			case 5:
				setup_prev_option();
				break;
			case 2:
				setup_next_value();
				break;
			case 6:
				setup_prev_value();
				break;
			case 3:
				reboot();
				break;
			case 7:
				fat_firmware_update();
				break;
		}
	} else {
		switch (k) {
			case 1:
				fat_key_nextdisk();
				break;
			case 2:
				fat_key_nextfile();
				break;
			case 3:
				fat_key_dirup();
				break;
			case 4:
				fat_key_enter();
				break;
			case 5:
				fat_key_prevdisk();
				break;
			case 6:
				fat_key_prevfile();
				break;
			case 7:
				fat_key_swapdisks();
				break;
			case 8:
				fat_key_clear();
				break;
		}
		fat_refresh();
		actiondelay = SIO_DEBUG_DELAY;
	}
}

void interface_task(void) {
	uint8_t k;
	k = keys_get();
	if (lastkey==k) { // same key still pressed
		if (repcnt>0) {
			repcnt--;
			if (repcnt==0) {
				repcnt=KEY_NEXT_REPEAT;
				interface_key_action(k);
			}
		}
	} else if (lastkey==0 && k!=0) {	// key pressed
		interface_key_action(k);
		repcnt = KEY_FIRST_REPEAT;
	} else { // key changed or released
		repcnt=0;
	}
	lastkey = k;
	if (actiondelay!=0) {
		actiondelay--;
	}
}

void interface_no_card(void) {
	lcd_put_line_p(0,ncstr);
	lcd_clear_line(1);
}

void interface_firmware_update(void) {
	lcd_put_line_p(0,fustr);
	lcd_put_line_p(1,pwstr);
}

void interface_setup_start(void) {
	lcd_put_line_p(0,sestr);
	lcd_put_line_p(1,fvstr);
}

void interface_card_atari_not_found(void) {
	lcd_put_line_p(0,nastr);
	lcd_clear_line(1);
}

void interface_card_no_partition(void) {
	lcd_put_line_p(0,npstr);
	lcd_clear_line(1);
}

void interface_card_unknown_format(void) {
	lcd_put_line_p(0,ufstr);
	lcd_clear_line(1);
}

void interface_too_many_clusters(void) {
	lcd_put_line_p(0,tbstr);
	lcd_clear_line(1);
}

void interface_card_init_error(void) {
	lcd_put_line_p(0,iestr);
	lcd_clear_line(1);
}

void interface_eio(void) {
	lcd_put_line_p(0,iostr);
	lcd_clear_line(1);
}

void interface_card_fat_type(uint8_t ft) {
	memset(linebuff,' ',40);
	memcpy_P(linebuff,ftstr,16);
	switch (ft) {
		case 1:
			linebuff[12]='1';
			linebuff[13]='2';
			break;
		case 2:
			linebuff[12]='1';
			linebuff[13]='6';
			break;
		case 3:
			linebuff[12]='3';
			linebuff[13]='2';
			break;
	}
	lcd_put_line(1,linebuff);
}

void interface_cfg_print(uint8_t lno,const PGM_P option) {
	char c;
	uint8_t *ptr;
	memset(linebuff,' ',40);
	ptr = linebuff;
	for (;;) {
		c = pgm_read_byte(option++);
		if (c) {
			*ptr++ = c;
		} else {
			break;
		}
	}
	lcd_put_line(lno,linebuff);
}

void interface_cfg_option_value(const PGM_P option,const PGM_P value) {
	interface_cfg_print(0,option);
	interface_cfg_print(1,value);
}

void interface_cfg_option_speed(const PGM_P option,uint32_t speed,uint8_t speedindex) {
	char c;
	interface_cfg_print(0,option);
	memset(linebuff,' ',40);
	c = conv_speed(linebuff,speed);
	linebuff[c+1]='(';
	linebuff[c+2]='$';
	linebuff[c+5]=')';
	conv_byte_to_hex(linebuff+c+3,speedindex);
	lcd_put_line(1,linebuff);
}

void interface_cfg_option_number(const PGM_P option,uint8_t number) {
	interface_cfg_print(0,option);
	memset(linebuff,' ',40);
	linebuff[0]=number+'0';
	lcd_put_line(1,linebuff);
}

void interface_show_fs_info(uint8_t cfno,uint8_t fname[39],uint8_t dir,uint8_t pname[39]) {
	linebuff[0]='D';
	linebuff[1]='1'+cfno;
	linebuff[2]=':';
	memcpy(linebuff+3,fname,37);
	lcd_put_line(0,linebuff);
	if (dir) {
		linebuff[0]='/';
		memcpy(linebuff+1,pname,39);
		lcd_put_line(1,linebuff);
	} else {
		memcpy(linebuff,pname,39);
		linebuff[39]=' ';
		lcd_put_line(1,linebuff);
	}
}

// 'XXXXXXXX XXXX   '
/*
void interface_debug(uint32_t f,uint16_t c) {
	if (actiondelay==0) {
		memset(linebuff,' ',40);
		conv_long_to_hex(linebuff,f);
		conv_word_to_hex(linebuff+9,c);
		lcd_put_line(1,linebuff);
	}
}
*/
// 'FFPP FFPP FFPP  '
/*
void interface_debug(uint8_t fno[3],uint8_t prio[3]) {
	if (actiondelay==0) {
		memset(linebuff,' ',40);
		conv_byte_to_hex(linebuff,fno[0]);
		conv_byte_to_hex(linebuff+2,prio[0]);
		conv_byte_to_hex(linebuff+5,fno[1]);
		conv_byte_to_hex(linebuff+7,prio[1]);
		conv_byte_to_hex(linebuff+10,fno[2]);
		conv_byte_to_hex(linebuff+12,prio[2]);
		lcd_put_line(1,linebuff);
	}
}
*/

// 'Dn: $xx(c) $xxxx'
void interface_sio_command(uint8_t cmddno,uint8_t command,uint16_t blockno) {
	if (actiondelay==0) {
		memset(linebuff,' ',40);
//		memcpy_P(linebuff,dbstr,16);
//		interface_clean_linebuff();
		if (cmddno>=0x31 && cmddno<=0x39) {
			linebuff[0]='D';
			linebuff[1]=cmddno;
		} else if (cmddno>=0x40 && cmddno<=0x48) {
			linebuff[0]='P';
			linebuff[1]=cmddno-0x0F;
		} else if (cmddno>=0x50 && cmddno<=0x58) {
			linebuff[0]='R';
			linebuff[1]=cmddno-0x1F;
		} else {
			conv_byte_to_hex(linebuff,cmddno);
		}
		linebuff[2]=':';
		linebuff[4]='$';
		conv_byte_to_hex(linebuff+5,command);
		linebuff[7]='(';
		if (command>=32 && command<127) {
			linebuff[8]=command;
		} else {
			linebuff[8]='.';
		}
		linebuff[9]=')';
		linebuff[11]='$';
		conv_word_to_hex(linebuff+12,blockno);
		lcd_put_line(1,linebuff);
	}
}
