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
#include <avr/pgmspace.h>
#include <string.h>
#include "delay.h"
#include "keys.h"
#include "lcd.h"
#include "interface.h"
#include "fat.h"
#include "setup.h"
#include "led.h"
#include "cbisbi.h"

#define NORMAL_SPEED_DIVISOR 46
#define HIGH_SPEED_DIVISOR (hsindex+6)

#define SIO_DDR DDRD
#define SIO_PORT PORTD
#define SIO_PIN PIND

#define SIO_CMD 2

#define SIO_ACK 'A'
#define SIO_NAK 'N'
#define SIO_COMPLETE 'C'
#define SIO_ERROR 'E'

#define SIO_ERROR_TO -1
#define SIO_ERROR_FRAMEERROR -2
#define SIO_ERROR_OVERRUN -3
#define SIO_ERROR_CHECKSUM -4

/* command timeouts/delays */
// command line active to command data
#define SIO_COMMAND_TIMEOUT 17000
// command data to command line inactive
#define SIO_COMMAND_LINE_TIMEOUT 1000
// command line inactive to ack/nak
#define SIO_COMMAND_TO_ACK_DELAY 50

/* to atari timeouts/delays */
// ack to complete
#define SIO_IN_ACK_TO_COMPLETE_DELAY 850
// complete to data
#define SIO_IN_COMPLETE_TO_DATA_DELAY 350

/* from atari timeouts/delays */
// ack to data
#define SIO_OUT_DATA_TIMEOUT 17000
// data to ack
#define SIO_OUT_DATA_TO_ACK_DELAY 900
// ack to complete
#define SIO_OUT_ACK_TO_COMPLETE_DELAY 800


static uint8_t speed;
static uint8_t cardstatus;
static uint8_t cfgtoolover;
static uint8_t hsindex;
static uint8_t deviceid;
static uint8_t wprotmode;

typedef struct ataridisk {
	uint8_t lastfdcstatus;
	uint8_t ssize:1; // 0 - 128B , 1 - 256B
	uint8_t writeprotect:1;
	uint8_t atrddpatch:1;	// 0 - sectors 1-3 in dd 128B / 1 - all sectors in dd 256B
	uint8_t fssize:1;
	uint8_t tracks;
	uint8_t ftracks;
	uint8_t sides;
	uint8_t fsides;
	uint16_t sectors;
	uint16_t fsectors;
	uint16_t totalsectors;
	uint16_t ftotalsectors;
} ataridisk;

#ifdef EIGHTDISKS
static ataridisk disks[8];	// D1,D2,D3,D4
#else
static ataridisk disks[4];	// D1,D2,D3,D4
#endif

static uint8_t data_buff[256];

// #include "exeloader.h"

static const uint8_t cfg_tool_loading[] __attribute__ ((progmem)) = "Loading SIO2SD cfg tool";

static const uint8_t xex_loader_info[] __attribute__ ((progmem)) = "SIO2SD xex loader";

static const uint8_t xex_loader[] __attribute__ ((progmem)) = {
#include "xex_loader/xex_loader.bin.h"
};

static const uint8_t sio2sd_xex[] __attribute__ ((progmem)) = {
#ifdef USE_PAJERO_CFG_TOOL
#include "atari_conf_tool/pajero_sio2sd.xex.h"
#else
#include "atari_conf_tool/sio2sd.xex.h"
#endif
};

void sio_fat_status(uint8_t f) {
	if (f) {
		cardstatus=2;
	} else {
		cardstatus=0;
	}
}

