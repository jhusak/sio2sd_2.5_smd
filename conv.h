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

#ifndef _CONV_H_
#define _CONV_H_

#include <inttypes.h>

void conv_byte_to_hex(uint8_t *b,uint8_t n);
void conv_word_to_hex(uint8_t *b,uint16_t n);
void conv_long_to_hex(uint8_t *b,uint32_t n);
uint8_t conv_speed(uint8_t *b,uint32_t s);
//void conv_word_to_dec(uint8_t *b,uint16_t n);

#endif
