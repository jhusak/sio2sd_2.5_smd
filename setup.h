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

#ifndef _SETUP_H_
#define _SETUP_H_

void setup_init(void);
void setup_next_option(void);
void setup_prev_option(void);
void setup_next_value(void);
void setup_prev_value(void);
uint8_t setup_get_option(uint8_t opt);
#define setup_get_sio_speed() setup_get_option(0)
#define setup_get_cfg_tool_mode() setup_get_option(1)
#define setup_get_led_mode() setup_get_option(2)
#define setup_get_device_id() setup_get_option(3)
#define setup_get_wprot_mode() setup_get_option(4)

#endif