void sio_check_disk(uint8_t dno) {
	uint32_t fsize;
	if (fat_was_changed(dno)) {
		cardstatus=3;
		memset(disks+dno,0,sizeof(ataridisk));
		if (fat_get_file_type(dno)==FTYPE_NONE || fat_get_file_type(dno)==FTYPE_DIR) {
			return;
		}
		fsize = fat_get_file_size(dno);
		if (fat_get_file_type(dno)==FTYPE_XFD) {
			if ((fsize&0x7F)==0) {
				disks[dno].ssize=0;
				fsize>>=7;
				disks[dno].writeprotect=1;
			} else {
				// this is not a valid XFD image
				return;
			}
		} else if (fat_get_file_type(dno)==FTYPE_EXE) {
			fat_pread(dno,data_buff,0,2);
			if (data_buff[0]!=0xFF || data_buff[1]!=0xFF) {
				// this is not a valid com/exe format
				return;
			}
			disks[dno].ssize=0;
			disks[dno].writeprotect=1;
			fsize+=(127+384);	// round up and add 384 for exe loader
			fsize>>=7;
		} else if (fat_get_file_type(dno)==FTYPE_ATR) {
			if (fsize==0) {	// for empty ATR - try to create simple 1-sector atr file
				memset(data_buff,0,16+128);
				data_buff[0]=0x96;
				data_buff[1]=0x02;
				data_buff[4]=128;
				data_buff[2]=8;
				if (fat_set_file_size(dno,16+128)) {
					fsize = 16+128;
					fat_pwrite(dno,data_buff,0,16+128);
				}
			}
			if ((fsize&0x7F)==0x10) {
				fsize-=0x10;	// fsize-=hdrsize
				fat_pread(dno,data_buff,0,16);	// read ATR header
				if (data_buff[0]!=0x96 || data_buff[1]!=0x02) {
					// wrong ATR signature
					return;
				}
				// disks[dno].atrhdr=1;
				//		if (fsize!=(((data_buff[6]*256+data_buff[3])*256+data_buff[2])<<4)) {
				// wrong file size
				//			return;
				//		}
				if (data_buff[4]==128 && data_buff[5]==0) { // sectorsize==128
					disks[dno].ssize=0;
					fsize>>=7;	// sectors = fsize/128
				} else if (data_buff[4]==0 && data_buff[5]==1) { // sectorsize==256
					if (fsize<384) {
						return;
					}
					if ((uint8_t)(fsize)==0x80) {
						fsize+=384;
						disks[dno].atrddpatch=0;
					} else {
						disks[dno].atrddpatch=1;
					}
					fsize>>=8;	// sectors = fsize/256
					disks[dno].ssize=1;
				} else {
					// wrong sector size
					return;
				}
				if (wprotmode==1) {
					if (data_buff[15]&0x01) {
						disks[dno].writeprotect=1;
					}
				} else if (wprotmode==2) {
					disks[dno].writeprotect=1;
				}
			} else {
				return;
			}
		} else {
			return;
		}
		if (fsize==0 || fsize>=65536) {
			// wrong number of sectors
			return;
		}
		disks[dno].totalsectors = fsize;
		switch (fsize) {
		case 720:	// SS/SD (128B) lub SS/DD (256B)
			disks[dno].sides = 1;
			disks[dno].tracks = 40;
			disks[dno].sectors = 18;
			break;
		case 1040:	// SS/ED (128B)
			disks[dno].sides = 1;
			disks[dno].tracks = 40;
			disks[dno].sectors = 26;
			break;
		case 1440:	// DS/DD (256B)
			disks[dno].sides = 2;
			disks[dno].tracks = 40;
			disks[dno].sectors = 18;
			break;
		case 2880:  // DS/QD (256B)
			disks[dno].sides = 2;
			disks[dno].tracks = 80;
			disks[dno].sectors = 18;
			break;
		case 5760:  // DS/HD (256B)
			disks[dno].sides = 2;
			disks[dno].tracks = 80;
			disks[dno].sectors = 36;
			break;
		default:	// Hard Drive
			disks[dno].sides = 1;
			disks[dno].tracks = 1;
			disks[dno].sectors = fsize;
		}
		disks[dno].lastfdcstatus=0xFF;
		disks[dno].fssize = disks[dno].ssize;
		disks[dno].fsides = disks[dno].sides;
		disks[dno].ftracks = disks[dno].tracks;
		disks[dno].fsectors = disks[dno].sectors;
		disks[dno].ftotalsectors = disks[dno].totalsectors;
	}
}

uint8_t sio_create_new_image(uint8_t dno) {
	uint32_t fsize;
	if (disks[dno].fssize && disks[dno].ftotalsectors<=3) {
		disks[dno].fssize=0;
	}
	if (disks[dno].fssize) {
		fsize = 16+256*(uint32_t)disks[dno].ftotalsectors-384;
	} else {
		fsize = 16+128*(uint32_t)disks[dno].ftotalsectors;
	}
	if (fat_set_file_size(dno,fsize)==0) {
		sio_check_disk(dno);
		return 0;	// create image failed
	}
	memset(data_buff,0,16);
	data_buff[0]=0x96;
	data_buff[1]=0x02;
	if (disks[dno].fssize) {
		data_buff[4]=0;
		data_buff[5]=1;
	} else {
		data_buff[4]=128;
		data_buff[5]=0;
	}
	fsize>>=4;
	fsize--;
	*(uint16_t*)(data_buff+2) = fsize;
	data_buff[6]=fsize>>16;
	fat_pwrite(dno,data_buff,0,16);
	sio_check_disk(dno);
	return 1;
}

/*
uint16_t sio_get_file_entries(void) {
	return entries;
}

uint16_t sio_get_file_number(uint8_t dno) {
	if (dno>'9' || dno<'1') {
		return 0;
	} else {
		return dnumbers[dno-'1'];
	}
}

void sio_set_file_number(uint8_t dno,uint16_t fno) {
	if (dno>='1' && dno<='9') {
		dnumbers[dno-'1'] = fno;
		sio_store_drive_numbers();
		actualdno=0;	//next sio command reloads file paramiters
	}
}

void sio_get_file_name(uint8_t *fname,uint8_t mleng,uint16_t fno) {
	uint8_t *ptr,c;
	if (fno>=entries) return;
	ptr = data_buff+(((fno+1)&0xF)<<5);
	mmc_read_sector(1+(fno>>4),data_buff);
	ptr+=11;
	if (mleng>21) {
		mleng=21;
	}
	do {
		c = *ptr++;
		if (c) {
			*fname++ = c;
		} else {
			return;
		}
		mleng--;
	} while (mleng);
}
*/


