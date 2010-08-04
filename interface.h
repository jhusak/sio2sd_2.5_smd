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

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <inttypes.h>
#include <avr/pgmspace.h>

void interface_init(uint8_t smode);

void interface_setup_start(void);
void interface_card_init_error(void);
void interface_card_unknown_format(void);
void interface_too_many_clusters(void);
void interface_card_no_partition(void);
void interface_card_atari_not_found(void);
void interface_no_card(void);
void interface_eio(void);

void interface_card_fat_type(uint8_t ft);

void interface_cfg_option_value(const prog_char *option,const prog_char *value);
void interface_cfg_option_speed(const prog_char *option,uint32_t speed,uint8_t speedindex);
void interface_cfg_option_number(const prog_char *option,uint8_t number);

void interface_firmware_update(void);

void interface_show_fs_info(uint8_t cfno,uint8_t fname[39],uint8_t dir,uint8_t pname[39]);

//void interface_debug(uint32_t f,uint16_t c);
//void interface_debug(uint8_t fno[3],uint8_t prio[3]);
void interface_sio_command(uint8_t cmddno,uint8_t command,uint16_t blockno);
void interface_task(void);

#endif
