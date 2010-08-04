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

#include <string.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include "interface.h"
#include "sio.h"
#include "mmc.h"
#include "led.h"
#include "lcd.h"
#include "fat.h"

//#define GET_SHORT(x) ((uint16_t)((x)[1])*256+(x)[0])
//#define GET_LONG(x) ((((uint32_t)((x)[3])*256+(x)[2])*256+(x)[1])*256+(x)[0])
#define GET_SHORT(x) (*((uint16_t*)(x)))
#define GET_LONG(x) (*((uint32_t*)(x)))

#define MAXRETRIES 10

struct partrecord {
	uint8_t prIsActive;       // 0x80 indicates active partition
	uint8_t prStartHead;      // starting head for partition
	uint16_t prStartCylSect;  // starting cylinder and sector
	uint8_t prPartType;       // partition type (see above)
	uint8_t prEndHead;        // ending head for this partition
	uint16_t prEndCylSect;    // ending cylinder and sector
	uint32_t prStartLBA;      // first LBA sector for this partition
	uint32_t prSize;          // size of this partition sectors
};

// BPB for DOS 7.10 (FAT32).
// This one has a few extensions to bpb50.
struct bpb710 {
	uint16_t bpbBytesPerSec;  // bytes per sector
	uint8_t bpbSecPerClust;   // sectors per cluster
	uint16_t bpbResSectors;   // number of reserved sectors
	uint8_t bpbFATs;          // number of FATs
	uint16_t bpbRootDirEnts;  // number of root directory entries
	uint16_t bpbSectors;      // total number of sectors
	uint8_t bpbMedia;         // media descriptor
	uint16_t bpbFATsecs;      // number of sectors per FAT
	uint16_t bpbSecPerTrack;  // sectors per track
	uint16_t bpbHeads;        // number of heads
	uint32_t bpbHiddenSecs;   // # of hidden sectors
	uint32_t bpbHugeSectors;  // # of sectors if bpbSectors == 0
	uint32_t bpbBigFATsecs;   // like bpbFATsecs for FAT32
	uint16_t bpbExtFlags;     // extended flags:
	uint16_t bpbFSVers;       // filesystem version
	uint32_t bpbRootClust;    // start cluster for root directory
	uint16_t bpbFSInfo;       // filesystem info structure sector
	uint16_t bpbBackup;       // backup boot sector
};

//file system data
static uint32_t partition_start;
static uint8_t fat_type;          // 1 - FAT12 ; 2 - FAT16 ; 3 - FAT32
static uint8_t root_type;
static uint32_t fat_start;        // primary fat - first sector
static uint32_t fat_size;         // fat size in sectors
static uint32_t dir_start;        // root dir - first sector
static uint32_t data_start;       // data - first sector
static uint32_t max_cluster;
static uint8_t clust_bits;        // sec_to_clust_bits
static uint8_t cluster_size;      //sectors per clusters = 1<<clust_bits

//logical data
static uint32_t atari_clust;
static uint32_t cfg_clust;
static uint32_t firmware_clust;

//main sector buffer
static uint32_t buffsector;
static uint8_t buff[512];

typedef struct _entrypos {
	uint32_t cluster;         // current cluster (fcluster..EOF)
	uint8_t secpos;           // current sector in cluster (0..cluster_size-1)
	uint8_t pos;              // current position in sector (0..15)
} entrypos; // 6B 

typedef struct _entryit {
	uint32_t fcluster;        // first cluster of directory
	entrypos epos;            // current iterator position
	uint8_t ftype:3;          // current object type
	uint8_t emptydir:1;       // empty dir flag
	uint8_t name[39];         // current object name
} entryit; // 11+39 (50B)

typedef struct _fileentry {
	uint32_t fcluster;        // first cluster of file
	uint32_t size;            // length of file in bytes
	entrypos epos;            // entry position in directory
	uint8_t ftype:3;          // file type (ATR,XEX/COM,XFD,...)
	uint8_t changed:1;        // entry was changed
	uint8_t name[39];         // file name
} fileentry; // 15+39 (54B)

typedef struct _cacheentry {
	uint16_t count;
	uint32_t cluster:24;      // 24 bits is enough for small SD card sizes
} cacheentry; // 5B

#ifdef DYNAMIC_FATCACHE
#define FAT_FRAGS_PER_FILE 20
#define FILES_IN_CACHE 3
static cacheentry fatcache[FILES_IN_CACHE][FAT_FRAGS_PER_FILE];
static uint8_t cachefno[FILES_IN_CACHE],cacheprio[FILES_IN_CACHE];
static fileentry files[8];
#else
 #ifdef EIGHTDISKS
 #define FAT_FRAGS_PER_FILE 8
static cacheentry fatcache[8][FAT_FRAGS_PER_FILE];
static fileentry files[8];
 #else
 #define FAT_FRAGS_PER_FILE 25
static cacheentry fatcache[4][FAT_FRAGS_PER_FILE];
static fileentry files[4];	// D1,D2,D3,D4 -> EEPROM
 #endif
#endif

/* keyboard+lcd */
static uint8_t cfno;
static entryit lcdit;
/* sio2sd cfg */
static entryit cfgit;

//#ifdef ALPHABETICAL_ORDER
//static entryit current,best;
//#endif

#define CFGSIZE (sizeof(files))

#define CLUST_FREE 0
#define CLUST_EOF 0xFFFFFFFF
#define CLUST_BAD 0xFFFFFFFE
#define CLUST_INCORRECT 0xFFFFFFFD
#define CLUST_IS_SPECIAL(x) (((x)-1)>=CLUST_INCORRECT-1)

static const uint8_t empty_file[39] __attribute__ ((progmem)) = " - OFF -                               ";
static const uint8_t empty_dir[39] __attribute__ ((progmem)) = " - EMPTY DIR -                         ";
//const uint8_t empty_file[7]="- OFF -";
//const uint8_t empty_dir[13]="- EMPTY DIR -";

static const char dir_up_name[11] __attribute__ ((progmem)) = "..         ";
static const char atari_name[11] __attribute__ ((progmem)) = "ATARI      ";
static const char config_name[11] __attribute__ ((progmem)) = "SIO2SD  CFG";
static const char firmware_name[11] __attribute__ ((progmem)) = "SIO2SD  BIN";