void sio_putc(uint8_t c) {
	while (bit_is_clear(UCSRA,UDRE)) {}
	UDR = c;
}

/* simple write with checksum - up to 256 bytes when s is 0 */
void sio_write(uint8_t *buff,uint8_t s) {
	uint16_t chk;
	uint8_t c;
	chk=0;
	do {
		c = *buff++;
		chk+=c;
		if (chk>=0x100) {
			chk = (chk&0xFF)+1;
		}
		sio_putc(c);
		s--;
	} while (s);
	sio_putc(chk);
}

int16_t sio_getc(void) {
	uint8_t resp;
	while (bit_is_clear(UCSRA,RXC)) {
	   	if (!DELAY_IN_PROGRESS) {
			return SIO_ERROR_TO;
		}
	}
	resp = UDR;
	if (bit_is_set(UCSRA,FE)) {
		return SIO_ERROR_FRAMEERROR;
	}
	if (bit_is_set(UCSRA,DOR)) {
		return SIO_ERROR_OVERRUN;
	}
	return resp;
}

int16_t sio_read(uint8_t *buff,uint8_t s,uint16_t to) {
	uint16_t chk;
	int16_t c;
	DELAY_START(to)
	chk=0;
	do {
		c = sio_getc();
		if (c<0) return c;
		*buff++ = (uint8_t)c;
		chk+=(uint8_t)c;
		if (chk>=0x100) {
			chk = (chk&0xFF)+1;
		}
		s--;
		DELAY_REFRESH
	} while (s);
	c = sio_getc();
	if (c<0) return c;
	if ((uint8_t)(c) != (uint8_t)(chk)) {
		return SIO_ERROR_CHECKSUM;
	}
	return 0;
}

void sio_cmd_cfg_read(uint16_t block) {
	if (block==0 || block > 3+(sizeof(sio2sd_xex)+127)/128) {
		sio_putc(SIO_NAK);
		return;
	}
	led_sio_read(1);
	sio_putc(SIO_ACK);
	DELAY_START(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY));
	block--;
	if (block<3) {
		if (block==0) {
			memcpy_P(data_buff,xex_loader,128);
		} else if (block==1) {
			memcpy_P(data_buff,xex_loader+128,128);
		} else {
			memset(data_buff,0,128);
			*((uint32_t*)(data_buff+80)) = sizeof(sio2sd_xex);
			memcpy_P(data_buff+8,cfg_tool_loading,sizeof(cfg_tool_loading));
		}
	} else {
		block-=3;
		memcpy_P(data_buff,sio2sd_xex+128*block,128);
		if (block==0 && data_buff[6]==0) {
			data_buff[6]=deviceid;
		}
	}
	DELAY_END
	sio_putc(SIO_COMPLETE);
	DELAY(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	sio_write(data_buff,128);
	led_sio_read(0);
}

