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
#include <inttypes.h>
#include "mmc.h"
#include "delay.h"
#include "cbisbi.h"

#define nop()  __asm__ __volatile__ ("nop" ::)

#define MMC_PORT PORTB
#define MMC_DDR DDRB
#define MMC_PIN PINB

#define MMC_CS 4
#define MMC_MOSI 5
#define MMC_SCK 7
#define MMC_MISO 6
#define MMC_MASK 0xF0

#define MMCDETECT_PORT PORTC
#define MMCDETECT_DDR DDRC
#define MMCDETECT_PIN PINC
#define MMC_DETECT 0
#define MMCDETECT_MASK (1<<MMC_DETECT)
// #define LED_1 5
// #define LED_2 6
// #define LED_3 7

#ifndef FIRMWARE
static uint8_t cardinslot;
#endif

typedef union tc {
	uint32_t l;
	uint16_t s[2];
	uint8_t c[4];
} tc;

/* physical layer */

static uint8_t mmc_spi_receive(void);
static void mmc_spi_send(uint8_t b);
static void mmc_spi_flush(void);

#ifndef FIRMWARE

void mmc_init(void) {
	MMC_PORT = (MMC_PORT & (~MMC_MASK)) | (1<<MMC_CS)|(1<<MMC_MOSI)|(1<<MMC_MISO);
	MMCDETECT_PORT = (MMCDETECT_PORT & (~MMCDETECT_MASK)) | (1<<MMC_DETECT);
//	MMC_PORT = (1<<MMC_CS)|(1<<MMC_MOSI)|(1<<MMC_MISO)|(1<<MMC_DETECT)|(1<<LED_1)|(1<<LED_2)|(1<<LED_3);

//	sbi(MMC_PORT,MMC_CS);
//	sbi(MMC_PORT,MMC_MOSI);
//	sbi(MMC_PORT,MMC_MISO);
//	sbi(MMC_PORT,MMC_DETECT);
//	cbi(MMC_PORT,MMC_SCK);

	MMC_DDR = (MMC_DDR & (~MMC_MASK)) | (1<<MMC_MOSI)|(1<<MMC_SCK)|(1<<MMC_CS);
	MMCDETECT_DDR = (MMCDETECT_DDR & (~MMCDETECT_MASK));
//	MMC_DDR = (1<<MMC_MOSI)|(1<<MMC_SCK)|(1<<MMC_CS)|(1<<LED_1)|(1<<LED_2)|(1<<LED_3);
//	cbi(MMC_DDR,MMC_MISO);
//	cbi(MMC_DDR,MMC_DETECT);
//	sbi(MMC_DDR,MMC_MOSI);
//	sbi(MMC_DDR,MMC_SCK);
//	sbi(MMC_DDR,MMC_CS);

	cardinslot = 2;
}

/*
void led_set(uint8_t no,uint8_t on) {
	if (no<1 || no>3) {
		return;
	}
	no = 1<<(3-no+LED_1);
	if (on) {
		MMC_PORT &= ~no;
	} else {
		MMC_PORT |= no;
	}
}
*/

uint8_t mmc_card_removed(void) {
	if (cardinslot!=0 && bit_is_set(MMCDETECT_PIN,MMC_DETECT)) {
		cardinslot=0;
		return 1;
	} else {
		return 0;
	}
}

uint8_t mmc_card_inserted(void) {
	if (cardinslot!=1 && bit_is_clear(MMCDETECT_PIN,MMC_DETECT)) {
		cardinslot=1;
		return 1;
	} else {
		return 0;
	}
}

#endif

#define RCV_ONE_BIT(v,mask) \
	if (bit_is_set(MMC_PIN,MMC_MISO)) { \
		v|=mask; \
	} \
	sbi(MMC_PORT,MMC_SCK); \
	nop(); \
	nop(); \
	nop(); \
	nop(); \
	cbi(MMC_PORT,MMC_SCK); \
	nop(); \
	nop(); \
	nop(); \
	nop();

uint8_t mmc_spi_receive(void) {
	uint8_t ret=0;
	sbi(MMC_PORT,MMC_MOSI);
	nop();
	nop();
	nop();
	nop();
	RCV_ONE_BIT(ret,0x80)
	RCV_ONE_BIT(ret,0x40)
	RCV_ONE_BIT(ret,0x20)
	RCV_ONE_BIT(ret,0x10)
	RCV_ONE_BIT(ret,0x08)
	RCV_ONE_BIT(ret,0x04)
	RCV_ONE_BIT(ret,0x02)
	RCV_ONE_BIT(ret,0x01)
	return ret;
}

/*
uint8_t mmc_spi_receive(void) {
	uint8_t ret=0;
	uint8_t i=8;
	sbi(MMC_PORT,MMC_MOSI);
	do {
		ret<<=1;
		if (bit_is_set(MMC_PIN,MMC_MISO)) {
			ret++;
		}
		sbi(MMC_PORT,MMC_SCK);
//		nop();
//		nop();
//		nop();
		cbi(MMC_PORT,MMC_SCK);
		i--;
	} while (i!=0);
	return ret;
}
*/

