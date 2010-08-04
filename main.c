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
//#include <avr/signal.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "delay.h"
#include "mmc.h"
#include "led.h"
#include "lcd.h"
#include "keys.h"
#include "interface.h"
#include "fat.h"
#include "sio.h"
#include "setup.h"

void mainloop (uint8_t setupmode) {
	uint8_t i;
	while (1) {
		if (setupmode==0) {
			if (sio_check_command()) {
				sio_process_command();
			}
		}
		if (mmc_card_removed()) {
			fat_removed(setupmode);
			led_error(0);
		}
		if (mmc_card_inserted()) {
			for (i=0 ; i<5 ; i++) {	// wait about 250ms
				DELAY(US_TO_TICKS(50000))
			}
			if (mmc_card_init()) {
				if (fat_init(setupmode)==0) {
					led_error(1);
				}
			} else {
				led_error(1);
				if (setupmode==0) {
					interface_card_init_error();
				}
			}
		}
		interface_task();
	}
}
/*
void main_setup(void) {
	while (1) {
		interface_task();
	}
}
*/
//int main(void) __attribute__((naked));

int main(void) {
	led_init();
	led_off();
	keys_init();
	lcd_init();
	mmc_init();
	if (keys_get()==4) {
		interface_init(1);
		setup_init();
		mainloop(1);
	} else {
		interface_init(0);
		sio_init(keys_shift());
		mainloop(0);
	}
	return 0;
}
