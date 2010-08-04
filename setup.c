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

#include <inttypes.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

#include "interface.h"

#define OPTIONS 5
#define EEPROM_ADDR(x) (uint8_t*)(255-x)
#define SPEED_EEPROM_ADDR (uint8_t*)255
#define CFGTOOL_EEPROM_ADDR (uint8_t*)254
#define LEDMODE_EEPROM_ADDR (uint8_t*)253
#define DEVID_EEPROM_ADDR (uint8_t*)252
#define WPROT_EEPROM_ADDR (uint8_t*)251

#define DEFAULT_SIO_SPEED 6

static uint8_t option;
static uint8_t value,maxvalue;
static uint8_t maxval[OPTIONS]={17,4,2,3,2};
static uint8_t defaultval[OPTIONS]={6,3,2,0,1};
static const char hsopt[] __attribute__ ((progmem)) = "sio high speed:";
static const char hsoff[] __attribute__ ((progmem)) = "off";
static const char ctopt[] __attribute__ ((progmem)) = "cfg tool load:";
static const char ctval0[] __attribute__ ((progmem)) = "never";
static const char ctval1[] __attribute__ ((progmem)) = "startup+shift";
static const char ctval2[] __attribute__ ((progmem)) = "every startup";
static const char ctval3[] __attribute__ ((progmem)) = "no card";
static const char ctval4[] __attribute__ ((progmem)) = "shift pressed";
static const char* ctval[] __attribute__ ((progmem)) = {ctval0,ctval1,ctval2,ctval3,ctval4};
static const char leopt[] __attribute__ ((progmem)) = "LED mode:";
static const char leval0[] __attribute__ ((progmem)) = "SD read/write";
static const char leval1[] __attribute__ ((progmem)) = "SIO read/write";
static const char leval2[] __attribute__ ((progmem)) = "SD act/SIO act";
static const char* leval[] __attribute__ ((progmem)) = {leval0,leval1,leval2};
static const char diopt[] __attribute__ ((progmem)) = "device id:";
static const char wpopt[] __attribute__ ((progmem)) = "write protect:";
static const char wpval0[] __attribute__ ((progmem)) = "never";
static const char wpval1[] __attribute__ ((progmem)) = "obey ATR flag";
static const char wpval2[] __attribute__ ((progmem)) = "always";
static const char* wpval[] __attribute__ ((progmem)) = {wpval0,wpval1,wpval2};

uint32_t setup_my_division(uint32_t d,uint16_t v) {
	uint32_t r=0,c=0;
	uint8_t i;
	for (i=0 ; i<32 ; i++) {
		r<<=1;
		c<<=1;
		if (d&0x80000000ULL) {
			c++;
		}
		d<<=1;
		if (v<=c) {
			c-=v;
			r++;
		}
	}
	// c == mod ; r == div
	if (c*2>=v) {	// round
		r++;
	}
	return r;
}

void setup_refresh(void) {
	if (option==0) {
		if (value==17) {
			interface_cfg_option_value(hsopt,hsoff);
		} else {
			interface_cfg_option_speed(hsopt,setup_my_division(178977250ULL,(2*(value+7))),value);
		}
	} else if (option==1) {
		interface_cfg_option_value(ctopt,(prog_char*)pgm_read_word(ctval+value));
	} else if (option==2) {
		interface_cfg_option_value(leopt,(prog_char*)pgm_read_word(leval+value));
	} else if (option==3) {
		interface_cfg_option_number(diopt,value);
	} else {
		interface_cfg_option_value(wpopt,(prog_char*)pgm_read_word(wpval+value));
	}
}

void setup_loadvalue(void) {
	maxvalue=maxval[option];
	value = eeprom_read_byte(EEPROM_ADDR(option));
	if (value>maxvalue) {
		value=defaultval[option];
	}
}

void setup_init(void) {
	option = 0xFF;
	interface_setup_start();
	// setup_refresh();
}

void setup_storevalue(void) {
	eeprom_write_byte(EEPROM_ADDR(option),value);
}

void setup_next_option(void) {
	if (option==0xFF) {
		option=0;
	} else {
		option++;
		if (option==OPTIONS) {
			option=0;
		}
	}
	setup_loadvalue();
	setup_refresh();
}

void setup_prev_option(void) {
	if (option==0xFF) {
		option=0;
	} else {
		option--;
		if (option==0xff) {
			option=OPTIONS-1;
		}
	}
	setup_loadvalue();
	setup_refresh();
}

void setup_next_value(void) {
	if (option==0xFF) {
		option=0;
		setup_loadvalue();
	} else {
		value++;
		if (value>maxvalue) {
			value=0;
		}
		setup_storevalue();
	}
	setup_refresh();
}

void setup_prev_value(void) {
	if (option==0xFF) {
		option=0;
		setup_loadvalue();
	} else {
		value--;
		if (value==0xff) {
			value = maxvalue;
		}
		setup_storevalue();
	}
	setup_refresh();
}

/*
uint8_t setup_get_sio_speed(void) {
	uint8_t c;
	c = eeprom_read_byte(SPEED_EEPROM_ADDR);
	if (c>17) {
		return DEFAULT_SIO_SPEED;
	}
	if (c==17) {
		return 255;
	}
	return c;
}
*/

uint8_t setup_get_option(uint8_t opt) {
	uint8_t c;
	c = eeprom_read_byte(EEPROM_ADDR(opt));
	if (c>maxval[opt]) {
		c=defaultval[opt];
	}
	return c;
}

/*
uint8_t setup_get_cfg_tool_mode(void) {
	uint8_t c;
	c = eeprom_read_byte(CFGTOOL_EEPROM_ADDR);
	if (c>=3) {
		return 3;
	} else {
		return c;
	}
}

uint8_t setup_get_led_mode(void) {
	uint8_t c;
	c = eeprom_read_byte(LEDMODE_EEPROM_ADDR);
	if (c>=2) {
		return 2;
	} else {
		return c;
	}
}

uint8_t setup_get_device_id(void) {
	uint8_t c;
	c = eeprom_read_byte(DEVID_EEPROM_ADDR);
	if (c>3) {
		return 0;
	} else {
		return c;
	}
}

uint8_t setup_get_wpmode(void) {
	uint8_t c;
	c = eeprom_read_byte(WPROT_EEPROM_ADDR);
	if (c>2) {
		return 1;
	} else {
		return c;
	}
}
*/