#define SEND_ONE_BIT(v,mask) \
	if (v&mask) { \
		sbi(MMC_PORT,MMC_MOSI); \
	} else { \
		cbi(MMC_PORT,MMC_MOSI); \
	} \
	nop(); \
	nop(); \
	nop(); \
	nop(); \
	sbi(MMC_PORT,MMC_SCK); \
	nop(); \
	nop(); \
	nop(); \
	nop(); \
	cbi(MMC_PORT,MMC_SCK); \

void mmc_spi_send(uint8_t b) {
	SEND_ONE_BIT(b,0x80)
	SEND_ONE_BIT(b,0x40)
	SEND_ONE_BIT(b,0x20)
	SEND_ONE_BIT(b,0x10)
	SEND_ONE_BIT(b,0x08)
	SEND_ONE_BIT(b,0x04)
	SEND_ONE_BIT(b,0x02)
	SEND_ONE_BIT(b,0x01)
}

/*
void mmc_spi_send(uint8_t b) {
	uint8_t i=8;
	do {
		if (b&0x80) {
			sbi(MMC_PORT,MMC_MOSI);
		} else {
			cbi(MMC_PORT,MMC_MOSI);
		}
		b<<=1;
		sbi(MMC_PORT,MMC_SCK);
//		nop();
//		nop();
//		nop();
		cbi(MMC_PORT,MMC_SCK);
		i--;
	} while (i!=0);
}
*/

void mmc_spi_flush(void) {
	uint8_t i=8;
	sbi(MMC_PORT,MMC_MOSI);
	nop();
	nop();
	nop();
	nop();
	do {
		sbi(MMC_PORT,MMC_SCK);
		nop();
		nop();
		nop();
		nop();
		cbi(MMC_PORT,MMC_SCK);
		nop();
		nop();
		i--;
	} while (i!=0);
}

/* logical layer */

static void mmc_send_command_params (uint8_t cmd,uint8_t p0,uint8_t p1,uint8_t p2,uint8_t p3);
#ifndef FIRMWARE
static void mmc_send_command (uint8_t cmd,uint8_t crc);
#endif
static uint8_t mmc_read_response(void);

void mmc_send_command_params (uint8_t cmd,uint8_t p0,uint8_t p1,uint8_t p2,uint8_t p3) {
	sbi(MMC_PORT,MMC_CS);
	mmc_spi_flush();
	cbi(MMC_PORT,MMC_CS);
	mmc_spi_send(cmd|0x40);
	mmc_spi_send(p0);
	mmc_spi_send(p1);
	mmc_spi_send(p2);
	mmc_spi_send(p3);
	mmc_spi_send(0xFF);
}
#ifndef FIRMWARE
void mmc_send_command (uint8_t cmd,uint8_t crc) {
	sbi(MMC_PORT,MMC_CS);
	mmc_spi_flush();
	cbi(MMC_PORT,MMC_CS);
	mmc_spi_send(cmd|0x40);
	mmc_spi_send(0);
	mmc_spi_send(0);
	mmc_spi_send(0);
	mmc_spi_send(0);
	mmc_spi_send(crc);
}
#endif
uint8_t mmc_read_response(void) {
	uint8_t ret;
	uint16_t trycnt=0;
	do {
		ret = mmc_spi_receive();
		if (ret!=0xff) return ret;
		trycnt--;
	} while (trycnt!=0);
	return ret;
}

#ifndef FIRMWARE
uint8_t mmc_card_init(void) {
	uint8_t r; //,b;
	uint16_t i;
	sbi(MMC_PORT,MMC_CS);
	DELAY(US_TO_TICKS(20000))
//	delay_ms(10);
	i = 1000;
	do {
		mmc_spi_flush();
		i--;
	} while (i!=0);
	mmc_send_command(0,0x95);	// send CMD0
	r = mmc_read_response();
	if (r!=1) {
		return 0;
	}
	i=1000;
	do {
		mmc_send_command(1,0xff);
		r = mmc_read_response();
		if (r==0) {
			return 1;	// ok
		}
		if (r==1) {
			DELAY(US_TO_TICKS(500))
//			delay_ms(1);
		}
		i--;
	} while (i!=0);
	return 0;
}

uint16_t mmc_get_status(void) {
	uint8_t i;
	uint16_t resp;
	mmc_send_command(13,0xff);
	i=mmc_read_response();
	if (i==0xFF) {
		return 0xFFFF;
	}
	resp = i;
	resp<<=8;
	resp |= mmc_spi_receive();
	return resp;
}

/*
uint8_t mmc_get_cid(uint8_t *buff) {
	uint8_t i;
	mmc_send_command(10,0xff);
	if (mmc_read_response()!=0) {
		return 0;
	}
	if (mmc_read_response()!=0xFE) {
		return 0;
	}
	i=16;
	do {
		*buff=mmc_spi_receive();
		buff++;
		i--;
	} while (i!=0);
	return 1;
}

uint8_t mmc_get_csd(uint8_t *buff) {
	uint8_t i;
	mmc_send_command(9,0xff);
	if (mmc_read_response()!=0) {
		return 0;
	}
	if (mmc_read_response()!=0xFE) {
		return 0;
	}
	i=16;
	do {
		*buff=mmc_spi_receive();
		buff++;
		i--;
	} while (i!=0);
	return 1;
}
*/

