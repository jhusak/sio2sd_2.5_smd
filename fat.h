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

#ifndef _FAT_H_
#define _FAT_H_

#include <inttypes.h>

#ifdef DYNAMIC_FATCACHE
#define EIGHTDISKS 1
#endif

enum {FTYPE_NONE=0,FTYPE_DIR,FTYPE_ATR,FTYPE_XFD,FTYPE_EXE};

int8_t fat_init(uint8_t setupmode);
void fat_removed(uint8_t setupmode);

void fat_key_dirup(void);
void fat_key_clear(void);
void fat_key_enter(void);
void fat_key_nextdisk(void);
void fat_key_prevdisk(void);
void fat_key_nextfile(void);
void fat_key_prevfile(void);
void fat_key_swapdisks(void);

//int fat_valid(void);
int8_t fat_sio_getdiskinfo(uint8_t *fbuff,uint8_t dno);
int8_t fat_sio_setdiskinfo(uint8_t *fbuff,uint8_t dno);
int8_t fat_sio_diskoff(uint8_t dno);
int8_t fat_sio_getnextentry(uint8_t *fbuff,uint8_t firstflag);
int8_t fat_sio_enterdir(uint8_t *fbuff);
int8_t fat_sio_dirup(void);

void fat_refresh(void);

void fat_firmware_update(void);

uint8_t* fat_get_file_name(uint8_t fno);
uint32_t fat_get_file_size(uint8_t fno);
uint8_t fat_set_file_size(uint8_t fno,uint32_t size);
//uint8_t fat_new_file(uint8_t fno);
uint16_t fat_pread(uint8_t fno,uint8_t *mbuff,uint32_t offset,uint16_t size);
uint16_t fat_pwrite(uint8_t fno,uint8_t *mbuff,uint32_t offset,uint16_t size);
uint8_t fat_get_file_type(uint8_t fno);
uint8_t fat_was_changed(uint8_t fno);

#endif