void sio_cmd_cfg_status(void) {
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	sio_putc(SIO_COMPLETE);
	DELAY(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	data_buff[0]=0x18;
	data_buff[1]=0xFF;
	data_buff[2]=0xE0;
	data_buff[3]=0x00;
	sio_write(data_buff,4);
	led_sio_other(0);
}

uint8_t sio_parse_percom(uint8_t dno,uint8_t *percom) {
	uint8_t ssize,tracks,sides;
	uint16_t sectors;
	uint32_t totalsectors;
	tracks = percom[0];
	percom[1]=percom[3];	// ugly hack
	sectors = *((uint16_t*)(percom+1));
	sides = percom[4]+1;
	if (*((uint16_t*)(percom+6))==0x8000) {
		ssize=0;
	} else if (*((uint16_t*)(percom+6))==0x0001) { 
		ssize=1;
	} else {
		return 0;
	}
	totalsectors = tracks;
	totalsectors *= sides;
	totalsectors *= sectors;
	if (totalsectors==0 || totalsectors>65535) {
		return 0;
	}
	disks[dno].fssize = ssize;
	disks[dno].ftracks = tracks;
	disks[dno].fsides = sides;
	disks[dno].fsectors = sectors;
	disks[dno].ftotalsectors = totalsectors;
	return 1;
}

void sio_make_percom(uint8_t dno,uint8_t *percom) {
	memset(percom,0,12);
	percom[0]=disks[dno].ftracks;
	percom[2]=disks[dno].fsectors>>8;
	percom[3]=disks[dno].fsectors;
	percom[4]=disks[dno].fsides-1;
	if (disks[dno].ftracks!=40 || disks[dno].fsides!=1 || disks[dno].fsectors!=18 || disks[dno].fssize!=0) {
		percom[5]=4;		//density: 0 - FM (SD) ; 4 - MFM (all others) ; 6 - 1.2M (5.25 HD)
	}
	if (disks[dno].fssize) {
		percom[6]=1;
		percom[7]=0;
	} else {
		percom[6]=0;		//16 bit: sector length
		percom[7]=128;
	}
	percom[8]=0xff;	// XF551 - 0x40
}

void sio_cmd_status(uint8_t dno) {
	uint8_t c;
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	sio_putc(SIO_COMPLETE);
	DELAY(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	c = 0x10;	// motor on
	if (disks[dno].fssize) {
		c|=0x20;	// DD flag
	} else {
		if (disks[dno].ftracks==40 && disks[dno].fsides==1 && disks[dno].fsectors==26) {
			c|=0x80;	// ED flag
		}
	}
	if (disks[dno].writeprotect) {
		c|=0x08;
	}
	data_buff[0]=c;	//enchanced = 0x90, single = 0x10, double = 0x30
	data_buff[1]=disks[dno].lastfdcstatus;
	data_buff[2]=0xE0;	//e0 (for XF551 set fe)
	data_buff[3]=0x00;
	sio_write(data_buff,4);
	led_sio_other(0);
}


void sio_cmd_get_percom(uint8_t dno) {
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	sio_putc(SIO_COMPLETE);
	DELAY(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	sio_make_percom(dno,data_buff);
	sio_write(data_buff,12);
	led_sio_other(0);
}

void sio_cmd_put_percom(uint8_t dno) {
	int16_t resp;
	led_sio_other(1);
	sio_putc(SIO_ACK);
	resp = sio_read(data_buff,12,US_TO_TICKS(SIO_OUT_DATA_TIMEOUT));
	if (resp<0) {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_NAK);
	} else {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_ACK);
		DELAY(US_TO_TICKS(SIO_OUT_ACK_TO_COMPLETE_DELAY))
		if (sio_parse_percom(dno,data_buff)) {
			sio_putc(SIO_COMPLETE);
		} else {
			sio_putc(SIO_ERROR);
		}
	}
	led_sio_other(0);
}

#if 0
void sio_cmd_format_auto(uint8_t dno) {
	sio_putc(SIO_ACK);
	DELAY(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	if (disks[dno].writeprotect) {
		disks[dno].lastfdcstatus = 0xbf;
		sio_putc(SIO_ERROR);
	} else {
		disks[dno].lastfdcstatus = 0xff;
		if (sio_create_new_image(dno)) {
			sio_putc(SIO_COMPLETE);
		} else {
			sio_putc(SIO_ERROR);
		}
	}
}
#endif

void sio_cmd_format(uint8_t dno) {
	uint8_t size;
	uint8_t code;
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY_START(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	code = 0;
	if (disks[dno].writeprotect) {
		disks[dno].lastfdcstatus = 0xbf;
		DELAY_END
		sio_putc(SIO_ERROR);
		led_sio_other(0);
		return;
	} else {
		disks[dno].lastfdcstatus = 0xff;
		if (sio_create_new_image(dno)) {
			DELAY_END
			sio_putc(SIO_COMPLETE);
			code = 0xFF;
		} else {
			DELAY_END
			sio_putc(SIO_ERROR);
			led_sio_other(0);
			return;
		}
	}
	DELAY_START(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	if (disks[dno].ssize) {
		memset(data_buff,code,256);
		size=0;
	} else {
		memset(data_buff,code,128);
		size=128;
	}
	DELAY_END
	sio_write(data_buff,size);
	led_sio_other(0);
}

void sio_cmd_format_medium(uint8_t dno) {
	disks[dno].fssize = 0;
	disks[dno].ftracks = 40;
	disks[dno].fsides = 1;
	disks[dno].fsectors = 26;
	disks[dno].ftotalsectors = 1040;
	sio_cmd_format(dno);
}

void sio_cmd_format_dsdd(uint8_t dno) {
	disks[dno].fssize = 1;
	disks[dno].ftracks = 40;
	disks[dno].fsides = 2;
	disks[dno].fsectors = 18;
	disks[dno].ftotalsectors = 1440;
	sio_cmd_format(dno);
}

#if 0
void sio_cmd_format_skew(uint8_t dno) {
	int16_t resp;
	sio_putc(SIO_ACK);
	resp = sio_read(data_buff,128,US_TO_TICKS(SIO_OUT_DATA_TIMEOUT));
	if (resp<0) {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_NAK);
	} else {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_ACK);
		DELAY_START(US_TO_TICKS(SIO_OUT_ACK_TO_COMPLETE_DELAY))
		if (disks[dno].writeprotect) {
			disks[dno].lastfdcstatus = 0xbf;
			DELAY_END
			sio_putc(SIO_ERROR);
		} else if (sio_parse_percom(dno,data_buff)) {
			disks[dno].lastfdcstatus = 0xff;
			if (sio_create_new_image(dno)) {
				DELAY_END
				sio_putc(SIO_COMPLETE);
			} else {
				DELAY_END
				sio_putc(SIO_ERROR);
			}
		} else {
			DELAY_END
			sio_putc(SIO_ERROR);
		}
	}
}
#endif

void sio_cmd_read(uint8_t dno,uint16_t block) {
	uint8_t size;
	if (block==0 || block > disks[dno].totalsectors) {
		sio_putc(SIO_NAK);
		return;
	}
	led_sio_read(1);
	sio_putc(SIO_ACK);
	DELAY_START(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY));
	block--;
	if (fat_get_file_type(dno)==FTYPE_ATR) {	// ATR
		if (disks[dno].ssize==0) {
			fat_pread(dno,data_buff,16+((uint32_t)block<<7),128);
			size=128;
		} else {
			if (disks[dno].atrddpatch) {
				if (block<3) {
					fat_pread(dno,data_buff,16+((uint32_t)block<<8),128);
					size=128;
				} else {
					fat_pread(dno,data_buff,16+((uint32_t)block<<8),256);
					size=0;	// 256
				}
			} else {
				if (block<3) {
					fat_pread(dno,data_buff,16+((uint32_t)block<<7),128);
					size=128;
				} else {
					fat_pread(dno,data_buff,16+((uint32_t)block<<8)-384,256);
					size=0;	// 256
				}
			}
		}
	} else if (fat_get_file_type(dno)==FTYPE_XFD) {	// XFD
		fat_pread(dno,data_buff,((uint32_t)block<<7),128);
		size=128;
	} else if (fat_get_file_type(dno)==FTYPE_EXE) {
		if (block<3) {
			if (block==0) {
				memcpy_P(data_buff,xex_loader,128);
			} else if (block==1) {
				memcpy_P(data_buff,xex_loader+128,128);
			} else {
				memset(data_buff,0,128);
				memcpy_P(data_buff,xex_loader_info,sizeof(xex_loader_info));
				*((uint32_t*)(data_buff+80)) = fat_get_file_size(dno);
				memcpy(data_buff+40,fat_get_file_name(dno),39);
			}
		} else {
			block-=3;
			fat_pread(dno,data_buff,(uint32_t)block*128,128);
		}
		size=128;
/* eksperyment - eksportowanie pliku bezposrednio jako plik (jeden na dysku) 
	} else if (fat_get_file_type(dno)==FTYPE_OTHER) {
		if (block<3) {
			memset(data_buff,0,128);
			size=128;
		} else if (block==359) {
			memset(data_buff,0,128);
			size=128;
			data_buff[0]=0x02; // hi (total sectors - 1)
			data_buff[1]=0xC3; // lo (total sectors - 1)
//			data_buff[2]=0x02; // hi (used sectors)
//			data_buff[3]=0xC
		} else if (block==360) {
			memset(data_buff,0,128);
			size=128;
			data_buff[0]=0x46; // 0x42 / 0x62 ??
			data_buff[1]=255;	// lo size sectors - lo(ceil(size/125));
			data_buff[2]=0;     // hi size sectors - hi(ceil(size/125));
			data_buff[3]=4;
			data_buff[4]=0;
			data_buff[5]='F';
			data_buff[6]='I';
			data_buff[7]='L';
			data_buff[8]='E';
			data_buff[9]='N';
			data_buff[10]='A';
			data_buff[11]='M';
			data_buff[12]='E';
			data_buff[13]='E';
			data_buff[14]='X';
			data_buff[15]='T';
		} else {
			fat_pread(dno,data_buff,(uint32_t)block*125,125);
			data_buff[125]=block&0xFF;
			data_buff[126]=block>>8;
			data_buff[127]=125;
			size=128;
*/
	} else {
		memset(data_buff,0,128);
		size=128;
	}
	DELAY_END
	disks[dno].lastfdcstatus = 0xFF;
	sio_putc(SIO_COMPLETE);
	DELAY(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	sio_write(data_buff,size);
	led_sio_read(0);
}

void sio_cmd_write(uint8_t dno,uint16_t block) {
	uint8_t size;
	int16_t resp;
	if (block==0 || block > disks[dno].totalsectors) {
		sio_putc(SIO_NAK);
		return;
	}
	led_sio_write(1);
	sio_putc(SIO_ACK);
	block--;
	if (block<3 || disks[dno].ssize==0) {
		size=128;
	} else {
		size=0;	//256
	}
	resp = sio_read(data_buff,size,US_TO_TICKS(SIO_OUT_DATA_TIMEOUT));
	if (resp>=0) {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_ACK);
		if (disks[dno].writeprotect || fat_get_file_type(dno)!=FTYPE_ATR) {
			DELAY(US_TO_TICKS(SIO_OUT_ACK_TO_COMPLETE_DELAY))
			disks[dno].lastfdcstatus = 0xbf;
			sio_putc(SIO_ERROR);
		} else {
			DELAY_START(US_TO_TICKS(SIO_OUT_ACK_TO_COMPLETE_DELAY))
			if (disks[dno].ssize==0) {
				fat_pwrite(dno,data_buff,16+((uint32_t)block<<7),128);
			} else {
				if (disks[dno].atrddpatch) {
					if (block<3) {
						fat_pwrite(dno,data_buff,16+((uint32_t)block<<8),128);
					} else {
						fat_pwrite(dno,data_buff,16+((uint32_t)block<<8),256);
					}
				} else {
					if (block<3) {
						fat_pwrite(dno,data_buff,16+((uint32_t)block<<7),128);
					} else {
						fat_pwrite(dno,data_buff,16+((uint32_t)block<<8)-384,256);
					}
				}
			}
			DELAY_END
			disks[dno].lastfdcstatus = 0xff;
			sio_putc(SIO_COMPLETE);
		}
	} else {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_NAK);
	}
	led_sio_write(0);
}


