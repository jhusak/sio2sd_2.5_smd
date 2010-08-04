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

#ifndef _MMC_H_
#define _MMC_H_

#include <inttypes.h>

#ifndef FIRMWARE
void mmc_init(void);
void led_set(uint8_t no,uint8_t on);
uint8_t mmc_card_removed(void);
uint8_t mmc_card_inserted(void);
uint8_t mmc_card_init(void);
//uint8_t mmc_get_cid(uint8_t *buff);
//uint8_t mmc_get_csd(uint8_t *buff);
#endif
uint8_t mmc_read_sector(uint32_t snum,uint8_t *buff);
#ifndef FIRMWARE
uint8_t mmc_write_sector(uint32_t snum,uint8_t *buff);
uint8_t mmc_erase_sector(uint32_t snum);
//uint8_t mmc_erase(uint32_t snum_start,uint32_t snum_end);
#endif

#endif