#endif
uint8_t mmc_read_sector(uint32_t snum,uint8_t *buff) {
	uint16_t i;
	tc sec;
	sec.l = snum<<1;
	mmc_send_command_params(17,sec.c[2],sec.c[1],sec.c[0],0);
	if (mmc_read_response()!=0) {
		return 0;
	}
	if (mmc_read_response()!=0xFE) {
		return 0;
	}
	i=512;
	do {
		*buff=mmc_spi_receive();
		buff++;
		i--;
	} while (i!=0);
	mmc_spi_flush();	// ignore CRC
	mmc_spi_flush();	// ignore CRC
	return 1;
}

#ifndef FIRMWARE
uint8_t mmc_write_sector(uint32_t snum,uint8_t *buff) {
	uint16_t i;
	tc sec;
	sec.l = snum<<1;
	mmc_send_command_params(24,sec.c[2],sec.c[1],sec.c[0],0);
//	snum<<=1;
//	mmc_send_command_params(24,(uint8_t)(snum>>16),(uint8_t)(snum>>8),(uint8_t)snum,0);
	if (mmc_read_response()!=0) {
		return 0;
	}
	mmc_spi_send(0xFE);
	i=512;
	do {
		mmc_spi_send(*buff);
		buff++;
		i--;
	} while (i!=0);
	mmc_spi_flush();	// send 0xFF as CRC
	mmc_spi_flush();	// send 0xFF as CRC
	if ((mmc_read_response()&0x1f)!=0x05) {
		return 0;
	}
	while (mmc_spi_receive()==0) ;	//wait for busy
	return 1;
}

uint8_t mmc_erase_sector(uint32_t snum) {
	uint16_t i;
	tc sec;
	sec.l = snum<<1;
	mmc_send_command_params(24,sec.c[2],sec.c[1],sec.c[0],0);
//	snum<<=1;
//	mmc_send_command_params(24,(uint8_t)(snum>>16),(uint8_t)(snum>>8),(uint8_t)snum,0);
	if (mmc_read_response()!=0) {
		return 0;
	}
	mmc_spi_send(0xFE);
	i=512;
	do {
		mmc_spi_send(0);
		i--;
	} while (i!=0);
	mmc_spi_flush();	// send 0xFF as CRC
	mmc_spi_flush();	// send 0xFF as CRC
	if ((mmc_read_response()&0x1f)!=0x05) {
		return 0;
	}
	while (mmc_spi_receive()==0) ;	//wait for busy
	return 1;
}

/*
uint8_t mmc_erase_sectors(uint32_t addr_start,uint32_t addr_end) {
	tc sec;
	sec.l = addr_start;
	mmc_send_command_params(32,sec.c[3],sec.c[2],sec.c[1],sec.c[0]);
	if (mmc_read_response()!=0) {
		return 0;
	}
	sec.l = addr_end;
	mmc_send_command_params(33,sec.c[3],sec.c[2],sec.c[1],sec.c[0]);
	if (mmc_read_response()!=0) {
		return 0;
	}
	mmc_send_command(38,0xff);
	if (mmc_read_response()!=0) {
		return 0;
	}
	return 1;
}

uint8_t mmc_erase_groups(uint32_t addr_start,uint32_t addr_end) {
	tc sec;
	sec.l = addr_start;
	mmc_send_command_params(35,sec.c[3],sec.c[2],sec.c[1],sec.c[0]);
	if (mmc_read_response()!=0) {
		return 0;
	}
	sec.l = addr_end;
	mmc_send_command_params(36,sec.c[3],sec.c[2],sec.c[1],sec.c[0]);
	if (mmc_read_response()!=0) {
		return 0;
	}
	mmc_send_command(38,0xff);
	if (mmc_read_response()!=0) {
		return 0;
	}
	return 1;
}

uint8_t mmc_erase(uint32_t snum_start,uint32_t snum_end) {
	uint32_t addr_start,addr_end;
	if (snum_start>=snum_end) {
		return 0;
	}
	snum_start = snum_start << 9;
	snum_end = (snum_end << 9) -1;
	addr_start = snum_start & 0xFFFFC000;
	addr_end = snum_end & 0xFFFFC000;
	if (addr_start==addr_end) {	// in same erase group (32 sectors - 16384 bytes)
		mmc_erase_sectors(snum_start,snum_end);
	} else {
		if (addr_end > (addr_start+0x4000)) {	// must erase groups between start group and end group
			mmc_erase_groups(addr_start+0x4000,addr_end-1);
		}
		mmc_erase_sectors(snum_start,addr_start+0x3FFF);
		mmc_erase_sectors(addr_end,snum_end);
	}
	return 1;
}
*/
#endif
