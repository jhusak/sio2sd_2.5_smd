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

#ifndef _DELAY_H_
#define _DELAY_H_

#include <avr/io.h>

#define US_TO_TICKS(us) (((us)*F_CPU)/64000000)
// TIMER1 MACROS for delay not less than us microseconds between DELAY_US_START and DELAY_US_END
#define DELAY_START(cnt) { \
	TCCR1B = 0;	\
	TCCR1A = 0;	\
	TCNT1 = 0; \
	OCR1A = cnt; \
	TIMSK &= 0xC3; \
	TIFR = 0x3C; \
	TCCR1B = 3;	\
}

#define DELAY_REFRESH TCNT1=0;
#define DELAY_IN_PROGRESS bit_is_clear(TIFR,OCF1A)
#define DELAY_END while(DELAY_IN_PROGRESS) {}
#define DELAY(cnt) { \
	DELAY_START(cnt) \
	DELAY_END \
}

#endif