void sio_cmd_hs_index(void) {
	if (hsindex==255) {
		sio_putc(SIO_NAK);
		return ;
	}
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	sio_putc(SIO_COMPLETE);
	DELAY(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	sio_putc(hsindex);
	sio_putc(hsindex);
	led_sio_other(0);
}

/*
void sio_cmd_just_ack(void) {
	sio_putc(SIO_ACK);
	DELAY(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	sio_putc(SIO_COMPLETE);
}
*/

/* config commands */

// returns one byte:
//  0 - no card
//  1 - card ok (not changed)
//  2 - card ok (changed)
void sio_conf_check(void) {
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	sio_putc(SIO_COMPLETE);
	DELAY(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	sio_putc(cardstatus);
	sio_putc(cardstatus);
	if (cardstatus>1) {
		cardstatus=1;
	}
	led_sio_other(0);
}

void sio_conf_get_disk(uint8_t dno,uint8_t cnt) {
	uint8_t *ptr;
	if (cnt==0) {
		cnt=1;
	}
#ifdef EIGHTDISKS
	if (dno<1 || dno>8 || cnt>4 || dno+cnt>9) {
#else
	if (dno<1 || dno>4 || cnt>4 || dno+cnt>5) {
#endif
		sio_putc(SIO_NAK);
		return;
	}
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY_START(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY));
	ptr = data_buff;
	dno--;
	while (cnt>0) {
		fat_sio_getdiskinfo(ptr,dno);
		ptr+=54;
		dno++;
		cnt--;
	}
	DELAY_END
	sio_putc(SIO_COMPLETE);
	DELAY(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	sio_write(data_buff,ptr-data_buff);
	led_sio_other(0);
}

void sio_conf_set_disk(uint8_t dno,uint8_t cnt) {
	int16_t resp;
	uint8_t *ptr;
	if (cnt==0) {
		cnt=1;
	}
#ifdef EIGHTDISKS
	if (dno<1 || dno>8 || cnt>4 || dno+cnt>9) {
#else
	if (dno<1 || dno>4 || cnt>4 || dno+cnt>5) {
#endif
		sio_putc(SIO_NAK);
		return;
	}
	led_sio_other(1);
	sio_putc(SIO_ACK);
	resp = sio_read(data_buff,54*cnt,US_TO_TICKS(SIO_OUT_DATA_TIMEOUT));
	if (resp>=0) {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_ACK);
		DELAY_START(US_TO_TICKS(SIO_OUT_ACK_TO_COMPLETE_DELAY))
		resp=0;
		ptr = data_buff;
		dno--;
		while (cnt>0) {
			if (fat_sio_setdiskinfo(ptr,dno)<0) {
				resp=-1;
			}
			ptr+=54;
			dno++;
			cnt--;
		}
		DELAY_END
		if (resp<0) {
			sio_putc(SIO_ERROR);
		} else {
			sio_putc(SIO_COMPLETE);
		}
	} else {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_NAK);
	}
	led_sio_other(0);
}

void sio_conf_off_disk(uint8_t dno,uint8_t cnt) {
	if (cnt==0) {
		cnt=1;
	}
#ifdef EIGHTDISKS
	if (dno<1 || dno>8 || dno+cnt>9) {
#else
	if (dno<1 || dno>4 || dno+cnt>5) {
#endif
		sio_putc(SIO_NAK);
		return;
	}
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	dno--;
	while (cnt>0) {
		fat_sio_diskoff(dno);
		dno++;
		cnt--;
	}
	DELAY_END
	sio_putc(SIO_COMPLETE);
	led_sio_other(0);
}

void sio_conf_get_next(uint8_t cnt,uint8_t firstflag) {
	uint8_t *ptr;
	uint8_t end;
	if (cnt==0) {
		cnt=1;
	}
	if (cnt>4) {
		sio_putc(SIO_NAK);
		return;
	}
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY_START(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	ptr = data_buff;
	end = 0;
	while (cnt>0) {
		if (end) {
			memset(ptr,0,54);
		} else {
			if (fat_sio_getnextentry(ptr,firstflag)<=0) {
				end=1;
			}
			firstflag=0;
		}
		ptr+=54;
		cnt--;
	}
	DELAY_END
	sio_putc(SIO_COMPLETE);
	DELAY(US_TO_TICKS(SIO_IN_COMPLETE_TO_DATA_DELAY))
	sio_write(data_buff,ptr-data_buff);
	led_sio_other(0);
}

void sio_conf_enter_dir(void) {
	int16_t resp;
	led_sio_other(1);
	sio_putc(SIO_ACK);
	resp = sio_read(data_buff,54,US_TO_TICKS(SIO_OUT_DATA_TIMEOUT));
	if (resp>=0) {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_ACK);
		DELAY_START(US_TO_TICKS(SIO_OUT_ACK_TO_COMPLETE_DELAY))
//		fat_sio_enterdir(data_buff);
		if (fat_sio_enterdir(data_buff)<0) {
			DELAY_END
			sio_putc(SIO_ERROR);
		} else {
			DELAY_END
			sio_putc(SIO_COMPLETE);
		}
	} else {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_NAK);
	}
	led_sio_other(0);
}

void sio_conf_dir_up(void) {
	led_sio_other(1);
	sio_putc(SIO_ACK);
	DELAY(US_TO_TICKS(SIO_IN_ACK_TO_COMPLETE_DELAY))
	fat_sio_dirup();
	DELAY_END
	sio_putc(SIO_COMPLETE);
	led_sio_other(0);
}

void sio_lcd_msg (uint8_t lno) {
	int16_t resp;
	led_sio_other(1);
	sio_putc(SIO_ACK);
	resp = sio_read(data_buff,40,US_TO_TICKS(SIO_OUT_DATA_TIMEOUT));
	if (resp>=0) {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_ACK);
		DELAY_START(US_TO_TICKS(SIO_OUT_ACK_TO_COMPLETE_DELAY))
		lcd_put_line(lno,data_buff);
		DELAY_END
		sio_putc(SIO_COMPLETE);
	} else {
		DELAY(US_TO_TICKS(SIO_OUT_DATA_TO_ACK_DELAY))
		sio_putc(SIO_NAK);
	}
	led_sio_other(0);
}




void sio_normal_speed(void) {
	speed = 0;
	UBRRH = 0;
	UBRRL = NORMAL_SPEED_DIVISOR; // (XTAL_CPU/NORMALSPEED)/16-1;
}

void sio_high_speed(void) {
	speed = 1;
	UBRRH = 0;
	UBRRL = HIGH_SPEED_DIVISOR; // (XTAL_CPU/HIGHSPEED)/16-1;
}

void sio_change_speed(void) {
	if (speed || hsindex==255) {
		sio_normal_speed();
	} else {
		sio_high_speed();
	}
}

void sio_init(uint8_t shift) {
	UCSRA = 0x00;	// UX2 (double speed (/8) on = 0x02 ; normal (/16)) 
	UCSRB = 0x18;	//TXE + RXE
	UCSRC = 0x86;	//UCSRCSEL (0x80) + USBS (0x08) + (UCSZ1+UCSZ0) (0x06 = 8bit)
	sbi(SIO_PORT,SIO_CMD); // pullup
	cbi(SIO_DDR,SIO_CMD);
	sio_normal_speed();
	//  uint8_t setup_get_cfg_tool_mode(void);
	hsindex = setup_get_sio_speed();
	if (hsindex>=17) {
		hsindex=255;
	}
	deviceid = setup_get_device_id();
	wprotmode = setup_get_wprot_mode();
	cfgtoolover = setup_get_cfg_tool_mode();
	// cfg_tool_mode:
	//  0 - never
	//  1 - shift on startup
	//  2 - every startup
	//  3 - no card
	//  4 - shift pressed
	if (cfgtoolover==1 && shift==0) {
		cfgtoolover=0;
	}
	if (cfgtoolover>=2) {
		cfgtoolover--;
	}
}

uint8_t sio_check_command(void) {
	if (bit_is_clear(SIO_PIN,SIO_CMD)) {
		return 1;
	} else {
		return 0;
	}
}

int16_t sio_next_command(uint8_t *buff) {
	int16_t resp;
	DELAY_START(US_TO_TICKS(50000))
	while (bit_is_set(SIO_PIN,SIO_CMD)) {
		if (!DELAY_IN_PROGRESS) {
			return -1;
		}
	}
	UCSRB = 0x08;	//restart uart
	UCSRB = 0x18;
	resp = sio_read(buff,4,US_TO_TICKS(SIO_COMMAND_TIMEOUT));
	DELAY_START(US_TO_TICKS(SIO_COMMAND_LINE_TIMEOUT))
	while (bit_is_clear(SIO_PIN,SIO_CMD)) {
		if (!DELAY_IN_PROGRESS) {
			return -1;
		}
	}
	if (resp<0) {
		sio_change_speed();
	}
	return resp;
}

void sio_process_command(void) {
	uint16_t aux;
	uint8_t dno;
	if (sio_next_command(data_buff)>=0) {
		DELAY_START(US_TO_TICKS(SIO_COMMAND_TO_ACK_DELAY))
		aux = *((uint16_t*)(data_buff+2));
		if (data_buff[0]<0x72 || data_buff[0]>0x75 || data_buff[1]) {	// hide check command
			interface_sio_command(data_buff[0],data_buff[1],aux);
		}
		DELAY_END
		if (data_buff[0]==0x72+deviceid) {
			if (cfgtoolover==1 || cfgtoolover==4) {
				cfgtoolover--;	// any setup command turns off cfg tool overrides
			}
			switch(data_buff[1]) {
				case 0x00:
					sio_conf_check();
					break;
				case 0x01:
					sio_conf_get_disk(data_buff[2],data_buff[3]);
					break;
				case 0x02:
					sio_conf_set_disk(data_buff[2],data_buff[3]);
					break;
				case 0x03:
					sio_conf_off_disk(data_buff[2],data_buff[3]);
					break;
				case 0x04:
					sio_conf_get_next(data_buff[2],data_buff[3]);
					break;
				case 0x05:
					sio_conf_enter_dir();
					break;
				case 0x06:
					sio_conf_dir_up();
					break;
				case 0x10:
					sio_lcd_msg(data_buff[2]&1);
					break;
				default:
					sio_putc(SIO_NAK);
			}
#ifdef EIGHTDISKS
		} else if (data_buff[0]>='1' && data_buff[0]<='8') {
#else
		} else if (data_buff[0]>='1' && data_buff[0]<='4') {
#endif
			dno = data_buff[0]-'1';
			if (dno==0 && data_buff[1]=='S' && (cfgtoolover==3 || cfgtoolover==4)) {
				cfgtoolover = keys_shift()?4:3;
			}
			if (dno==0 && (cfgtoolover==1 || (cfgtoolover==2 && cardstatus==0) || cfgtoolover==4)) {
				if (data_buff[1]=='S') {
					sio_cmd_cfg_status();
				} else if (data_buff[1]=='R') {
					sio_cmd_cfg_read(aux);
				} else if (data_buff[1]=='?') {
					sio_cmd_hs_index();
				} else {
					sio_putc(SIO_NAK);
				}
			} else if (disks[dno].totalsectors>0) {
				switch(data_buff[1]) {
					case 'S':	// STATUS
						sio_cmd_status(dno);
						break;
					case 'R':	// READ DATA
						sio_cmd_read(dno,aux);
						break;
					case 'P':	// PUT DATA
					case 'W':	// WRITE DATA
						sio_cmd_write(dno,aux);
						break;
					case '?':	// HSINDEX
						sio_cmd_hs_index();
						break;
//					case ' ':
//						sio_cmd_format_auto(dno);
//						break;
					case '!':	// FORMAT
						sio_cmd_format(dno);
						break;
					case '"':	// FORMAT_ENCHANCED
						sio_cmd_format_medium(dno);
						break;
					case '#':	// FORMAT_DSDD
						sio_cmd_format_dsdd(dno);
						break;
//					case 0x66:	// FORMAT WITH SKEW
//						sio_cmd_format_skew(dno);
//						break;
					case 'N':	// GET PERCOM BLOCK
						sio_cmd_get_percom(dno);
						break;
					case 'O':	// PUT PERCOM BLOCK
						sio_cmd_put_percom(dno);
						break;
//					case 0x44:	//configure drive
//					case 0x4b:	//slow/fast
//					case 0x51:	//flush disks
//					case 0x55:	//motor on
//					case 0x48:	//happy command
//						sio_cmd_just_ack();
//						break;
//					case 0x93:	// APE special aux==0xA0EE -> get time (6 bytes d,m,y,H,M,S) ; aux==0x..f0 -> get image name (256 bytes ; 254 name,0,'M' for virtual / 'A' for normal) ; 0x..f1 -> get version (256 bytes any string)
					default:
						sio_putc(SIO_NAK);
				}
			}
		}
	}
}