void fat_init_state(void) {
	uint8_t i;
	buffsector=0xffffffff;
	fat_type=0;
	memset(files,0,sizeof(files));
#ifdef EIGHTDISKS
	for (i=0 ; i<8 ; i++) {
#else
	for (i=0 ; i<4 ; i++) {
#endif
		files[i].changed=1;
		memcpy_P(files[i].name,empty_file,39);
		sio_check_disk(i);
	}
	sio_fat_status(0);
}

void fat_ioerror(void) {
	fat_init_state();
	interface_eio();
}

void fat_readsector(uint32_t number) {
	uint8_t tc;
	if (number!=buffsector) {
		led_sd_read(1);
		tc=0;
		while (mmc_read_sector(number,buff)==0 && tc<MAXRETRIES) {
			led_error(1);
			tc++;
			if ((tc&3)==3) {
				mmc_card_init();
			}
		}
		led_sd_read(0);
		if (tc==MAXRETRIES) {
			fat_ioerror();
			return;
		}
		led_error(0);
		buffsector=number;
	}
}

void fat_updatesector(void) {
	uint8_t tc;
	led_sd_write(1);
	tc=0;
	while (mmc_write_sector(buffsector,buff)==0 && tc<MAXRETRIES) {
		led_error(1);
		tc++;
		if ((tc&3)==3) {
			mmc_card_init();
		}
	}
	led_sd_write(0);
	if (tc==MAXRETRIES) {
		fat_ioerror();
		return;
	}
	led_error(0);
}

void fat_erasesector(uint32_t number) {
	uint8_t tc;
	led_sd_write(1);
	tc=0;
	while (mmc_erase_sector(number)==0 && tc<MAXRETRIES) {
		led_error(1);
		tc++;
		if ((tc&3)==3) {
			mmc_card_init();
		}
	}
	led_sd_write(0);
	if (tc==MAXRETRIES) {
		fat_ioerror();
		return;
	}
	led_error(0);
}

int8_t fat_partition_test(void) {
	struct partrecord *prec;
	struct bpb710 *bpb;
	uint8_t c;

	fat_readsector(0);
// first test if there is a valid FAT boot-sector
	bpb = (struct bpb710 *)(buff+11);
	c = bpb->bpbSecPerClust;
	if (bpb->bpbBytesPerSec==512 && c>0 && (c & (c-1))==0 && bpb->bpbResSectors>0 && bpb->bpbFATs==2) {
		partition_start = 0;
		return 1;
	} else { // else check first entry in partition table
		prec = (struct partrecord*)(buff+0x1BE);
		partition_start = prec->prStartLBA;
		switch (prec->prPartType) {
		case 1:
		case 4:
		case 6:
		case 14:
		case 11:
		case 12:
			return 1;
		}
	}
	return 0;
}

int8_t fat_fsinit(void) {
	struct bpb710 *bpb;
	uint8_t spc;
	uint32_t clust,sectors;
	uint16_t rootdirsecs;

	fat_readsector(partition_start);
	bpb = (struct bpb710 *)(buff+11);

	if (bpb->bpbBytesPerSec!=512 || bpb->bpbSecPerClust==0 || bpb->bpbResSectors==0 || bpb->bpbFATs!=2) {
		fat_type = 0;
		return 0;
	}

	//check FAT type
	sectors = bpb->bpbSectors;
	if (sectors==0) {
		sectors = bpb->bpbHugeSectors;
	}
	fat_size = bpb->bpbFATsecs;
	if (fat_size==0) {
		fat_size = bpb->bpbBigFATsecs;
	}
	rootdirsecs = bpb->bpbRootDirEnts>>4;
	clust = sectors - bpb->bpbResSectors - fat_size - fat_size - rootdirsecs;
	spc = bpb->bpbSecPerClust;
	if (spc & (spc-1)) {
		fat_type = 0;
//		printf("UNKNOWN PARTITION\n");
		return 0;
	}
	cluster_size = spc;
	clust_bits = 0;
	while (spc>1) {
		clust>>=1;
		spc>>=1;
		clust_bits++;
	}
	max_cluster = clust+1;
	if (max_cluster<=0xfef) {
		fat_type = 1; // FAT12
	} else if (max_cluster<=0xffef) {
		fat_type = 2; // FAT16
	} else if (max_cluster<=0xffffff) {
		fat_type = 3; // FAT32 (up to 16m clusters)
	} else {
		fat_type = 4; // FAT32 (too big for SD card)
	}

	fat_start = partition_start + bpb->bpbResSectors;
	if (rootdirsecs>0) {
		root_type = 0;
		dir_start = fat_start + fat_size + fat_size;
		data_start = dir_start + rootdirsecs;
	} else {
		root_type = 1;
		data_start = fat_start + fat_size + fat_size;
		dir_start = bpb->bpbRootClust;
	}
	return 1;
}

uint32_t fat_nextcluster(uint32_t cluster) {
	uint8_t c,*fptr;
	uint32_t sect;
	uint16_t offset;
	if (cluster<2 || cluster>max_cluster) {
		return CLUST_EOF;
	}
	switch (fat_type) {
	case 1: //FAT12
		offset=cluster;
		offset+=offset>>1;
		sect = fat_start + (offset>>9);
		offset &= 0x1FF;
		fat_readsector(sect);
		fptr = buff+offset;
		c = *fptr;
		if (offset==0x1FF) {
			fat_readsector(sect+1);
			fptr = buff;
		} else {
			fptr++;
		}
		if (cluster&1) {
			offset = (((uint16_t)(*fptr))<<4)+(c>>4);
		} else {
			offset = ((((uint16_t)(*fptr))&0xF)<<8)+c;
		}
		if (offset>=0xff8) {
			return CLUST_EOF;
		} else if (offset==0xff7) {
			return CLUST_BAD;
		} else if (offset>=0xff0 || offset==1) {
			return CLUST_INCORRECT;
		} else {
			return offset;
		}
		break;
	case 2: //FAT16
		offset = cluster;
		sect = fat_start + (offset>>8);
		offset &= 0xff;
		fptr = buff+(offset<<1);
		fat_readsector(sect);
		offset = GET_SHORT(fptr);
		if (offset>=0xfff8) {
			return CLUST_EOF;
		} else if (offset==0xfff7) {
			return CLUST_BAD;
		} else if (offset>=0xfff0 || offset==1) {
			return CLUST_INCORRECT;
		} else {
			return offset;
		}
		break;
	case 3: //FAT32
		sect = fat_start + (cluster>>7);
		offset = cluster & 0x7f;
		fptr = buff+(offset<<2);
		fat_readsector(sect);
		cluster = GET_LONG(fptr);
		if (cluster>=0x0ffffff8) {
			return CLUST_EOF;
		} else if (cluster==0x0ffffff7) {
			return CLUST_BAD;
		} else if (cluster>0x0ffffff0 || cluster==1) {
			return CLUST_INCORRECT;
		} else {
			return cluster;
		}
		break;
	}
	return CLUST_EOF;
}

uint32_t fat_prevcluster(uint32_t fcluster,uint32_t cluster) {
	uint32_t pcluster;
	if (fcluster==cluster) {
		return CLUST_EOF;
	}
	while (!(fcluster==CLUST_EOF || fcluster==CLUST_BAD || fcluster==CLUST_INCORRECT || fcluster==CLUST_FREE)) {
		pcluster = fcluster;
		fcluster = fat_nextcluster(fcluster);
		if (fcluster==cluster) {
			return pcluster;
		}
	}
	return CLUST_INCORRECT;
}

void fat_setnextcluster(uint32_t cluster,uint32_t next) {
	uint8_t *fptr;
	uint32_t sect;
	uint16_t offset;
	if (cluster<2 || cluster>max_cluster || ((next<2 || next>max_cluster) && next!=CLUST_FREE && next!=CLUST_EOF)) {
		return;
	}
	switch (fat_type) {
	case 1: //FAT12
		// first copy
		offset=cluster;
		offset+=(offset>>1);
		sect = fat_start + (offset>>9);
		offset &= 0x1FF;
		fat_readsector(sect);
		fptr = buff+offset;
		if (cluster&1) {
			*fptr = ((*fptr)&0x0F) | ((next<<4)&0xF0);
		} else {
			*fptr = next;
		}
		if (offset==0x1FF) {
			fat_updatesector();
			fat_readsector(sect+1);
			fptr = buff;
		} else {
			fptr++;
		}
		if (cluster&1) {
			*fptr = next>>4;
		} else {
			*fptr = ((*fptr)&0xF0) | ((next>>8)&0x0F);
		}
		fat_updatesector();
		// second copy
		sect += fat_size;
		fat_readsector(sect);
		fptr = buff+offset;
		if (cluster&1) {
			*fptr = ((*fptr)&0x0F) | ((next<<4)&0xF0);
		} else {
			*fptr = next;
		}
		if (offset==0x1FF) {
			fat_updatesector();
			fat_readsector(sect+1);
			fptr = buff;
		} else {
			fptr++;
		}
		if (cluster&1) {
			*fptr = next>>4;
		} else {
			*fptr = ((*fptr)&0xF0) | ((next>>8)&0x0F);
		}
		fat_updatesector();
		break;
	case 2: //FAT16
		offset = cluster;
		sect = fat_start + (offset>>8);
		offset &= 0xff;
		fptr = buff+(offset<<1);
		fat_readsector(sect);
		*(uint16_t*)fptr = next;
		fat_updatesector();
		fat_readsector(sect+fat_size);
		*(uint16_t*)fptr = next;
		fat_updatesector();
		break;
	case 3: //FAT32
		sect = fat_start + (cluster>>7);
		offset = cluster & 0x7f;
		fptr = buff+(offset<<2);
		fat_readsector(sect);
		*(uint32_t*)fptr = next;
		fat_updatesector();
		fat_readsector(sect+fat_size);
		*(uint32_t*)fptr = next;
		fat_updatesector();
		break;
	}
}

uint32_t fat_nextfree(uint32_t first) {
	while (first<=max_cluster) {
		if (fat_nextcluster(first)==CLUST_FREE) {
			return first;
		}
		first++;
	}
	return CLUST_INCORRECT;	// out of space
}



uint32_t fat_clusttosect(uint32_t cluster) {
	return data_start + ((cluster-2)<<clust_bits);
}


uint32_t fat_get_fcluster(entrypos *epos) {
	uint32_t sector;
	uint8_t *bptr;
	sector = fat_clusttosect(epos->cluster)+epos->secpos;
	fat_readsector(sector);
	bptr = buff+(((uint16_t)(epos->pos))<<5);
	if (fat_type==3) {
		return (((uint32_t)(GET_SHORT(bptr+20)))<<16)+GET_SHORT(bptr+26);
	} else {
		return GET_SHORT(bptr+26);
	}
}

void fat_set_fcluster(entrypos *epos,uint32_t fcluster) {
	uint32_t sector;
	uint8_t *bptr;
	if ((fcluster<2 || fcluster>max_cluster) && fcluster!=CLUST_FREE) {
		return;
	}
	sector = fat_clusttosect(epos->cluster)+epos->secpos;
	fat_readsector(sector);
	bptr = buff+(((uint16_t)(epos->pos))<<5);
	*(uint16_t*)(bptr+26) = fcluster;
	if (fat_type==3) {
		*(uint16_t*)(bptr+20) = fcluster>>16;
	}
	fat_updatesector();
}

uint32_t fat_get_size(entrypos *epos) {
	uint32_t sector;
	uint8_t *bptr;
	sector = fat_clusttosect(epos->cluster)+epos->secpos;
	fat_readsector(sector);
	bptr = buff+(((uint16_t)(epos->pos))<<5);
	return GET_LONG(bptr+28);
}

void fat_set_size(entrypos *epos,uint32_t size) {
	uint32_t sector;
	uint8_t *bptr;
	sector = fat_clusttosect(epos->cluster)+epos->secpos;
	fat_readsector(sector);
	bptr = buff+(((uint16_t)(epos->pos))<<5);
	*(uint32_t*)(bptr+28) = size;
	fat_updatesector();
}

#ifdef DYNAMIC_FATCACHE
void fat_clear_cache(uint8_t fno) {
	uint8_t c,cno;
	for (cno=0 ; cno<FILES_IN_CACHE ; cno++) {
		if (cachefno[cno]==fno) {
			for (c=0 ; c<FILES_IN_CACHE ; c++) {
				if (cacheprio[c]<cacheprio[cno]) {
					cacheprio[c]++;
				}
			}
			cacheprio[cno]=0;
			memset(fatcache+cno,0,sizeof(cacheentry)*FAT_FRAGS_PER_FILE);
			cachefno[cno]=0xFF;
		}
	}
}
#else
void fat_clear_cache(uint8_t fno) {
	memset(fatcache+fno,0,sizeof(cacheentry)*FAT_FRAGS_PER_FILE);
}
#endif

/*
void fat_fill_cache(uint8_t fno) {
	uint32_t acluster,cluster;
	uint8_t c;
	c=0xff; // -1
	cluster = files[fno].fcluster;
	acluster = cluster;
	for (;;) {
		if (CLUST_IS_SPECIAL(cluster)) {
			return;
		}
		if (cluster==acluster+1) {
			fatcache[fno][c].count++;
		} else {
			c++;
			if (c==FAT_FRAGS_PER_FILE) {
				return;
			}
			fatcache[fno][c].cluster = cluster;
			fatcache[fno][c].count = 1;
		}
		acluster = cluster;
		cluster = fat_nextcluster(cluster);
	}
}
*/

// find sector of file given by position
#ifdef DYNAMIC_FATCACHE
uint8_t fat_getcno(uint8_t fno) {
	uint8_t c,cno;
	c=0xff;
	for (cno=0 ; cno<FILES_IN_CACHE ; cno++) {
		if (cachefno[cno]==fno) {
			if (cacheprio[cno]!=FILES_IN_CACHE-1) {
				for (c=0 ; c<FILES_IN_CACHE ; c++) {
					if (cacheprio[c]>cacheprio[cno]) {
						cacheprio[c]--;
					}
				}
				cacheprio[cno]=FILES_IN_CACHE-1;
			}
			return cno;
		}
		if (cacheprio[cno]==0) {
			c=cno;
		}
	}
	cno = c;
	for (c=0 ; c<FILES_IN_CACHE ; c++) {
		cacheprio[c]--;
	}
	cacheprio[cno]=FILES_IN_CACHE-1;
	memset(fatcache+cno,0,sizeof(cacheentry)*FAT_FRAGS_PER_FILE);
	cachefno[cno]=fno;
	return cno;
}
#endif

uint32_t fat_findsector(uint8_t fno,uint16_t position) {
	uint16_t clustno;
	uint32_t acluster,cluster;
	uint8_t c,cno;
#ifdef DYNAMIC_FATCACHE
	cno = fat_getcno(fno);
#else
	cno = fno;
#endif
	clustno = position>>clust_bits;
	// first check in cache
	if (fatcache[cno][0].count==0) {	// empty
		fatcache[cno][0].count=1;
		fatcache[cno][0].cluster = files[fno].fcluster;
	}
	c=0;
	while (c<(FAT_FRAGS_PER_FILE-1) && fatcache[cno][c+1].count>0 && fatcache[cno][c].count<=clustno) {
		clustno-=fatcache[cno][c].count;
		c++;
	}
	if (clustno<fatcache[cno][c].count) {
		// found in cache
		cluster = fatcache[cno][c].cluster+clustno;
	} else {
		// else ordinary find + store trace in cache
		clustno -= fatcache[cno][c].count-1;
		cluster = fatcache[cno][c].cluster+fatcache[cno][c].count-1;
		while (clustno>0) {
			acluster = cluster;
			cluster = fat_nextcluster(cluster);
			if (cluster==acluster+1) {
				fatcache[cno][c].count++;
			} else if (c<FAT_FRAGS_PER_FILE) {
				c++;
				fatcache[cno][c].count=1;
				fatcache[cno][c].cluster=cluster;
			}
			clustno--;
		}
	}
	cluster = fat_clusttosect(cluster);
	cluster += position & (cluster_size - 1);
	return cluster;
}

void fat_store_index(void) {
	if (cfg_clust>0 && cfg_clust!=CLUST_INCORRECT) {
		fat_readsector(fat_clusttosect(cfg_clust));
		memcpy(buff,files,sizeof(files));
		fat_updatesector();
	}
}

void fat_load_index(void) {
	uint8_t c,ok;
	if (cfg_clust>0 && cfg_clust!=CLUST_INCORRECT) {
		fat_readsector(fat_clusttosect(cfg_clust));
		memcpy(files,buff,sizeof(files));
		ok=1;
#ifdef EIGHTDISKS
		for (c=0 ; c<8 && ok ; c++) {
#else
		for (c=0 ; c<4 && ok ; c++) {
#endif
			if (files[c].fcluster!=0 && files[c].size!=0 &&
				( files[c].fcluster != fat_get_fcluster(&(files[c].epos)) ||
				  files[c].size != fat_get_size(&(files[c].epos))            ) ) {
				ok=0;
			}
		}
		if (ok==0) {
			memset(files,0,sizeof(files));
#ifdef EIGHTDISKS
			for (c=0 ; c<8 ; c++) {
#else
			for (c=0 ; c<4 ; c++) {
#endif
				memcpy_P(files[c].name,empty_file,39);
				//memset(files[c].name,' ',39);
				//memcpy(files[c].name+1,empty_file,7);
				files[c].changed=1;
			}
		} else {
#ifdef EIGHTDISKS
			for (c=0 ; c<8 ; c++) {
#else
			for (c=0 ; c<4 ; c++) {
#endif
				fat_clear_cache(c);
//				fat_fill_cache(c);
			}
		}
	}
}

void fat_file_changed(uint8_t fno) {
	fat_clear_cache(fno);
//	fat_fill_cache(fno);
	fat_store_index();
	files[fno].changed=1;
}

uint8_t fat_was_changed(uint8_t fno) {
	uint8_t i = files[fno].changed;
	files[fno].changed=0;
	return i;
}

uint32_t fat_get_file_size(uint8_t fno) {
	return files[fno].size;
}

uint8_t* fat_get_file_name(uint8_t fno) {
	return files[fno].name;
}

uint8_t fat_set_file_size(uint8_t fno,uint32_t size) {
	uint32_t acluster,cluster,clusters,nclusters;
	if (files[fno].size==size) {
		return 1;
	}
	clusters = files[fno].size;
	clusters+=511;	//ceil(size/512)
	clusters>>=9;	//    -"-
	clusters+=(cluster_size-1);	// ceil(sectors/cluster_size);
	clusters>>=clust_bits;
	nclusters = size;
	nclusters+=511;
	nclusters>>=9;
	nclusters+=(cluster_size-1);
	nclusters>>=clust_bits;
	if (nclusters<clusters) {
		cluster = files[fno].fcluster;
		if (nclusters>0) {
			while (nclusters>0) {
				acluster = cluster;
				cluster = fat_nextcluster(cluster);
				nclusters--;
				clusters--;
			}
			fat_setnextcluster(acluster,CLUST_EOF);
		} else {
			fat_set_fcluster(&(files[fno].epos),CLUST_FREE);
			files[fno].fcluster = 0;
		}
		while (clusters>0) {
			acluster = cluster;
			cluster = fat_nextcluster(cluster);
			fat_setnextcluster(acluster,CLUST_FREE);
			clusters--;
		}
		// tu zawsze cluster powinien byc rowny CLUST_EOF - moze warto sprawdzic
	} else if (nclusters>clusters) {
		acluster = CLUST_INCORRECT;
		cluster = files[fno].fcluster;
		while (clusters>0) {
			acluster = cluster;
			cluster = fat_nextcluster(cluster);
			nclusters--;
			clusters--;
		}
		// cluster powinien byc rowny CLUST_EOF lub ( CLUST_FREE i acluster CLUST_INCORRECT )
		cluster = 1;
		while (nclusters>0) {
			cluster = fat_nextfree(cluster+1);
			if (cluster==CLUST_INCORRECT) {
				// out of space
				if (acluster==CLUST_INCORRECT) {
					return 0;	// previous size was 0, so can exit now
				}
				fat_setnextcluster(acluster,CLUST_EOF);
				size+=511;
				size>>=9;
				size+=(cluster_size-1);
				size>>=clust_bits;
				size-=nclusters;
				size<<=(clust_bits+9);
				files[fno].size = size;
				fat_set_size(&(files[fno].epos),size);
				fat_file_changed(fno);
				return 0;
			}
			if (acluster==CLUST_INCORRECT) {
				fat_set_fcluster(&(files[fno].epos),cluster);
				files[fno].fcluster = cluster;
			} else {
				fat_setnextcluster(acluster,cluster);
			}
			acluster = cluster;
			nclusters--;
		}
		fat_setnextcluster(acluster,CLUST_EOF);
	}
	files[fno].size = size;
	fat_set_size(&(files[fno].epos),size);
	fat_file_changed(fno);
	return 1;
}

uint16_t fat_pread(uint8_t fno,uint8_t *mbuff,uint32_t offset,uint16_t size) {
	uint16_t rsize,ssize,soff,sectorno;
	uint32_t sector;
	if (fat_type==0 || files[fno].size<=offset) {
		return 0;
	}
	if (offset+size>files[fno].size) {
		size = files[fno].size-offset;
	}
	rsize = size;
	while (size>0) {
		soff = offset&0x1FF;
		ssize = 512-soff;
		if (ssize>size) {
			ssize=size;
		}
		sectorno = offset>>9;
		sector = fat_findsector(fno,sectorno);
		fat_readsector(sector);
		memcpy(mbuff,buff+soff,ssize);
		size-=ssize;
		offset+=ssize;
		mbuff+=ssize;
	}
	return rsize;
}

uint16_t fat_pwrite(uint8_t fno,uint8_t *mbuff,uint32_t offset,uint16_t size) {
	uint16_t rsize,ssize,soff,sectorno;
	uint32_t sector;
	if (fat_type==0 || files[fno].size<=offset) {
		return 0;
	}
	if (offset+size>files[fno].size) {
		size = files[fno].size-offset;
	}
	rsize = size;
	while (size>0) {
		soff = offset&0x1FF;
		ssize = 512-soff;
		if (ssize>size) {
			ssize=size;
		}
		sectorno = offset>>9;
		sector = fat_findsector(fno,sectorno);
		fat_readsector(sector);
		memcpy(buff+soff,mbuff,ssize);
		fat_updatesector();
		size-=ssize;
		offset+=ssize;
		mbuff+=ssize;
	}
	return rsize;
}

uint8_t fat_get_file_type(uint8_t fno) {
	if (fat_type==0) {
		return FTYPE_NONE;
	}
	return files[fno].ftype;
}

uint32_t fat_get_parent(uint32_t fcluster) {
	uint32_t sector;
	uint8_t *bptr;
	sector = fat_clusttosect(fcluster);
	fat_readsector(sector);
	bptr = buff+32;
	if (strncmp_P((char*)bptr,dir_up_name,11)!=0) {
		return atari_clust;
	}
	if (fat_type==3) {
		return (((uint32_t)(GET_SHORT(bptr+20)))<<16)+GET_SHORT(bptr+26);
	} else {
		return GET_SHORT(bptr+26);
	}
//	return fat_get_fcluster(fcluster,0,1);
}



uint8_t fat_get_type(uint8_t *bptr) {
	if ((bptr[11]&0x10)==0x10 && bptr[0]!='.') {
		return FTYPE_DIR;
	} else if (bptr[0]!='.' && bptr[0]!='_') {
		if (bptr[8]=='A' && bptr[9]=='T' && bptr[10]=='R') {
			return FTYPE_ATR;
		} else if (bptr[8]=='X' && bptr[9]=='F' && bptr[10]=='D') {
			return FTYPE_XFD;
		} else if (bptr[8]=='X' && bptr[9]=='E' && bptr[10]=='X') {
			return FTYPE_EXE;
		} else if (bptr[8]=='C' && bptr[9]=='O' && bptr[10]=='M') {
			return FTYPE_EXE;
		}
	}
	return FTYPE_NONE;
}

void fat_prepare_name(uint8_t name[39]) {
	uint8_t c=0;
	while (name[c] && c<39) {c++;}
	while (c<39) {
		name[c]=' ';
		c++;
	}
}

void fat_get_short_name(uint8_t name[39],uint8_t *bptr) {
	uint8_t c;
	for (c=0 ; c<8 && bptr[c]!=' ' ; c++) {
		name[c]=bptr[c];
	}
	if (bptr[8]!=' ') {
		name[c++]='.';
		name[c++]=bptr[8];
		if (bptr[9]!=' ') {
			name[c++]=bptr[9];
			if (bptr[10]!=' ') {
				name[c++]=bptr[10];
			}
		}
	}
	name[c]='\0';
}

void fat_get_long_name_frag(uint8_t name[39],uint8_t *bptr,uint8_t seq) {
	if (seq==3) {
		name+=26;
	} else if (seq==2) {
		name+=13;
	} else if (seq!=1) {
		return;
	}
	*name++ = bptr[1];
	*name++ = bptr[3];
	*name++ = bptr[5];
	*name++ = bptr[7];
	*name++ = bptr[9];
	*name++ = bptr[14];
	*name++ = bptr[16];
	*name++ = bptr[18];
	*name++ = bptr[20];
	*name++ = bptr[22];
	*name++ = bptr[24];
	*name++ = bptr[28];
	*name++ = bptr[30];
}

uint8_t fat_name_check_sum(uint8_t *bptr) {
	uint8_t c,sum = 0;
	for (c=0 ; c<11 ; c++) {
		if (sum&1) {
			sum = (sum >> 1) + 0x80;
		} else {
			sum = (sum >> 1);
		}
		sum += bptr[c];
	}
	return sum;
}


uint8_t fat_prev_entry(entryit *it) {
	uint32_t sector;
	uint8_t *bptr;
	uint8_t lfnseq,lfnchk,seq;
	entrypos epos;

	if (fat_type==0) {
		return 0;
	}
	lfnchk=0;
	lfnseq=0;
	seq=255;
	epos = it->epos;
	it->name[0]='\0';
	sector = fat_clusttosect(epos.cluster);
	it->ftype = FTYPE_NONE;
	do {
		if (epos.pos!=0) {
			epos.pos--;
		} else {
			epos.pos=15;
			if (epos.secpos!=0) {
				epos.secpos--;
			} else {
				epos.secpos=cluster_size-1;
				epos.cluster = fat_prevcluster(it->fcluster,epos.cluster);
				if (epos.cluster==CLUST_FREE || epos.cluster==CLUST_EOF || epos.cluster==CLUST_BAD || epos.cluster==CLUST_INCORRECT) {
					if (it->ftype!=FTYPE_NONE) {
						fat_prepare_name(it->name);
						return 1;
					} else {
						return 0;
					}
				}
				sector = fat_clusttosect(epos.cluster);
			}
		}
		fat_readsector(sector+epos.secpos);
		bptr = buff+(((uint16_t)(epos.pos))<<5);

		if (bptr[0]!=0xE5 && ((bptr[11]&0x10)==0x10 || (bptr[11]&0x1E)==0)) {
			if (it->ftype!=FTYPE_NONE) {
				fat_prepare_name(it->name);
				return 1;
			}
			it->ftype = fat_get_type(bptr);
			if (it->ftype!=FTYPE_NONE) {
				lfnchk = fat_name_check_sum(bptr);
				fat_get_short_name(it->name,bptr);
				it->epos = epos;
				lfnseq=1;
			}
		} else if (bptr[11]==0x0f && it->ftype!=FTYPE_NONE) {
			seq = ((*bptr)&0x3f);
			if (lfnseq==seq && lfnchk==bptr[13]) {
				lfnseq++;
			} else {
				fat_prepare_name(it->name);
				return 1;
			}
			fat_get_long_name_frag(it->name,bptr,seq);
			if (seq==1) {
				it->name[13]='\0';
			} else if (seq==2) {
				it->name[26]='\0';
			}
			if (*bptr&0x40 || seq==3) {
				fat_prepare_name(it->name);
				return 1;
			}
		}
	} while (1);
}

uint8_t fat_next_entry(entryit *it) {
	uint32_t sector;
	uint8_t *bptr;
	uint8_t lfnseq,lfnchk,seq;

	if (fat_type==0) {
		return 0;
	}
	lfnchk=0;
	lfnseq=0;
	seq=255;
	it->name[0]='\0';
	sector = fat_clusttosect(it->epos.cluster);
	it->ftype = FTYPE_NONE;
	do {
		it->epos.pos++;
		if (it->epos.pos==16) {
			it->epos.pos=0;
			it->epos.secpos++;
			if (it->epos.secpos==cluster_size) {
				it->epos.secpos=0;
				it->epos.cluster = fat_nextcluster(it->epos.cluster);
				if (it->epos.cluster==CLUST_FREE || it->epos.cluster==CLUST_EOF || it->epos.cluster==CLUST_BAD || it->epos.cluster==CLUST_INCORRECT) {
					it->epos.cluster = 0;
					it->epos.secpos = 0;
					it->epos.pos = 0;
					return 0;
				}
				sector = fat_clusttosect(it->epos.cluster);
			}
		}
		fat_readsector(sector+it->epos.secpos);
		bptr = buff+(((uint16_t)(it->epos.pos))<<5);

		if (*bptr==0) {
			it->epos.cluster = 0;
			it->epos.secpos = 0;
			it->epos.pos = 0;
			return 0;
		}

		if (bptr[11]==0x0f) {
			seq = ((*bptr)&0x3f);
			if (*bptr&0x40) {
				lfnseq=seq;
				lfnchk=bptr[13];
				if (seq==1) {
					it->name[13]='\0';
				} else if (seq==2) {
					it->name[26]='\0';
				}
			} else {
				if (lfnseq-1==seq && lfnchk==bptr[13]) {
					lfnseq=seq;
				} else {
					lfnchk=0;
					lfnseq=0;
					seq=255;
					it->name[0]='\0';
				}
			}
			fat_get_long_name_frag(it->name,bptr,seq);
		} else if (bptr[0]!=0xE5 && ((bptr[11]&0x10)==0x10 || (bptr[11]&0x1E)==0)) {	// valid entry
			if (lfnseq!=1 || fat_name_check_sum(bptr)!=lfnchk) {
				fat_get_short_name(it->name,bptr);
			}
			it->ftype = fat_get_type(bptr);
			if (it->ftype!=FTYPE_NONE) {
				fat_prepare_name(it->name);
			}
		}
	} while (it->ftype==FTYPE_NONE);
	return 1;
}

void fat_resetpos(entryit *it) {
	it->epos.cluster = it->fcluster;
	it->epos.secpos = 0;
	it->epos.pos = 0;
}

void fat_firstpos(entryit *it) {
	it->epos.cluster = it->fcluster;
	it->epos.secpos = 0;
	it->epos.pos = 0;
	if (fat_next_entry(it)==0) {	// empty dir
		it->emptydir=1;
		memcpy_P(it->name,empty_dir,39);
	} else {
		it->emptydir=0;
	}
}

void fat_lastpos(entryit *it) {
	it->epos.cluster = CLUST_EOF;
	it->epos.secpos = 0;
	it->epos.pos = 0;
	if (fat_prev_entry(it)==0) {	// empty dir
		it->emptydir=1;
		memcpy_P(it->name,empty_dir,39);
	} else {
		it->emptydir=0;
	}
}

/* lcd+keyboard */

void fat_key_nextfile(void) {
	if (fat_type==0) {
		return;
	}
	if (lcdit.emptydir) {
		return;
	}
	if (fat_next_entry(&lcdit)==0) {
		fat_firstpos(&lcdit);
	}
}

void fat_key_prevfile(void) {
	if (fat_type==0) {
		return;
	}
	if (lcdit.emptydir) {
		return;
	}
	if (fat_prev_entry(&lcdit)==0) {
		fat_lastpos(&lcdit);
	}
}

void fat_key_dirup(void) {
	if (fat_type==0) {
		return;
	}
	if (lcdit.fcluster != atari_clust) {
		lcdit.fcluster = fat_get_parent(lcdit.fcluster);
	}
	fat_firstpos(&lcdit);
}

void fat_key_clear(void) {
	if (fat_type==0) {
		return;
	}
	memset(files+cfno,0,sizeof(fileentry));
	memcpy_P(files[cfno].name,empty_file,39);
	fat_file_changed(cfno);
	sio_check_disk(cfno);
}

void fat_key_enter(void) {
	if (fat_type==0) {
		return;
	}
	if (lcdit.ftype>FTYPE_DIR) {	// file
		memcpy(files[cfno].name,lcdit.name,39);
		files[cfno].epos = lcdit.epos;
		files[cfno].fcluster = fat_get_fcluster(&(lcdit.epos));
		files[cfno].size = fat_get_size(&(lcdit.epos));
		files[cfno].ftype = lcdit.ftype;
		fat_file_changed(cfno);
		sio_check_disk(cfno);
	} else if (lcdit.ftype==FTYPE_DIR) {	// dir
		lcdit.fcluster = fat_get_fcluster(&(lcdit.epos));
		fat_firstpos(&lcdit);
	}
}

void fat_key_nextdisk(void) {
	if (fat_type==0) {
		return;
	}
	cfno++;
#ifdef EIGHTDISKS
	if (cfno==8) {
#else
	if (cfno==4) {
#endif
		cfno=0;
	}
}

void fat_key_prevdisk(void) {
	if (fat_type==0) {
		return;
	}
	if (cfno==0) {
#ifdef EIGHTDISKS
		cfno=7;
#else
		cfno=3;
#endif
	} else {
		cfno--;
	}
}

void fat_key_swapdisks(void) {
	uint8_t i,x,*p1,*p2;
	if (fat_type==0) {
		return;
	}
	p1 = (uint8_t*)files;
	p2 = (uint8_t*)(files+1);
	for (i=0 ; i<sizeof(fileentry) ; i++) {
		x = p1[i];
		p1[i] = p2[i];
		p2[i] = x;
	}
	fat_file_changed(0);
	sio_check_disk(0);
	fat_file_changed(1);
	sio_check_disk(1);
	//if (cfno==0 || cfno==1) {
	//	fat_refresh();
	//}
}

void fat_refresh(void) {
	if (fat_type==0) {
		return;
	}
	interface_show_fs_info(cfno,files[cfno].name,(lcdit.ftype==FTYPE_DIR)?1:0,lcdit.name);
}

/* ----------------------------------------------------- */


/* sio cmd */

int8_t fat_sio_getdiskinfo(uint8_t *fbuff,uint8_t dno) {
	if (fat_type==0) {
		return -1;
	}
	memcpy(fbuff,files[dno].name,39);
	fbuff[39]=files[dno].ftype;
	GET_LONG(fbuff+40) = files[dno].fcluster;
	GET_LONG(fbuff+44) = files[dno].size;
	GET_LONG(fbuff+48) = files[dno].epos.cluster;
	fbuff[52] = files[dno].epos.secpos;
	fbuff[53] = files[dno].epos.pos;
	return 1;
}

int8_t fat_sio_setdiskinfo(uint8_t *fbuff,uint8_t dno) {
	entrypos epos;
	uint32_t fcluster,size;
	if (fat_type==0) {
		return -1;
	}
	if (fbuff[39]<=FTYPE_DIR) {
		return -1;	// not a file
	}
	fcluster = GET_LONG(fbuff+40);
	size = GET_LONG(fbuff+44);
	epos.cluster = GET_LONG(fbuff+48);
	epos.secpos = fbuff[52];
	epos.pos = fbuff[53];
	if (fcluster!=fat_get_fcluster(&epos) || size!=fat_get_size(&epos)) {
		return -1;
	}
	memcpy(files[dno].name,fbuff,39);
	files[dno].epos = epos;
	files[dno].fcluster = fcluster;
	files[dno].size = size;
	files[dno].ftype = fbuff[39];	
	fat_file_changed(dno);
	sio_check_disk(dno);
	fat_refresh();
	return 1;
}

int8_t fat_sio_diskoff(uint8_t dno) {
	if (fat_type==0) {
		return -1;
	}
	memset(files+dno,0,sizeof(fileentry));
	memcpy_P(files[dno].name,empty_file,39);
	fat_file_changed(dno);
	sio_check_disk(dno);
	fat_refresh();
	return 1;
}

int8_t fat_sio_getnextentry(uint8_t *fbuff,uint8_t firstflag) {
	memset(fbuff,0,54);
	if (fat_type==0) {
		return -1;
	}
	if (cfgit.emptydir) {
		return -1;
	}
	if (firstflag) {
		fat_resetpos(&cfgit);
	}
	if (fat_next_entry(&cfgit)==0) {
		fat_resetpos(&cfgit);
		return 0;
	}
	memcpy(fbuff,cfgit.name,39);
	fbuff[39]=cfgit.ftype;
	GET_LONG(fbuff+40)=fat_get_fcluster(&(cfgit.epos));
	if (cfgit.ftype>FTYPE_DIR) {
		GET_LONG(fbuff+44)=fat_get_size(&(cfgit.epos));
	} else {
		GET_LONG(fbuff+44)=0;
	}
	GET_LONG(fbuff+48)=cfgit.epos.cluster;
	fbuff[52]=cfgit.epos.secpos;
	fbuff[53]=cfgit.epos.pos;
	return 1;
}

int8_t fat_sio_enterdir(uint8_t *fbuff) {
	entrypos epos;
	uint32_t fcluster;
	if (fat_type==0) {
		return -1;
	}
	if (fbuff[39]!=FTYPE_DIR) {
		return -1;	// not a directory
	}
	fcluster = GET_LONG(fbuff+40);
	epos.cluster = GET_LONG(fbuff+48);
	epos.secpos = fbuff[52];
	epos.pos = fbuff[53];
	if (GET_LONG(fbuff+44)!=0 || fcluster!=fat_get_fcluster(&epos)) {
		return -1;
	}
	cfgit.fcluster = fcluster;
	fat_resetpos(&cfgit);
	return 1;
}

int8_t fat_sio_dirup(void) {
	if (fat_type==0) {
		return -1;
	}
	if (cfgit.fcluster != atari_clust) {
		cfgit.fcluster = fat_get_parent(cfgit.fcluster);
	}
	fat_resetpos(&cfgit);
	return 1;
}

/*    */


typedef void(*fn)(void);

void fat_firmware_update(void) {
	uint8_t i,j;
	uint32_t cluster;
	union {
		uint32_t l;
		uint8_t b[4];
	} sector;
	uint8_t *eeprptr;
	fn updater_start = (fn)0x3C00;	// AVR program memory is adressed in words, so 0x3C00 in words means 0x7800 in bytes !!!

	if (fat_type==0 || firmware_clust==0 || firmware_clust==CLUST_INCORRECT) {
		return;
	}
	interface_firmware_update();
	lcd_set_cursor();
	cluster = firmware_clust;
	eeprptr = (uint8_t*)8;
	j=0;
	do {
		sector.l = fat_clusttosect(cluster);
		for (i=0 ; i<cluster_size ; i++) {
			eeprom_write_byte(eeprptr++,sector.b[0]);
			eeprom_write_byte(eeprptr++,sector.b[1]);
			eeprom_write_byte(eeprptr++,sector.b[2]);
			eeprom_write_byte(eeprptr++,sector.b[3]);
			sector.l++;
			j++;
			if (j==(0x3C00/512)) {
				lcd_next_hash();
			}
			if (j==(0x7800/512)) {
				lcd_next_hash();
				eeprom_write_byte((uint8_t*)0,'S');
				eeprom_write_byte((uint8_t*)1,'I');
				eeprom_write_byte((uint8_t*)2,'O');
				eeprom_write_byte((uint8_t*)3,'2');
				eeprom_write_byte((uint8_t*)4,'S');
				eeprom_write_byte((uint8_t*)5,'D');
				eeprom_write_byte((uint8_t*)6,'F');
				eeprom_write_byte((uint8_t*)7,'W');
				eeprom_busy_wait();
				updater_start();
			}
		}
		cluster = fat_nextcluster(cluster);
	} while (cluster!=CLUST_FREE && cluster!=CLUST_EOF && cluster!=CLUST_BAD && cluster!=CLUST_INCORRECT);
}

//const uint8_t atari_name[12]="ATARI      ";
//const uint8_t config_name[12]="SIO2SD  CFG";
//const uint8_t firmware_name[12]="SIO2SD  BIN";

uint8_t fat_entry_check(uint32_t sect,uint8_t pos) {
	uint8_t *bptr;
	uint32_t fsize;
	fat_readsector(sect);
	bptr = buff+((uint16_t)pos<<5);
	if (bptr[11]&0x10 && strncmp_P((char*)bptr,atari_name,11)==0) {
		if (fat_type==3) {
			atari_clust = (((uint32_t)(GET_SHORT(bptr+20)))<<16)+GET_SHORT(bptr+26);
		} else {
			atari_clust = GET_SHORT(bptr+26);
		}
		if (cfg_clust>0 && firmware_clust>0) {
			return 1;	// found all
		}
	} else if ((bptr[11]&0x1E)==0 && strncmp_P((char*)bptr,config_name,11)==0) {
		if (fat_type==3) {
			cfg_clust = (((uint32_t)(GET_SHORT(bptr+20)))<<16)+GET_SHORT(bptr+26);
		} else {
			cfg_clust = GET_SHORT(bptr+26);
		}
		if (cfg_clust==0) {
			cfg_clust = fat_nextfree(2);
			if (cfg_clust!=CLUST_INCORRECT) {
				fat_setnextcluster(cfg_clust,CLUST_EOF);
				fat_readsector(sect);	// reread sector - important
				*(uint16_t*)(bptr+26) = cfg_clust;
				if (fat_type==3) {
					*(uint16_t*)(bptr+20)=cfg_clust>>16;
				}
				*(uint32_t*)(bptr+28) = CFGSIZE;
				fat_updatesector();
				fat_store_index();	// store empty
			}
		} else {
			fsize = GET_LONG(bptr+28);
			if (fsize<CFGSIZE) {
				*(uint32_t*)(bptr+28) = CFGSIZE;
				fat_updatesector();
			}
		}
		if (atari_clust>0 && firmware_clust>0) {
			return 1;
		}
	} else if ((bptr[11]&0x1E)==0 && strncmp_P((char*)bptr,firmware_name,11)==0) {
		fsize = GET_LONG(bptr+28);
		if (fsize==0x7800) {
			if (fat_type==3) {
				firmware_clust = (((uint32_t)(GET_SHORT(bptr+20)))<<16)+GET_SHORT(bptr+26);
			} else {
				firmware_clust = GET_SHORT(bptr+26);
			}
		} else {
			firmware_clust = CLUST_INCORRECT;
		}
		if (atari_clust>0 && cfg_clust>0) {
			return 1;
		}
//	} else if (*bptr==0xE5 && bptr[26]==0 && bptr[27]==0 && bptr[20]==0 && bptr[21]==0) {
//		fdelsect = sect;
//		fdelpos = pos;
	} else if (*bptr==0) {
		if (atari_clust>0 && cfg_clust==0) {
			cfg_clust = fat_nextfree(2);
			if (cfg_clust!=CLUST_INCORRECT) {
				fat_setnextcluster(cfg_clust,CLUST_EOF);
				fat_readsector(sect);
				memcpy_P(bptr,config_name,11);
				bptr[11]=0x20;
				bptr[12]=0x00;	// LowerCase (0x08 - BASE , 0x10 - EXT)
				bptr[13]=0x00;  // hundredth of seconds in CTime
				bptr[14]=0x00;  // ctime
				bptr[15]=0x00;
				bptr[16]=0x21;  // cdata
				bptr[17]=0x36;
				bptr[18]=0x21;  // adate 7:4:5 (y:m:d)
				bptr[19]=0x36;
				if (fat_type==3) {
					*(uint16_t*)(bptr+20) = cfg_clust>>16;
				} else {
					bptr[20]=0;
					bptr[21]=0;
				}
				bptr[22]=0x00;  // mtime 5:6:5 (H:M:S/2)
				bptr[23]=0x00;
				bptr[24]=0x21;  // mdate
				bptr[25]=0x36;
				*(uint16_t*)(bptr+26) = cfg_clust;
				*(uint32_t*)(bptr+28) = CFGSIZE;
				fat_updatesector();
				fat_store_index();	// store empty
			}
		}
		return 1;
	}
	return 0;
}

void fat_scan_root_dir(void) {
	uint32_t sect,acluster,cluster;
	uint8_t i,j;
	atari_clust = 0;
	cfg_clust = 0;
	firmware_clust = 0;
	if (root_type==0) {
		sect = dir_start;
		while (sect<data_start) {
			for (i=0 ; i<16 ; i++) {
				if (fat_entry_check(sect,i)) {
					return;
				}
			}
			sect++;
		}
	} else {
		cluster = dir_start;
		do {
			sect = data_start + ((cluster-2)<<clust_bits);
			for (j=0 ; j<cluster_size ; j++) {
				for (i=0 ; i<16 ; i++) {
					if (fat_entry_check(sect,i)) {
						return;
					}
				}
				sect++;
			}
			acluster = cluster;
			cluster = fat_nextcluster(cluster);
			if (cluster==CLUST_EOF) {
				cluster = fat_nextfree(acluster);
				if (cluster==CLUST_INCORRECT) {
					cluster = fat_nextfree(2);
				}
				if (cluster!=CLUST_INCORRECT) {
					fat_setnextcluster(acluster,cluster);
					fat_setnextcluster(cluster,CLUST_EOF);
					sect = data_start + ((cluster-2)<<clust_bits);
					for (j=0 ; j<cluster_size ; j++) {
						fat_erasesector(sect);
						sect++;
					}
				}
			}
		} while (cluster!=CLUST_EOF && cluster!=CLUST_FREE && cluster!=CLUST_BAD && cluster!=CLUST_INCORRECT);
	}
	return;
}


int8_t fat_init(uint8_t setupmode) {
	uint8_t i;
	fat_init_state();
	memset(fatcache,0,sizeof(fatcache));
	for (i=0 ; i<FILES_IN_CACHE ; i++) {
		cachefno[i]=0xFF;
		cacheprio[i]=i;
	}
	cfno=0;
	if (fat_partition_test()) {
		if (fat_fsinit()) {
			if (fat_type==4) {
				fat_type=0;
				if (setupmode==0) {
					interface_too_many_clusters();
				}
			} else {
				fat_scan_root_dir();
				if (atari_clust==0) {
					fat_type=0;
					if (setupmode==0) {
						interface_card_atari_not_found();
					}
				} else {
					if (setupmode==0) {
						fat_load_index();
						lcdit.fcluster = atari_clust;
						fat_firstpos(&lcdit);
						cfgit.fcluster = atari_clust;
						fat_resetpos(&cfgit);
						fat_refresh();
#ifdef EIGHTDISKS
						for (i=0 ; i<8 ; i++) {
#else
						for (i=0 ; i<4 ; i++) {
#endif
							files[i].changed=1;
							sio_check_disk(i);
						}
						sio_fat_status(1);
						interface_card_fat_type(fat_type);
					}
					return 1;
				}
			}
		} else {
			if (setupmode==0) {
				interface_card_unknown_format();
			}
		}
	} else {
		if (setupmode==0) {
			interface_card_no_partition();
		}
	}
	return 0;
}

void fat_removed(uint8_t setupmode) {
	fat_init_state();
	if (setupmode==0) {
		interface_no_card();
	}
}

